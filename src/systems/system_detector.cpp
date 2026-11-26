#include "systems/system_detector.hpp"
#include "api/call.hpp"
#include "backends/synthetic/synthetic_backend.hpp"
#include <algorithm>
namespace tessera {
namespace {
std::uint32_t big32(wire::View b, std::size_t offset) {
  return (std::uint32_t(b[offset]) << 24) |
         (std::uint32_t(b[offset + 1]) << 16) |
         (std::uint32_t(b[offset + 2]) << 8) | b[offset + 3];
}
bool signature(wire::View b) {
  return b.size() >= 0x104 && b[0x100] == 'S' && b[0x101] == 'E' &&
         b[0x102] == 'G' && b[0x103] == 'A';
}
bool valid(wire::View b) {
  if (b.size() < 0x200 || (b.size() & 1))
    return false;
  // The 68000 drives 24 address lines: the top byte of the vectors does not
  // count, and a stack pointer of 0 is valid (the first push wraps to the top
  // of work RAM). Streets of Rage 2 starts with SP 0, Phantasy Star IV with
  // $FFFF4FF0 and Gunstar Heroes with $FFFFFF00.
  const auto sp = big32(b, 0) & 0xffffffU, pc = big32(b, 4) & 0xffffffU;
  const auto start = big32(b, 0x1a0), end = big32(b, 0x1a4);
  return (sp & 1) == 0 && (pc & 1) == 0 && pc >= 0x200 && pc < b.size() &&
         start == 0 && end >= pc && std::uint64_t(end) < b.size();
}
bool has_tmr_sega(wire::View b, std::size_t &header_at) {
  for (const std::size_t at :
       {std::size_t(0x7FF0), std::size_t(0x3FF0), std::size_t(0x1FF0)}) {
    if (b.size() >= at + 16 && b[at] == 'T' && b[at + 1] == 'M' &&
        b[at + 2] == 'R' && b[at + 3] == ' ' && b[at + 4] == 'S' &&
        b[at + 5] == 'E' && b[at + 6] == 'G' && b[at + 7] == 'A') {
      header_at = at;
      return true;
    }
  }
  return false;
}
} // namespace
Detection SystemDetector::detect(wire::View bytes,
                                 std::uint32_t override_system) {
  require(override_system <= TESSERA_SYSTEM_SG1000);
  Detection d;
  d.info = api::initialized<tessera_detection_v1>();
  const auto digest = wire::sha(bytes);
  std::copy(digest.begin(), digest.end(), d.info.content_sha256);
  bool malformed = bytes.empty();
  const auto synthetic = wire::text("TSSYN001");
  if (bytes.size() >= 8 &&
      std::equal(synthetic.begin(), synthetic.end(), bytes.begin())) {
    try {
      (void)parse_synthetic_content(bytes);
      require(override_system == TESSERA_SYSTEM_NONE, TESSERA_INVALID_CONTENT);
      d.candidates.push_back({0, 3, 16});
      d.info.candidate_count = 1;
      d.info.evidence = 16;
      d.info.evidence_bytes = bytes.size();
      d.info.outcome = TESSERA_DETECTION_RECOGNIZED;
      d.status = TESSERA_OK;
    } catch (const Failure &) {
      malformed = true;
    }
    if (d.status == TESSERA_OK && !d.candidates.empty())
      return d;
  }
  for (const std::size_t offset : {std::size_t(0), std::size_t(512)}) {
    if (offset > bytes.size())
      continue;
    const auto payload = bytes.subspan(offset);
    if (!signature(payload))
      continue;
    if (!valid(payload)) {
      malformed = true;
      continue;
    }
    d.candidates.push_back({TESSERA_SYSTEM_MEGA_DRIVE, offset ? 2U : 1U,
                            UINT64_C(12) | (offset ? 2U : 1U)});
    if (d.candidates.size() == 1) {
      d.info.evidence_offset = offset;
      d.info.evidence_bytes = payload.size();
      d.info.evidence = d.candidates.back().evidence;
    }
  }
  if (d.candidates.empty()) {
    for (const std::size_t offset : {std::size_t(0), std::size_t(512)}) {
      if (offset > bytes.size())
        continue;
      const auto payload = bytes.subspan(offset);
      std::size_t tmr_at = 0;
      if (has_tmr_sega(payload, tmr_at)) {
        const std::uint8_t region_code =
            static_cast<std::uint8_t>((payload[tmr_at + 15] >> 4) & 0x0FU);
        const std::uint32_t sys = (region_code >= 5 && region_code <= 7)
                                      ? TESSERA_SYSTEM_GAME_GEAR
                                      : TESSERA_SYSTEM_MASTER_SYSTEM;
        d.candidates.push_back({sys, offset ? 2U : 1U, UINT64_C(16)});
        d.info.evidence_offset = offset + tmr_at;
        d.info.evidence_bytes = payload.size();
        d.info.evidence = 16;
        break;
      }
    }
  }
  d.info.candidate_count = static_cast<std::uint32_t>(d.candidates.size());
  if (d.candidates.size() > 1) {
    d.status = TESSERA_AMBIGUOUS_CONTENT;
    d.info.outcome = TESSERA_DETECTION_AMBIGUOUS;
    d.info.evidence_offset = d.info.evidence_bytes = 0;
    d.info.evidence = 0;
  } else if (d.candidates.size() == 1) {
    d.status = TESSERA_OK;
    d.info.outcome = TESSERA_DETECTION_RECOGNIZED;
    d.info.system = d.candidates[0].system;
  } else {
    d.status = malformed ? TESSERA_INVALID_CONTENT : TESSERA_UNKNOWN_CONTENT;
    d.info.outcome =
        malformed ? TESSERA_DETECTION_INVALID : TESSERA_DETECTION_UNKNOWN;
  }
  return d;
}
} // namespace tessera
