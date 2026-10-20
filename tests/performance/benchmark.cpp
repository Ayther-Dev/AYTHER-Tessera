#include "domain/wire.hpp"
#include <tessera/tessera.h>
#include <nlohmann/json.hpp>
#include <windows.h>
#include <psapi.h>

#include <array>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
namespace fs = std::filesystem;
using Json = nlohmann::json;
using Bytes = tessera::Bytes;

void require(bool condition, const char *message) {
  if (!condition) throw std::runtime_error(message);
}

template <typename T> T init() {
  T result{};
  result.header = {sizeof(T), TESSERA_ABI_MAJOR, TESSERA_ABI_MINOR};
  return result;
}

Bytes read(const fs::path &path) {
  std::ifstream file(path, std::ios::binary);
  require(file.good(), "Cannot open benchmark input");
  return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

void write(const fs::path &path, const Bytes &bytes) {
  std::ofstream file(path, std::ios::binary | std::ios::trunc);
  require(file.good(), "Cannot create benchmark fixture");
  file.write(reinterpret_cast<const char *>(bytes.data()),
             static_cast<std::streamsize>(bytes.size()));
  require(file.good(), "Cannot write benchmark fixture");
}

std::wstring quote(const fs::path &path) {
  return L"\"" + path.wstring() + L"\"";
}

std::uint64_t run(const fs::path &exe, const std::wstring &arguments,
                  const fs::path &log) {
  HANDLE output = CreateFileW(log.c_str(), GENERIC_WRITE, FILE_SHARE_READ,
                              nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL,
                              nullptr);
  require(output != INVALID_HANDLE_VALUE, "Cannot create benchmark log");
  SetHandleInformation(output, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
  STARTUPINFOW startup{};
  startup.cb = sizeof(startup);
  startup.dwFlags = STARTF_USESTDHANDLES;
  startup.hStdOutput = output;
  startup.hStdError = output;
  startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
  PROCESS_INFORMATION process{};
  auto command = quote(exe) + L" " + arguments;
  const BOOL started = CreateProcessW(exe.c_str(), command.data(), nullptr,
                                      nullptr, TRUE, CREATE_NO_WINDOW, nullptr,
                                      nullptr, &startup, &process);
  CloseHandle(output);
  require(started != FALSE, "Cannot start benchmark consumer");
  const DWORD waited = WaitForSingleObject(process.hProcess, 120000);
  if (waited != WAIT_OBJECT_0) {
    TerminateProcess(process.hProcess, 124);
    WaitForSingleObject(process.hProcess, 1000);
  }
  DWORD exit_code = 1;
  GetExitCodeProcess(process.hProcess, &exit_code);
  PROCESS_MEMORY_COUNTERS memory{};
  memory.cb = sizeof(memory);
  const auto measured = GetProcessMemoryInfo(process.hProcess, &memory,
                                              sizeof(memory));
  CloseHandle(process.hThread);
  CloseHandle(process.hProcess);
  require(waited == WAIT_OBJECT_0 && exit_code == 0,
          "Benchmark consumer failed; inspect its log");
  require(measured != FALSE, "Cannot read process peak memory");
  return memory.PeakWorkingSetSize;
}

std::map<std::string, std::string> fields(std::string line) {
  std::istringstream input(std::move(line));
  std::map<std::string, std::string> result;
  std::string word;
  input >> word;
  while (input >> word) {
    const auto separator = word.find('=');
    if (separator != std::string::npos)
      result[word.substr(0, separator)] = word.substr(separator + 1);
  }
  return result;
}

Json runtime_report(const fs::path &log, bool devices) {
  std::ifstream file(log);
  require(file.good(), "Cannot open Runtime benchmark log");
  Json report = {{"costs", Json::object()}, {"frames", Json::object()}};
  std::string line;
  while (std::getline(file, line)) {
    if (line.starts_with("AYTHER_NATIVE_COST ")) {
      const auto values = fields(line);
      report["costs"][values.at("stage")] = {
          {"unit", values.at("unit")},
          {"warmup", std::stoull(values.at("warmup"))},
          {"count", std::stoull(values.at("count"))},
          {"p50", std::stod(values.at("p50"))},
          {"p95", std::stod(values.at("p95"))},
          {"p99", std::stod(values.at("p99"))},
          {"max", std::stod(values.at("max"))}};
    } else if (line.starts_with("AYTHER_NATIVE_FRAME ")) {
      const auto values = fields(line);
      const auto frame = values.at("frame");
      if (frame == "1" || frame == "180")
        report["frames"][frame] = {
            {"original", values.at("original")},
            {"pcm", values.at("pcm")}};
    } else if (line.starts_with("AYTHER_NATIVE_PRESENT ")) {
      const auto values = fields(line);
      report["presentation"] = {
          {"frames", std::stoull(values.at("frames"))},
          {"intervals", std::stoull(values.at("intervals"))},
          {"p50_ms", std::stod(values.at("p50_ms"))},
          {"p95_ms", std::stod(values.at("p95_ms"))},
          {"p99_ms", std::stod(values.at("p99_ms"))},
          {"max_ms", std::stod(values.at("max_ms"))},
          {"validation_errors", std::stoull(values.at("validation_errors"))},
          {"effective_timestamps", values.at("effective_timestamps")}};
    } else if (line.starts_with("AYTHER_NATIVE_AUDIO ")) {
      const auto values = fields(line);
      report["audio"] = {
          {"source_frames", std::stoull(values.at("source_frames"))},
          {"source_peak", std::stoi(values.at("source_peak"))},
          {"active_samples", std::stoull(values.at("active_samples"))},
          {"episodes", std::stoull(values.at("episodes"))},
          {"missing_samples", std::stoull(values.at("missing_samples"))},
          {"total_ms", std::stod(values.at("total_ms"))},
          {"max_ms", std::stod(values.at("max_ms"))}};
    } else if (line.starts_with("AYTHER_NATIVE_DONE ")) {
      report["completed"] = std::stoull(fields(line).at("frames"));
    }
  }
  require(report.value("completed", 0ULL) == 180 &&
              report["costs"].size() == 3 && report["frames"].size() == 2,
          "Runtime benchmark coverage is incomplete");
  require(!devices || (report.contains("presentation") &&
                       report.contains("audio") &&
                       report["presentation"]["validation_errors"] == 0),
          "Device benchmark coverage is incomplete");
  return report;
}

Json direct_report(const fs::path &path) {
  std::ifstream file(path);
  require(file.good(), "Cannot open direct benchmark report");
  auto report = Json::parse(file);
  require(report.at("completed_frames") == 180 && report.at("code") == 0,
          "Direct benchmark did not complete 180 frames");
  Json costs = Json::object();
  for (const char *stage : {"step", "canonical_hashes", "observation"}) {
    const auto &source = report.at("costs").at(stage).at("samples");
    require(source.size() == 180, "Direct benchmark sample count is wrong");
    std::vector<double> samples;
    for (std::size_t index = 30; index < source.size(); ++index)
      samples.push_back(source[index].get<double>());
    std::sort(samples.begin(), samples.end());
    const auto percentile = [&samples](std::size_t percent) {
      return samples[(samples.size() * percent + 99) / 100 - 1];
    };
    costs[stage] = {{"unit", "us"}, {"warmup", 30}, {"count", 150},
                    {"p50", percentile(50)}, {"p95", percentile(95)},
                    {"p99", percentile(99)}, {"max", samples.back()}};
  }
  return {{"costs", costs}, {"first", report.at("frames").at(0)},
          {"last", report.at("frames").at(179)},
          {"content_sha256", report.at("content")},
          {"config_sha256", report.at("config")},
          {"trace_sha256", report.at("trace")}};
}

} // namespace

int main(int argc, char **argv) {
  if (argc != 5 && argc != 6) {
    std::cerr << "Usage: benchmark TESSERA_CLI RUNTIME FX02_ROM OUTPUT_DIR [--devices]\n";
    return 2;
  }
  try {
    const fs::path cli = fs::absolute(argv[1]), runtime = fs::absolute(argv[2]);
    const fs::path rom_path = fs::absolute(argv[3]), output = fs::absolute(argv[4]);
    const bool devices = argc == 6 && std::string(argv[5]) == "--devices";
    require(argc == 5 || devices, "Unsupported benchmark option");
    fs::create_directories(output);
    const auto rom = read(rom_path);
    const Bytes memory(65536, 0);
    auto context = init<tessera_context_v1>();
    std::array<tessera_backend_info_v1, 16> backends{};
    for (auto &backend : backends) backend = init<tessera_backend_info_v1>();
    std::uint32_t required = 0;
    require(tessera_backends_enumerate_v1(backends.data(), backends.size(),
                                          &required, &context) == TESSERA_OK,
            "Cannot enumerate Tessera backends");
    const tessera_backend_info_v1 *selected = nullptr;
    for (std::uint32_t index = 0; index < required; ++index)
      if (std::string_view(backends[index].id) == "clownmdemu")
        selected = &backends[index];
    require(selected != nullptr, "ClownMDEmu backend is unavailable");
    const auto config = tessera::wire::configuration(
        "clownmdemu", {selected->build_id, 32}, memory);
    Bytes trace = tessera::wire::text("TSTRACE1");
    tessera::wire::field(trace, 1, TESSERA_ENCODING_BYTES,
                         tessera::wire::sha(rom));
    tessera::wire::field(trace, 2, TESSERA_ENCODING_BYTES,
                         tessera::wire::sha(config));
    tessera::wire::scalar(trace, 3, TESSERA_ENCODING_U64, 180);
    std::vector<Bytes> frames;
    for (std::uint64_t frame = 1; frame <= 180; ++frame) {
      Bytes item;
      tessera::wire::scalar(item, 1, TESSERA_ENCODING_U64, frame);
      tessera::wire::field(item, 2, TESSERA_ENCODING_ARRAY,
                           tessera::wire::inputs(tessera::InputState{}));
      frames.push_back(std::move(item));
    }
    tessera::wire::field(trace, 4, TESSERA_ENCODING_ARRAY,
                         tessera::wire::array(frames));
    write(output / "config.bin", config);
    write(output / "memory.bin", memory);
    write(output / "trace.bin", trace);

    Json result = {{"schema", 1}, {"frames", 180}, {"warmup", 30},
                   {"repetitions", 3}, {"direct_off", Json::array()},
                   {"direct_full", Json::array()},
                   {"engine_headless", Json::array()},
                   {"runtime_devices", Json::array()}};
    for (int repetition = 0; repetition < 3; ++repetition) {
      for (const char *mode : {"off", "full"}) {
        const auto name = std::string("direct-") + mode + "-" +
                          std::to_string(repetition);
        const auto report = output / (name + ".json");
        const auto peak = run(cli, L"replay --backend clownmdemu --content " + quote(rom_path) +
                     L" --config " + quote(output / "config.bin") +
                     L" --initial-memory " + quote(output / "memory.bin") +
                     L" --trace " + quote(output / "trace.bin") +
                     L" --frames 180 --observer " +
                     std::wstring(mode, mode + std::char_traits<char>::length(mode)) +
                     L" --report " + quote(report), output / (name + ".log"));
        auto direct = direct_report(report);
        direct["peak_working_set_bytes"] = peak;
        result[std::string("direct_") + mode].push_back(std::move(direct));
      }
      const auto name = "engine-" + std::to_string(repetition);
      const auto engine_peak = run(runtime, L"--provider tessera --rom " + quote(rom_path) +
                       L" --native-trace " + quote(output / "trace.bin") +
                       L" --frames 180", output / (name + ".log"));
      auto engine = runtime_report(output / (name + ".log"), false);
      engine["peak_working_set_bytes"] = engine_peak;
      result["engine_headless"].push_back(std::move(engine));
      if (devices) {
        const auto device_name = "devices-" + std::to_string(repetition);
        const auto device_peak = run(runtime, L"--provider tessera --rom " + quote(rom_path) +
                         L" --native-trace " + quote(output / "trace.bin") +
                         L" --frames 180 --native-visible --native-audio-device",
            output / (device_name + ".log"));
        auto device = runtime_report(output / (device_name + ".log"), true);
        device["peak_working_set_bytes"] = device_peak;
        result["runtime_devices"].push_back(std::move(device));
        require(result["runtime_devices"].back()["frames"] ==
                    result["engine_headless"].back()["frames"],
                "Runtime device path changed original frame outputs");
      }
    }
    std::ofstream summary(output / "summary.json");
    summary << result.dump(2) << '\n';
    require(summary.good(), "Cannot write benchmark summary");
    std::cout << "Benchmark completed: " << (output / "summary.json") << '\n';
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
