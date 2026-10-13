// slg_bench (03 section 8): cost of one tick of the Full evaluator on deterministic netlists.
//
// Not production core code: it is the only place that may read a clock. Two kinds of output lines:
//   `counters ...` are deterministic (structure, per-tick work, byte sizes, a state checksum) and are
//   what CI may diff; `timing ...` are wall-clock and informational.
// Environment: SLG_BENCH_BIG=1 adds N = 100000; SLG_BENCH_BUDGET_NS and SLG_BENCH_MARGIN print the
// N_max estimate of 03 section 8 (budget B in ns, safety margin, default 4).
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
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

constexpr std::uint64_t kBenchBase = 2026101300ULL;
constexpr std::uint64_t kFnvBasis = 0xcbf29ce484222325ULL;
constexpr std::uint64_t kFnvPrime = 0x100000001b3ULL;
constexpr std::uint32_t kGroupDrivers = 120;      // conductors per fan-in net
constexpr std::uint32_t kGroupSize = 2 * kGroupDrivers + 2;
constexpr std::uint32_t kWarmupTicks = 16;

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
  std::uint32_t below(std::uint32_t bound) noexcept { return static_cast<std::uint32_t>(next() % bound); }

 private:
  std::uint64_t state_;
};

std::uint64_t foldU64(std::uint64_t hash, std::uint64_t value) noexcept {
  for (int i = 0; i < 8; ++i) {
    hash ^= (value >> (8 * i)) & 0xFFU;
    hash *= kFnvPrime;
  }
  return hash;
}

std::string hex16(std::uint64_t v) {
  static constexpr char kDigits[] = "0123456789ABCDEF";
  std::string out(16, '0');
  for (std::size_t i = 0; i < 16; ++i) out[15 - i] = kDigits[(v >> (4 * i)) & 0xFU];
  return out;
}

// value / 1000 with three decimals, from integers only.
std::string milli(std::uint64_t thousandths) {
  std::string frac = std::to_string(thousandths % 1000);
  frac.insert(0, 3 - frac.size(), '0');
  return std::to_string(thousandths / 1000) + "." + frac;
}

std::uint64_t envNumber(const char* name, std::uint64_t fallback) {
  const char* text = std::getenv(name);
  if (text == nullptr || text[0] == '\0') return fallback;
  std::uint64_t value = 0;
  for (const char* p = text; *p != '\0'; ++p) {
    if (*p < '0' || *p > '9') return fallback;
    value = value * 10 + static_cast<std::uint64_t>(*p - '0');
  }
  return value;
}

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

enum class Shape : std::uint8_t { Random, Chain, FanIn, Ring };

const char* shapeName(Shape shape) {
  switch (shape) {
    case Shape::Random: return "random";
    case Shape::Chain: return "chain";
    case Shape::FanIn: return "fanin";
    case Shape::Ring: return "ring";
  }
  return "?";
}

Spec randomSpec(SplitMix64& rng) {
  const std::uint32_t pick = rng.below(20);
  if (pick < 1) return slg::makeConst(static_cast<std::uint16_t>(rng.below(16)));
  if (pick < 3) return slg::makeSensor();
  if (pick < 5) return slg::makeActuator();
  if (pick < 13) {
    const std::uint32_t op = rng.below(3);
    const std::uint32_t inv = rng.below(2);
    const std::uint32_t arity = 1 + rng.below(4);
    return Spec{Kind::Gate, static_cast<std::uint16_t>(op), static_cast<std::uint16_t>(inv),
                static_cast<std::uint16_t>(arity)};
  }
  if (pick < 15) return Spec{Kind::Delay, static_cast<std::uint16_t>(1 + rng.below(4)), 0, 0};
  if (pick < 17) return Spec{Kind::Compare, static_cast<std::uint16_t>(rng.below(2)), 0, 0};
  if (pick < 18) return slg::makeLatch();
  if (pick < 19) return slg::makeToggle();
  const std::uint32_t period = 2 + rng.below(15);
  const std::uint32_t high = 1 + rng.below(period - 1);
  return Spec{Kind::Clock, static_cast<std::uint16_t>(period), static_cast<std::uint16_t>(high), 0};
}

void fillRandom(EditBatch& batch, std::uint32_t n, SplitMix64& rng) {
  struct Planned {
    ComponentId id;
    Spec spec;
  };
  std::vector<Planned> planned;
  std::vector<ComponentId> sources;
  planned.reserve(n);
  for (std::uint32_t i = 0; i < n; ++i) {
    const Spec spec = randomSpec(rng);
    const ComponentId id = batch.add(spec, slg::restSeed(spec));
    planned.push_back(Planned{id, spec});
    if (spec.kind != Kind::Actuator) sources.push_back(id);
  }
  for (const Planned& p : planned) {
    const unsigned inputs = inputCount(p.spec);
    for (unsigned pin = 0; pin < inputs; ++pin) {
      const std::uint32_t wired = rng.below(100);
      const std::uint32_t first = rng.below(static_cast<std::uint32_t>(sources.size()));
      const std::uint32_t extra = rng.below(100);
      const std::uint32_t second = rng.below(static_cast<std::uint32_t>(sources.size()));
      if (wired >= 90) continue;
      batch.connect(Pin{p.id, static_cast<std::uint8_t>(pin)}, Pin{sources[first], slg::kOut});
      if (extra < 5 && second != first) {
        batch.connect(Pin{p.id, static_cast<std::uint8_t>(pin)}, Pin{sources[second], slg::kOut});
      }
    }
  }
}

