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
  const auto sp = big32(b, 0), pc = big32(b, 4);
  const auto start = big32(b, 0x1a0), end = big32(b, 0x1a4);
  return (sp & 1) == 0 && sp >= 0xff0000 && sp <= 0xffffff && (pc & 1) == 0 &&
         pc >= 0x200 && pc < b.size() && start == 0 && end >= pc &&
         std::uint64_t(end) < b.size();
}
} // namespace
Detection SystemDetector::detect(wire::View bytes,
                                 std::uint32_t override_system) {
  require(override_system <= TESSERA_SYSTEM_MEGA_DRIVE);
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
  d.info.candidate_count = static_cast<std::uint32_t>(d.candidates.size());
  if (d.candidates.size() > 1) {
    d.status = TESSERA_AMBIGUOUS_CONTENT;
    d.info.outcome = TESSERA_DETECTION_AMBIGUOUS;
    d.info.evidence_offset = d.info.evidence_bytes = 0;
    d.info.evidence = 0;
  } else if (d.candidates.size() == 1) {
    d.status = TESSERA_OK;
    d.info.outcome = TESSERA_DETECTION_RECOGNIZED;
    d.info.system = TESSERA_SYSTEM_MEGA_DRIVE;
  } else {
    d.status = malformed ? TESSERA_INVALID_CONTENT : TESSERA_UNKNOWN_CONTENT;
    d.info.outcome =
        malformed ? TESSERA_DETECTION_INVALID : TESSERA_DETECTION_UNKNOWN;
  }
  return d;
}
} // namespace tessera
