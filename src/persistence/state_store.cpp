#include "persistence/state_store.hpp"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <atomic>
#include <new>

namespace tessera::persistence {
namespace {
constexpr std::uint64_t maximum_bytes = 64 * 1024 * 1024;
std::atomic<std::uint64_t> nonce{1};

StoreResult failure(tessera_status code, std::uint32_t phase,
                    std::uint32_t os_code) {
  return {code, phase, TESSERA_OS_WIN32, os_code};
}

StoreResult os_failure(std::uint32_t phase, DWORD error) {
  const auto code =
      error == ERROR_DISK_FULL || error == ERROR_HANDLE_DISK_FULL
          ? TESSERA_IO_NO_SPACE
      : error == ERROR_ACCESS_DENIED || error == ERROR_SHARING_VIOLATION
          ? TESSERA_IO_ACCESS_DENIED
      : phase == TESSERA_PHASE_PUBLISH ? TESSERA_IO_PUBLISH_FAILED
                                       : TESSERA_IO_OTHER;
  return failure(code, phase, error);
}

std::filesystem::path temporary_path(const std::filesystem::path &path) {
  auto temporary = path;
  temporary += L".tmp-" + std::to_wstring(GetCurrentProcessId()) + L"-" +
               std::to_wstring(nonce.fetch_add(1));
  return temporary;
}
} // namespace

StoreResult save_checkpoint(const std::filesystem::path &path,
                            std::span<const std::uint8_t> bytes,
                            StoreFault fault) {
  if (bytes.empty() || bytes.size() > maximum_bytes || path.empty())
    return {TESSERA_INVALID_ARGUMENT, TESSERA_PHASE_VALIDATE, TESSERA_OS_NONE,
            0};
  if (fault == StoreFault::write_denied)
    return failure(TESSERA_IO_ACCESS_DENIED, TESSERA_PHASE_WRITE,
                   ERROR_ACCESS_DENIED);
  const auto temporary = temporary_path(path);
  const HANDLE file = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr,
                                  CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE)
    return os_failure(TESSERA_PHASE_WRITE, GetLastError());

  StoreResult result{};
  const auto size = fault == StoreFault::partial_write_interruption
                        ? bytes.size() / 2
                        : bytes.size();
  DWORD written{};
  const bool write_ok = WriteFile(file, bytes.data(), static_cast<DWORD>(size),
                                  &written, nullptr) &&
                        written == size;
  const auto write_error = write_ok ? ERROR_SUCCESS : GetLastError();
  const bool flush_ok = write_ok && FlushFileBuffers(file);
  const auto flush_error = flush_ok ? ERROR_SUCCESS : GetLastError();
  CloseHandle(file);
  if (fault == StoreFault::partial_write_interruption)
    return failure(TESSERA_IO_OTHER, TESSERA_PHASE_WRITE,
                   ERROR_OPERATION_ABORTED);
  if (!write_ok || !flush_ok)
    result =
        os_failure(TESSERA_PHASE_WRITE, write_ok ? flush_error : write_error);
  else if (fault == StoreFault::write_no_space)
    result = failure(TESSERA_IO_NO_SPACE, TESSERA_PHASE_WRITE, ERROR_DISK_FULL);
  if (!result.ok()) {
    DeleteFileW(temporary.c_str());
    return result;
  }

  if (fault == StoreFault::publish_denied) {
    DeleteFileW(temporary.c_str());
    return failure(TESSERA_IO_PUBLISH_FAILED, TESSERA_PHASE_PUBLISH,
                   ERROR_ACCESS_DENIED);
  }
  const auto attributes = GetFileAttributesW(path.c_str());
  const bool exists = attributes != INVALID_FILE_ATTRIBUTES;
  const DWORD attributes_error = exists ? ERROR_SUCCESS : GetLastError();
  if (!exists && attributes_error != ERROR_FILE_NOT_FOUND &&
      attributes_error != ERROR_PATH_NOT_FOUND) {
    DeleteFileW(temporary.c_str());
    return os_failure(TESSERA_PHASE_PUBLISH, attributes_error);
  }
  const bool published =
      exists ? bool(ReplaceFileW(path.c_str(), temporary.c_str(), nullptr, 0,
                                 nullptr, nullptr))
             : bool(MoveFileExW(temporary.c_str(), path.c_str(),
                                MOVEFILE_WRITE_THROUGH));
  if (!published) {
    const auto error = GetLastError();
    DeleteFileW(temporary.c_str());
    return os_failure(TESSERA_PHASE_PUBLISH, error);
  }
  return {TESSERA_OK, TESSERA_PHASE_PUBLISH, TESSERA_OS_NONE, 0};
}

