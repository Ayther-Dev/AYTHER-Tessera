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
  Json report = {{"costs", Json::object()}, {"frames", Json::object()},
                 {"frame_hashes", Json::array()},
                 {"allocations", {{"scope", "executable_cpp_new"},
                                   {"calls_per_frame", Json::array()},
                                   {"bytes_per_frame", Json::array()}}}};
  std::string line;
  while (std::getline(file, line)) {
    if (line.starts_with("AYTHER_NATIVE_CONFIG ")) {
      report["config_sha256"] = fields(line).at("sha256");
    } else if (line.starts_with("AYTHER_NATIVE_COST ")) {
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
      require(std::stoull(frame) == report["frame_hashes"].size() + 1,
              "Runtime frame hash sequence is incomplete");
      report["frame_hashes"].push_back({{"original", values.at("original")},
                                         {"pcm", values.at("pcm")},
                                         {"presented", values.at("presented")}});
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
          {"callback_calls", std::stoull(values.at("callback_calls"))},
          {"episodes", std::stoull(values.at("episodes"))},
          {"missing_samples", std::stoull(values.at("missing_samples"))},
          {"total_ms", std::stod(values.at("total_ms"))},
          {"max_ms", std::stod(values.at("max_ms"))}};
    } else if (line.starts_with("AYTHER_NATIVE_FLOW ")) {
      const auto values = fields(line);
      report["flow"] = {{"frames", std::stoull(values.at("frames"))},
                        {"borrowed_video_bytes", std::stoull(values.at("borrowed_video"))},
                        {"composed_frames", std::stoull(values.at("composed_frames"))},
                        {"composed_copy_bytes", std::stoull(values.at("composed_copy"))},
                        {"fallback_frames", std::stoull(values.at("fallback_frames"))},
                        {"fallback_no_pack", std::stoull(values.at("fallback_no_pack"))},
                        {"fallback_policy", std::stoull(values.at("fallback_policy"))},
                        {"fallback_no_rule", std::stoull(values.at("fallback_no_rule"))},
                        {"scope", values.at("scope")}};
    } else if (line.starts_with("AYTHER_NATIVE_DEVICE_BYTES ")) {
      const auto values = fields(line);
      report["device_bytes"] = {{"gpu_staging", std::stoull(values.at("gpu_staging"))},
                                {"audio_stream_queue", std::stoull(values.at("audio_stream_queue"))},
                                {"scope", values.at("scope")}};
    } else if (line.starts_with("AYTHER_NATIVE_EVENTS ")) {
      const auto values = fields(line);
      report["events"] = {{"emitted", std::stoull(values.at("emitted"))},
                          {"retained", std::stoull(values.at("retained"))},
                          {"batch_bytes", std::stoull(values.at("batch_bytes"))},
                          {"losses", std::stoull(values.at("losses"))},
                          {"scope", values.at("scope")}};
    } else if (line.starts_with("AYTHER_NATIVE_ALLOC ")) {
      const auto values = fields(line);
      require(std::stoull(values.at("frame")) ==
                  report["allocations"]["calls_per_frame"].size() + 1,
              "Runtime allocation frame sequence is incomplete");
      report["allocations"]["calls_per_frame"].push_back(
          std::stoull(values.at("calls")));
      report["allocations"]["bytes_per_frame"].push_back(
          std::stoull(values.at("bytes")));
    } else if (line.starts_with("AYTHER_NATIVE_GPU_TIMESTAMP ")) {
      const auto values = fields(line);
      report["gpu_timestamp"] = {
          {"supported", std::stoull(values.at("supported"))},
          {"valid_bits", std::stoull(values.at("valid_bits"))},
          {"period_ns", std::stod(values.at("period_ns"))},
          {"samples", std::stoull(values.at("samples"))},
          {"scope", values.at("scope")}};
    } else if (line.starts_with("AYTHER_NATIVE_DONE ")) {
      report["completed"] = std::stoull(fields(line).at("frames"));
    }
  }
  require(report.value("completed", 0ULL) == 180 &&
              report["costs"].contains("engine_step") &&
              report["costs"].contains("engine_frame") &&
              report["costs"].contains("runtime_hash") &&
              report["costs"].contains("runtime_device") &&
              report["frames"].size() == 2 &&
              report["frame_hashes"].size() == 180 &&
              report.contains("config_sha256") &&
              report.contains("flow") &&
              report.contains("events") && report.contains("device_bytes") &&
              report["flow"]["frames"] == 180 &&
              report["flow"]["composed_frames"].get<std::uint64_t>() +
                  report["flow"]["fallback_frames"].get<std::uint64_t>() == 180 &&
              report["events"]["emitted"] == report["events"]["retained"] &&
              report["events"]["losses"] == 0,
          "Runtime benchmark coverage is incomplete");
  const auto allocation_count = report["allocations"]["calls_per_frame"].size();
  require(allocation_count == 0 ||
              (allocation_count == 180 &&
               report["allocations"]["bytes_per_frame"].size() == 180),
          "Runtime allocation coverage is incomplete");
  require(!devices || (report.contains("presentation") &&
                       report.contains("audio") &&
                       report.contains("gpu_timestamp") &&
                       report["gpu_timestamp"]["supported"] == 1 &&
                       report["gpu_timestamp"]["samples"] == 180 &&
                       report["costs"].contains("gpu_execute") &&
                       report["costs"].contains("gpu_image_wait") &&
                       report["costs"].contains("gpu_command_span") &&
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
  const auto &observer = report.at("observer");
  require(observer.at("event_losses") == 0,
          "Direct benchmark lost observation events");
  if (observer.at("mode") == "full")
    require(observer.at("snapshot_reads") == 180 &&
                observer.at("event_batches") == 180 &&
                observer.at("event_records").get<std::uint64_t>() > 0,
            "Direct observation coverage is incomplete");
  const auto allocations = report.value("allocations", Json::object());
  require(allocations.empty() ||
              (allocations.at("calls_per_frame").size() == 180 &&
               allocations.at("bytes_per_frame").size() == 180),
          "Direct allocation coverage is incomplete");
  return {{"costs", costs}, {"observer", observer},
          {"allocations", allocations},
          {"frame_hashes", report.at("frames")},
          {"first", report.at("frames").at(0)},
          {"last", report.at("frames").at(179)},
          {"content_sha256", report.at("content")},
          {"config_sha256", report.at("config")},
          {"trace_sha256", report.at("trace")}};
}

} // namespace

