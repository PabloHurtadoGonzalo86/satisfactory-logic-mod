// Random netlists, scenarios, edit batches and corrupted blobs (03 section 6). Every generator takes a
// SplitMix64 so a case is fully determined by (SLG_SEED, case index). Coverage promised by 03:
// DAGs, cyclic nets, rings, wide fan-in, latches, edit batches, and always unconnected pins, self-loops
// and several drivers on one net.
#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "blob.hpp"
#include "prng.hpp"
#include "reference_sim.hpp"
#include "tmodel.hpp"

namespace slgt {

struct GenConfig {
  unsigned minComps = 2;
  unsigned maxComps = 14;
  bool sparseIds = false;       // ids ascending with random gaps instead of 1..n
  unsigned unconnectedPct = 12;  // chance an input pin gets no wire
  unsigned extraDriverPct = 15;  // chance a net gets a second driver
  unsigned wideGatePct = 8;      // chance a gate is 5..8 wide
  bool allowSelfLoops = true;
};

Spec genSpec(SplitMix64& rng, bool allowWide = true);                  // any kind, legal parameters
Spec genSpecOfKind(SplitMix64& rng, Kind k, bool allowWide = true);
unsigned genSeed(SplitMix64& rng, const Spec& s);                      // legal seed for the spec

Model genModel(SplitMix64& rng, const GenConfig& cfg);                  // mixed: cycles, merges, floating pins
Model genRing(SplitMix64& rng, unsigned n, bool randomSeeds);           // n NOT gates in a ring
// Ring of 01 section 9.1: g1..gn are NOT gates, g(i+1) reads g(i) and g1 reads g(n); bit i-1 of mask is the
// seed of g(i) (1 means 15); gates beyond the 32nd are seeded 15.
Model ringModel(unsigned n, std::uint32_t seedMask);
Model genWide(SplitMix64& rng, unsigned arity);                         // one wide gate fed by many drivers
Model genChain(unsigned length);                                        // sensor -> NOT x length -> actuator

struct DagConfig {
  unsigned minComps = 3;
  unsigned maxComps = 16;
  bool withCompare = true;
  bool withDelay = true;
};
// Netlists whose net-induced graph is acyclic and made only of Const, Sensor, Gate, Compare, Delay and
// Actuator (the cone of 01 section 8). Nets are built first and every reader comes after all drivers of
// its net, so merging never creates a loop.
Model genDag(SplitMix64& rng, const DagConfig& cfg);

// A mixture of everything above plus the degenerate cases (empty netlist, a lone self-loop, tiny nets); the
// default model of the randomized properties.
Model genAnyModel(SplitMix64& rng);

// Random legal state for every component of the model (not just the seed state).
BlobData randomLegalState(SplitMix64& rng, const Model& m, Tick tick = 0);

// ---- samples ---------------------------------------------------------------------------------------------

enum class SampleStyle : std::uint8_t { Random, Constant, Sparse, Steps };
// frames[t][i] feeds the i-th sensor (ascending id) at tick t, for a fixed sensor count.
std::vector<std::vector<Signal>> genFrames(SplitMix64& rng, std::size_t sensors, unsigned ticks, SampleStyle style);
SampleStyle genStyle(SplitMix64& rng);

// The scenario whose sensor orders reproduce frames (frames[t][i] feeds the i-th sensor of m at row t).
Scenario scenarioFromFrames(const Model& m, const std::vector<std::vector<Signal>>& frames);

// frames[t][i]: the held value of the i-th sensor of sc.net at row t = 0..sc.ticks (edits ignored).
std::vector<std::vector<Signal>> framesOf(const Scenario& sc);

// ---- edits -----------------------------------------------------------------------------------------------

// Mirror of the live netlist used to produce edit batches that are valid by construction.
struct GenState {
  std::map<ComponentId, Spec> comps;
  std::set<Wire> wires;
  std::set<ComponentId> retired;
  std::uint32_t nextId = 1;

  static GenState fromModel(const Model& m);
  void apply(const EditOp& op);
  std::vector<Pin> allPins() const;
};

// A valid batch of 1..maxOps operations (adds viaAdd or with a gap in the ids, removes, replaces,
// connects, disconnects), applied to st as it is generated.
std::vector<EditOp> genBatch(SplitMix64& rng, GenState& st, unsigned maxOps, unsigned maxComps = 30);

enum class Fault : std::uint8_t {
  BadSpecDomain, BadSpecSeed, BadSpecUnused, BadPinConnect, BadPinReplace, UnknownRemove, UnknownConnect,
  DuplicateId, RetiredId, UnknownWire, DuplicateWire, StaleBase, NextIdExhausted,
  OverCap,                      // arity, Delay length or Clock period above the cap: LimitExceeded, ahead of the seed and of the high width (B2)
  SpecOrder,                    // a malformed parameter next to one above its cap: BadSpec (B2)
  DisconnectUnknownComponent,   // disconnect of a wire that names a missing component: UnknownComponent, not UnknownWire (B4)
  DisconnectBadPin,             // disconnect of a wire with a bad pin, a pin to itself, an actuator output: BadPin (B4)
  WireOrder,                    // connect or disconnect with a defect in each pin: pin a is judged first (B4)
  AutoIdCollision,              // an explicit id where add() will hand out the next one: DuplicateId or RetiredId (B3)
  kCount
};
const char* faultName(Fault f) noexcept;
// A valid prefix followed by one op that breaks the batch in the given way (the broken op is always the last one that
// matters: ops after it are never judged). `want` is the exact status the spec gives; st is advanced through the valid
// prefix only. Returns false when the state has no way to express the fault (e.g. no wires).
bool genFaultyBatch(SplitMix64& rng, GenState& st, Fault f, std::vector<EditOp>& ops, Status& want);

// ---- scenarios -------------------------------------------------------------------------------------------

struct ScenarioConfig {
  GenConfig net;
  unsigned minTicks = 10;
  unsigned maxTicks = 32;
  unsigned editPct = 0;          // chance per tick of an edit batch
  bool randomState = false;      // start from a random legal state instead of the seeds (not expressible in lnet)
};
Scenario genScenario(SplitMix64& rng, const ScenarioConfig& cfg);

// Delta-debugging: shrinks sc while fails(sc) stays true (ticks, edit batches, input orders, wires,
// components that nothing else refers to).
Scenario reduceScenario(const Scenario& sc, const std::function<bool(const Scenario&)>& fails);

// ---- blob corruption (T12) -------------------------------------------------------------------------------

enum class Corruption : std::uint8_t {
  GateState7, ClockPhaseAtPeriod, DelaySlot16, NextIdAtMaxId, WireUnnormalized, WireUnsorted, WireDuplicate,
  WireMissingComponent, WirePinOutOfRange, IdsNotAscending, UnusedParamNonzero, KindZero, KindTen, GateOpThree,
  GateArityZero, LatchState7, TogglePhaseTwo, ToggleLevel7, CompareState16, ActuatorState16, ClockPeriodOne,
  ClockHighZero, ClockHighEqualsPeriod, DelayLengthZero, ExtraByteWithTail, kCount
};
const char* corruptionName(Corruption c) noexcept;
// A blob (with a freshly computed tail) that violates exactly one invariant of 02 section 6.3; false if the
// base blob has no component the corruption can apply to.
bool corruptBlob(const BlobData& base, Corruption c, std::vector<std::uint8_t>& out);

}  // namespace slgt