StoreResult load_checkpoint(const std::filesystem::path &path, Bytes &out,
                            StoreFault fault) {
  if (path.empty())
    return {TESSERA_INVALID_ARGUMENT, TESSERA_PHASE_VALIDATE, TESSERA_OS_NONE,
            0};
  if (fault == StoreFault::read_denied)
    return failure(TESSERA_IO_ACCESS_DENIED, TESSERA_PHASE_READ,
                   ERROR_ACCESS_DENIED);
  const HANDLE file =
      CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                  OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE)
    return os_failure(TESSERA_PHASE_READ, GetLastError());
  LARGE_INTEGER extent{};
  const bool size_ok = GetFileSizeEx(file, &extent);
  const auto size_error = size_ok ? ERROR_SUCCESS : GetLastError();
  if (!size_ok || extent.QuadPart <= 0 ||
      static_cast<std::uint64_t>(extent.QuadPart) > maximum_bytes) {
    CloseHandle(file);
    return size_ok ? StoreResult{TESSERA_CHECKPOINT_INVALID, TESSERA_PHASE_READ,
                                 TESSERA_OS_NONE, 0}
                   : os_failure(TESSERA_PHASE_READ, size_error);
  }
  Bytes candidate;
  try {
    candidate.resize(static_cast<std::size_t>(extent.QuadPart));
  } catch (const std::bad_alloc &) {
    CloseHandle(file);
    return {TESSERA_OUT_OF_MEMORY, TESSERA_PHASE_READ, TESSERA_OS_NONE, 0};
  }
  DWORD read{};
  const bool read_ok =
      ReadFile(file, candidate.data(), static_cast<DWORD>(candidate.size()),
               &read, nullptr) &&
      read == candidate.size();
  const auto read_error = read_ok ? ERROR_SUCCESS : GetLastError();
  CloseHandle(file);
  if (!read_ok)
    return os_failure(TESSERA_PHASE_READ, read_error);
  out.swap(candidate);
  return {TESSERA_OK, TESSERA_PHASE_READ, TESSERA_OS_NONE, 0};
}

} // namespace tessera::persistence
#else // POSIX

#include <cerrno>
#include <fcntl.h>
#include <new>
#include <sys/stat.h>
#include <unistd.h>

#include <atomic>
#include <string>

