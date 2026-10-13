// slg_selfcheck: prints one digest that must be identical on every compiler, optimisation level and
// platform (03 section 9). The owner runs it with MSVC and compares it with the CI digest.
//
// Exit status 0 only when every internal check passed; the digest is printed either way.
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "slg/lint.hpp"
#include "slg/netlist.hpp"
#include "slg/simulator.hpp"
#include "slg/types.hpp"

namespace {

using slg::ComponentId;
using slg::EditBatch;
using slg::Kind;
using slg::Limits;
using slg::Pin;
using slg::Signal;
using slg::Simulator;
using slg::Spec;
using slg::Status;

constexpr std::uint64_t kFnvBasis = 0xcbf29ce484222325ULL;
constexpr std::uint64_t kFnvPrime = 0x100000001b3ULL;
constexpr std::uint32_t kDefaultScenarios = 500;
constexpr std::uint32_t kTicksPerScenario = 24;
constexpr std::uint32_t kEditTick = 8;
constexpr std::uint32_t kRoundTripTick = 16;
constexpr std::uint64_t kScenarioBase = 2026101399ULL;

// The 95-byte G7 blob of 02 section 6.4 (Sensor 1 -> NOT 2 -> Actuator 3, hostStepMicros 100000, tick 0).
constexpr std::uint8_t kG7Blob[95] = {
    0x53, 0x4C, 0x47, 0x43, 0x01, 0x00, 0x01, 0x00, 0xA0, 0x86, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x04,
    0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x0F, 0x03, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0xFF, 0x02, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00,
    0x00, 0xFF, 0x03, 0x00, 0x00, 0x00, 0x00, 0x4F, 0x95, 0xAE, 0x13, 0xE3, 0x4E, 0x14, 0x3D};
constexpr std::uint64_t kG7Tail = 0x3D144EE313AE954FULL;
constexpr std::uint64_t kG7Structure = 0x84C3FB7A3ACB833CULL;
constexpr std::uint64_t kG1Hashes[5] = {0x9D16793581913B31ULL, 0x9D16723581912F4CULL, 0x9D16723581912F4CULL,
                                        0x4622C4783DE15BB9ULL, 0x4622CD783DE16B04ULL};
constexpr Signal kG1Samples[4] = {0, 0, 15, 15};  // e at tau 0..3; the table's row 4 needs no sample

class SplitMix64 {
 public:
  explicit SplitMix64(std::uint64_t seed) noexcept : state_(seed) {}
  std::uint64_t next() noexcept {
    state_ += 0x9E3779B97F4A7C15ULL;
    std::uint64_t z = state_;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
  }
  // One draw per call: argument evaluation order differs between compilers, so never nest two draws.
  std::uint32_t below(std::uint32_t bound) noexcept { return static_cast<std::uint32_t>(next() % bound); }

 private:
  std::uint64_t state_;
};

class Fold {
 public:
  void u8(std::uint8_t v) noexcept {
    hash_ ^= v;
    hash_ *= kFnvPrime;
  }
  void u16(std::uint16_t v) noexcept {
    u8(static_cast<std::uint8_t>(v & 0xFFU));
    u8(static_cast<std::uint8_t>(v >> 8));
  }
  void u32(std::uint32_t v) noexcept {
    u16(static_cast<std::uint16_t>(v & 0xFFFFU));
    u16(static_cast<std::uint16_t>(v >> 16));
  }
  void u64(std::uint64_t v) noexcept {
    u32(static_cast<std::uint32_t>(v & 0xFFFFFFFFULL));
    u32(static_cast<std::uint32_t>(v >> 32));
  }
  void bytes(std::span<const std::uint8_t> data) noexcept {
    for (const std::uint8_t b : data) u8(b);
  }
  void status(Status s) noexcept { u8(static_cast<std::uint8_t>(s)); }
  std::uint64_t value() const noexcept { return hash_; }

