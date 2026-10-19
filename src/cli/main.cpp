#include "public_client.hpp"
#include "replay_trace.hpp"
#include <windows.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace {
using namespace tessera::cli;
using Clock = std::chrono::steady_clock;

std::string utf8(std::wstring_view text) {
  ensure(text.size() <=
         static_cast<std::size_t>(std::numeric_limits<int>::max()));
  if (text.empty())
    return {};
  const auto count = WideCharToMultiByte(
      CP_UTF8, WC_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()),
      nullptr, 0, nullptr, nullptr);
  ensure(count > 0);
  std::string result(static_cast<std::size_t>(count), '\0');
  ensure(WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(),
                             static_cast<int>(text.size()), result.data(),
                             count, nullptr, nullptr) == count);
  return result;
}

struct Arguments {
  std::string command;
  std::map<std::string, std::wstring> options;
  std::filesystem::path detected_file;
  std::optional<std::uint64_t> frames;
  std::uint32_t system_override{};
  std::string backend = "synthetic.complete";
  std::string scenario = "action";
  std::string observer = "off";
};

Arguments arguments(int argc, wchar_t **argv) {
  ensure(argc >= 2, TESSERA_INVALID_ARGUMENT,
         "Uso: tessera backend list | detect ARCHIVO | demo | replay.");
  Arguments result;
  result.command = utf8(argv[1]);
  if (result.command == "backend") {
    ensure(argc == 3 && std::wstring_view(argv[2]) == L"list",
           TESSERA_INVALID_ARGUMENT, "Uso: tessera backend list.");
    result.command = "backend list";
    return result;
  }
  ensure(result.command == "detect" || result.command == "demo" ||
             result.command == "replay",
         TESSERA_INVALID_ARGUMENT, "El comando no es válido.");
  int next = 2;
  if (result.command == "detect") {
    ensure(argc >= 3, TESSERA_INVALID_ARGUMENT,
           "Falta el archivo que se desea detectar.");
    result.detected_file = argv[next++];
  }
  while (next < argc) {
    const auto key = utf8(argv[next++]);
    ensure(
        next < argc && key.starts_with("--") && !result.options.contains(key),
        TESSERA_INVALID_ARGUMENT, "La opción está repetida o no tiene valor.");
    const bool supported =
        result.command == "detect"
            ? key == "--system"
            : key == "--backend" || key == "--frames" || key == "--observer" ||
                  key == "--report" ||
                  (result.command == "demo"
                       ? key == "--scenario" || key == "--fixture-dir"
                       : key == "--content" || key == "--config" ||
                             key == "--initial-memory" || key == "--trace");
    ensure(supported, TESSERA_INVALID_ARGUMENT,
           "La opción no es válida para este comando.");
    result.options.emplace(key, argv[next++]);
  }
  const auto string_option = [&result](std::string_view name,
                                       std::string fallback) {
    const auto found = result.options.find(std::string(name));
    return found == result.options.end() ? fallback : utf8(found->second);
  };
  result.backend = string_option("--backend", result.backend);
  result.scenario = string_option("--scenario", result.scenario);
  result.observer = string_option("--observer", result.observer);
  ensure(result.observer == "off" || result.observer == "full" ||
             result.observer == "two-readers",
         TESSERA_INVALID_ARGUMENT,
         "La observación debe ser off, full o two-readers.");
  if (result.options.contains("--frames")) {
    const auto text = utf8(result.options.at("--frames"));
    std::uint64_t count = 0;
    ensure(!text.empty());
    for (const auto ch : text) {
      ensure(ch >= '0' && ch <= '9' &&
             count <= (65536ULL - static_cast<unsigned>(ch - '0')) / 10);
      count = count * 10 + static_cast<unsigned>(ch - '0');
    }
    ensure(count > 0 && count <= 65536, TESSERA_INVALID_ARGUMENT,
           "La cantidad de frames debe estar entre 1 y 65536.");
    result.frames = count;
  }
  if (result.command == "detect" && result.options.contains("--system")) {
    ensure(utf8(result.options.at("--system")) == "mega-drive",
           TESSERA_INVALID_ARGUMENT,
           "El sistema explícito admitido es mega-drive.");
    result.system_override = TESSERA_SYSTEM_MEGA_DRIVE;
  }
  if (result.command == "demo") {
    ensure(result.backend == "synthetic.minimal" ||
           result.backend == "synthetic.complete");
    ensure(result.scenario == "neutral" || result.scenario == "action");
    if (!result.frames)
      result.frames = 60;
  }
  if (result.command == "replay") {
    for (const auto key :
         {"--backend", "--content", "--config", "--initial-memory", "--trace"})
      ensure(result.options.contains(key), TESSERA_INVALID_ARGUMENT,
             "Replay exige backend, contenido, configuración, memoria inicial "
             "y traza.");
  }
  return result;
}

