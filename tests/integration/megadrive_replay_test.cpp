#define NOMINMAX
#include "cli/public_client.hpp"
#include "domain/wire.hpp"
#include "persistence/state_store.hpp"

#include <nlohmann/json.hpp>
#include <windows.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <set>
#include <stdexcept>

namespace {
using Json = nlohmann::json;
using Bytes = tessera::Bytes;
using View = tessera::wire::View;
namespace client = tessera::cli;

void require(bool valid, std::string_view reason) {
  if (!valid)
    throw std::runtime_error(std::string(reason));
}
Bytes read(const std::filesystem::path &path) {
  std::ifstream input(path, std::ios::binary);
  require(input.good(), "FILE_OPEN_FAILED");
  Bytes bytes{std::istreambuf_iterator<char>{input},
              std::istreambuf_iterator<char>{}};
  require(bytes.size() <= 64ULL * 1024 * 1024, "FILE_TOO_LARGE");
  return bytes;
}
Json json(const std::filesystem::path &path) {
  const auto bytes = read(path);
  return Json::parse(bytes.begin(), bytes.end());
}
void write(const std::filesystem::path &path, View bytes) {
  std::ofstream stream(path, std::ios::binary);
  require(stream.good(), "FILE_WRITE_OPEN_FAILED");
  stream.write(reinterpret_cast<const char *>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
  stream.close();
  require(stream.good(), "FILE_WRITE_FAILED");
}
void write_json(const std::filesystem::path &path, const Json &value) {
  const auto data = value.dump(2) + "\n";
  write(path, {reinterpret_cast<const std::uint8_t *>(data.data()),
               data.size()});
}
std::wstring quote(std::wstring_view input) {
  std::wstring result = L"\"";
  std::size_t slashes{};
  for (const auto ch : input) {
    if (ch == L'\\') {
      ++slashes;
      continue;
    }
    result.append(ch == L'"' ? slashes * 2 + 1 : slashes, L'\\');
    slashes = 0;
    result.push_back(ch);
  }
  result.append(slashes * 2, L'\\');
  return result + L'"';
}
struct Child {
  DWORD pid{};
  std::uint64_t born{};
};
Child run(const std::filesystem::path &executable,
          const std::vector<std::wstring> &args,
          const std::filesystem::path &log, DWORD timeout_ms = 600000) {
  SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
  const HANDLE output = CreateFileW(log.c_str(), GENERIC_WRITE, FILE_SHARE_READ,
                                    &security, CREATE_ALWAYS,
                                    FILE_ATTRIBUTE_NORMAL, nullptr);
  require(output != INVALID_HANDLE_VALUE, "PROCESS_LOG_OPEN_FAILED");
  std::wstring command = quote(executable.wstring());
  for (const auto &arg : args)
    command += L" " + quote(arg);
  STARTUPINFOW startup{};
  startup.cb = sizeof(startup);
  startup.dwFlags = STARTF_USESTDHANDLES;
  startup.hStdOutput = startup.hStdError = output;
  startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
  PROCESS_INFORMATION process{};
  const auto created =
      CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr,
                     TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &startup,
                     &process);
  CloseHandle(output);
  require(created != FALSE, "PROCESS_CREATE_FAILED");
  FILETIME born{}, died{}, kernel{}, user{};
  const auto timing =
      GetProcessTimes(process.hProcess, &born, &died, &kernel, &user);
  const auto waited = WaitForSingleObject(process.hProcess, timeout_ms);
  if (waited != WAIT_OBJECT_0) {
    TerminateProcess(process.hProcess, 124);
    WaitForSingleObject(process.hProcess, 1000);
  }
  DWORD status{};
  GetExitCodeProcess(process.hProcess, &status);
  const Child result{process.dwProcessId,
                     (std::uint64_t(born.dwHighDateTime) << 32) |
                         born.dwLowDateTime};
  CloseHandle(process.hThread);
  CloseHandle(process.hProcess);
  if (!created || !timing || waited != WAIT_OBJECT_0 || status != 0) {
    const auto output = read(log);
    std::cerr << std::string(output.begin(), output.end());
    throw std::runtime_error("PROCESS_FAILED:" + std::to_string(status));
  }
  return result;
}
Bytes trace(View content, View config, const Json &source) {
  require(source.is_array() && source.size() == 3000,
          "FIXTURE_TRACE_LENGTH_INVALID");
  Bytes result = tessera::wire::text("TSTRACE1");
  tessera::wire::field(result, 1, TESSERA_ENCODING_BYTES,
                       tessera::wire::sha(content));
  tessera::wire::field(result, 2, TESSERA_ENCODING_BYTES,
                       tessera::wire::sha(config));
  tessera::wire::scalar(result, 3, TESSERA_ENCODING_U64, source.size());
  std::vector<Bytes> frames;
  frames.reserve(source.size());
  for (std::size_t index = 0; index < source.size(); ++index) {
    const auto &frame = source[index];
    require(frame.at("frame") == index + 1, "FIXTURE_TRACE_ORDER_INVALID");
    const auto &ports = frame.at("ports");
    require(ports.size() == 1 && ports[0].at("port") == 0,
            "FIXTURE_PORT_INVALID");
    const auto &controls = ports[0].at("controls");
    constexpr std::array<std::string_view, 8> names{
        "up", "down", "left", "right", "a", "b", "c", "start"};
    std::vector<Bytes> assignments;
    for (std::size_t control = 0; control < names.size(); ++control) {
      const auto name = std::string(names[control]);
      require(controls.at(name).is_boolean(), "FIXTURE_INPUT_INVALID");
      Bytes item;
      tessera::wire::scalar(item, 1, TESSERA_ENCODING_U32, 0);
      tessera::wire::scalar(item, 2, TESSERA_ENCODING_U32, control);
      tessera::wire::scalar(item, 3, TESSERA_ENCODING_I32,
                            controls.at(name).get<bool>() ? 1 : 0);
      assignments.push_back(std::move(item));
    }
    Bytes item;
    tessera::wire::scalar(item, 1, TESSERA_ENCODING_U64, index + 1);
    tessera::wire::field(item, 2, TESSERA_ENCODING_ARRAY,
                         tessera::wire::array(assignments));
    frames.push_back(std::move(item));
  }
  tessera::wire::field(result, 4, TESSERA_ENCODING_ARRAY,
                       tessera::wire::array(frames));
  return result;
}
void oracle(const tessera_original_frame_v1 &frame, tessera_session session,
            tessera_context_v1 &context, bool active) {
  require(frame.width == 320 && frame.height == 224 &&
              frame.pcm.bytes > 0 && frame.pcm_frames > 0,
          "FX02_GEOMETRY_OR_AUDIO_MISSING");
  const auto center = (std::size_t(frame.height / 2) * frame.width +
                       frame.width / 2) *
                      4;
  require(frame.pixels.data[center] == (active ? 255 : 0) &&
              frame.pixels.data[center + 2] == (active ? 0 : 255),
          "FX02_VIDEO_ORACLE_FAILED");
  std::int32_t minimum = 32767, maximum = -32768;
  for (std::size_t offset = 0; offset < frame.pcm.bytes; offset += 2) {
    const auto bits = std::uint16_t(frame.pcm.data[offset]) |
                      std::uint16_t(frame.pcm.data[offset + 1]) << 8;
    const auto sample = bits < 32768 ? std::int32_t(bits)
                                     : std::int32_t(bits) - 65536;
    minimum = std::min(minimum, sample);
    maximum = std::max(maximum, sample);
  }
  require(active ? maximum - minimum > 16
                 : minimum >= -2 && maximum <= 2,
          "FX02_AUDIO_ORACLE_FAILED");
  tessera_snapshot snapshot{};
  require(tessera_snapshot_acquire_v1(session, &snapshot, &context) ==
              TESSERA_OK,
          "FX02_SNAPSHOT_MISSING");
  auto memory = client::init<tessera_snapshot_region_v1>();
  auto palette = client::init<tessera_snapshot_region_v1>();
  require(tessera_snapshot_region_query_v1(snapshot, 0, &memory, &context) ==
              TESSERA_OK &&
              tessera_snapshot_region_query_v1(snapshot, 1, &palette,
                                                &context) == TESSERA_OK,
          "FX02_REGIONS_MISSING");
  require(memory.kind == 1 && memory.data.bytes >= 131072 &&
              palette.kind == 2 && palette.data.bytes >= 2,
          "FX02_REGION_LAYOUT_INVALID");
  const auto marker = std::uint16_t(memory.data.data[65536 + 0xFFFC]) << 8 |
                      memory.data.data[65536 + 0xFFFD];
  const auto colour = std::uint16_t(palette.data.data[0]) |
                      std::uint16_t(palette.data.data[1]) << 8;
  require(memory.data.data[1] == (active ? 0 : 16) &&
              marker == (active ? 0x2468 : 0x1357) &&
              colour == (active ? 0x000E : 0x0E00),
          "FX02_MACHINE_ORACLE_FAILED");
  require(tessera_snapshot_release_v1(snapshot, &context) == TESSERA_OK,
          "FX02_SNAPSHOT_RELEASE_FAILED");
}

int worker(const std::filesystem::path &rom_path,
           const std::filesystem::path &config_path,
           const std::filesystem::path &memory_path,
           const std::filesystem::path &trace_path,
           const std::filesystem::path &checkpoint_path,
           const std::filesystem::path &checkpoint_directory,
           const std::filesystem::path &report_path, bool action,
           bool observe, bool fx02, std::uint64_t frames) {
  const auto rom = read(rom_path), config = read(config_path),
             memory = read(memory_path), trace = read(trace_path);
  client::PublicSession session("clownmdemu", rom, config, memory);
  if (checkpoint_path == "-") {
    session.bind(trace);
  } else {
    const auto encoded = read(checkpoint_path);
    auto context = client::init<tessera_context_v1>();
    require(tessera_checkpoint_restore_v1(
                session.handle(), {encoded.data(), encoded.size()},
                {trace.data(), trace.size()}, &context) == TESSERA_OK,
            "CHECKPOINT_RESTORE_FAILED");
  }
  auto info = session.info();
  const auto start = info.frame_id.frame;
  if (observe) {
    const std::array<tessera_control_item_v1, 2> controls{{{1, 1, 15},
                                                           {2, 1, 31}}};
    session.submit(900 + start, controls);
  }
  Json report{{"start", start},
              {"frames", Json::array()},
              {"oracle_checks", 0},
              {"audio_active_checks", 0},
              {"audio_neutral_checks", 0}};
  for (std::uint64_t index = 0; index < frames; ++index) {
    const auto status = session.replay_next();
    require(status == TESSERA_OK, "REPLAY_SHORT_OR_FAILED");
    const auto current = session.info();
    const auto frame = start + index + 1;
    require(current.frame_id.frame == frame, "REPLAY_FRAME_WRONG");
    const auto original = session.original();
    const auto hashes = client::canonical_digests(session);
    report["frames"].push_back({{"frame", frame},
                                {"state", hashes.state},
                                {"video", hashes.video},
                                {"audio", hashes.audio}});
    if (fx02 && observe &&
        ((frame >= 100 && frame <= 200) ||
         (frame >= 350 && frame <= 550) ||
         (frame >= 650 && frame <= 850))) {
      auto context = client::init<tessera_context_v1>();
      const bool active = action && frame >= 350 && frame <= 550;
      oracle(original, session.handle(), context, active);
      report["oracle_checks"] = report["oracle_checks"].get<unsigned>() + 1;
      const auto key =
          active ? "audio_active_checks" : "audio_neutral_checks";
      report[key] = report[key].get<unsigned>() + 1;
    }
    if (checkpoint_path == "-" && checkpoint_directory != "-" &&
        (frame == 500 || frame == 1500 || frame == 2500)) {
      auto context = client::init<tessera_context_v1>();
      std::uint64_t required{};
      require(tessera_checkpoint_export_v1(session.handle(), {nullptr, 0},
                                           &required, &context) ==
                  TESSERA_BUFFER_TOO_SMALL,
              "CHECKPOINT_SIZE_FAILED");
      Bytes encoded(static_cast<std::size_t>(required));
      require(tessera_checkpoint_export_v1(
                  session.handle(), {encoded.data(), encoded.size()},
                  &required, &context) == TESSERA_OK,
              "CHECKPOINT_EXPORT_FAILED");
      const auto path = checkpoint_directory /
                        (std::to_string(frame) + ".tsc");
      require(tessera::persistence::save_checkpoint(path, encoded).ok(),
              "CHECKPOINT_PUBLISH_FAILED");
    }
  }
  if (start + frames == 3000)
    require(session.replay_next() == TESSERA_END_OF_TRACE,
            "REPLAY_EOF_MISSING");
  report["completed"] = frames;
  write_json(report_path, report);
  return 0;
}

void verify_fixture(const std::filesystem::path &generated,
                    const Json &manifest) {
  const auto source = read(TESSERA_FX02_GENERATOR_SOURCE);
  require(client::digest(source) ==
              manifest.at("generator_sha256").get<std::string>(),
          "FX02_GENERATOR_HASH_MISMATCH");
  for (const auto key : {"rom", "trace_control", "trace_action", "oracle"}) {
    const auto &entry = manifest.at(key);
    const auto bytes = read(generated / entry.at("generated_name")
                                           .get<std::string>());
    require(client::digest(bytes) == entry.at("sha256").get<std::string>(),
            "FX02_ARTIFACT_HASH_MISMATCH");
  }
}

int parent(const std::filesystem::path &self,
           const std::filesystem::path &generator,
           const std::filesystem::path &evidence_root) {
  const auto unique = "t021-" + std::to_string(GetCurrentProcessId()) + "-" +
                      std::to_string(GetTickCount64());
  const auto evidence = evidence_root / unique;
  const auto generated = evidence / "fx02";
  std::filesystem::create_directories(generated);
  run(generator, {L"--generate", generated.wstring()},
      evidence / "generator.log", 30000);
  const auto manifest = json(TESSERA_FX02_MANIFEST);
  verify_fixture(generated, manifest);
  const auto action_json = json(generated / "full-action.json");
  const auto neutral_json = json(generated / "full-control.json");
  const auto memory = Bytes(65536, 0);
  const auto backend = client::find_backend("clownmdemu");
  const auto config = tessera::wire::configuration("clownmdemu",
                                                   backend.build_id, memory);
  write(evidence / "memory.bin", memory);
  write(evidence / "config.bin", config);
  struct Scenario {
    std::string name;
    std::filesystem::path rom;
    const Json *source;
    bool action, fx02;
  };
  const std::array<Scenario, 3> scenarios{{
      {"original", TESSERA_ORIGINAL_ROM, &neutral_json, false, false},
      {"fx02-control", generated / "program.md", &neutral_json, false, true},
      {"fx02-action", generated / "program.md", &action_json, true, true}}};
  std::set<std::pair<DWORD, std::uint64_t>> processes;
  Json summary{{"schema", 1}, {"runs", Json::array()},
               {"first_divergence", nullptr}};
  for (const auto &scenario : scenarios) {
    const auto rom = read(scenario.rom);
    const auto encoded_trace = trace(rom, config,
                                     scenario.source->at("frames"));
    const auto trace_path = evidence / (scenario.name + ".trace");
    write(trace_path, encoded_trace);
    const auto checkpoints = evidence / (scenario.name + "-checkpoints");
    std::filesystem::create_directory(checkpoints);
    Json baseline;
    for (const bool observe : {false, true}) {
      const auto report_path = evidence /
                               (scenario.name + (observe ? "-full" : "-off") +
                                ".json");
      const auto identity = run(
          self,
          {L"--worker", scenario.rom.wstring(),
           (evidence / "config.bin").wstring(),
           (evidence / "memory.bin").wstring(), trace_path.wstring(), L"-",
           observe ? checkpoints.wstring() : L"-", report_path.wstring(),
           scenario.action ? L"action" : L"control",
           observe ? L"full" : L"off",
           scenario.fx02 ? L"fx02" : L"original", L"3000"},
          evidence / (scenario.name + (observe ? "-full" : "-off") + ".log"));
      require(processes.emplace(identity.pid, identity.born).second,
              "PROCESS_IDENTITY_REUSED");
      const auto report = json(report_path);
      require(report.at("completed") == 3000 &&
                  report.at("frames").size() == 3000,
              "REPLAY_INCOMPLETE");
      if (observe) {
        require(report.at("oracle_checks").get<unsigned>() ==
                    (scenario.fx02 ? 503U : 0U),
                "FX02_ORACLE_COUNT_WRONG");
        if (scenario.action)
          require(report.at("audio_active_checks") == 201 &&
                      report.at("audio_neutral_checks") == 302,
                  "FX02_AUDIO_TRANSITION_MISSING");
        require(report.at("frames") == baseline,
                "OBSERVATION_CHANGED_CANONICAL_OUTPUT");
      } else {
        baseline = report.at("frames");
      }
      summary["runs"].push_back({{"name", scenario.name},
                                  {"mode", observe ? "full" : "off"},
                                  {"pid", identity.pid},
                                  {"born_utc_100ns", identity.born},
                                  {"frames", 3000},
                                  {"divergences", 0}});
    }
    for (const auto checkpoint_frame : {500, 1500, 2500}) {
      const auto report_path =
          evidence / (scenario.name + "-restore-" +
                      std::to_string(checkpoint_frame) + ".json");
      const auto identity = run(
          self,
          {L"--worker", scenario.rom.wstring(),
           (evidence / "config.bin").wstring(),
           (evidence / "memory.bin").wstring(), trace_path.wstring(),
           (checkpoints / (std::to_string(checkpoint_frame) + ".tsc"))
               .wstring(),
           L"-", report_path.wstring(),
           scenario.action ? L"action" : L"control", L"full",
           scenario.fx02 ? L"fx02" : L"original", L"500"},
          evidence / (scenario.name + "-restore-" +
                      std::to_string(checkpoint_frame) + ".log"));
      require(processes.emplace(identity.pid, identity.born).second,
              "RESTORE_PROCESS_IDENTITY_REUSED");
      const auto restored = json(report_path);
      require(restored.at("start") == checkpoint_frame &&
                  restored.at("completed") == 500 &&
                  restored.at("frames").size() == 500,
              "RESTORE_LENGTH_WRONG");
      for (std::size_t index = 0; index < 500; ++index) {
        if (restored.at("frames")[index] !=
            baseline[checkpoint_frame + index]) {
          summary["first_divergence"] =
              {{"scenario", scenario.name},
               {"frame", checkpoint_frame + index + 1}};
          write_json(evidence / "summary.json", summary);
          throw std::runtime_error("RESTORE_CANONICAL_DIVERGENCE");
        }
      }
      summary["runs"].push_back({{"name", scenario.name},
                                  {"mode", "restore"},
                                  {"from", checkpoint_frame},
                                  {"pid", identity.pid},
                                  {"born_utc_100ns", identity.born},
                                  {"frames", 500},
                                  {"divergences", 0}});
    }
  }
  summary["processes"] = processes.size();
  summary["full_frames"] = 18000;
  summary["continuation_frames"] = 4500;
  summary["divergences"] = 0;
  write_json(evidence / "summary.json", summary);
  std::cout << "MEGADRIVE_REPLAY_PASS evidence=" << evidence.string()
            << " processes=" << processes.size() << '\n';
  return 0;
}
} // namespace

int main(int argc, char **argv) {
  try {
    if (argc == 13 && std::string_view(argv[1]) == "--worker") {
      return worker(argv[2], argv[3], argv[4], argv[5], argv[6], argv[7],
                    argv[8], std::string_view(argv[9]) == "action",
                    std::string_view(argv[10]) == "full",
                    std::string_view(argv[11]) == "fx02",
                    std::stoull(argv[12]));
    }
    if (argc == 3)
      return parent(argv[0], argv[1], argv[2]);
    return 2;
  } catch (const std::exception &error) {
    std::cerr << "MEGADRIVE_REPLAY_FAILED: " << error.what() << '\n';
    return 1;
  }
}
