#include "spec_rules.hpp"

namespace slg::internal {
namespace {

bool isNibbleMask(std::uint8_t v) noexcept { return v == 0 || v == 15; }

}  // namespace

bool validKindByte(std::uint8_t raw) noexcept {
  return raw >= static_cast<std::uint8_t>(Kind::Const) && raw <= static_cast<std::uint8_t>(Kind::Clock);
}

std::uint8_t inputCount(const Spec& spec) noexcept {
  switch (spec.kind) {
    case Kind::Const:
    case Kind::Sensor:
    case Kind::Clock:
      return 0;
    case Kind::Actuator:
    case Kind::Delay:
    case Kind::Toggle:
      return 1;
    case Kind::Compare:
    case Kind::Latch:
      return 2;
    case Kind::Gate:
      return static_cast<std::uint8_t>(spec.p2);
  }
  return 0;
}

bool hasOutput(Kind kind) noexcept { return kind != Kind::Actuator; }

bool pinExists(const Spec& spec, std::uint8_t index) noexcept {
  if (index == kOut) return hasOutput(spec.kind);
  return index < inputCount(spec);
}

Status validateParams(const Spec& spec, const Limits& limits) noexcept {
  switch (spec.kind) {
    case Kind::Const:
      if (spec.p1 != 0 || spec.p2 != 0 || spec.p0 > 15) return Status::BadSpec;
      return Status::Ok;
    case Kind::Sensor:
    case Kind::Actuator:
    case Kind::Latch:
    case Kind::Toggle:
      return (spec.p0 == 0 && spec.p1 == 0 && spec.p2 == 0) ? Status::Ok : Status::BadSpec;
    case Kind::Gate:
      if (spec.p0 > static_cast<std::uint16_t>(GateOp::Xor) || spec.p1 > 1 || spec.p2 == 0) return Status::BadSpec;
      return spec.p2 > limits.maxArity ? Status::LimitExceeded : Status::Ok;
    case Kind::Delay:
      if (spec.p1 != 0 || spec.p2 != 0 || spec.p0 == 0) return Status::BadSpec;
      return spec.p0 > limits.maxDelay ? Status::LimitExceeded : Status::Ok;
    case Kind::Compare:
      if (spec.p1 != 0 || spec.p2 != 0 || spec.p0 > static_cast<std::uint16_t>(CompareMode::Subtract)) {
        return Status::BadSpec;
      }
      return Status::Ok;
    case Kind::Clock:
      // 01 section 7: over its cap the period is LimitExceeded even when the high width is invalid as well.
      if (spec.p0 < 2 || spec.p2 != 0) return Status::BadSpec;
      if (spec.p0 > limits.maxClockPeriod) return Status::LimitExceeded;
      return (spec.p1 < 1 || spec.p1 >= spec.p0) ? Status::BadSpec : Status::Ok;
  }
  return Status::BadSpec;
}

bool seedInDomain(const Spec& spec, std::uint16_t seed) noexcept {
  switch (spec.kind) {
    case Kind::Const:
    case Kind::Sensor:
      return seed == 0;
    case Kind::Gate:
    case Kind::Latch:
    case Kind::Toggle:
      return seed == 0 || seed == 15;
    case Kind::Delay:
    case Kind::Compare:
    case Kind::Actuator:
      return seed <= 15;
    case Kind::Clock:
      return seed < spec.p0;
  }
  return false;
}

std::uint32_t stateSize(const Spec& spec) noexcept {
  switch (spec.kind) {
    case Kind::Const:
    case Kind::Sensor:
      return 0;
    case Kind::Actuator:
    case Kind::Gate:
    case Kind::Compare:
    case Kind::Latch:
      return 1;
    case Kind::Toggle:
    case Kind::Clock:
      return 2;
    case Kind::Delay:
      return spec.p0;
  }
  return 0;
}

void writeInitialState(const Spec& spec, std::uint16_t seed, std::span<std::uint8_t> dst) noexcept {
  const auto value = static_cast<std::uint8_t>(seed & 0xFFu);
  switch (spec.kind) {
    case Kind::Const:
    case Kind::Sensor:
      break;
    case Kind::Actuator:
    case Kind::Gate:
    case Kind::Compare:
    case Kind::Latch:
      dst[0] = value;
      break;
    case Kind::Toggle:
      dst[0] = value;
      dst[1] = 1;  // the first tick must not see an edge (01 section 6.2)
      break;
    case Kind::Clock:
      dst[0] = static_cast<std::uint8_t>(seed & 0xFFu);
      dst[1] = static_cast<std::uint8_t>((seed >> 8) & 0xFFu);
      break;
    case Kind::Delay:
      for (std::uint8_t& cell : dst) cell = value;
      break;
  }
}

bool stateInDomain(const Spec& spec, std::span<const std::uint8_t> state) noexcept {
  switch (spec.kind) {
    case Kind::Const:
    case Kind::Sensor:
      return true;
    case Kind::Actuator:
    case Kind::Compare:
      return state[0] <= 15;
    case Kind::Gate:
    case Kind::Latch:
      return isNibbleMask(state[0]);
    case Kind::Toggle:
      return isNibbleMask(state[0]) && state[1] <= 1;
    case Kind::Clock: {
      const unsigned phase = static_cast<unsigned>(state[0]) | (static_cast<unsigned>(state[1]) << 8);
      return phase < spec.p0;
    }
    case Kind::Delay:
      for (const std::uint8_t cell : state) {
        if (cell > 15) return false;
      }
      return true;
  }
  return false;
}

}  // namespace slg::internal