Bytes read_file(const std::filesystem::path &path) {
  std::error_code error;
  const auto size = std::filesystem::file_size(path, error);
  if (error)
    fail(TESSERA_IO_OTHER, TESSERA_PHASE_READ,
         "No se puede leer el archivo solicitado.");
  ensure(size <= file_limit, TESSERA_BUDGET_EXCEEDED,
         "El archivo supera el límite de 64 MiB.");
  std::ifstream input(path, std::ios::binary);
  if (!input)
    fail(TESSERA_IO_OTHER, TESSERA_PHASE_READ,
         "No se puede abrir el archivo solicitado.");
  Bytes result(static_cast<std::size_t>(size));
  if (!result.empty())
    input.read(reinterpret_cast<char *>(result.data()),
               static_cast<std::streamsize>(result.size()));
  if (!input)
    fail(TESSERA_IO_OTHER, TESSERA_PHASE_READ,
         "No se pudo leer el archivo completo.");
  return result;
}

void write_file(const std::filesystem::path &path, View bytes) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  if (!output)
    fail(TESSERA_IO_OTHER, TESSERA_PHASE_WRITE,
         "No se puede abrir el archivo de salida.");
  if (!bytes.empty())
    output.write(reinterpret_cast<const char *>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
  output.flush();
  if (!output)
    fail(TESSERA_IO_OTHER, TESSERA_PHASE_WRITE,
         "No se pudo escribir el archivo completo.");
}

Json backend_json(const tessera_backend_info_v1 &backend) {
  return {{"id", fixed_string(backend.id)},
          {"version", fixed_string(backend.version)},
          {"build_id", hex_bytes(backend.build_id)},
          {"kind", backend.kind},
          {"system", backend.system},
          {"capabilities", backend.capabilities}};
}

std::uint64_t creation_time() {
  FILETIME created{}, exited{}, kernel{}, user{};
  if (!GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user))
    fail(TESSERA_BACKEND_FAILURE, TESSERA_PHASE_NONE,
         "No se pudo consultar la identidad del proceso.");
  return (std::uint64_t{created.dwHighDateTime} << 32) | created.dwLowDateTime;
}

struct Samples {
  std::vector<double> step, hashes, observation;
  static Json series(const std::vector<double> &source) {
    auto sorted = source;
    std::sort(sorted.begin(), sorted.end());
    const auto percentile = [&sorted](std::size_t percent) {
      if (sorted.empty())
        return 0.0;
      const auto rank = (sorted.size() * percent + 99) / 100;
      return sorted[rank - 1];
    };
    return {{"unit", "microseconds"}, {"count", source.size()},
            {"samples", source},      {"p50", percentile(50)},
            {"p95", percentile(95)},  {"p99", percentile(99)}};
  }
  Json json() const {
    return {{"step", series(step)},
            {"canonical_hashes", series(hashes)},
            {"observation", series(observation)}};
  }
};
struct FrameRecord {
  std::uint64_t frame{};
  CanonicalDigests hashes;
};

void publish_frames(Json &report, const std::vector<FrameRecord> &records,
                    std::uint64_t completed, const Samples &costs) {
  auto frames = Json::array();
  frames.get_ref<Json::array_t &>().reserve(records.size());
  for (const auto &record : records) {
    frames.push_back({{"frame", record.frame},
                      {"state", record.hashes.state},
                      {"video", record.hashes.video},
                      {"audio", record.hashes.audio}});
  }
  report["frames"] = std::move(frames);
  report["completed_frames"] = completed;
  if (report.contains("requested_frames")) {
    report["costs"] = costs.json();
    report["cost_scope"] =
        "Per-process wall-clock microseconds: API replay step; canonical "
        "exports/encoding/SHA-256; observation reads. Setup, preflight, frame "
        "metadata collection and final JSON/file output excluded; not T032 "
        "acceptance.";
  }
}
double microseconds(Clock::time_point begin, Clock::time_point end) {
  return std::chrono::duration<double, std::micro>(end - begin).count();
}

struct SnapshotReference {
  tessera_snapshot handle{};
  SnapshotReference() = default;
  SnapshotReference(const SnapshotReference &) = delete;
  SnapshotReference &operator=(const SnapshotReference &) = delete;
  ~SnapshotReference() {
    if (handle != 0) {
      auto context = init<tessera_context_v1>();
      (void)tessera_snapshot_release_v1(handle, &context);
    }
  }
};