void fillChain(EditBatch& batch, std::uint32_t n) {
  ComponentId previous = batch.add(slg::makeSensor(), 0);
  for (std::uint32_t i = 2; i < n; ++i) {
    const ComponentId id = batch.add(slg::makeNot(), 15);
    batch.connect(Pin{previous, slg::kOut}, Pin{id, 0});
    previous = id;
  }
  const ComponentId sink = batch.add(slg::makeActuator(), 0);
  batch.connect(Pin{previous, slg::kOut}, Pin{sink, 0});
}

void fillRing(EditBatch& batch, std::uint32_t n, SplitMix64& rng) {
  std::vector<ComponentId> ids;
  ids.reserve(n);
  for (std::uint32_t i = 0; i < n; ++i) {
    const std::uint16_t seed = static_cast<std::uint16_t>(rng.below(2) * 15);
    ids.push_back(batch.add(slg::makeNot(), seed));
  }
  for (std::uint32_t i = 0; i < n; ++i) {
    batch.connect(Pin{ids[i], slg::kOut}, Pin{ids[(i + 1) % n], 0});
  }
}

// One AND gate whose two inputs each read a net of kGroupDrivers conductors, then an actuator.
void fillFanIn(EditBatch& batch, std::uint32_t n, SplitMix64& rng) {
  const std::uint32_t groups = std::max<std::uint32_t>(1, n / kGroupSize);
  for (std::uint32_t g = 0; g < groups; ++g) {
    const ComponentId gate = batch.add(slg::makeGate(slg::GateOp::And, false, 2), 0);
    const ComponentId sink = batch.add(slg::makeActuator(), 0);
    batch.connect(Pin{gate, slg::kOut}, Pin{sink, 0});
    for (std::uint8_t input = 0; input < 2; ++input) {
      for (std::uint32_t d = 0; d < kGroupDrivers; ++d) {
        const std::uint32_t value = 1 + rng.below(15);
        const ComponentId driver =
            d % 4 == 0 ? batch.add(slg::makeSensor(), 0) : batch.add(slg::makeConst(static_cast<std::uint16_t>(value)), 0);
        batch.connect(Pin{gate, input}, Pin{driver, slg::kOut});
      }
    }
  }
}

Limits limitsFor(std::uint32_t n) {
  Limits limits;
  limits.maxComponents = n + kGroupSize;
  limits.maxWires = 8 * n + 4 * kGroupSize;
  limits.maxDelayCells = 4 * n + kGroupSize;
  return limits;
}

struct Counters {
  std::uint32_t components = 0, wires = 0, sensors = 0, actuators = 0;
  std::uint64_t inputPins = 0, delayCells = 0;
  std::size_t feedbackGroups = 0, multiDriverNets = 0, largestMultiNet = 0, undrivenInputs = 0;
  std::size_t netlistBytes = 0, blobBytes = 0;
};

Counters measureStructure(const Simulator& sim) {
  Counters c;
  c.components = static_cast<std::uint32_t>(sim.netlist().components().size());
  c.wires = static_cast<std::uint32_t>(sim.netlist().wires().size());
  c.sensors = static_cast<std::uint32_t>(sim.sensorIds().size());
  c.actuators = static_cast<std::uint32_t>(sim.actuatorIds().size());
  for (const auto& entry : sim.netlist().components()) {
    c.inputPins += inputCount(entry.second);
    if (entry.second.kind == Kind::Delay) c.delayCells += entry.second.p0;
  }
  const slg::FeedbackReport lint = slg::analyze(sim.netlist());
  c.feedbackGroups = lint.groups.size();
  c.multiDriverNets = lint.multiDriverNets.size();
  for (const auto& net : lint.multiDriverNets) c.largestMultiNet = std::max(c.largestMultiNet, net.size());
  c.undrivenInputs = lint.undrivenInputs.size();
  c.netlistBytes = sim.netlist().components().size_bytes() + sim.netlist().wires().size_bytes();
  c.blobBytes = sim.encode().size();
  return c;
}

