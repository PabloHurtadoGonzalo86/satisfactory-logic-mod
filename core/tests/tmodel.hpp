// Test-side mirror of the public types in 02 section 2, plus the netlist model the test generators
// and the golden files share. Deliberately includes nothing from slg/: the whole spec-only layer
// (reference interpreter, blob codec, generators, golden I/O) builds and runs without the core.
// Numeric codes follow 02 section 2 and 6.1 and must never be renumbered.
#pragma once

#include <compare>
#include <cstdint>
#include <string>
#include <vector>

namespace slgt {

using Signal = std::uint8_t;
using ComponentId = std::uint32_t;
using Tick = std::uint64_t;

inline constexpr std::uint8_t kOut = 0xFF;
inline constexpr Signal kMaxSignal = 15;

constexpr std::uint8_t u8(std::uint64_t v) noexcept { return static_cast<std::uint8_t>(v); }
constexpr std::uint16_t u16(std::uint64_t v) noexcept { return static_cast<std::uint16_t>(v); }
constexpr std::uint32_t u32(std::uint64_t v) noexcept { return static_cast<std::uint32_t>(v); }

enum class Status : std::uint8_t {
  Ok, BadSpec, BadPin, UnknownComponent, DuplicateId, RetiredId, UnknownWire, DuplicateWire,
  StaleBatch, LimitExceeded, BadFrame, WrongPhase, FormatCorrupt, FormatTooNew
};
const char* statusName(Status s) noexcept;

enum class Kind : std::uint8_t { Const = 1, Sensor, Actuator, Gate, Delay, Compare, Latch, Toggle, Clock };
enum class GateOp : std::uint8_t { And, Or, Xor };
enum class CompareMode : std::uint8_t { Compare, Subtract };

struct Limits {
  std::uint32_t maxComponents = 8192, maxWires = 32768, maxNetPins = 256, maxDelayCells = 65536;
  std::uint16_t maxDelay = 255, maxClockPeriod = 4096;
  std::uint8_t maxArity = 8;
};

struct Spec {
  Kind kind = Kind::Const;
  std::uint16_t p0 = 0, p1 = 0, p2 = 0;
  friend bool operator==(const Spec&, const Spec&) = default;
};

struct Pin {
  ComponentId component = 0;
  std::uint8_t index = 0;
  friend auto operator<=>(const Pin&, const Pin&) = default;
};

struct Wire {
  Pin a, b;
  friend auto operator<=>(const Wire&, const Wire&) = default;
};

inline Wire makeWire(Pin x, Pin y) { return (y < x) ? Wire{y, x} : Wire{x, y}; }
inline Pin outPin(ComponentId c) { return Pin{c, kOut}; }
inline Pin inPin(ComponentId c, unsigned index) { return Pin{c, u8(index)}; }

// Spec factories (values per 02 section 6.1). They mirror, and are cross-checked against, the
// public makeXxx factories in the T11 spec-shape test.
inline Spec specConst(unsigned v) { return Spec{Kind::Const, u16(v), 0, 0}; }
inline Spec specSensor() { return Spec{Kind::Sensor, 0, 0, 0}; }
inline Spec specActuator() { return Spec{Kind::Actuator, 0, 0, 0}; }
inline Spec specGate(GateOp op, bool inv, unsigned arity) {
  return Spec{Kind::Gate, u16(static_cast<unsigned>(op)), u16(inv ? 1U : 0U), u16(arity)};
}
inline Spec specNot() { return specGate(GateOp::Or, true, 1); }
inline Spec specDelay(unsigned d) { return Spec{Kind::Delay, u16(d), 0, 0}; }
inline Spec specCompare(CompareMode m) { return Spec{Kind::Compare, u16(static_cast<unsigned>(m)), 0, 0}; }
inline Spec specLatch() { return Spec{Kind::Latch, 0, 0, 0}; }
inline Spec specToggle() { return Spec{Kind::Toggle, 0, 0, 0}; }
inline Spec specClock(unsigned period, unsigned high) { return Spec{Kind::Clock, u16(period), u16(high), 0}; }

bool kindInRange(Kind k) noexcept;
const char* kindToken(Kind k) noexcept;          // lnet token: const sensor actuator gate delay cmp latch toggle clock
unsigned inputCount(const Spec& s) noexcept;     // meaningful for valid specs
bool hasOutput(const Spec& s) noexcept;          // everything but the actuator
bool pinValid(const Spec& s, std::uint8_t index) noexcept;
unsigned stateSize(const Spec& s) noexcept;      // bytes of state in the blob (02 section 6.1)
std::uint16_t restSeed(const Spec& s) noexcept;  // 01 section 6.1, last column
std::uint64_t stateSpaceSize(const Spec& s) noexcept;   // |S_c|, saturating at 2^62

// The status the spec gives a parameter set, and a parameter set with its seed. BadSpec: a malformed parameter (01 section 3,
// p0..p2 unused = 0) or a seed outside its domain (01 section 6.1, B1); LimitExceeded: arity, Delay length or Clock period
// above its Limits cap, whatever the catalog range says (01 section 7, B2). Order of the checks: malformed parameters, the
// caps (the Clock period before its high width), then the seed.
Status checkParams(const Spec& s, const Limits& lim) noexcept;
Status checkSpec(const Spec& s, unsigned seed, const Limits& lim) noexcept;

struct CompDef {
  ComponentId id = 0;
  Spec spec;
  std::uint16_t seed = 0;
  friend bool operator==(const CompDef&, const CompDef&) = default;
};

// A netlist with seeds: exactly the content of an lnet file.
struct Model {
  std::vector<CompDef> comps;   // ascending id
  std::vector<Wire> wires;      // normalized, ascending, unique