 private:
  std::uint64_t hash_ = kFnvBasis;
};

std::uint64_t fnv1a(std::span<const std::uint8_t> data) noexcept {
  Fold fold;
  fold.bytes(data);
  return fold.value();
}

std::string hex16(std::uint64_t v) {
  static constexpr char kDigits[] = "0123456789ABCDEF";
  std::string out(16, '0');
  for (std::size_t i = 0; i < 16; ++i) out[15 - i] = kDigits[(v >> (4 * i)) & 0xFU];
  return out;
}

struct Report {
  std::uint32_t failures = 0;
  void expect(bool ok, const char* what, std::uint32_t scenario = 0xFFFFFFFFU) {
    if (ok) return;
    ++failures;
    if (scenario == 0xFFFFFFFFU) {
      std::printf("FAIL %s\n", what);
    } else {
      std::printf("FAIL scenario %u: %s\n", static_cast<unsigned>(scenario), what);
    }
  }
};

unsigned inputCount(const Spec& spec) {
  switch (spec.kind) {
    case Kind::Actuator:
    case Kind::Delay:
    case Kind::Toggle:
      return 1;
    case Kind::Compare:
    case Kind::Latch:
      return 2;
    case Kind::Gate:
      return spec.p2;
    case Kind::Const:
    case Kind::Sensor:
    case Kind::Clock:
      return 0;
  }
  return 0;
}

Spec randomSpec(SplitMix64& rng) {
  const std::uint32_t pick = rng.below(18);
  if (pick < 1) {
    return slg::makeConst(static_cast<std::uint16_t>(rng.below(16)));
  }
  if (pick < 4) return slg::makeSensor();
  if (pick < 6) return slg::makeActuator();
  if (pick < 11) {
    const std::uint32_t op = rng.below(3);
    const std::uint32_t inv = rng.below(2);
    const std::uint32_t arity = 1 + rng.below(4);
    return Spec{Kind::Gate, static_cast<std::uint16_t>(op), static_cast<std::uint16_t>(inv),
                static_cast<std::uint16_t>(arity)};
  }
  if (pick < 13) return Spec{Kind::Delay, static_cast<std::uint16_t>(1 + rng.below(5)), 0, 0};
  if (pick < 15) return Spec{Kind::Compare, static_cast<std::uint16_t>(rng.below(2)), 0, 0};
  if (pick < 16) return slg::makeLatch();
  if (pick < 17) return slg::makeToggle();
  const std::uint32_t period = 2 + rng.below(9);
  const std::uint32_t high = 1 + rng.below(period - 1);
  return Spec{Kind::Clock, static_cast<std::uint16_t>(period), static_cast<std::uint16_t>(high), 0};
}

std::uint16_t randomSeed(SplitMix64& rng, const Spec& spec) {
  const std::uint32_t coin = rng.below(2);
  if (coin == 0) return slg::restSeed(spec);
  switch (spec.kind) {
    case Kind::Const:
    case Kind::Sensor:
      return 0;
    case Kind::Gate:
    case Kind::Latch:
    case Kind::Toggle:
      return static_cast<std::uint16_t>(rng.below(2) * 15);
    case Kind::Delay:
    case Kind::Compare:
    case Kind::Actuator:
      return static_cast<std::uint16_t>(rng.below(16));
    case Kind::Clock:
      return static_cast<std::uint16_t>(rng.below(spec.p0));
  }
  return 0;
}

struct Live {
  ComponentId id;
  Spec spec;
};

std::vector<Live> liveComponents(const Simulator& sim) {
  std::vector<Live> out;
  for (const auto& entry : sim.netlist().components()) out.push_back(Live{entry.first, entry.second});
  return out;
}

std::vector<ComponentId> drivers(const std::vector<Live>& live) {
  std::vector<ComponentId> out;
  for (const Live& c : live) {
    if (c.spec.kind != Kind::Actuator) out.push_back(c.id);
  }
  return out;
}

// Carries the held readings across an edit: a sensor that survives keeps its value, a new one reads 0.
std::vector<Signal> carryReadings(std::span<const ComponentId> oldIds, const std::vector<Signal>& oldValues,
                                  std::span<const ComponentId> newIds) {
  std::vector<Signal> out(newIds.size(), 0);
  for (std::size_t i = 0; i < newIds.size(); ++i) {
    for (std::size_t j = 0; j < oldIds.size(); ++j) {
      if (oldIds[j] == newIds[i]) out[i] = oldValues[j];
    }
  }
  return out;
}

void foldTick(Fold& fold, const Simulator& sim) {
  fold.u64(sim.tick());
  fold.u64(sim.stateHash());
  for (const Signal c : sim.commands()) fold.u8(c);
  fold.u32(sim.clampedSamples());
}

struct Snapshot {
  slg::Tick tick;
  std::uint64_t stateHash;
  std::uint64_t structureHash;
  ComponentId nextId;
  std::uint32_t clamped;
  bool operator==(const Snapshot&) const = default;
};

Snapshot snapshot(const Simulator& sim) {
  return Snapshot{sim.tick(), sim.stateHash(), sim.netlist().structureHash(), sim.netlist().nextId(),
                  sim.clampedSamples()};
}

// Applies a batch that must be rejected and checks I3: nothing observable changed.
void applyRejected(Simulator& sim, const EditBatch& batch, Fold& fold, Report& report, std::uint32_t scenario) {
  const Snapshot before = snapshot(sim);
  const Status status = sim.apply(batch);
  fold.status(status);
  report.expect(status != Status::Ok, "an invalid batch was accepted", scenario);
  report.expect(snapshot(sim) == before, "a rejected batch changed the simulator (I3)", scenario);
}

void editSimulator(Simulator& sim, SplitMix64& rng, Fold& fold, Report& report, std::uint32_t scenario) {
  const std::vector<Live> live = liveComponents(sim);
  const std::vector<ComponentId> sources = drivers(live);
  const std::uint32_t choice = rng.below(6);
  EditBatch batch = sim.newBatch();
  bool expectOk = true;
  switch (choice) {
    case 0: {
      const std::uint32_t at = rng.below(static_cast<std::uint32_t>(live.size()));
      batch.remove(live[at].id);
      break;
    }
    case 1: {
      const Spec spec = randomSpec(rng);
      const std::uint16_t seed = randomSeed(rng, spec);
      const ComponentId fresh = batch.add(spec, seed);
      if (inputCount(spec) > 0 && !sources.empty()) {
        const std::uint32_t at = rng.below(static_cast<std::uint32_t>(sources.size()));
        batch.connect(Pin{fresh, 0}, Pin{sources[at], slg::kOut});
      }
      break;
    }
    case 2: {
      const std::uint32_t at = rng.below(static_cast<std::uint32_t>(live.size()));
      const Live& target = live[at];
      Spec spec = target.spec;
      if (spec.kind == Kind::Delay) spec.p0 = static_cast<std::uint16_t>(1 + rng.below(5));
      if (spec.kind == Kind::Const) spec.p0 = static_cast<std::uint16_t>(rng.below(16));
      if (spec.kind == Kind::Compare) spec.p0 = static_cast<std::uint16_t>(1 - spec.p0);
      if (spec.kind == Kind::Gate) spec.p0 = static_cast<std::uint16_t>((spec.p0 + 1) % 3);
      const std::uint16_t seed = randomSeed(rng, spec);
      batch.replace(target.id, spec, seed);
      break;
    }
    case 3: {
      const auto wires = sim.netlist().wires();
      if (!wires.empty()) {
        const std::uint32_t at = rng.below(static_cast<std::uint32_t>(wires.size()));
        batch.disconnect(wires[at].a, wires[at].b);
      }
      break;
    }
    case 4: {
      batch.connect(Pin{0x00FFFFFFU, 0}, Pin{live.front().id, slg::kOut});
      expectOk = false;
      break;
    }
    default: {
      batch.add(Spec{Kind::Gate, 9, 0, 1}, 0);
      expectOk = false;
      break;
    }
  }
  if (!expectOk) {
    applyRejected(sim, batch, fold, report, scenario);
    return;
  }
  const Status status = sim.apply(batch);
  fold.status(status);
  report.expect(status == Status::Ok, "a valid edit batch was rejected", scenario);
}

void foldLint(Fold& fold, const slg::Netlist& netlist) {
  const slg::FeedbackReport lint = slg::analyze(netlist);
  fold.u32(static_cast<std::uint32_t>(lint.groups.size()));
  for (const auto& group : lint.groups) {
    fold.u32(static_cast<std::uint32_t>(group.size()));
    for (const ComponentId id : group) fold.u32(id);
  }
  fold.u32(static_cast<std::uint32_t>(lint.undrivenInputs.size()));
  for (const Pin& pin : lint.undrivenInputs) {
    fold.u32(pin.component);
    fold.u8(pin.index);
  }
  fold.u32(static_cast<std::uint32_t>(lint.multiDriverNets.size()));
  for (const auto& net : lint.multiDriverNets) {
    fold.u32(static_cast<std::uint32_t>(net.size()));
    for (const Pin& pin : net) {
      fold.u32(pin.component);
      fold.u8(pin.index);
    }
  }
  for (const std::optional<std::uint32_t>& bound : slg::settlingBounds(netlist)) {
    fold.u8(bound.has_value() ? 1 : 0);
    fold.u32(bound.value_or(0));
  }
}

void roundTrip(Simulator& sim, Simulator& twin, SplitMix64& rng, const Limits& limits, Fold& fold, Report& report,
               std::uint32_t scenario) {
  const std::vector<std::uint8_t> blob = sim.encode();
  fold.u64(fnv1a(blob));
  const Status decoded = Simulator::decode(blob, limits, twin);
  fold.status(decoded);
  report.expect(decoded == Status::Ok, "decode(encode(S)) failed", scenario);
  report.expect(twin.stateHash() == sim.stateHash(), "decode changed the state hash", scenario);
  report.expect(twin.encode() == blob, "encode(decode(b)) != b (I6)", scenario);

  std::vector<std::uint8_t> broken = blob;
  const std::uint32_t at = rng.below(static_cast<std::uint32_t>(broken.size()));
  const std::uint32_t delta = 1 + rng.below(255);
  broken[at] = static_cast<std::uint8_t>(broken[at] ^ delta);
  Simulator scratch(limits, 0);
  const Status rejected = Simulator::decode(broken, limits, scratch);
  fold.status(rejected);
  report.expect(rejected != Status::Ok, "a blob with one substituted byte was accepted (T12)", scenario);
}

void runScenario(std::uint32_t index, Fold& total, Report& report) {
  SplitMix64 rng(kScenarioBase ^ index);
  const Limits limits;
  Simulator sim(limits, 100000);
  Simulator twin(limits, 0);
  Fold fold;

  EditBatch batch = sim.newBatch();
  struct Planned {
    ComponentId id;
    Spec spec;
  };
  std::vector<Planned> planned;
  const std::uint32_t count = 2 + rng.below(13);
  for (std::uint32_t i = 0; i < count; ++i) {
    const Spec spec = randomSpec(rng);
    const std::uint16_t seed = randomSeed(rng, spec);
    const ComponentId id = batch.add(spec, seed);
    planned.push_back(Planned{id, spec});
  }
  std::vector<ComponentId> sources;
  for (const Planned& p : planned) {
    if (p.spec.kind != Kind::Actuator) sources.push_back(p.id);
  }
  for (const Planned& p : planned) {
    const unsigned inputs = inputCount(p.spec);
    for (unsigned pin = 0; pin < inputs && !sources.empty(); ++pin) {
      const std::uint32_t wired = rng.below(100);
      if (wired >= 85) continue;
      const std::uint32_t first = rng.below(static_cast<std::uint32_t>(sources.size()));
      batch.connect(Pin{p.id, static_cast<std::uint8_t>(pin)}, Pin{sources[first], slg::kOut});
      const std::uint32_t extra = rng.below(100);
      const std::uint32_t second = rng.below(static_cast<std::uint32_t>(sources.size()));
      if (extra < 10 && second != first) {
        batch.connect(Pin{p.id, static_cast<std::uint8_t>(pin)}, Pin{sources[second], slg::kOut});
      }
    }
  }
  const Status built = sim.apply(batch);
  fold.status(built);
  report.expect(built == Status::Ok, "the generated netlist was rejected", index);
  if (built != Status::Ok) {
    total.u64(fold.value());
    return;
  }

  std::vector<Signal> held(sim.sensorIds().size(), 0);
  bool twinLive = false;
  for (std::uint32_t tick = 0; tick < kTicksPerScenario; ++tick) {
    if (tick == kEditTick) {
      const std::vector<ComponentId> oldIds(sim.sensorIds().begin(), sim.sensorIds().end());
      editSimulator(sim, rng, fold, report, index);
      held = carryReadings(oldIds, held, sim.sensorIds());
    }
    if (tick == kRoundTripTick) {
      roundTrip(sim, twin, rng, limits, fold, report, index);
      twinLive = true;
    }
    for (Signal& reading : held) {
      const std::uint32_t change = rng.below(3);
      const std::uint32_t wild = rng.below(40);
      const std::uint32_t value = rng.below(16);
      if (change == 0) reading = static_cast<Signal>(wild == 0 ? 200U : value);
    }
    const Status stepped = sim.step(held);
    fold.status(stepped);
    report.expect(stepped == Status::Ok, "step rejected a well-formed frame", index);
    foldTick(fold, sim);
    if (twinLive) {
      const Status twinStepped = twin.step(held);
      report.expect(twinStepped == Status::Ok, "the decoded twin rejected a frame", index);
      report.expect(twin.stateHash() == sim.stateHash(), "decoded twin diverged from the original (T10)", index);
    }
  }
  fold.u64(sim.netlist().structureHash());
  fold.u32(sim.netlist().nextId());
  fold.u32(sim.hostStepMicros());
  foldLint(fold, sim.netlist());

  std::vector<Signal> wrong(sim.sensorIds().size() + 1, 0);
  const Snapshot before = snapshot(sim);
  const Status badFrame = sim.step(wrong);
  fold.status(badFrame);
  report.expect(badFrame == Status::BadFrame, "a frame of the wrong length was not rejected (I8)", index);
  report.expect(snapshot(sim) == before, "BadFrame changed the simulator (I3)", index);

  total.u64(fold.value());
}

void checkPrimitives(Report& report) {
  SplitMix64 zero(0);
  const std::uint64_t expected[4] = {0xE220A8397B1DCDAFULL, 0x6E789E6AA1B965F4ULL, 0x06C45D188009454FULL,
                                     0xF88BB8A8724C81ECULL};
  bool splitmixOk = true;
  for (const std::uint64_t want : expected) splitmixOk = splitmixOk && zero.next() == want;
  report.expect(splitmixOk, "SplitMix64(0) does not reproduce the published vectors");
  const std::uint8_t a[1] = {'a'};
  report.expect(fnv1a({}) == kFnvBasis, "FNV-1a of the empty string");
  report.expect(fnv1a(std::span<const std::uint8_t>(a)) == 0xAF63DC4C8601EC8CULL, "FNV-1a of \"a\"");
}

Simulator buildG1() {
  Simulator sim(Limits{}, 100000);
  EditBatch batch = sim.newBatch();
  const ComponentId sensor = batch.add(slg::makeSensor(), 0);
  const ComponentId inverter = batch.add(slg::makeNot(), 15);
  const ComponentId actuator = batch.add(slg::makeActuator(), 0);
  batch.connect(Pin{sensor, slg::kOut}, Pin{inverter, 0});
  batch.connect(Pin{inverter, slg::kOut}, Pin{actuator, 0});
  const Status status = sim.apply(batch);
  if (status != Status::Ok) std::printf("FAIL G1 netlist rejected: status %u\n", static_cast<unsigned>(status));
  return sim;
}

void checkG1(Fold& digest, Report& report) {
  Simulator sim = buildG1();
  report.expect(sim.sensorIds().size() == 1 && sim.sensorIds()[0] == 1, "G1 sensor id");
  report.expect(sim.actuatorIds().size() == 1 && sim.actuatorIds()[0] == 3, "G1 actuator id");
  std::uint64_t hash = sim.stateHash();
  digest.u64(hash);
  report.expect(hash == kG1Hashes[0], "G1 row 0 stateHash");
  for (std::uint32_t row = 1; row < 5; ++row) {
    const Signal sample = kG1Samples[row - 1];
    const Signal frame[1] = {sample};
    report.expect(sim.step(frame) == Status::Ok, "G1 step");
    hash = sim.stateHash();
    digest.u64(hash);
    report.expect(hash == kG1Hashes[row], "G1 stateHash row");
    std::printf("g1 row %u stateHash %s%s\n", static_cast<unsigned>(row), hex16(hash).c_str(),
                hash == kG1Hashes[row] ? "" : "  <-- MISMATCH");
  }
  const std::uint8_t expectedCommand = 0;
  report.expect(sim.commands().size() == 1 && sim.commands()[0] == expectedCommand, "G1 final command");
}

void checkG7(Fold& digest, Report& report) {
  const Simulator sim = buildG1();
  const std::vector<std::uint8_t> blob = sim.encode();
  report.expect(blob.size() == sizeof kG7Blob, "G7 blob size is not 95 bytes");
  bool same = blob.size() == sizeof kG7Blob;
  for (std::size_t i = 0; same && i < blob.size(); ++i) same = blob[i] == kG7Blob[i];
  report.expect(same, "G7 blob differs from 02 section 6.4");
  const std::uint64_t tail = blob.size() >= 8 ? fnv1a(std::span<const std::uint8_t>(blob).first(blob.size() - 8)) : 0;
  report.expect(tail == kG7Tail, "G7 tail (FNV-1a of the body)");
  report.expect(sim.netlist().structureHash() == kG7Structure, "G7 structureHash");
  digest.u64(tail);
  digest.u64(sim.netlist().structureHash());
  digest.u64(fnv1a(blob));
  std::printf("g7 size %zu tail %s structure %s\n", blob.size(), hex16(tail).c_str(),
              hex16(sim.netlist().structureHash()).c_str());

  Simulator decoded(Limits{}, 0);
  report.expect(Simulator::decode(blob, Limits{}, decoded) == Status::Ok, "G7 decode");
  report.expect(decoded.encode() == blob, "G7 re-encode");
}

bool parseCount(const char* text, std::uint32_t& out) {
  if (text == nullptr || text[0] == '\0') return false;
  std::uint64_t value = 0;
  for (const char* p = text; *p != '\0'; ++p) {
    if (*p < '0' || *p > '9') return false;
    value = value * 10 + static_cast<std::uint64_t>(*p - '0');
    if (value > 1000000) return false;
  }
  out = static_cast<std::uint32_t>(value);
  return true;
}

bool parseHex16(const char* text, std::uint64_t& out) {
  if (text == nullptr) return false;
  std::uint64_t value = 0;
  std::size_t digits = 0;
  for (const char* p = text; *p != '\0'; ++p, ++digits) {
    std::uint64_t nibble = 0;
    if (*p >= '0' && *p <= '9') nibble = static_cast<std::uint64_t>(*p - '0');
    else if (*p >= 'A' && *p <= 'F') nibble = static_cast<std::uint64_t>(*p - 'A' + 10);
    else if (*p >= 'a' && *p <= 'f') nibble = static_cast<std::uint64_t>(*p - 'a' + 10);
    else return false;
    value = (value << 4) | nibble;
  }
  out = value;
  return digits == 16;
}

}  // namespace

