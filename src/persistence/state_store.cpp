#include "persistence/state_store.hpp"

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
  const auto code = error == ERROR_DISK_FULL || error == ERROR_HANDLE_DISK_FULL
                        ? TESSERA_IO_NO_SPACE
                        : error == ERROR_ACCESS_DENIED ||
                                  error == ERROR_SHARING_VIOLATION
                              ? TESSERA_IO_ACCESS_DENIED
                              : phase == TESSERA_PHASE_PUBLISH
                                    ? TESSERA_IO_PUBLISH_FAILED
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
    result = os_failure(TESSERA_PHASE_WRITE,
                        write_ok ? flush_error : write_error);
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
  const HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
                                  nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                                  nullptr);
  if (file == INVALID_HANDLE_VALUE)
    return os_failure(TESSERA_PHASE_READ, GetLastError());
  LARGE_INTEGER extent{};
  const bool size_ok = GetFileSizeEx(file, &extent);
  const auto size_error = size_ok ? ERROR_SUCCESS : GetLastError();
  if (!size_ok || extent.QuadPart <= 0 ||
      static_cast<std::uint64_t>(extent.QuadPart) > maximum_bytes) {
    CloseHandle(file);
    return size_ok
               ? StoreResult{TESSERA_CHECKPOINT_INVALID, TESSERA_PHASE_READ,
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
  const bool read_ok = ReadFile(file, candidate.data(),
                                static_cast<DWORD>(candidate.size()), &read,
                                nullptr) &&
                       read == candidate.size();
  const auto read_error = read_ok ? ERROR_SUCCESS : GetLastError();
  CloseHandle(file);
  if (!read_ok)
    return os_failure(TESSERA_PHASE_READ, read_error);
  out.swap(candidate);
  return {TESSERA_OK, TESSERA_PHASE_READ, TESSERA_OS_NONE, 0};
}

} // namespace tessera::persistence
