#define DOCTEST_CONFIG_IMPLEMENT
#define NOMINMAX
#include "support/canonical_hash.hpp"
#include <chrono>
#include <doctest/doctest.h>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <set>
#include <windows.h>

namespace {
using Json = nlohmann::json;
std::filesystem::path executable, evidence;
std::wstring quote(std::wstring_view s) {
  std::wstring out = L"\"";
  std::size_t slashes{};
  for (auto c : s) {
    if (c == L'\\') {
      ++slashes;
      continue;
    }
    out.append(c == L'\"' ? slashes * 2 + 1 : slashes, L'\\');
    slashes = 0;
    out.push_back(c);
  }
  out.append(slashes * 2, L'\\');
  return out + L'\"';
}
std::string read(const std::filesystem::path &p) {
  std::ifstream s(p, std::ios::binary);
  return {std::istreambuf_iterator<char>{s}, std::istreambuf_iterator<char>{}};
}
struct ProcessReport {
  Json value;
  std::uint64_t creation;
  std::uint32_t pid;
};
ProcessReport run(const std::string &name, bool complete, bool action,
                  const std::wstring &observer) {
  const auto output = evidence / (name + ".json"),
             errors = evidence / (name + ".stderr");
  SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
  HANDLE out =
      CreateFileW(output.c_str(), GENERIC_WRITE, FILE_SHARE_READ, &security,
                  CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  HANDLE err =
      CreateFileW(errors.c_str(), GENERIC_WRITE, FILE_SHARE_READ, &security,
                  CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  REQUIRE(out != INVALID_HANDLE_VALUE);
  REQUIRE(err != INVALID_HANDLE_VALUE);
  std::wstring command =
      quote(executable.wstring()) + L" demo --backend " +
      (complete ? L"synthetic.complete" : L"synthetic.minimal") +
      L" --scenario " + (action ? L"action" : L"neutral") +
      L" --frames 3000 --observer " + observer;
  STARTUPINFOW startup{};
  startup.cb = sizeof(startup);
  startup.dwFlags = STARTF_USESTDHANDLES;
  startup.hStdOutput = out;
  startup.hStdError = err;
  startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
  PROCESS_INFORMATION process{};
  const auto created =
      CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr, TRUE,
                     CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process);
  CloseHandle(out);
  CloseHandle(err);
  REQUIRE(created != FALSE);
  FILETIME born{}, exited{}, kernel{}, user{};
  REQUIRE(GetProcessTimes(process.hProcess, &born, &exited, &kernel, &user) !=
          FALSE);
  const auto creation =
      (std::uint64_t(born.dwHighDateTime) << 32) | born.dwLowDateTime;
  const auto waited = WaitForSingleObject(process.hProcess, 60000);
  if (waited != WAIT_OBJECT_0) {
    TerminateProcess(process.hProcess, 124);
    WaitForSingleObject(process.hProcess, 1000);
  }
  DWORD code{};
  GetExitCodeProcess(process.hProcess, &code);
  const auto pid = process.dwProcessId;
  CloseHandle(process.hThread);
  CloseHandle(process.hProcess);
  INFO(name);
  INFO(read(errors));
  REQUIRE(waited == WAIT_OBJECT_0);
  REQUIRE(code == 0);
  return {Json::parse(read(output)), creation, pid};
}
} // namespace
TEST_CASE("Every FX-01 demo completes 3000 frames in fresh processes against "
          "an independent full oracle") {
  std::filesystem::create_directories(evidence);
  std::set<std::pair<std::uint32_t, std::uint64_t>> processes;
  Json summary = {{"schema", 1},
                  {"frame_count", 3000},
                  {"runs", Json::array()},
                  {"first_divergence", nullptr}};
  for (const bool complete : {false, true})
    for (const bool action : {false, true}) {
      std::vector<independent_oracle::Digests> oracle;
      oracle.reserve(3000);
      for (unsigned frame = 1; frame <= 3000; ++frame)
        oracle.push_back(independent_oracle::frame(complete, action, frame));
      Json baseline;
      const std::vector<std::wstring> modes =
          complete ? std::vector<std::wstring>{L"off", L"two-readers"}
                   : std::vector<std::wstring>{L"off"};
      for (const auto &mode : modes)
        for (unsigned repeat = 0; repeat < 2; ++repeat) {
          const auto name = std::string(complete ? "complete" : "minimal") +
                            "-" + (action ? "action" : "neutral") + "-" +
                            std::string(mode.begin(), mode.end()) + "-" +
                            std::to_string(repeat);
          auto report = run(name, complete, action, mode);
          auto &value = report.value;
          INFO(name);
          REQUIRE(processes.emplace(report.pid, report.creation).second);
          CHECK(value.at("pid") == report.pid);
          CHECK(value.at("process_creation_time_utc_100ns") == report.creation);
          REQUIRE(value.at("requested_frames") == 3000);
          REQUIRE(value.at("completed_frames") == 3000);
          REQUIRE(value.at("frames").size() == 3000);
          CHECK(value.at("eof_code") == TESSERA_END_OF_TRACE);
          CHECK(value.at("initial_state") ==
                independent_oracle::frame(complete, false, 0).state);
          const auto metadata = fx01::backend(complete);
          const auto content = fx01::content(complete ? 2 : 1);
          const auto config = fx01::config(metadata, fx01::memory());
          std::vector<unsigned> masks(3000);
          if (action)
            std::fill(masks.begin() + 300, masks.begin() + 600, 32);
          CHECK(value.at("content") == fx01::hex(content));
          CHECK(value.at("config") == fx01::hex(config));
          CHECK(value.at("trace") ==
                fx01::hex(fx01::trace(content, config, masks)));
          CHECK(value.at("clock").at("tick_hz_num") == 60000);
          CHECK(value.at("clock").at("tick_hz_den") == 1);
          CHECK(value.at("observer").at("snapshot_reads") ==
                (mode == L"off" ? 0 : 6000));
          CHECK(value.at("observer").at("event_batches") ==
                (mode == L"off" ? 0 : 6000));
          for (const auto stage : {"step", "canonical_hashes", "observation"}) {
            const auto &series = value.at("costs").at(stage);
            CHECK(series.at("count") == 3000);
            CHECK(series.at("samples").size() == 3000);
            CHECK(series.at("p50").get<double>() >= 0);
            CHECK(series.at("p95").get<double>() >=
                  series.at("p50").get<double>());
            CHECK(series.at("p99").get<double>() >=
                  series.at("p95").get<double>());
          }
          for (std::size_t index = 0; index < 3000; ++index) {
            INFO("frame=" << index + 1);
            const auto &actual = value.at("frames")[index];
            REQUIRE(actual.at("frame") == index + 1);
            const auto &expected = oracle[index];
            if (actual.at("state") != expected.state ||
                actual.at("video") != expected.video ||
                actual.at("audio") != expected.audio) {
              summary["first_divergence"] = {{"run", name},
                                             {"frame", index + 1}};
              std::ofstream(evidence / "summary.json") << summary.dump(2);
              FAIL("Independent canonical oracle divergence");
            }
          }
          if (baseline.is_null())
            baseline = value.at("frames");
          else
            CHECK(value.at("frames") == baseline);
          summary["runs"].push_back({{"name", name},
                                     {"pid", report.pid},
                                     {"creation_utc_100ns", report.creation},
                                     {"frames", 3000},
                                     {"divergences", 0}});
        }
    }
  CHECK(processes.size() == 12);
  summary["completed_frames"] = 36000;
  summary["divergences"] = 0;
  std::ofstream(evidence / "summary.json") << summary.dump(2);
}
int main(int argc, char **argv) {
  if (argc != 3)
    return 2;
  executable = argv[1];
  evidence = argv[2];
  doctest::Context context;
  context.setOption("no-breaks", true);
  return context.run();
}