struct Observer {
  std::string mode;
  std::array<std::uint64_t, 2> cursors{};
  std::uint64_t snapshot_reads{}, event_batches{};
  explicit Observer(std::string value) : mode(std::move(value)) {}
  void prepare(PublicSession &session) const {
    if (mode == "off")
      return;
    ensure((session.info().capabilities & TESSERA_CAP_SAFE_CONTROLS) != 0,
           TESSERA_UNSUPPORTED_CAPABILITY, "El backend no admite observación.");
    const std::array<tessera_control_item_v1, 2> controls{
        {{1, 1, 31}, {2, 1, 31}}};
    session.submit(1, controls);
  }
  Bytes snapshot(tessera_snapshot handle,
                 const tessera_session_info_v1 &current) {
    auto context = init<tessera_context_v1>();
    auto info = init<tessera_snapshot_info_v1>();
    require_ok(tessera_snapshot_query_v1(handle, &info, &context), context);
    ensure(info.frame_id.session == current.frame_id.session &&
               info.frame_id.epoch == current.frame_id.epoch &&
               info.frame_id.frame == current.frame_id.frame &&
               info.region_mask == 31 && info.complete_mask == 31,
           TESSERA_BACKEND_FAILURE,
           "La captura no corresponde al frame completo.");
    Bytes data;
    for (std::uint32_t index = 0; index < info.region_count; ++index) {
      auto region = init<tessera_snapshot_region_v1>();
      require_ok(
          tessera_snapshot_region_query_v1(handle, index, &region, &context),
          context);
      ensure(region.data.bytes <= file_limit &&
                 (region.data.bytes == 0 || region.data.data != nullptr),
             TESSERA_BACKEND_FAILURE);
      put(data, region.kind, 4);
      put(data, region.data.bytes, 8);
      if (region.data.bytes != 0)
        data.insert(data.end(), region.data.data,
                    region.data.data + region.data.bytes);
    }
    ++snapshot_reads;
    return data;
  }
  Bytes events(PublicSession &session, unsigned reader) {
    auto context = init<tessera_context_v1>();
    std::uint64_t required = 0, next = cursors[reader];
    const auto first =
        tessera_events_read_v1(session.handle(), cursors[reader], {nullptr, 0},
                               &required, &next, &context);
    if (first != TESSERA_BUFFER_TOO_SMALL)
      require_ok(first, context);
    ensure(required <= file_limit, TESSERA_BUDGET_EXCEEDED);
    Bytes data(static_cast<std::size_t>(required));
    require_ok(tessera_events_read_v1(session.handle(), cursors[reader],
                                      {data.data(), data.size()}, &required,
                                      &next, &context),
               context);
    cursors[reader] = next;
    ++event_batches;
    return data;
  }
  void read(PublicSession &session, const tessera_context_v1 &step_context) {
    if (mode == "off")
      return;
    const auto current = session.info();
    if (current.frame_id.frame == 1)
      ensure((step_context.flags & TESSERA_CONTEXT_CONTROLS_APPLIED) != 0 &&
                 step_context.transaction == 1 &&
                 step_context.frame_id.epoch == current.frame_id.epoch &&
                 step_context.frame_id.frame == 1,
             TESSERA_BACKEND_FAILURE,
             "Falta la confirmación de los controles de observación.");
    auto context = init<tessera_context_v1>();
    SnapshotReference first;
    require_ok(
        tessera_snapshot_acquire_v1(session.handle(), &first.handle, &context),
        context);
    SnapshotReference second;
    if (mode == "two-readers") {
      require_ok(tessera_snapshot_retain_v1(first.handle, &context), context);
      second.handle = first.handle;
    }
    const auto first_data = snapshot(first.handle, current);
    if (second.handle != 0)
      ensure(first_data == snapshot(second.handle, current),
             TESSERA_BACKEND_FAILURE,
             "Los lectores no recibieron la misma captura.");
    const auto first_events = events(session, 0);
    if (second.handle != 0)
      ensure(first_events == events(session, 1), TESSERA_BACKEND_FAILURE,
             "Los cursores no recibieron el mismo historial.");
  }
  Json json() const {
    return {{"mode", mode},
            {"readers", mode == "off"           ? 0
                        : mode == "two-readers" ? 2
                                                : 1},
            {"concurrent", false},
            {"snapshot_reads", snapshot_reads},
            {"event_batches", event_batches}};
  }
};