  const CompDef* find(ComponentId id) const noexcept;
  void sortAndNormalize();
  std::uint32_t nextIdHint() const noexcept;   // max id + 1 (1 when empty)
  friend bool operator==(const Model&, const Model&) = default;
};

enum class EditKind : std::uint8_t { Add, Remove, Replace, Connect, Disconnect };

// One operation of an EditBatch. Add always carries its id; viaAdd selects EditBatch::add (the core
// assigns the id) over addWithId, and the test checks that the core hands out exactly op.id.
struct EditOp {
  EditKind kind = EditKind::Add;
  ComponentId id = 0;
  Spec spec;
  std::uint16_t seed = 0;
  Pin a, b;
  bool viaAdd = false;
  friend bool operator==(const EditOp&, const EditOp&) = default;
};
inline EditOp opAdd(ComponentId id, Spec s, unsigned seed, bool viaAdd = false) {
  EditOp o; o.kind = EditKind::Add; o.id = id; o.spec = s; o.seed = u16(seed); o.viaAdd = viaAdd; return o;
}
inline EditOp opRemove(ComponentId id) { EditOp o; o.kind = EditKind::Remove; o.id = id; return o; }
inline EditOp opReplace(ComponentId id, Spec s, unsigned seed) {
  EditOp o; o.kind = EditKind::Replace; o.id = id; o.spec = s; o.seed = u16(seed); return o;
}
inline EditOp opConnect(Pin a, Pin b) { EditOp o; o.kind = EditKind::Connect; o.a = a; o.b = b; return o; }
inline EditOp opDisconnect(Pin a, Pin b) { EditOp o; o.kind = EditKind::Disconnect; o.a = a; o.b = b; return o; }

// The ops that build a model from empty (adds ascending, then wires).
std::vector<EditOp> opsForModel(const Model& m, bool viaAdd = false);

struct SensorOrder {
  Tick tau = 0;
  ComponentId id = 0;
  Signal value = 0;
  friend bool operator==(const SensorOrder&, const SensorOrder&) = default;
};
struct TimedEdit {
  Tick tau = 0;
  EditOp op;
  friend bool operator==(const TimedEdit&, const TimedEdit&) = default;
};

// Content of an lscn file (plus its lnet).
struct Scenario {
  Model net;
  std::uint32_t ticks = 0;
  std::vector<SensorOrder> inputs;   // ordered by tau, then file order
  std::vector<TimedEdit> edits;      // ordered by tau, then file order (a batch is the run of one tau)
};

}  // namespace slgt