int main(int argc, char** argv) {
  std::uint32_t scenarios = kDefaultScenarios;
  std::optional<std::uint64_t> expected;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    const char* value = i + 1 < argc ? argv[i + 1] : nullptr;
    std::uint64_t parsedHex = 0;
    if (arg == "--scenarios" && parseCount(value, scenarios)) {
      ++i;
    } else if (arg == "--expect" && parseHex16(value, parsedHex)) {
      expected = parsedHex;
      ++i;
    } else {
      std::fputs("usage: slg_selfcheck [--scenarios N] [--expect HEX16]\n", stderr);
      return 2;
    }
  }

  Report report;
  Fold digest;
  std::printf("slg_selfcheck semantics 1 format 1\n");
  checkPrimitives(report);
  checkG1(digest, report);
  checkG7(digest, report);

  Fold scenarioFold;
  for (std::uint32_t i = 0; i < scenarios; ++i) runScenario(i, scenarioFold, report);
  digest.u64(scenarioFold.value());
  std::printf("scenarios %u fold %s\n", static_cast<unsigned>(scenarios), hex16(scenarioFold.value()).c_str());
  std::printf("digest %s\n", hex16(digest.value()).c_str());

  if (expected.has_value() && *expected != digest.value()) {
    std::printf("FAIL digest differs from the expected %s\n", hex16(*expected).c_str());
    ++report.failures;
  }
  if (scenarios != kDefaultScenarios) std::printf("note: %u scenarios instead of the standard %u; the digest is not comparable\n",
                                                   static_cast<unsigned>(scenarios), static_cast<unsigned>(kDefaultScenarios));
  std::printf("result %s\n", report.failures == 0 ? "OK" : "FAIL");
  return report.failures == 0 ? 0 : 1;
}