namespace tessera::persistence {
namespace {
constexpr std::uint64_t maximum_bytes = 64 * 1024 * 1024;
std::atomic<std::uint64_t> nonce{1};

StoreResult failure(tessera_status code, std::uint32_t phase,
                    std::uint32_t os_code) {
  return {code, phase, TESSERA_OS_POSIX, os_code};
}

StoreResult os_failure(std::uint32_t phase, int error) {
  const auto code = error == ENOSPC || error == EDQUOT ? TESSERA_IO_NO_SPACE
                    : error == EACCES || error == EPERM || error == EROFS
                        ? TESSERA_IO_ACCESS_DENIED
                    : phase == TESSERA_PHASE_PUBLISH ? TESSERA_IO_PUBLISH_FAILED
                                                     : TESSERA_IO_OTHER;
  return failure(code, phase, static_cast<std::uint32_t>(error));
}

std::filesystem::path temporary_path(const std::filesystem::path &path) {
  auto temporary = path;
  temporary += ".tmp-" + std::to_string(::getpid()) + "-" +
               std::to_string(nonce.fetch_add(1));
  return temporary;
}

bool write_all(int fd, const std::uint8_t *data, std::size_t size) {
  while (size > 0) {
    const auto n = ::write(fd, data, size);
    if (n < 0) {
      if (errno == EINTR)
        continue;
      return false;
    }
    data += n;
    size -= static_cast<std::size_t>(n);
  }
  return true;
}
} // namespace

StoreResult save_checkpoint(const std::filesystem::path &path,
                            std::span<const std::uint8_t> bytes,
                            StoreFault fault) {
  if (bytes.empty() || bytes.size() > maximum_bytes || path.empty())
    return {TESSERA_INVALID_ARGUMENT, TESSERA_PHASE_VALIDATE, TESSERA_OS_NONE,
            0};
  if (fault == StoreFault::write_denied)
    return failure(TESSERA_IO_ACCESS_DENIED, TESSERA_PHASE_WRITE, EACCES);
  const auto temporary = temporary_path(path);
  const int fd = ::open(temporary.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0644);
  if (fd < 0)
    return os_failure(TESSERA_PHASE_WRITE, errno);

  const auto size = fault == StoreFault::partial_write_interruption
                        ? bytes.size() / 2
                        : bytes.size();
  const bool write_ok = write_all(fd, bytes.data(), size);
  int error = write_ok ? 0 : errno;
  const bool flush_ok = write_ok && ::fsync(fd) == 0;
  if (write_ok && !flush_ok)
    error = errno;
  ::close(fd);
  if (fault == StoreFault::partial_write_interruption) {
    ::unlink(temporary.c_str());
    return failure(TESSERA_IO_OTHER, TESSERA_PHASE_WRITE, ECANCELED);
  }
  StoreResult result{};
  if (!write_ok || !flush_ok)
    result = os_failure(TESSERA_PHASE_WRITE, error);
  else if (fault == StoreFault::write_no_space)
    result = failure(TESSERA_IO_NO_SPACE, TESSERA_PHASE_WRITE, ENOSPC);
  if (!result.ok()) {
    ::unlink(temporary.c_str());
    return result;
  }
  if (fault == StoreFault::publish_denied) {
    ::unlink(temporary.c_str());
    return failure(TESSERA_IO_PUBLISH_FAILED, TESSERA_PHASE_PUBLISH, EACCES);
  }
  // rename(2) replaces the destination atomically on POSIX.
  if (::rename(temporary.c_str(), path.c_str()) != 0) {
    const int rename_error = errno;
    ::unlink(temporary.c_str());
    return os_failure(TESSERA_PHASE_PUBLISH, rename_error);
  }
  return {TESSERA_OK, TESSERA_PHASE_PUBLISH, TESSERA_OS_NONE, 0};
}

StoreResult load_checkpoint(const std::filesystem::path &path, Bytes &out,
                            StoreFault fault) {
  if (path.empty())
    return {TESSERA_INVALID_ARGUMENT, TESSERA_PHASE_VALIDATE, TESSERA_OS_NONE,
            0};
  if (fault == StoreFault::read_denied)
    return failure(TESSERA_IO_ACCESS_DENIED, TESSERA_PHASE_READ, EACCES);
  const int fd = ::open(path.c_str(), O_RDONLY);
  if (fd < 0)
    return os_failure(TESSERA_PHASE_READ, errno);
  struct stat info{};
  if (::fstat(fd, &info) != 0) {
    const int stat_error = errno;
    ::close(fd);
    return os_failure(TESSERA_PHASE_READ, stat_error);
  }
  if (info.st_size <= 0 ||
      static_cast<std::uint64_t>(info.st_size) > maximum_bytes) {
    ::close(fd);
    return {TESSERA_CHECKPOINT_INVALID, TESSERA_PHASE_READ, TESSERA_OS_NONE, 0};
  }
  Bytes candidate;
  try {
    candidate.resize(static_cast<std::size_t>(info.st_size));
  } catch (const std::bad_alloc &) {
    ::close(fd);
    return {TESSERA_OUT_OF_MEMORY, TESSERA_PHASE_READ, TESSERA_OS_NONE, 0};
  }
  std::size_t done = 0;
  int read_error = 0;
  while (done < candidate.size()) {
    const auto n = ::read(fd, candidate.data() + done, candidate.size() - done);
    if (n < 0 && errno == EINTR)
      continue;
    if (n <= 0) {
      read_error = n < 0 ? errno : EIO;
      break;
    }
    done += static_cast<std::size_t>(n);
  }
  ::close(fd);
  if (read_error != 0)
    return os_failure(TESSERA_PHASE_READ, read_error);
  out.swap(candidate);
  return {TESSERA_OK, TESSERA_PHASE_READ, TESSERA_OS_NONE, 0};
}

} // namespace tessera::persistence
#endif