void detect(const Arguments &args, Json &report) {
  const auto bytes = read_file(args.detected_file);
  auto context = init<tessera_context_v1>();
  auto detection = init<tessera_detection_v1>();
  std::uint32_t required = 0;
  auto status = tessera_detect_v1(view(bytes), args.system_override, &detection,
                                  nullptr, 0, &required, &context);
  ensure(required <= 1024, TESSERA_BACKEND_FAILURE);
  std::vector<tessera_candidate_v1> candidates(required);
  if (required != 0)
    status =
        tessera_detect_v1(view(bytes), args.system_override, &detection,
                          candidates.data(), required, &required, &context);
  report["outcome"] = detection.outcome;
  report["system"] = detection.system;
  report["content"] = hex_bytes(detection.content_sha256);
  report["evidence"] = detection.evidence;
  report["evidence_offset"] = detection.evidence_offset;
  report["evidence_bytes"] = detection.evidence_bytes;
  report["candidates"] = Json::array();
  for (const auto &candidate : candidates)
    report["candidates"].push_back(
        {{"system", candidate.system},
         {"evidence_class", candidate.evidence_class},
         {"evidence", candidate.evidence}});
  require_ok(status, context);
}

void execute(const Arguments &args, Json &report,
             std::vector<FrameRecord> &records, std::uint64_t &completed,
             Samples &costs) {
  const auto backend = find_backend(args.backend);
  report["backend"] = backend_json(backend);
  report["build_id"] = hex_bytes(backend.build_id);
  Bytes content, config, memory;
  ReplayTrace trace;
  if (args.command == "demo") {
    content = demo_content(args.backend);
    memory = demo_memory();
    config = demo_config(backend, memory);
    trace = make_demo_trace(content, config, *args.frames, args.scenario);
    if (args.options.contains("--fixture-dir")) {
      const std::filesystem::path directory{args.options.at("--fixture-dir")};
      std::error_code error;
      std::filesystem::create_directories(directory, error);
      if (error)
        fail(TESSERA_IO_OTHER, TESSERA_PHASE_WRITE,
             "No se pudo preparar el directorio de fixtures.");
      write_file(directory / "content.bin", content);
      write_file(directory / "config.bin", config);
      write_file(directory / "memory.bin", memory);
      write_file(directory / "trace.bin", trace.bytes);
      write_file(directory / "trace.json", text_bytes(trace.json.dump(2)));
    }
  } else {
    content = read_file(args.options.at("--content"));
    config = read_file(args.options.at("--config"));
    memory = read_file(args.options.at("--initial-memory"));
    trace = parse_trace(read_file(args.options.at("--trace")));
  }
  const auto requested = args.frames.value_or(trace.length);
  report["requested_frames"] = requested;
  report["trace_length"] = trace.length;
  report["content"] = digest(content);
  report["config"] = digest(config);
  report["trace"] = digest(trace.bytes);
  PublicSession session(args.backend, content, config, memory);
  auto initial = session.info();
  const auto initial_state = digest(session.state());
  report["lifecycle"] = {{"step_completed", 0},
                         {"reset_completed", 0},
                         {"preflight", args.command == "demo"}};
  if (args.command == "demo") {
    session.pause();
    ensure(session.info().frame_id.frame == 0, TESSERA_BACKEND_FAILURE);
    session.step(digital_inputs());
    ensure(session.info().frame_id.frame == 1, TESSERA_BACKEND_FAILURE);
    report["lifecycle"]["step_completed"] = 1;
    session.reset();
    const auto reset = session.info();
    ensure(reset.frame_id.frame == 0 && reset.logical_tick == 0 &&
               reset.frame_id.epoch != initial.frame_id.epoch &&
               reset.state == TESSERA_SESSION_PAUSED &&
               initial_state == digest(session.state()),
           TESSERA_BACKEND_FAILURE,
           "El reinicio de la demo no reprodujo el estado inicial.");
  }
  initial = session.info();
  report["initial_state"] = digest(session.state());
  report["clock"] = {{"tick_hz_num", initial.tick_hz_num},
                     {"tick_hz_den", initial.tick_hz_den}};
  session.bind(trace.bytes);
  Observer observer(args.observer);
  observer.prepare(session);
  records.reserve(static_cast<std::size_t>(requested));
  costs.step.reserve(static_cast<std::size_t>(requested));
  costs.hashes.reserve(static_cast<std::size_t>(requested));
  costs.observation.reserve(static_cast<std::size_t>(requested));
  for (std::uint64_t frame = 1; frame <= requested; ++frame) {
    const auto started = Clock::now();
    const auto status = session.replay_next();
    const auto advanced = Clock::now();
    const auto step_context = session.context();
    if (status == TESSERA_END_OF_TRACE)
      fail(TESSERA_INVALID_TRACE, TESSERA_PHASE_STEP,
           "La traza terminó antes de completar los frames solicitados.");
    require_ok(status, step_context);
    const auto hash_started = Clock::now();
    auto hashes = canonical_digests(session);
    const auto hashed = Clock::now();
    const auto current = session.info();
    ensure(current.frame_id.frame == frame &&
               current.state == TESSERA_SESSION_PAUSED,
           TESSERA_BACKEND_FAILURE);
    records.push_back({frame, std::move(hashes)});
    completed = frame;
    const auto observation_started = Clock::now();
    observer.read(session, step_context);
    const auto observed = Clock::now();
    costs.step.push_back(microseconds(started, advanced));
    costs.hashes.push_back(microseconds(hash_started, hashed));
    costs.observation.push_back(microseconds(observation_started, observed));
  }
  session.pause();
  report["eof_code"] = 0;
  if (requested == trace.length) {
    const auto before = session.info();
    const auto before_state = digest(session.state());
    const auto status = session.replay_next();
    ensure(status == TESSERA_END_OF_TRACE, TESSERA_BACKEND_FAILURE,
           "Falta el final explícito de la traza.");
    const auto after = session.info();
    ensure(after.frame_id.frame == before.frame_id.frame &&
               after.frame_id.epoch == before.frame_id.epoch &&
               after.next_replay_frame == before.next_replay_frame &&
               after.state == TESSERA_SESSION_PAUSED &&
               before_state == digest(session.state()),
           TESSERA_BACKEND_FAILURE, "El final de la traza alteró la sesión.");
    report["eof_code"] = TESSERA_END_OF_TRACE;
  }
  report["observer"] = observer.json();
  session.close();
  report["closed"] = true;
}
} // namespace