int main(int argc, char **argv) {
  if (argc < 5) {
    std::cerr << "Usage: benchmark TESSERA_CLI RUNTIME ROM OUTPUT_DIR [--devices] [--pack PACK TRUST] [--press-b]\n";
    return 2;
  }
  try {
    const fs::path cli = fs::absolute(argv[1]), runtime = fs::absolute(argv[2]);
    const fs::path rom_path = fs::absolute(argv[3]), output = fs::absolute(argv[4]);
    bool devices = false;
    bool press_b = false;
    fs::path pack, trust;
    for (int index = 5; index < argc; ++index) {
      if (std::string_view(argv[index]) == "--devices") {
        require(!devices, "Duplicate --devices");
        devices = true;
      } else if (std::string_view(argv[index]) == "--pack") {
        require(pack.empty() && index + 2 < argc, "--pack requires PACK and TRUST");
        pack = fs::absolute(argv[++index]);
        trust = fs::absolute(argv[++index]);
      } else if (std::string_view(argv[index]) == "--press-b") {
        require(!press_b, "Duplicate --press-b");
        press_b = true;
      } else {
        throw std::runtime_error("Unsupported benchmark option");
      }
    }
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
    tessera::InputState input{};
    if (press_b) input[5] = 1;
    for (std::uint64_t frame = 1; frame <= 180; ++frame) {
      Bytes item;
      tessera::wire::scalar(item, 1, TESSERA_ENCODING_U64, frame);
      tessera::wire::field(item, 2, TESSERA_ENCODING_ARRAY,
                           tessera::wire::inputs(input));
      frames.push_back(std::move(item));
    }
    tessera::wire::field(trace, 4, TESSERA_ENCODING_ARRAY,
                         tessera::wire::array(frames));
    write(output / "config.bin", config);
    write(output / "memory.bin", memory);
    write(output / "trace.bin", trace);

    Json result = {{"schema", 2}, {"frames", 180}, {"warmup", 30},
                   {"input_profile", press_b ? "b-held" : "neutral"},
                   {"repetitions", 3}, {"direct_off", Json::array()},
                   {"direct_full", Json::array()},
                   {"engine_minimal", Json::array()},
                   {"engine_headless", Json::array()},
                   {"engine_composition", Json::array()},
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
      require(result["direct_off"].back().at("frame_hashes") ==
                  result["direct_full"].back().at("frame_hashes"),
              "Tessera observation changed canonical frame hashes");
      const auto minimal_name = "minimal-" + std::to_string(repetition);
      const auto minimal_peak = run(runtime, L"--provider tessera --rom " + quote(rom_path) +
                       L" --native-trace " + quote(output / "trace.bin") +
                       L" --frames 180 --native-deactivate-at 2",
                       output / (minimal_name + ".log"));
      auto minimal = runtime_report(output / (minimal_name + ".log"), false);
      minimal["peak_working_set_bytes"] = minimal_peak;
      result["engine_minimal"].push_back(std::move(minimal));
      const auto name = "engine-" + std::to_string(repetition);
      const auto engine_peak = run(runtime, L"--provider tessera --rom " + quote(rom_path) +
                       L" --native-trace " + quote(output / "trace.bin") +
                       L" --frames 180", output / (name + ".log"));
      auto engine = runtime_report(output / (name + ".log"), false);
      require(engine.at("config_sha256") ==
                  result["direct_off"].back().at("config_sha256") &&
                  engine.at("config_sha256") ==
                      result["direct_full"].back().at("config_sha256") &&
                  engine.at("config_sha256") ==
                      result["engine_minimal"].back().at("config_sha256"),
              "Engine and direct Tessera did not use the same configuration");
      for (std::size_t frame = 0; frame < 180; ++frame) {
        require(engine["frame_hashes"][frame]["original"] ==
                    result["engine_minimal"].back()["frame_hashes"][frame]["original"] &&
                engine["frame_hashes"][frame]["pcm"] ==
                    result["engine_minimal"].back()["frame_hashes"][frame]["pcm"],
                "Engine observation changed original video or PCM");
      }
      engine["peak_working_set_bytes"] = engine_peak;
      result["engine_headless"].push_back(std::move(engine));
      if (!pack.empty()) {
        const auto composed_name = "composition-" + std::to_string(repetition);
        const auto composed_peak = run(runtime,
            L"--provider tessera --rom " + quote(rom_path) +
            L" --native-trace " + quote(output / "trace.bin") +
            L" --frames 180 --pack " + quote(pack) +
            L" --trust-registry " + quote(trust),
            output / (composed_name + ".log"));
        auto composed = runtime_report(output / (composed_name + ".log"), false);
        require(composed.at("config_sha256") ==
                    result["engine_headless"].back().at("config_sha256"),
                "Composition changed native configuration");
        for (std::size_t frame = 0; frame < 180; ++frame) {
          require(composed["frame_hashes"][frame]["original"] ==
                      result["engine_headless"].back()["frame_hashes"][frame]["original"] &&
                  composed["frame_hashes"][frame]["pcm"] ==
                      result["engine_headless"].back()["frame_hashes"][frame]["pcm"],
                  "Composition changed original video or PCM");
        }
        require(composed["flow"]["composed_frames"].get<std::uint64_t>() > 0,
                "Pack never composed a frame");
        composed["peak_working_set_bytes"] = composed_peak;
        result["engine_composition"].push_back(std::move(composed));
      }
      if (devices) {
        const auto device_name = "devices-" + std::to_string(repetition);
        const auto pack_arguments = pack.empty() ? std::wstring{} :
            L" --pack " + quote(pack) + L" --trust-registry " + quote(trust);
        const auto device_peak = run(runtime, L"--provider tessera --rom " + quote(rom_path) +
                         L" --native-trace " + quote(output / "trace.bin") +
                         L" --frames 180" + pack_arguments +
                         L" --native-visible --native-audio-device",
            output / (device_name + ".log"));
        auto device = runtime_report(output / (device_name + ".log"), true);
        require(device.at("config_sha256") ==
                    result["engine_headless"].back().at("config_sha256"),
                "Device and headless Runtime used different configurations");
        device["peak_working_set_bytes"] = device_peak;
        result["runtime_devices"].push_back(std::move(device));
        const auto &reference = pack.empty() ? result["engine_headless"].back()
                                             : result["engine_composition"].back();
        require(result["runtime_devices"].back()["frame_hashes"] ==
                    reference["frame_hashes"],
                "Runtime device path changed frame hashes");
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
