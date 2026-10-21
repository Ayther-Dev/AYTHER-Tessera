#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>
#include <nlohmann/json.hpp>
#include <windows.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

namespace {
using Json = nlohmann::json;
std::filesystem::path cli_executable;

struct TemporaryDirectory {
  std::filesystem::path path;
  TemporaryDirectory() {
    const auto stamp =
        std::chrono::steady_clock::now().time_since_epoch().count();
    path = std::filesystem::temp_directory_path() /
           ("tessera-cli-" + std::to_string(GetCurrentProcessId()) + "-" +
            std::to_string(stamp));
    std::filesystem::create_directories(path);
  }
  ~TemporaryDirectory() {
    std::error_code ignored;
    std::filesystem::remove_all(path, ignored);
  }
};

std::wstring quote(std::wstring_view value) {
  std::wstring result = L"\"";
  std::size_t slashes = 0;
  for (const auto ch : value) {
    if (ch == L'\\') {
      ++slashes;
      continue;
    }
    result.append(ch == L'\"' ? slashes * 2 + 1 : slashes, L'\\');
    slashes = 0;
    result.push_back(ch);
  }
  result.append(slashes * 2, L'\\');
  result.push_back(L'\"');
  return result;
}

std::string read_file(const std::filesystem::path &path) {
  std::ifstream stream(path, std::ios::binary);
  return {std::istreambuf_iterator<char>{stream},
          std::istreambuf_iterator<char>{}};
}

struct Result {
  std::uint32_t exit_code{};
  Json report;
  std::string errors;
};

Result run(const std::vector<std::wstring> &arguments) {
  TemporaryDirectory temp;
  const auto output_path = temp.path / "stdout.json";
  const auto error_path = temp.path / "stderr.txt";
  SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
  const auto output =
      CreateFileW(output_path.c_str(), GENERIC_WRITE, FILE_SHARE_READ,
                  &security, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  const auto errors =
      CreateFileW(error_path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, &security,
                  CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  REQUIRE(output != INVALID_HANDLE_VALUE);
  REQUIRE(errors != INVALID_HANDLE_VALUE);
  std::wstring command = quote(cli_executable.wstring());
  for (const auto &argument : arguments)
    command += L" " + quote(argument);
  STARTUPINFOW startup{};
  startup.cb = sizeof(startup);
  startup.dwFlags = STARTF_USESTDHANDLES;
  startup.hStdOutput = output;
  startup.hStdError = errors;
  startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
  PROCESS_INFORMATION process{};
  const auto created = CreateProcessW(cli_executable.c_str(), command.data(),
                                      nullptr, nullptr, TRUE, CREATE_NO_WINDOW,
                                      nullptr, nullptr, &startup, &process);
  CloseHandle(output);
  CloseHandle(errors);
  REQUIRE(created != FALSE);
  const auto waited = WaitForSingleObject(process.hProcess, 60000);
  if (waited != WAIT_OBJECT_0)
    TerminateProcess(process.hProcess, 124);
  DWORD exit_code = 124;
  GetExitCodeProcess(process.hProcess, &exit_code);
  CloseHandle(process.hThread);
  CloseHandle(process.hProcess);
  REQUIRE(waited == WAIT_OBJECT_0);
  const auto output_text = read_file(output_path);
  const auto error_text = read_file(error_path);
  INFO("child_exit=" << exit_code << "; stderr=" << error_text);
  REQUIRE_FALSE(output_text.empty());
  return {exit_code, Json::parse(output_text), error_text};
}

std::vector<std::wstring>
demo(std::wstring_view backend = L"synthetic.complete",
     std::wstring_view observer = L"off",
     std::wstring_view scenario = L"action") {
  return {L"demo",       L"--backend",           std::wstring(backend),
          L"--scenario", std::wstring(scenario), L"--frames",
          L"60",         L"--observer",          std::wstring(observer)};
}

std::vector<std::wstring> replay(const std::filesystem::path &fixture,
                                 const std::filesystem::path &trace,
                                 std::wstring frames = L"60") {
  return {L"replay",
          L"--backend",
          L"synthetic.complete",
          L"--content",
          (fixture / "content.bin").wstring(),
          L"--config",
          (fixture / "config.bin").wstring(),
          L"--initial-memory",
          (fixture / "memory.bin").wstring(),
          L"--trace",
          trace.wstring(),
          L"--frames",
          std::move(frames)};
}
} // namespace

TEST_CASE("CLI lists implemented test backends and detects bytes independently "
          "of filename") {
  const auto list = run({L"backend", L"list"});
  REQUIRE(list.exit_code == 0);
  REQUIRE(list.report.at("backends").size() == 3);
  CHECK(list.report.at("code") == 0);
  CHECK(list.report.at("backends")[0].at("kind") == 1);
  TemporaryDirectory directory;
  const auto fixture = directory.path / L"fixture with spaces";
  auto args = demo();
  args.insert(args.end(), {L"--fixture-dir", fixture.wstring()});
  REQUIRE(run(args).exit_code == 0);
  const auto renamed = directory.path / "misleading.jpg";
  std::filesystem::copy_file(fixture / "content.bin", renamed);
  const auto detected = run({L"detect", renamed.wstring()});
  CHECK(detected.exit_code == 0);
  CHECK(detected.report.at("outcome") == 1);
  CHECK(detected.report.at("system") == 0);
}

TEST_CASE("CLI demo completes sixty frames with lifecycle and independent "
          "identities") {
  TemporaryDirectory directory;
  const auto report_path = directory.path / "report.json";
  auto args = demo();
  args.insert(args.end(), {L"--report", report_path.wstring()});
  const auto result = run(args);
  REQUIRE(result.exit_code == 0);
  CHECK(result.report.at("requested_frames") == 60);
  CHECK(result.report.at("completed_frames") == 60);
  REQUIRE(result.report.at("frames").size() == 60);
  CHECK(result.report.at("lifecycle").at("step_completed") == 1);
  CHECK(result.report.at("lifecycle").at("reset_completed") == 0);
  CHECK(result.report.at("eof_code") == 1);
  CHECK(result.report.at("pid").get<std::uint64_t>() > 0);
  CHECK(
      result.report.at("process_creation_time_utc_100ns").get<std::uint64_t>() >
      0);
  CHECK(result.report.at("initial_state").get<std::string>().size() == 64);
  CHECK(result.report.at("content").get<std::string>().size() == 64);
  CHECK(result.report.at("config").get<std::string>().size() == 64);
  CHECK(result.report.at("trace").get<std::string>().size() == 64);
  CHECK(result.report.at("frames")[0].at("state").get<std::string>().size() ==
        64);
  CHECK(result.report.at("frames")[0].at("video").get<std::string>().size() ==
        64);
  CHECK(result.report.at("frames")[0].at("audio").get<std::string>().size() ==
        64);
  CHECK(Json::parse(read_file(report_path)) == result.report);
}

TEST_CASE("CLI action affects all three original digests and observation "
          "preserves them") {
  const auto control = run(demo(L"synthetic.complete", L"off", L"neutral"));
  const auto action = run(demo());
  REQUIRE(control.exit_code == 0);
  REQUIRE(action.exit_code == 0);
  for (const auto name : {"state", "video", "audio"}) {
    CHECK(control.report.at("frames")[29].at(name) !=
          action.report.at("frames")[29].at(name));
  }
  for (const auto observer : {L"full", L"two-readers"}) {
    const auto observed = run(demo(L"synthetic.complete", observer));
    REQUIRE(observed.exit_code == 0);
    CHECK(observed.report.at("frames") == action.report.at("frames"));
  }
  const auto rejected = run(demo(L"synthetic.minimal", L"full"));
  CHECK(rejected.exit_code == 1);
  CHECK(rejected.report.at("code") == -6);
}

TEST_CASE("CLI replays JSON and binary traces and rejects a premature EOF") {
  TemporaryDirectory directory;
  auto args = demo();
  args.insert(args.end(), {L"--fixture-dir", directory.path.wstring()});
  const auto reference = run(args);
  REQUIRE(reference.exit_code == 0);
  const auto binary = run(replay(directory.path, directory.path / "trace.bin"));
  const auto json = run(replay(directory.path, directory.path / "trace.json"));
  REQUIRE(binary.exit_code == 0);
  REQUIRE(json.exit_code == 0);
  CHECK(binary.report.at("frames") == reference.report.at("frames"));
  CHECK(json.report.at("frames") == reference.report.at("frames"));
  CHECK(binary.report.at("trace") == json.report.at("trace"));
  const auto short_trace =
      run(replay(directory.path, directory.path / "trace.bin", L"61"));
  CHECK(short_trace.exit_code == 1);
  CHECK(short_trace.report.at("code") == -17);
  CHECK(short_trace.report.at("completed_frames") == 60);
}

TEST_CASE(
    "CLI rejects malformed duplicate or incomplete JSON before execution") {
  TemporaryDirectory directory;
  auto args = demo();
  args.insert(args.end(), {L"--fixture-dir", directory.path.wstring()});
  REQUIRE(run(args).exit_code == 0);
  const auto valid = Json::parse(read_file(directory.path / "trace.json"));
  for (unsigned variant = 0; variant < 9; ++variant) {
    CAPTURE(variant);
    auto altered = valid;
    std::string text;
    if (variant == 0)
      text = "{\"trace_version\":1,\"trace_version\":1}";
    if (variant == 1)
      text = "{";
    if (variant == 2) {
      altered["frames"][0]["assignments"].erase(
          altered["frames"][0]["assignments"].begin());
      text = altered.dump();
    }
    if (variant == 3) {
      altered["frames"][1]["frame"] = 3;
      text = altered.dump();
    }
    if (variant == 4) {
      text = valid.dump();
      const auto offset = text.find("\"port\":0");
      REQUIRE(offset != std::string::npos);
      text.replace(offset, 8, "\"port\":0,\"port\":0");
    }
    if (variant == 5)
      text = "null";
    if (variant == 6)
      text = "[";
    if (variant == 7)
      text = valid.dump() + "{}";
    if (variant == 8) {
      text = valid.dump();
      const auto offset = text.find("\"value\":0");
      REQUIRE(offset != std::string::npos);
      text.replace(offset, 9, "\"value\":1.0");
    }
    const auto path =
        directory.path / ("invalid-" + std::to_string(variant) + ".json");
    {
      std::ofstream stream(path, std::ios::binary);
      stream << text;
    }
    const auto result = run(replay(directory.path, path));
    CHECK(result.exit_code == 1);
    CHECK(result.report.at("code") == -17);
    CHECK(result.report.at("completed_frames") == 0);
  }
}

int main(int argc, char **argv) {
  if (argc != 2)
    return 2;
  cli_executable = std::filesystem::path(argv[1]);
  doctest::Context context;
  context.setOption("no-breaks", true);
  return context.run();
}
