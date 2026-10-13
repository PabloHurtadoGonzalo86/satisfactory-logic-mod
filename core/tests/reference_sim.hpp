// ReferenceSim: a naive, map-based interpreter of 01 (semantics) and 02 sections 4 to 6 (edits, blob),
// written from the spec text and independent of slg::Simulator. Nothing here is fast; everything is
// the plainest reading of the rules:
//   - a net is the connected component of the wire graph, found by walking from the pin each time;
//   - step() is a true two-buffer tick: every next state is computed from the snapshot, then committed;
//   - stepInPlace() is the negative control: components are updated one at a time in the given order,
//     and every net value is recomputed at the moment of each read.
#pragma once

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "blob.hpp"
#include "tmodel.hpp"

namespace slgt {

// Evaluation order handed to a step: components in this order, wires entering the adjacency in this
// order (so net walks visit pins in a different order each time).
struct StepOrder {
  std::vector<ComponentId> comps;
  std::vector<std::size_t> wires;
};

// What a batch answers. The status is exact: the reference reads every rule of 02 sections 4 to 6 down to the order in
// which one operation's defects are judged (ambiguities.md B2, B4, B8), so a test never accepts "either of two".
struct ApplyResult {
  Status status = Status::Ok;
};
// Does a status returned by the core satisfy what the reference expects? Plain equality.
bool compatible(const ApplyResult& expected, Status got) noexcept;
const char* describe(const ApplyResult& r) noexcept;

// Outcome of the strict decode of 02 section 6.3, in the order of the checks of the specification.
struct DecodeResult {
  Status status = Status::Ok;
};
bool compatible(const DecodeResult& expected, Status got) noexcept;
std::string describe(const DecodeResult& r);
// Strict, total decode in the order of 02 section 6.3. out is written only when the result is Ok.
DecodeResult refDecode(const std::uint8_t* data, std::size_t n, const Limits& lim, BlobData& out);
inline DecodeResult refDecode(const std::vector<std::uint8_t>& v, const Limits& lim, BlobData& out) {
  return refDecode(v.data(), v.size(), lim, out);
}

// Deliberate misreadings of the spec, used only to check that the test suite can tell them apart from the
// right reading (s01_sensitivity.cpp). They mirror the mutants of 03 section 7 at the level of the reference.
enum class Quirk : std::uint8_t {
  None, InPlaceStep, SumFusion, LastDriverWins, TruthyAboveOne, DelayShort, DelayLong, CompareLess, LatchSetDominates,
  ToggleByLevel, ToggleStartsLow, ToggleRearmsOnConnect, SensorLate, ActuatorLate, NextIdNotSaved, DelayBlobReversed, kCount
};
const char* quirkName(Quirk q) noexcept;

class ReferenceSim {
 public:
  struct Comp {
    Spec spec;
    std::vector<std::uint16_t> st;   // gate/compare/latch/actuator: [s]; toggle: [q,p]; clock: [c]; delay: [f1..fd]
  };

  explicit ReferenceSim(const Limits& lim = {}, std::uint32_t hostStepMicros = 0);
  void setQuirk(Quirk q) noexcept { quirk_ = q; }

  // EditBatch semantics (02 sections 4 and 5): validated in order on a copy, all-or-nothing; the limits of 01 section 7
  // judge the netlist the batch leaves, after the last operation.
  ApplyResult apply(const std::vector<EditOp>& ops);
  ApplyResult applyWithBase(const std::vector<EditOp>& ops, std::uint32_t baseNextId);

  // One tick; samples[i] feeds sensorIds()[i]. BadFrame (state untouched) on a wrong length, values
  // above 15 saturate and are counted. order == nullptr means ascending ids and wires.
  Status step(const std::vector<Signal>& samples, const StepOrder* order = nullptr);
  Status stepInPlace(const std::vector<Signal>& samples, const StepOrder& order);

  Tick tick() const noexcept { return tick_; }
  std::uint32_t nextId() const noexcept { return nextId_; }
  std::uint32_t hostStepMicros() const noexcept { return hostStep_; }
  std::uint32_t clampedSamples() const noexcept { return clamped_; }
  const Limits& limits() const noexcept { return limits_; }
  const std::map<ComponentId, Comp>& comps() const noexcept { return comps_; }
  const std::set<Wire>& wires() const noexcept { return wires_; }
  std::vector<ComponentId> sensorIds() const;
  std::vector<ComponentId> actuatorIds() const;
  std::vector<Signal> commands() const;      // o of each actuator in the current state

  BlobData toBlobData() const;
  std::vector<std::uint8_t> encode() const;
  std::uint64_t stateHash() const;
  std::uint64_t structureHash() const;
  // Replaces the whole content (netlist, state, tick, nextId) with a decoded blob; false if a record
  // cannot be interpreted.
  bool load(const BlobData& d);
  void setTick(Tick t) noexcept { tick_ = t; }

  // Value of the net containing pin p in the current state under the given samples (01 section 2.3).
  Signal netValue(Pin p, const std::vector<Signal>& samples) const;
  // All pins of the net containing p, ascending.
  std::vector<Pin> netPins(Pin p) const;

 private:
  using SampleMap = std::map<ComponentId, Signal>;
  using Adjacency = std::map<Pin, std::vector<Pin>>;

  Adjacency buildAdjacency(const StepOrder* order) const;
  SampleMap sampleMap(const std::vector<Signal>& samples) const;
  unsigned outputOf(ComponentId id, const Comp& c, const SampleMap& sm) const;
  unsigned netValueOf(const Adjacency& adj, Pin p, const SampleMap& sm) const;
  std::vector<std::uint16_t> nextState(ComponentId id, const Comp& c, const Adjacency& adj, const SampleMap& sm) const;
  std::vector<std::uint16_t> seedState(const Spec& s, unsigned seed) const;
  bool truthy(unsigned v) const noexcept { return quirk_ == Quirk::TruthyAboveOne ? v > 1 : v > 0; }
  std::vector<Signal> rawCommands() const;

  Quirk quirk_ = Quirk::None;
  SampleMap prevSamples_;
  std::vector<Signal> prevCommands_;
  Limits limits_;
  std::uint32_t hostStep_ = 0;
  std::map<ComponentId, Comp> comps_;
  std::set<Wire> wires_;
  std::uint32_t nextId_ = 1;
  Tick tick_ = 0;
  std::uint32_t clamped_ = 0;
};

}  // namespace slgt
