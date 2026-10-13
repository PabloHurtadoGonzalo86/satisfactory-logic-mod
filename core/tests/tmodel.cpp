#include "tmodel.hpp"

#include <algorithm>

namespace slgt {

const char* statusName(Status s) noexcept {
  static const char* const kNames[] = {"Ok", "BadSpec", "BadPin", "UnknownComponent", "DuplicateId", "RetiredId",
                                       "UnknownWire", "DuplicateWire", "StaleBatch", "LimitExceeded", "BadFrame",
                                       "WrongPhase", "FormatCorrupt", "FormatTooNew"};
  const auto i = static_cast<unsigned>(s);
  return i < sizeof(kNames) / sizeof(kNames[0]) ? kNames[i] : "?";
}

bool kindInRange(Kind k) noexcept {
  const auto v = static_cast<unsigned>(k);
  return v >= 1 && v <= 9;
}

const char* kindToken(Kind k) noexcept {
  switch (k) {
    case Kind::Const: return "const";
    case Kind::Sensor: return "sensor";
    case Kind::Actuator: return "actuator";
    case Kind::Gate: return "gate";
    case Kind::Delay: return "delay";
    case Kind::Compare: return "cmp";
    case Kind::Latch: return "latch";
    case Kind::Toggle: return "toggle";
    case Kind::Clock: return "clock";
  }
  return "?";
}

unsigned inputCount(const Spec& s) noexcept {
  switch (s.kind) {
    case Kind::Const: case Kind::Sensor: case Kind::Clock: return 0;
    case Kind::Actuator: case Kind::Delay: case Kind::Toggle: return 1;
    case Kind::Compare: case Kind::Latch: return 2;
    case Kind::Gate: return s.p2;
  }
  return 0;
}

bool hasOutput(const Spec& s) noexcept { return s.kind != Kind::Actuator; }

bool pinValid(const Spec& s, std::uint8_t index) noexcept {
  if (index == kOut) return hasOutput(s);
  return index < inputCount(s);
}

unsigned stateSize(const Spec& s) noexcept {
  switch (s.kind) {
    case Kind::Const: case Kind::Sensor: return 0;
    case Kind::Actuator: case Kind::Gate: case Kind::Compare: case Kind::Latch: return 1;
    case Kind::Toggle: case Kind::Clock: return 2;
    case Kind::Delay: return s.p0;
  }
  return 0;
}

std::uint16_t restSeed(const Spec& s) noexcept {
  if (s.kind == Kind::Gate && s.p1 != 0) return 15;
  return 0;
}

std::uint64_t stateSpaceSize(const Spec& s) noexcept {
  constexpr std::uint64_t kCap = std::uint64_t{1} << 62;
  switch (s.kind) {
    case Kind::Const: case Kind::Sensor: return 1;
    case Kind::Actuator: case Kind::Compare: return 16;
    case Kind::Gate: case Kind::Latch: return 2;
    case Kind::Toggle: return 4;
    case Kind::Clock: return s.p0;
    case Kind::Delay: {
      std::uint64_t r = 1;
      for (unsigned i = 0; i < s.p0; ++i) {
        r *= 16;
        if (r >= kCap) return kCap;
      }
      return r;
    }
  }
  return 1;
}

Status checkParams(const Spec& s, const Limits& lim) noexcept {
  if (!kindInRange(s.kind)) return Status::BadSpec;
  switch (s.kind) {
    case Kind::Const:
      return (s.p0 > 15 || s.p1 != 0 || s.p2 != 0) ? Status::BadSpec : Status::Ok;
    case Kind::Sensor: case Kind::Actuator: case Kind::Latch: case Kind::Toggle:
      return (s.p0 != 0 || s.p1 != 0 || s.p2 != 0) ? Status::BadSpec : Status::Ok;
    case Kind::Gate:
      if (s.p0 > 2 || s.p1 > 1 || s.p2 == 0) return Status::BadSpec;
      return s.p2 > lim.maxArity ? Status::LimitExceeded : Status::Ok;
    case Kind::Delay:
      if (s.p0 == 0 || s.p1 != 0 || s.p2 != 0) return Status::BadSpec;
      return s.p0 > lim.maxDelay ? Status::LimitExceeded : Status::Ok;
    case Kind::Compare:
      return (s.p0 > 1 || s.p1 != 0 || s.p2 != 0) ? Status::BadSpec : Status::Ok;
    case Kind::Clock:
      if (s.p0 < 2 || s.p2 != 0) return Status::BadSpec;
      if (s.p0 > lim.maxClockPeriod) return Status::LimitExceeded;
      return (s.p1 < 1 || s.p1 >= s.p0) ? Status::BadSpec : Status::Ok;
  }
  return Status::BadSpec;
}

Status checkSpec(const Spec& s, unsigned seed, const Limits& lim) noexcept {
  const Status params = checkParams(s, lim);
  if (params != Status::Ok) return params;
  bool inDomain = false;
  switch (s.kind) {
    case Kind::Const: case Kind::Sensor: inDomain = seed == 0; break;
    case Kind::Actuator: case Kind::Delay: case Kind::Compare: inDomain = seed <= 15; break;
    case Kind::Gate: case Kind::Latch: case Kind::Toggle: inDomain = seed == 0 || seed == 15; break;
    case Kind::Clock: inDomain = seed < s.p0; break;
  }
  return inDomain ? Status::Ok : Status::BadSpec;
}

const CompDef* Model::find(ComponentId id) const noexcept {
  std::size_t lo = 0;
  std::size_t hi = comps.size();
  while (lo < hi) {
    const std::size_t mid = lo + (hi - lo) / 2;
    if (comps[mid].id < id) lo = mid + 1; else hi = mid;
  }
  return (lo < comps.size() && comps[lo].id == id) ? &comps[lo] : nullptr;
}

void Model::sortAndNormalize() {
  std::stable_sort(comps.begin(), comps.end(),
                   [](const CompDef& x, const CompDef& y) { return x.id < y.id; });
  for (Wire& w : wires) {
    if (w.b < w.a) std::swap(w.a, w.b);
  }
  std::stable_sort(wires.begin(), wires.end());
  wires.erase(std::unique(wires.begin(), wires.end()), wires.end());
}

std::uint32_t Model::nextIdHint() const noexcept {
  return comps.empty() ? 1U : comps.back().id + 1U;
}

std::vector<EditOp> opsForModel(const Model& m, bool viaAdd) {
  std::vector<EditOp> ops;
  ops.reserve(m.comps.size() + m.wires.size());
  for (const CompDef& c : m.comps) ops.push_back(opAdd(c.id, c.spec, c.seed, viaAdd));
  for (const Wire& w : m.wires) ops.push_back(opConnect(w.a, w.b));
  return ops;
}

}  // namespace slgt
