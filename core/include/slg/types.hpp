#pragma once

#include <cstdint>

namespace slg {

using Signal = std::uint8_t;        // invariant 0..15
using ComponentId = std::uint32_t;  // 1..0xFFFFFFFE, never reused
using Tick = std::uint64_t;

inline constexpr std::uint8_t kOut = 0xFF;  // index of the output pin

enum class Status : std::uint8_t {
  Ok,
  BadSpec,
  BadPin,
  UnknownComponent,
  DuplicateId,
  RetiredId,
  UnknownWire,
  DuplicateWire,
  StaleBatch,
  LimitExceeded,
  BadFrame,
  WrongPhase,
  FormatCorrupt,
  FormatTooNew
};

enum class Kind : std::uint8_t { Const = 1, Sensor, Actuator, Gate, Delay, Compare, Latch, Toggle, Clock };
enum class GateOp : std::uint8_t { And, Or, Xor };
enum class CompareMode : std::uint8_t { Compare, Subtract };

struct Limits {
  std::uint32_t maxComponents = 8192;
  std::uint32_t maxWires = 32768;
  std::uint32_t maxNetPins = 256;
  std::uint32_t maxDelayCells = 65536;
  std::uint16_t maxDelay = 255;
  std::uint16_t maxClockPeriod = 4096;
  std::uint8_t maxArity = 8;
};

struct Spec {
  Kind kind = Kind::Const;
  std::uint16_t p0 = 0, p1 = 0, p2 = 0;
  friend bool operator==(const Spec&, const Spec&) = default;
};

constexpr Spec makeConst(std::uint16_t v) noexcept { return Spec{Kind::Const, v, 0, 0}; }
constexpr Spec makeSensor() noexcept { return Spec{Kind::Sensor, 0, 0, 0}; }
constexpr Spec makeActuator() noexcept { return Spec{Kind::Actuator, 0, 0, 0}; }
constexpr Spec makeGate(GateOp op, bool invert, std::uint16_t arity) noexcept {
  return Spec{Kind::Gate, static_cast<std::uint16_t>(op), static_cast<std::uint16_t>(invert ? 1 : 0), arity};
}
constexpr Spec makeNot() noexcept { return makeGate(GateOp::Or, true, 1); }
constexpr Spec makeDelay(std::uint16_t length) noexcept { return Spec{Kind::Delay, length, 0, 0}; }
constexpr Spec makeCompare(CompareMode mode) noexcept {
  return Spec{Kind::Compare, static_cast<std::uint16_t>(mode), 0, 0};
}
constexpr Spec makeLatch() noexcept { return Spec{Kind::Latch, 0, 0, 0}; }
constexpr Spec makeToggle() noexcept { return Spec{Kind::Toggle, 0, 0, 0}; }
constexpr Spec makeClock(std::uint16_t period, std::uint16_t high) noexcept {
  return Spec{Kind::Clock, period, high, 0};
}

// State a component has when every input reads 0 (01 section 6.1, last column).
std::uint16_t restSeed(const Spec&) noexcept;

// BadSpec for a malformed spec or seed; LimitExceeded when arity, Delay length or Clock period exceeds its `Limits` cap.
Status validate(const Spec&, std::uint16_t seed, const Limits&) noexcept;

}  // namespace slg
