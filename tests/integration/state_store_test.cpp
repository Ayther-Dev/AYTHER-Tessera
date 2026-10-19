#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "persistence/state_store.hpp"
#include "support/fx01.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <atomic>
#include <filesystem>
#include <fstream>

namespace {
std::atomic<unsigned> test_number{1};
struct File {
  std::filesystem::path directory, path;
  File() {
    directory = std::filesystem::current_path() /
                ("tessera-state-store-" + std::to_string(GetCurrentProcessId()) +
                 "-" + std::to_string(test_number.fetch_add(1)));
    REQUIRE(std::filesystem::create_directory(directory));
    path = directory / "checkpoint.tsc";
  }
  ~File() {
    for (const auto &entry : std::filesystem::directory_iterator(directory))
      std::filesystem::remove(entry.path());
    std::filesystem::remove(directory);
  }
};
fx01::Bytes checkpoint(fx01::Session &session) {
  std::uint64_t required{};
  REQUIRE(tessera_checkpoint_export_v1(session.handle, {nullptr, 0},
                                       &required, &session.context) ==
          TESSERA_BUFFER_TOO_SMALL);
  fx01::Bytes result(static_cast<std::size_t>(required));
  fx01::expect(tessera_checkpoint_export_v1(
      session.handle, {result.data(), result.size()}, &required,
      &session.context));
  return result;
}
} // namespace

TEST_CASE("First checkpoint appears only after complete publication") {
  File file;
  fx01::Session session;
  fx01::expect(session.step());
  const auto expected = checkpoint(session);
  using namespace tessera::persistence;
  CHECK(save_checkpoint(file.path, expected, StoreFault::write_no_space).code ==
        TESSERA_IO_NO_SPACE);
  CHECK_FALSE(std::filesystem::exists(file.path));
  CHECK(save_checkpoint(file.path, expected, StoreFault::publish_denied).code ==
        TESSERA_IO_PUBLISH_FAILED);
  CHECK_FALSE(std::filesystem::exists(file.path));
  CHECK(save_checkpoint(file.path, expected).ok());
  fx01::Bytes loaded;
  CHECK(load_checkpoint(file.path, loaded).ok());
  CHECK(loaded == expected);
}

TEST_CASE("Write and publish failures preserve an existing complete checkpoint") {
  File file;
  fx01::Session session;
  fx01::expect(session.step());
  const auto old = checkpoint(session);
  fx01::expect(session.step(32));
  const auto replacement = checkpoint(session);
  using namespace tessera::persistence;
  REQUIRE(save_checkpoint(file.path, old).ok());
  const auto check_preserved = [&] {
    fx01::Bytes loaded;
    REQUIRE(load_checkpoint(file.path, loaded).ok());
    CHECK(loaded == old);
  };
  auto failure = save_checkpoint(file.path, replacement, StoreFault::write_denied);
  CHECK(failure.code == TESSERA_IO_ACCESS_DENIED);
  CHECK(failure.phase == TESSERA_PHASE_WRITE);
  CHECK(failure.os_domain == TESSERA_OS_WIN32);
  CHECK(failure.os_code == ERROR_ACCESS_DENIED);
  check_preserved();
  failure = save_checkpoint(file.path, replacement, StoreFault::write_no_space);
  CHECK(failure.code == TESSERA_IO_NO_SPACE);
  CHECK(failure.os_code == ERROR_DISK_FULL);
  check_preserved();
  failure = save_checkpoint(file.path, replacement, StoreFault::publish_denied);
  CHECK(failure.code == TESSERA_IO_PUBLISH_FAILED);
  CHECK(failure.phase == TESSERA_PHASE_PUBLISH);
  check_preserved();
  failure = save_checkpoint(file.path, replacement,
                            StoreFault::partial_write_interruption);
  CHECK(failure.code == TESSERA_IO_OTHER);
  check_preserved();
  REQUIRE(save_checkpoint(file.path, replacement).ok());
  fx01::Bytes loaded;
  REQUIRE(load_checkpoint(file.path, loaded).ok());
  CHECK(loaded == replacement);
}

TEST_CASE("Read failures preserve caller bytes and the running session") {
  File file;
  fx01::Session session;
  fx01::expect(session.step());
  fx01::expect(session.observe(76));
  const auto state = session.state();
  const auto encoded = checkpoint(session);
  using namespace tessera::persistence;
  REQUIRE(save_checkpoint(file.path, encoded).ok());
  const fx01::Bytes sentinel{1, 2, 3};
  fx01::Bytes output = sentinel;
  auto result = load_checkpoint(file.path, output, StoreFault::read_denied);
  CHECK(result.code == TESSERA_IO_ACCESS_DENIED);
  CHECK(result.phase == TESSERA_PHASE_READ);
  CHECK(result.os_code == ERROR_ACCESS_DENIED);
  CHECK(output == sentinel);
  CHECK(session.state() == state);
  CHECK(session.info().pending_transaction == 76);
  result = load_checkpoint(file.directory / "absent.tsc", output);
  CHECK(result.code == TESSERA_IO_OTHER);
  CHECK(result.os_domain == TESSERA_OS_WIN32);
  CHECK(output == sentinel);
}

TEST_CASE("A process stopped after a partial temporary write never publishes it") {
  wchar_t child_path[MAX_PATH]{};
  const auto length = GetEnvironmentVariableW(L"TESSERA_STORE_INTERRUPT_PATH",
                                              child_path, MAX_PATH);
  if (length > 0 && length < MAX_PATH) {
    const tessera::Bytes candidate(1024 * 1024, 0xA5);
    (void)tessera::persistence::save_checkpoint(
        child_path, candidate,
        tessera::persistence::StoreFault::partial_write_interruption);
    ExitProcess(73);
  }

  File file;
  wchar_t executable[MAX_PATH]{};
  REQUIRE(GetModuleFileNameW(nullptr, executable, MAX_PATH) > 0);
  for (const bool existing : {false, true}) {
    const tessera::Bytes old(128, 0x3C);
    if (existing)
      REQUIRE(tessera::persistence::save_checkpoint(file.path, old).ok());
    REQUIRE(SetEnvironmentVariableW(L"TESSERA_STORE_INTERRUPT_PATH",
                                    file.path.c_str()) != FALSE);
    std::wstring command = std::wstring(L"\"") + executable +
                           L"\" --test-case=\"A process stopped after a "
                           L"partial temporary write never publishes it\"";
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION child{};
    const auto created = CreateProcessW(executable, command.data(), nullptr,
                                        nullptr, FALSE, CREATE_NO_WINDOW,
                                        nullptr, nullptr, &startup, &child);
    REQUIRE(SetEnvironmentVariableW(L"TESSERA_STORE_INTERRUPT_PATH", nullptr) !=
            FALSE);
    REQUIRE(created != FALSE);
    const auto waited = WaitForSingleObject(child.hProcess, 30000);
    DWORD exit_code{};
    REQUIRE(GetExitCodeProcess(child.hProcess, &exit_code) != FALSE);
    CloseHandle(child.hThread);
    CloseHandle(child.hProcess);
    REQUIRE(waited == WAIT_OBJECT_0);
    CHECK(exit_code == 73);
    if (existing) {
      tessera::Bytes loaded;
      REQUIRE(tessera::persistence::load_checkpoint(file.path, loaded).ok());
      CHECK(loaded == old);
    } else {
      CHECK_FALSE(std::filesystem::exists(file.path));
    }
  }
}
