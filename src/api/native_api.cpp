#include "api/call.hpp"
#include "build_identity.hpp"
#include "observation/region_table.hpp"
#include "session/session.hpp"
#include "systems/system_detector.hpp"
#include "systems/system_registry.hpp"
#include <array>
#include <cstring>
#include <string>
#include <string_view>
#include <tessera/tessera.h>

namespace {
using namespace tessera;
using namespace tessera::api;
constexpr std::uint64_t complete_caps = 0x1FFF;
std::array<std::uint8_t, 32> build_identity() {
  const std::string_view text = TESSERA_CORE_BUILD_ID;
  std::array<std::uint8_t, 32> result{};
  auto value = [](char c) { return c <= '9' ? c - '0' : c - 'a' + 10; };
  for (std::size_t i = 0; i < 32; ++i)
    result[i] = static_cast<std::uint8_t>(value(text[i * 2]) * 16 +
                                          value(text[i * 2 + 1]));
  return result;
}
tessera_limits_v1 limits() {
  auto v = initialized<tessera_limits_v1>();
  v.content_bytes = v.checkpoint_bytes = v.retained_bytes = v.trace_bytes =
      byte_limit;
  v.video_width = v.video_height = 1024;
  v.pcm_frames = 16384;
  v.event_count = 4096;
  v.event_payload_bytes = 256;
  v.input_count = v.control_count = 64;
  v.retained_count = 8;
  v.trace_frames = 65536;
  v.ports = 2;
  v.controls_per_port = 32;
  v.snapshot_bytes = 8 * 1024 * 1024;
  return v;
}
std::array<tessera_backend_info_v1, kRegistrationCount> backends() {
  std::array<tessera_backend_info_v1, kRegistrationCount> values{};
  const auto registrations = SystemRegistry::registrations();
  const auto hash = build_identity();
  for (std::size_t i = 0; i < values.size(); ++i) {
    auto &v = values[i];
    v = initialized<tessera_backend_info_v1>();
    v.kind = registrations[i].kind;
    v.system = registrations[i].system;
    v.capabilities = registrations[i].capabilities;
    std::memcpy(v.id, registrations[i].id.data(), registrations[i].id.size());
    std::memcpy(v.version, "0.1.0", 5);
    std::copy(hash.begin(), hash.end(), v.build_id);
  }
  return values;
}
std::string status_text(tessera_status code) {
  switch (code) {
  case TESSERA_OK:
    return "Operación completada";
  case TESSERA_END_OF_TRACE:
    return "Fin de traza";
  case TESSERA_INVALID_ARGUMENT:
    return "Argumento inválido";
  case TESSERA_ABI_INCOMPATIBLE:
    return "ABI incompatible";
  case TESSERA_BUFFER_TOO_SMALL:
    return "Buffer insuficiente";
  case TESSERA_INVALID_HANDLE:
    return "Identificador inválido";
  case TESSERA_WRONG_STATE:
    return "Estado incorrecto";
  case TESSERA_UNSUPPORTED_CAPABILITY:
    return "Capacidad no disponible";
  case TESSERA_UNKNOWN_CONTENT:
    return "Contenido desconocido";
  case TESSERA_AMBIGUOUS_CONTENT:
    return "Contenido ambiguo";
  case TESSERA_INVALID_CONTENT:
    return "Contenido inválido";
  case TESSERA_IMPLEMENTATION_UNAVAILABLE:
    return "Implementación no disponible";
  case TESSERA_STALE_EPOCH:
    return "Época obsoleta";
  case TESSERA_INPUT_LATE:
    return "Entrada tardía";
  case TESSERA_INPUT_OUT_OF_ORDER:
    return "Entrada fuera de orden";
  case TESSERA_INPUT_DUPLICATE:
    return "Entrada duplicada";
  case TESSERA_INPUT_INCOMPLETE:
    return "Entrada incompleta";
  case TESSERA_INPUT_CONTRADICTORY:
    return "Entrada contradictoria";
  case TESSERA_INVALID_TRACE:
    return "Traza inválida";
  case TESSERA_BUDGET_EXCEEDED:
    return "Presupuesto agotado";
  case TESSERA_CONTROL_PENDING:
    return "Transacción pendiente";
  case TESSERA_CHECKPOINT_INCOMPATIBLE:
    return "Checkpoint incompatible";
  case TESSERA_CHECKPOINT_INVALID:
    return "Checkpoint inválido";
  case TESSERA_IO_NO_SPACE:
    return "E/S sin espacio";
  case TESSERA_IO_ACCESS_DENIED:
    return "E/S acceso denegado";
  case TESSERA_IO_PUBLISH_FAILED:
    return "E/S publicación fallida";
  case TESSERA_IO_OTHER:
    return "Error de E/S";
  case TESSERA_OUT_OF_MEMORY:
    return "Memoria insuficiente";
  case TESSERA_BACKEND_FAILURE:
    return "Fallo del backend";
  case TESSERA_NOT_SUBSCRIBED:
    return "Suscripción inactiva";
  case TESSERA_BUSY:
    return "Núcleo ocupado ejecutando un frame";
  default:
    return "Código de estado desconocido";
  }
}
} // namespace
extern "C" {
tessera_status TESSERA_CALL tessera_api_query_v1(std::uint16_t major,
                                                 std::uint16_t minor,
                                                 tessera_api_info_v1 *out,
                                                 tessera_context_v1 *ctx) {
  return invoke(ctx, TESSERA_PHASE_VALIDATE, [&](Call &c) {
    c.object(out);
    require(major == 1 && minor <= 1, TESSERA_ABI_INCOMPATIBLE);
    c.start();
    auto v = initialized<tessera_api_info_v1>();
    v.major = 1;
    v.minor = 1;
    v.capabilities = complete_caps;
    v.pointer_bytes = 8;
    v.max_alignment = 8;
    publish(out, v);
    return TESSERA_OK;
  });
}
tessera_status TESSERA_CALL tessera_observation_limits_query_v1(
    tessera_session session, tessera_observation_limits_v1 *out,
    tessera_context_v1 *ctx) {
  return invoke(ctx, TESSERA_PHASE_READ, [&](Call &c) {
    c.object(out);
    c.start();
    if (session)
      (void)sessions.get(session);
    auto v = initialized<tessera_observation_limits_v1>();
    v.header.minor = 1;
    v.max_rom_bytes = UINT64_C(16) * 1024 * 1024;
    v.max_state_bytes = UINT64_C(2) * 1024 * 1024;
    v.max_journal_events = TESSERA_OBS_JOURNAL_MAX_EVENTS;
    v.max_audio_writes = 8192;
    v.max_parsed_sprites = 128;
    publish(out, v);
    return TESSERA_OK;
  });
}
tessera_status TESSERA_CALL tessera_limits_query_v1(tessera_session session,
                                                    tessera_limits_v1 *out,
                                                    tessera_context_v1 *ctx) {
  return invoke(ctx, TESSERA_PHASE_READ, [&](Call &c) {
    c.object(out);
    c.start();
    if (session)
      (void)sessions.get(session);
    publish(out, limits());
    return TESSERA_OK;
  });
}
tessera_status TESSERA_CALL tessera_backends_enumerate_v1(
    tessera_backend_info_v1 *out, std::uint32_t capacity,
    std::uint32_t *required, tessera_context_v1 *ctx) {
  return invoke(ctx, TESSERA_PHASE_READ, [&](Call &c) {
    c.object(required);
    constexpr auto count = static_cast<std::uint32_t>(kRegistrationCount);
    std::array<tessera_backend_info_v1 *, kRegistrationCount> slots{};
    auto cursor = reinterpret_cast<std::uintptr_t>(out);
    for (std::uint32_t i = 0; i < std::min(capacity, count); ++i) {
      auto *slot = reinterpret_cast<tessera_backend_info_v1 *>(cursor);
      c.object(slot);
      slots[i] = slot;
      cursor += slot->header.size;
    }
    if (capacity < count) {
      c.start();
      *required = count;
      return TESSERA_BUFFER_TOO_SMALL;
    }
    // Validate every prefix before publishing the first array element.
    c.start();
    const auto values = backends();
    for (std::size_t i = 0; i < values.size(); ++i)
      publish(slots[i], values[i]);
    *required = count;
    return TESSERA_OK;
  });
}
tessera_status TESSERA_CALL tessera_detect_v1(
    tessera_bytes_v1 content, std::uint32_t system_override,
    tessera_detection_v1 *out, tessera_candidate_v1 *candidates,
    std::uint32_t capacity, std::uint32_t *required, tessera_context_v1 *ctx) {
  return invoke(ctx, TESSERA_PHASE_DETECT, [&](Call &c) {
    c.input(content);
    c.object(out);
    c.object(required);
    c.range(candidates, std::uint64_t(capacity) * sizeof(*candidates),
            alignof(tessera_candidate_v1));
    c.start();
    const auto result = SystemDetector::detect(
        {content.data, static_cast<std::size_t>(content.bytes)},
        system_override);
    *required = static_cast<std::uint32_t>(result.candidates.size());
    if (capacity < result.candidates.size())
      return TESSERA_BUFFER_TOO_SMALL;
    publish(out, result.info);
    std::copy(result.candidates.begin(), result.candidates.end(), candidates);
    return result.status;
  });
}
tessera_status TESSERA_CALL
tessera_session_create_v1(const tessera_session_create_info_v1 *in,
                          tessera_session *out, tessera_context_v1 *ctx) {
  return invoke(ctx, TESSERA_PHASE_CREATE, [&](Call &c) {
    c.object(in);
    c.object(out);
    c.input(in->backend_id, 64);
    c.input(in->content);
    c.input(in->config);
    c.input(in->initial_persistent_memory);
    c.start();
    require(in->flags == 0 && in->system_override <= 1);
    const auto requested =
        in->backend_id.bytes
            ? std::string(reinterpret_cast<const char *>(in->backend_id.data),
                          static_cast<std::size_t>(in->backend_id.bytes))
            : std::string{};
    const auto id = SystemRegistry::resolve(
        requested,
        {in->content.data, static_cast<std::size_t>(in->content.bytes)},
        in->system_override);
    const auto build = build_identity();
    auto session = std::make_shared<Session>(
        id,
        wire::View{in->content.data,
                   static_cast<std::size_t>(in->content.bytes)},
        wire::View{in->config.data, static_cast<std::size_t>(in->config.bytes)},
        wire::View{
            in->initial_persistent_memory.data,
            static_cast<std::size_t>(in->initial_persistent_memory.bytes)},
        build);
    const auto token = sessions.add(session);
    session->handle = token;
    *out = token;
    c.context().frame_id = session->identity();
    return TESSERA_OK;
  });
}
tessera_status TESSERA_CALL tessera_session_query_v1(
    tessera_session h, tessera_session_info_v1 *out, tessera_context_v1 *ctx) {
  return invoke(ctx, TESSERA_PHASE_READ, [&](Call &c) {
    auto s = sessions.get(h);
    c.object(out);
    c.start();
    publish(out, s->info());
    return TESSERA_OK;
  });
}
tessera_status TESSERA_CALL tessera_session_pause_v1(tessera_session h,
                                                     tessera_context_v1 *ctx) {
  return invoke(ctx, TESSERA_PHASE_VALIDATE, [&](Call &c) {
    auto s = sessions.get(h);
    c.context().frame_id = s->identity();
    return TESSERA_OK;
  });
}
tessera_status TESSERA_CALL
tessera_session_step_v1(tessera_session h, const tessera_input_frame_v1 *in,
                        tessera_context_v1 *ctx) {
  return invoke(ctx, TESSERA_PHASE_STEP, [&](Call &c) {
    auto s = sessions.get(h);
    c.object(in);
    require(in->count <= 64 && in->reserved == 0);
    c.range(in->assignments,
            std::uint64_t(in->count) * sizeof(*in->assignments),
            alignof(tessera_input_assignment_v1));
    c.start();
    s->validate_target(in->epoch, in->frame);
    const auto inputs = s->validate_inputs({in->assignments, in->count});
    s->step(inputs, c.context());
    return TESSERA_OK;
  });
}
tessera_status TESSERA_CALL tessera_session_reset_v1(tessera_session h,
                                                     tessera_context_v1 *ctx) {
  return invoke(ctx, TESSERA_PHASE_RESET, [&](Call &c) {
    auto s = sessions.get(h);
    s->reset(c.context());
    return TESSERA_OK;
  });
}
tessera_status TESSERA_CALL tessera_session_close_v1(tessera_session h,
                                                     tessera_context_v1 *ctx) {
  return invoke(ctx, TESSERA_PHASE_CLOSE, [&](Call &c) {
    auto s = sessions.get(h);
    s->cancel(c.context(), TESSERA_CANCEL_CLOSE);
    sessions.release(h);
    return TESSERA_OK;
  });
}
tessera_status TESSERA_CALL tessera_trace_validate_v1(tessera_bytes_v1 trace,
                                                      tessera_context_v1 *ctx) {
  return invoke(ctx, TESSERA_PHASE_VALIDATE, [&](Call &c) {
    c.input(trace);
    c.start();
    (void)wire::trace({trace.data, static_cast<std::size_t>(trace.bytes)});
    return TESSERA_OK;
  });
}
tessera_status TESSERA_CALL tessera_session_bind_trace_v1(
    tessera_session h, tessera_bytes_v1 trace, tessera_context_v1 *ctx) {
  return invoke(ctx, TESSERA_PHASE_VALIDATE, [&](Call &c) {
    auto s = sessions.get(h);
    c.input(trace);
    c.start();
    auto candidate =
        wire::trace({trace.data, static_cast<std::size_t>(trace.bytes)});
    require(candidate.content_sha == s->content_sha &&
                candidate.config_sha == s->config_sha &&
                candidate.layout == s->machine->input_layout(),
            TESSERA_INVALID_TRACE);
    require(s->machine->frame() == 0, TESSERA_WRONG_STATE);
    Bytes raw(trace.data, trace.data + trace.bytes);
    s->replay = std::move(candidate);
    s->replay_raw = std::move(raw);
    s->replay_next = 1;
    return TESSERA_OK;
  });
}
tessera_status TESSERA_CALL
tessera_session_replay_next_v1(tessera_session h, tessera_context_v1 *ctx) {
  return invoke(ctx, TESSERA_PHASE_STEP, [&](Call &c) {
    auto s = sessions.get(h);
    c.start();
    require(s->replay.has_value(), TESSERA_WRONG_STATE);
    if (s->replay_next > s->replay->frames.size())
      return TESSERA_END_OF_TRACE;
    require(s->replay_next == s->machine->frame() + 1,
            TESSERA_INPUT_OUT_OF_ORDER);
    s->step(s->replay->frames[static_cast<std::size_t>(s->replay_next - 1)],
            c.context());
    ++s->replay_next;
    return TESSERA_OK;
  });
}
tessera_status TESSERA_CALL tessera_controls_submit_v1(
    tessera_session h, const tessera_control_transaction_v1 *in,
    tessera_context_v1 *ctx) {
  return invoke(ctx, TESSERA_PHASE_VALIDATE, [&](Call &c) {
    auto s = sessions.get(h);
    c.object(in);
    require(in->count <= 64 && in->reserved == 0);
    c.range(in->items, std::uint64_t(in->count) * sizeof(*in->items),
            alignof(tessera_control_item_v1));
    c.start();
    require(s->machine->capabilities() & TESSERA_CAP_SAFE_CONTROLS,
            TESSERA_UNSUPPORTED_CAPABILITY);
    s->validate_target(in->epoch, in->frame);
    require(in->id != 0 && in->count > 0);
    auto regions = s->region_mask, events = s->event_mask;
    unsigned seen{};
    for (std::uint32_t i = 0; i < in->count; ++i) {
      const auto &item = in->items[i];
      require(item.id >= 1 && item.id <= 2 && item.value_type == 1 &&
              (item.value & ~UINT64_C(31)) == 0);
      const auto bit = 1U << item.id;
      require(!(seen & bit));
      seen |= bit;
      if (item.id == 1)
        regions = item.value;
      else
        events = item.value;
    }
    require(!s->pending, TESSERA_CONTROL_PENDING);
    s->pending = Pending{in->id, in->epoch, in->frame, regions, events};
    return TESSERA_OK;
  });
}
tessera_status TESSERA_CALL tessera_original_frame_query_v1(
    tessera_session h, tessera_original_frame_v1 *out,
    tessera_context_v1 *ctx) {
  return invoke(ctx, TESSERA_PHASE_READ, [&](Call &c) {
    auto s = sessions.get(h);
    c.object(out);
    c.start();
    require(s->machine->original_available(), TESSERA_WRONG_STATE);
    auto v = initialized<tessera_original_frame_v1>();
    v.frame_id = s->identity();
    v.width = s->machine->frame_width();
    v.height = s->machine->frame_height();
    v.row_bytes = v.width * 4;
    v.video_format = 1;
    v.aspect_num = v.aspect_den = 1;
    const auto &pixels = s->machine->pixels();
    const auto &pcm = s->machine->pcm();
    v.pixels = {pixels.data(), pixels.size()};
    v.pcm = {pcm.data(), pcm.size()};
    v.sample_rate = 48000;
    v.channels = 2;
    v.pcm_frames = pcm.size() / 4;
    v.tick_begin = s->machine->tick() - s->machine->frame_ticks();
    v.tick_end = s->machine->tick();
    publish(out, v);
    return TESSERA_OK;
  });
}
tessera_status TESSERA_CALL tessera_snapshot_acquire_v1(
    tessera_session h, tessera_snapshot *out, tessera_context_v1 *ctx) {
  return invoke(ctx, TESSERA_PHASE_READ, [&](Call &c) {
    auto s = sessions.get(h);
    c.object(out);
    c.start();
    require(s->machine->capabilities() & TESSERA_CAP_MEMORY,
            TESSERA_UNSUPPORTED_CAPABILITY);
    require(s->machine->frame() > 0, TESSERA_WRONG_STATE);
    require(s->latest != nullptr, TESSERA_BUDGET_EXCEEDED);
    const auto token = snapshots.add(s->latest);
    *out = token;
    return TESSERA_OK;
  });
}
tessera_status TESSERA_CALL
tessera_snapshot_retain_v1(tessera_snapshot h, tessera_context_v1 *ctx) {
  return invoke(ctx, TESSERA_PHASE_READ, [&](Call &c) {
    c.start();
    snapshots.retain(h);
    return TESSERA_OK;
  });
}
tessera_status TESSERA_CALL
tessera_snapshot_query_v1(tessera_snapshot h, tessera_snapshot_info_v1 *out,
                          tessera_context_v1 *ctx) {
  return invoke(ctx, TESSERA_PHASE_READ, [&](Call &c) {
    auto s = snapshots.get(h);
    c.object(out);
    c.start();
    publish(out, s->info);
    return TESSERA_OK;
  });
}
tessera_status TESSERA_CALL tessera_snapshot_region_query_v1(
    tessera_snapshot h, std::uint32_t index, tessera_snapshot_region_v1 *out,
    tessera_context_v1 *ctx) {
  return invoke(ctx, TESSERA_PHASE_READ, [&](Call &c) {
    auto s = snapshots.get(h);
    c.object(out);
    c.start();
    require(index < s->regions.size());
    const auto &r = s->regions[index];
    auto v = initialized<tessera_snapshot_region_v1>();
    v.kind = r.kind;
    v.encoding = r.encoding;
    v.address = r.address;
    v.byte_count = r.data.size();
    v.element_bytes = r.element_bytes;
    v.element_count = r.element_count;
    v.data = {r.data.data(), r.data.size()};
    v.bank = r.bank;
    v.flags = r.flags;
    publish(out, v);
    return TESSERA_OK;
  });
}
tessera_status TESSERA_CALL
tessera_snapshot_release_v1(tessera_snapshot h, tessera_context_v1 *ctx) {
  return invoke(ctx, TESSERA_PHASE_READ, [&](Call &c) {
    c.start();
    snapshots.release(h);
    return TESSERA_OK;
  });
}
tessera_status TESSERA_CALL tessera_events_read_v1(
    tessera_session h, std::uint64_t cursor, tessera_buffer_v1 out,
    std::uint64_t *required, std::uint64_t *next, tessera_context_v1 *ctx) {
  return invoke(ctx, TESSERA_PHASE_READ, [&](Call &c) {
    auto s = sessions.get(h);
    c.output(out);
    c.object(required);
    c.object(next);
    c.start();
    require(s->machine->capabilities() & TESSERA_CAP_EVENTS,
            TESSERA_UNSUPPORTED_CAPABILITY);
    std::uint64_t next_value{};
    const auto b = s->event_batch(cursor, next_value);
    const auto result = copy_bytes(b, out, required);
    if (result == TESSERA_OK)
      *next = next_value;
    return result;
  });
}
tessera_status TESSERA_CALL tessera_losses_query_v1(tessera_session h,
                                                    tessera_buffer_v1 out,
                                                    std::uint64_t *required,
                                                    tessera_context_v1 *ctx) {
  return invoke(ctx, TESSERA_PHASE_READ, [&](Call &c) {
    auto s = sessions.get(h);
    c.output(out);
    c.object(required);
    c.start();
    require(s->machine->capabilities() & TESSERA_CAP_EVENTS,
            TESSERA_UNSUPPORTED_CAPABILITY);
    return copy_bytes(s->losses(), out, required);
  });
}
tessera_status TESSERA_CALL tessera_state_export_canonical_v1(
    tessera_session h, tessera_buffer_v1 out, std::uint64_t *required,
    tessera_context_v1 *ctx) {
  return invoke(ctx, TESSERA_PHASE_READ, [&](Call &c) {
    auto s = sessions.get(h);
    c.output(out);
    c.object(required);
    c.start();
    return copy_bytes(s->state(), out, required);
  });
}
tessera_status TESSERA_CALL
tessera_checkpoint_export_v1(tessera_session h, tessera_buffer_v1 out,
                             std::uint64_t *required, tessera_context_v1 *ctx) {
  return invoke(ctx, TESSERA_PHASE_READ, [&](Call &c) {
    auto s = sessions.get(h);
    c.output(out);
    c.object(required);
    c.start();
    require(s->machine->capabilities() & TESSERA_CAP_CHECKPOINT,
            TESSERA_UNSUPPORTED_CAPABILITY);
    return copy_bytes(s->checkpoint(), out, required);
  });
}
tessera_status TESSERA_CALL
tessera_checkpoint_restore_v1(tessera_session h, tessera_bytes_v1 checkpoint,
                              tessera_bytes_v1 trace, tessera_context_v1 *ctx) {
  return invoke(ctx, TESSERA_PHASE_RESTORE, [&](Call &c) {
    auto s = sessions.get(h);
    c.input(checkpoint);
    c.input(trace);
    c.start();
    require(s->machine->capabilities() & TESSERA_CAP_CHECKPOINT,
            TESSERA_UNSUPPORTED_CAPABILITY);
    s->restore({checkpoint.data, static_cast<std::size_t>(checkpoint.bytes)},
               {trace.data, static_cast<std::size_t>(trace.bytes)},
               c.context());
    return TESSERA_OK;
  });
}
tessera_status TESSERA_CALL
tessera_status_format_v1(const tessera_context_v1 *in, tessera_buffer_v1 out,
                         std::uint64_t *required) {
  if (const auto valid = context_valid(in); valid != TESSERA_OK)
    return valid;
  // Reuse range validation against a private context; never mutate the input
  // context.
  auto scratch = initialized<tessera_context_v1>();
  return invoke(&scratch, TESSERA_PHASE_READ, [&](Call &c) {
    c.object(in);
    c.output(out);
    c.object(required);
    c.start();
    std::string message = status_text(in->code) +
                          " (code=" + std::to_string(in->code) +
                          ", phase=" + std::to_string(in->phase) +
                          ", os=" + std::to_string(in->os_code) + ")";
    if (in->flags & TESSERA_CONTEXT_CONTROL_CANCELLED)
      message += "; transacción " + std::to_string(in->transaction) +
                 " cancelada, causa=" + std::to_string(in->cancel_reason);
    return copy_bytes({reinterpret_cast<const std::uint8_t *>(message.data()),
                       message.size()},
                      out, required);
  });
}
}
namespace {
// Sirven observación el backend sintético completo (TP-006.5, contenido nulo) y
// los que ofrecen IObservable (el Mega Drive nativo, TP-201). Una llamada que
// llega con un frame en marcha responde BUSY, como gpgx.
std::shared_ptr<Session> observed_session(tessera_session handle) {
  auto s = sessions.get(handle);
  require(s->backend == "synthetic.complete" ||
              s->machine->observation() != nullptr,
          TESSERA_UNSUPPORTED_CAPABILITY);
  require(!s->running.load(), TESSERA_BUSY);
  return s;
}
// Solo los backends con IObservable llevan suscripciones.
IObservable &observable(Session &s) {
  auto *o = s.machine->observation();
  require(o != nullptr, TESSERA_UNSUPPORTED_CAPABILITY);
  return *o;
}
const observation::RegionRow &region_row(std::uint32_t id) {
  require(id >= 1 && id <= observation::kRegionCount);
  return observation::kRegions[id - 1];
}
/// Byte size of the region now: the system's when it varies, else the table's.
std::uint32_t region_bytes(Session &s, const observation::RegionRow &r) {
  if (const auto *o = s.machine->observation())
    if (const auto live = o->region_byte_size(r.id); live != UINT32_MAX)
      return live;
  return r.byte_size;
}
} // namespace
extern "C" {
tessera_status TESSERA_CALL tessera_observation_region_query_v1(
    tessera_session session, std::uint32_t region_id,
    tessera_obs_region_info_v1 *out, std::uint32_t out_size,
    tessera_context_v1 *ctx) {
  return invoke(ctx, TESSERA_PHASE_READ, [&](Call &c) {
    auto s = observed_session(session);
    require(out && out_size >= sizeof(*out));
    const auto &r = region_row(region_id);
    c.start();
    tessera_obs_region_info_v1 info{};
    info.struct_size = sizeof(info);
    info.region_id = r.id;
    info.data_version = r.data_version;
    info.element_size = r.element_size;
    info.capacity = r.capacity;
    info.byte_size = region_bytes(*s, r);
    if (r.element_size == 1)
      info.capacity = info.byte_size;
    info.access_flags = r.access_flags;
    info.legacy_memory_id = r.legacy_id;
    *out = info;
    return TESSERA_OK;
  });
}
tessera_status TESSERA_CALL tessera_observation_region_read_v1(
    tessera_session session, std::uint32_t region_id, std::uint32_t offset,
    void *out, std::uint32_t byte_count, std::uint64_t expected_generation,
    std::uint64_t *actual_generation, tessera_context_v1 *ctx) {
  return invoke(ctx, TESSERA_PHASE_READ, [&](Call &c) {
    auto s = observed_session(session);
    const auto &r = region_row(region_id);
    const std::uint32_t size = region_bytes(*s, r);
    // A read of 0 bytes may pass no buffer: gpgx answers it with the state of
    // the subscription (measured, ATTRIBUTION of 0 bytes before any frame).
    require((out || byte_count == 0) && (byte_count == 0 || offset < size) &&
            std::uint64_t{offset} + byte_count <= size);
    require(expected_generation == UINT64_MAX ||
                expected_generation == s->generation,
            TESSERA_STALE_EPOCH);
    c.range(out, byte_count);
    c.start();
    if (auto *o = s->machine->observation()) {
      const auto status = o->read_region(
          region_id, offset, {static_cast<std::uint8_t *>(out), byte_count});
      require(status == TESSERA_OK, status);
    } else if (byte_count != 0) {
      std::memset(out, 0, byte_count); // regiones fijas: contenido nulo
    }
    if (actual_generation)
      *actual_generation = s->generation;
    return TESSERA_OK;
  });
}
tessera_status TESSERA_CALL tessera_observation_write_control_v1(
    tessera_session session, std::uint32_t region_id, std::uint32_t offset,
    const void *data, std::uint32_t byte_count,
    std::uint64_t expected_generation, std::uint64_t *new_generation,
    tessera_context_v1 *ctx) {
  return invoke(ctx, TESSERA_PHASE_WRITE, [&](Call &c) {
    auto s = observed_session(session);
    const auto &r = region_row(region_id);
    // Canales de supresión del core: fuera del contrato (PQ-1).
    require(region_id < 6 || region_id > 9, TESSERA_UNSUPPORTED_CAPABILITY);
    require((r.access_flags & TESSERA_OBS_REGION_ACCESS_CONTROL_WRITE) != 0);
    require(data && byte_count != 0 && offset < r.byte_size &&
            std::uint64_t{offset} + byte_count <= r.byte_size);
    require(expected_generation == UINT64_MAX ||
                expected_generation == s->generation,
            TESSERA_STALE_EPOCH);
    // Los controles del núcleo nativo llegan con TP-203 y TP-209.
    require(s->machine->observation() == nullptr,
            TESSERA_UNSUPPORTED_CAPABILITY);
    c.start();
    if (new_generation)
      *new_generation = s->generation;
    return TESSERA_OK;
  });
}
tessera_status TESSERA_CALL tessera_observation_subscriptions_get_v1(
    tessera_session session, tessera_obs_subscription_state_v1 *out,
    std::uint32_t out_size, tessera_context_v1 *ctx) {
  return invoke(ctx, TESSERA_PHASE_READ, [&](Call &c) {
    auto s = observed_session(session);
    auto &o = observable(*s);
    require(out && out_size >= sizeof(*out));
    c.range(out, sizeof(*out), alignof(tessera_obs_subscription_state_v1));
    c.start();
    const auto state = o.subscriptions();
    tessera_obs_subscription_state_v1 v{};
    v.struct_size = sizeof(v);
    v.state_version = 1;
    v.supported_mask = state.supported;
    v.active_mask = state.active;
    v.requested_mask = state.requested;
    v.activation_frame = state.activation_frame;
    *out = v;
    return TESSERA_OK;
  });
}
tessera_status TESSERA_CALL tessera_observation_subscriptions_set_v1(
    tessera_session session, std::uint32_t requested_mask,
    tessera_context_v1 *ctx) {
  return invoke(ctx, TESSERA_PHASE_WRITE, [&](Call &c) {
    auto s = observed_session(session);
    auto &o = observable(*s);
    c.start();
    // Los bits desconocidos se rechazan, no se ignoran.
    require(o.set_subscriptions(requested_mask), TESSERA_INVALID_ARGUMENT);
    return TESSERA_OK;
  });
}
tessera_status TESSERA_CALL tessera_observation_audio_events_poll_v1(
    tessera_session session, void *out, std::uint32_t event_capacity,
    std::uint32_t event_size, std::uint32_t *out_event_count,
    tessera_context_v1 *ctx) {
  return invoke(ctx, TESSERA_PHASE_READ, [&](Call &c) {
    auto s = observed_session(session);
    auto &o = observable(*s);
    require(out && out_event_count && event_capacity != 0 && event_size != 0 &&
            event_size <= 4096 &&
            std::uint64_t{event_capacity} * event_size <= byte_limit);
    c.range(out, std::uint64_t{event_capacity} * event_size);
    c.range(out_event_count, sizeof(*out_event_count), alignof(std::uint32_t));
    c.start();
    std::uint32_t count = 0;
    const auto status =
        o.poll_audio_events({static_cast<std::uint8_t *>(out),
                             std::size_t{event_capacity} * event_size},
                            event_capacity, event_size, count);
    *out_event_count = count;
    require(status == TESSERA_OK, status);
    return TESSERA_OK;
  });
}
tessera_status TESSERA_CALL tessera_observation_audio_transport_stats_v1(
    tessera_session session, tessera_obs_audio_transport_stats_v1 *out,
    std::uint32_t out_size, tessera_context_v1 *ctx) {
  return invoke(ctx, TESSERA_PHASE_READ, [&](Call &c) {
    auto s = observed_session(session);
    auto &o = observable(*s);
    require(out && out_size >= sizeof(*out));
    c.range(out, sizeof(*out), alignof(tessera_obs_audio_transport_stats_v1));
    c.start();
    const auto status = o.audio_transport_stats(
        {reinterpret_cast<std::uint8_t *>(out), sizeof(*out)});
    require(status == TESSERA_OK, status);
    return TESSERA_OK;
  });
}
}