int wmain(int argc, wchar_t **argv) {
  (void)SetConsoleOutputCP(CP_UTF8);
  Json report{{"schema_version", 1},
              {"code", 0},
              {"phase", 0},
              {"text", "Operación completada."},
              {"completed_frames", 0},
              {"frames", Json::array()}};
  int failure_exit = 2;
  std::optional<std::filesystem::path> report_path;
  std::vector<FrameRecord> records;
  std::uint64_t completed = 0;
  Samples costs;
  try {
    const auto args = arguments(argc, argv);
    failure_exit = 1;
    report["command"] = args.command;
    if (args.options.contains("--report"))
      report_path = args.options.at("--report");
    report["pid"] = GetCurrentProcessId();
    report["process_creation_time_utc_100ns"] = creation_time();
    auto context = init<tessera_context_v1>();
    auto api = init<tessera_api_info_v1>();
    require_ok(tessera_api_query_v1(1, 0, &api, &context), context);
    report["api"] = {{"major", api.major}, {"minor", api.minor}};
    if (args.command == "backend list") {
      report["backends"] = Json::array();
      for (const auto &backend : backends())
        report["backends"].push_back(backend_json(backend));
    } else if (args.command == "detect")
      detect(args, report);
    else
      execute(args, report, records, completed, costs);
    publish_frames(report, records, completed, costs);
    const auto text = report.dump(2);
    if (report_path)
      write_file(*report_path, text_bytes(text));
    std::cout << text << '\n';
    return 0;
  } catch (const ClientError &error) {
    report["code"] = error.context.code;
    report["phase"] = error.context.phase;
    report["text"] =
        error.text.empty() ? status_text(error.context) : error.text;
    report["context"] = {{"session", error.context.frame_id.session},
                         {"epoch", error.context.frame_id.epoch},
                         {"frame", error.context.frame_id.frame},
                         {"transaction", error.context.transaction},
                         {"cancel_reason", error.context.cancel_reason},
                         {"flags", error.context.flags},
                         {"os_domain", error.context.os_domain},
                         {"os_code", error.context.os_code}};
  } catch (const std::bad_alloc &) {
    report["code"] = TESSERA_OUT_OF_MEMORY;
    report["text"] = "No hay memoria suficiente para completar la operación.";
  } catch (const std::exception &) {
    report["code"] = TESSERA_IO_OTHER;
    report["text"] = "La operación no pudo completarse.";
  }
  publish_frames(report, records, completed, costs);
  const auto text = report.dump(2);
  if (report_path) {
    try {
      write_file(*report_path, text_bytes(text));
    } catch (...) {
    }
  }
  std::cout << text << '\n';
  std::cerr << report.at("text").get<std::string>() << '\n';
  return failure_exit;
}