bool runScenario(Shape shape, std::uint32_t n) {
  SplitMix64 rng(kBenchBase ^ (static_cast<std::uint64_t>(shape) << 40) ^ n);
  Simulator sim(limitsFor(n), 100000);
  EditBatch batch = sim.newBatch();
  switch (shape) {
    case Shape::Random: fillRandom(batch, n, rng); break;
    case Shape::Chain: fillChain(batch, n); break;
    case Shape::FanIn: fillFanIn(batch, n, rng); break;
    case Shape::Ring: fillRing(batch, n, rng); break;
  }
  const Status built = sim.apply(batch);
  if (built != Status::Ok) {
    std::printf("error scenario=%s n=%u apply status=%u\n", shapeName(shape), static_cast<unsigned>(n),
                static_cast<unsigned>(built));
    return false;
  }

  const Counters c = measureStructure(sim);
  std::vector<std::vector<Signal>> frames(2, std::vector<Signal>(sim.sensorIds().size(), 0));
  for (std::vector<Signal>& frame : frames) {
    for (Signal& reading : frame) reading = static_cast<Signal>(rng.below(16));
  }

  const std::uint32_t ticks = static_cast<std::uint32_t>(std::clamp<std::uint64_t>(2000000 / n, 20, 400));
  for (std::uint32_t i = 0; i < kWarmupTicks; ++i) {
    if (sim.step(frames[i & 1U]) != Status::Ok) {
      std::printf("error scenario=%s n=%u warm-up step rejected\n", shapeName(shape), static_cast<unsigned>(n));
      return false;
    }
  }
  std::vector<std::uint64_t> nanos;
  nanos.reserve(ticks);
  std::uint64_t checksum = kFnvBasis;
  for (std::uint32_t i = 0; i < ticks; ++i) {
    const std::vector<Signal>& frame = frames[i & 1U];
    const auto begin = std::chrono::steady_clock::now();
    const Status stepped = sim.step(frame);
    const auto end = std::chrono::steady_clock::now();
    if (stepped != Status::Ok) {
      std::printf("error scenario=%s n=%u step rejected\n", shapeName(shape), static_cast<unsigned>(n));
      return false;
    }
    nanos.push_back(static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count()));
    checksum = foldU64(checksum, sim.stateHash());
    for (const Signal command : sim.commands()) checksum = foldU64(checksum, command);
  }
  std::sort(nanos.begin(), nanos.end());
  const std::uint64_t median = nanos[nanos.size() / 2];
  const std::uint64_t p99 = nanos[std::min<std::size_t>(nanos.size() - 1, nanos.size() * 99 / 100)];
  const std::uint64_t worst = nanos.back();

  std::printf("counters scenario=%s n=%u components=%u wires=%u sensors=%u actuators=%u input_pins=%llu "
              "delay_cells=%llu evaluated_per_tick=%u net_reads_per_tick=%llu feedback_groups=%zu "
              "multi_driver_nets=%zu largest_multi_net=%zu undriven_inputs=%zu ticks=%u "
              "netlist_bytes_per_component=%zu blob_bytes_per_component=%zu state_checksum=%s\n",
              shapeName(shape), static_cast<unsigned>(n), static_cast<unsigned>(c.components),
              static_cast<unsigned>(c.wires), static_cast<unsigned>(c.sensors), static_cast<unsigned>(c.actuators),
              static_cast<unsigned long long>(c.inputPins), static_cast<unsigned long long>(c.delayCells),
              static_cast<unsigned>(c.components), static_cast<unsigned long long>(c.inputPins), c.feedbackGroups,
              c.multiDriverNets, c.largestMultiNet, c.undrivenInputs, static_cast<unsigned>(ticks),
              c.netlistBytes / c.components, c.blobBytes / c.components, hex16(checksum).c_str());

  std::string line = "timing scenario=" + std::string(shapeName(shape)) + " n=" + std::to_string(n) +
                     " median_ns=" + std::to_string(median) + " p99_ns=" + std::to_string(p99) +
                     " max_ns=" + std::to_string(worst) +
                     " p99_ns_per_component=" + milli(p99 * 1000 / c.components);
  if (p99 > 0) {
    const std::uint64_t budget = envNumber("SLG_BENCH_BUDGET_NS", 0);
    const std::uint64_t margin = std::max<std::uint64_t>(1, envNumber("SLG_BENCH_MARGIN", 4));
    if (budget > 0) line += " n_max_estimate=" + std::to_string(budget * c.components / (p99 * margin));
  }
  std::printf("%s\n", line.c_str());
  return true;
}

}  // namespace

int main() {
  const bool big = envNumber("SLG_BENCH_BIG", 0) == 1;
  std::vector<std::uint32_t> sizes = {1000, 10000};
  if (big) sizes.push_back(100000);
  std::printf("slg_bench semantics 1 big %d sizeof Simulator=%zu Spec=%zu Pin=%zu Wire=%zu Component=%zu\n",
              big ? 1 : 0, sizeof(Simulator), sizeof(Spec), sizeof(Pin), sizeof(slg::Wire),
              sizeof(std::pair<ComponentId, Spec>));
  bool ok = true;
  for (const Shape shape : {Shape::Random, Shape::Chain, Shape::FanIn, Shape::Ring}) {
    for (const std::uint32_t n : sizes) ok = runScenario(shape, n) && ok;
  }
  return ok ? 0 : 1;
}
