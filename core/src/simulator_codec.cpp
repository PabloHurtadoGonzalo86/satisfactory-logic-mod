#include <cstddef>

#include "internal/bytes.hpp"
#include "internal/fnv.hpp"
#include "internal/nets.hpp"
#include "internal/spec_rules.hpp"
#include "slg/simulator.hpp"

namespace slg {
namespace {

constexpr std::uint16_t kFormatVersion = 1;
constexpr std::uint16_t kSemanticsVersion = 1;
constexpr std::size_t kHeaderBytes = 32;
constexpr std::size_t kTailBytes = 8;
constexpr std::uint64_t kComponentRecordBytes = 11;
constexpr std::uint64_t kWireRecordBytes = 10;
constexpr std::uint8_t kMagic[4] = {0x53, 0x4C, 0x47, 0x43};  // "SLGC"

}  // namespace

std::vector<std::uint8_t> Simulator::encode() const {
  const auto comps = netlist_.components();
  const auto wires = netlist_.wires();

  std::vector<std::uint8_t> bytes;
  bytes.reserve(kHeaderBytes + kTailBytes + comps.size() * kComponentRecordBytes + state_.size() +
                wires.size() * kWireRecordBytes);
  internal::ByteWriter out(bytes);
  for (const std::uint8_t m : kMagic) out.u8(m);
  out.u16(kFormatVersion);
  out.u16(kSemanticsVersion);
  out.u32(hostStepMicros_);
  out.u64(tick_);
  out.u32(netlist_.nextId());
  out.u32(static_cast<std::uint32_t>(comps.size()));
  out.u32(static_cast<std::uint32_t>(wires.size()));

  for (std::size_t i = 0; i < comps.size(); ++i) {
    const Spec& spec = comps[i].second;
    const detail::Slot& slot = slots_[i];
    out.u32(comps[i].first);
    out.u8(static_cast<std::uint8_t>(spec.kind));
    out.u16(spec.p0);
    out.u16(spec.p1);
    out.u16(spec.p2);
    for (std::uint32_t k = 0; k < slot.stateSize; ++k) out.u8(state_[slot.stateOffset + k]);
  }
  for (const Wire& wire : wires) {
    out.u32(wire.a.component);
    out.u8(wire.a.index);
    out.u32(wire.b.component);
    out.u8(wire.b.index);
  }

  internal::Fnv64 tail;
  for (const std::uint8_t b : bytes) tail.byte(b);
  out.u64(tail.value());
  return bytes;
}

Status Simulator::decode(std::span<const std::uint8_t> bytes, const Limits& limits, Simulator& out) {
  using internal::ByteReader;

  if (bytes.size() < 4) return Status::FormatCorrupt;
  for (std::size_t i = 0; i < 4; ++i) {
    if (bytes[i] != kMagic[i]) return Status::FormatCorrupt;
  }
  if (bytes.size() < 8) return Status::FormatCorrupt;
  {
    ByteReader versions(bytes.subspan(4));
    std::uint16_t formatVersion = 0;
    std::uint16_t semanticsVersion = 0;
    versions.u16(formatVersion);
    versions.u16(semanticsVersion);
    if (formatVersion > kFormatVersion || semanticsVersion > kSemanticsVersion) return Status::FormatTooNew;
    if (formatVersion != kFormatVersion || semanticsVersion != kSemanticsVersion) return Status::FormatCorrupt;
  }
  if (bytes.size() < kHeaderBytes + kTailBytes) return Status::FormatCorrupt;

  const auto body = bytes.first(bytes.size() - kTailBytes);
  {
    internal::Fnv64 check;
    for (const std::uint8_t b : body) check.byte(b);
    std::uint64_t stored = 0;
    ByteReader tail(bytes.last(kTailBytes));
    tail.u64(stored);
    if (stored != check.value()) return Status::FormatCorrupt;
  }

  ByteReader r(body.subspan(8));
  std::uint32_t hostStepMicros = 0;
  std::uint64_t tick = 0;
  std::uint32_t nextId = 0;
  std::uint32_t componentCount = 0;
  std::uint32_t wireCount = 0;
  if (!r.u32(hostStepMicros) || !r.u64(tick) || !r.u32(nextId) || !r.u32(componentCount) || !r.u32(wireCount)) {
    return Status::FormatCorrupt;
  }
  if (componentCount > limits.maxComponents || wireCount > limits.maxWires) return Status::LimitExceeded;
  const std::uint64_t minimumBytes = componentCount * kComponentRecordBytes + wireCount * kWireRecordBytes;
  if (minimumBytes > r.remaining()) return Status::FormatCorrupt;

  std::vector<std::pair<ComponentId, Spec>> comps;
  comps.reserve(componentCount);
  std::vector<std::uint8_t> state;
  std::uint64_t delayCells = 0;
  ComponentId previousId = 0;
  for (std::uint32_t i = 0; i < componentCount; ++i) {
    std::uint32_t id = 0;
    std::uint8_t kind = 0;
    std::uint16_t p0 = 0;
    std::uint16_t p1 = 0;
    std::uint16_t p2 = 0;
    if (!r.u32(id) || !r.u8(kind) || !r.u16(p0) || !r.u16(p1) || !r.u16(p2)) return Status::FormatCorrupt;
    if (id == internal::kIdSentinel || id <= previousId) return Status::FormatCorrupt;
    if (!internal::validKindByte(kind)) return Status::FormatCorrupt;

    const Spec spec{static_cast<Kind>(kind), p0, p1, p2};
    const Status params = internal::validateParams(spec, limits);
    if (params == Status::LimitExceeded) return Status::LimitExceeded;
    if (params != Status::Ok) return Status::FormatCorrupt;

    std::span<const std::uint8_t> stateBytes;
    if (!r.take(internal::stateSize(spec), stateBytes)) return Status::FormatCorrupt;
    if (!internal::stateInDomain(spec, stateBytes)) return Status::FormatCorrupt;
    if (spec.kind == Kind::Delay) {
      delayCells += spec.p0;
      if (delayCells > limits.maxDelayCells) return Status::LimitExceeded;
    }
    state.insert(state.end(), stateBytes.begin(), stateBytes.end());
    comps.emplace_back(id, spec);
    previousId = id;
  }
  if (nextId == 0 || nextId <= previousId) return Status::FormatCorrupt;

  std::vector<Wire> wires;
  wires.reserve(wireCount);
  for (std::uint32_t i = 0; i < wireCount; ++i) {
    std::uint32_t idA = 0;
    std::uint8_t indexA = 0;
    std::uint32_t idB = 0;
    std::uint8_t indexB = 0;
    if (!r.u32(idA) || !r.u8(indexA) || !r.u32(idB) || !r.u8(indexB)) return Status::FormatCorrupt;
    const Pin a{idA, indexA};
    const Pin b{idB, indexB};
    if (!(a < b)) return Status::FormatCorrupt;
    if (!wires.empty()) {
      const Wire& last = wires.back();
      if (!(last.a < a || (last.a == a && last.b < b))) return Status::FormatCorrupt;
    }
    const std::size_t slotA = internal::findSlot(comps, idA);
    const std::size_t slotB = internal::findSlot(comps, idB);
    if (slotA == internal::kNoSlot || slotB == internal::kNoSlot) return Status::FormatCorrupt;
    if (!internal::pinExists(comps[slotA].second, indexA) || !internal::pinExists(comps[slotB].second, indexB)) {
      return Status::FormatCorrupt;
    }
    wires.push_back(Wire{a, b});
  }
  if (r.remaining() != 0) return Status::FormatCorrupt;

  if (!wires.empty()) {
    const internal::PinTable table = internal::buildPinTable(comps);
    const internal::Nets nets = internal::buildNets(table, comps, wires);
    for (const std::uint32_t pins : nets.pinsInNet) {
      if (pins > limits.maxNetPins) return Status::LimitExceeded;
    }
  }

  Simulator candidate(limits, hostStepMicros);
  candidate.netlist_.components_ = std::move(comps);
  candidate.netlist_.wires_ = std::move(wires);
  candidate.netlist_.nextId_ = nextId;
  candidate.netlist_.finalize();
  candidate.tick_ = tick;
  candidate.state_ = std::move(state);
  candidate.rebuildPlan();
  candidate.refreshCommands();
  out = std::move(candidate);
  return Status::Ok;
}

}  // namespace slg
