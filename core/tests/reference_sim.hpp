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

struct ApplyResult {
  enum class Soft : std::uint8_t {
    Exact,              // the spec names this status
    ParamLimit,         // BadSpec or LimitExceeded (parameter bounded by a configurable limit)
    AnyError,           // any status but Ok (the spec does not pin the code)
    UnknownOrRetired    // UnknownComponent or RetiredId (operation on an id that was removed)
  };
  Status status = Status::Ok;
  Soft soft = Soft::Exact;
};
// Does a status returned by the core satisfy what the reference expects?
bool compatible(const ApplyResult& expected, Status got) noexcept;
const char* describe(const ApplyResult& r) noexcept;

// Outcome of the strict decode of 02 section 6.3, as the reference reads it. Where the spec leaves the
// code open the result says which alternatives are acceptable.
struct DecodeResult {
  Status status = Status::Ok;
  bool orLimit = false;     // FormatCorrupt or LimitExceeded are both acceptable (the spec does not order the checks)
  bool maybeOk = false;     // the spec does not say whether this is rejected (id 0, nextId 0): Ok is also acceptable
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

  // EditBatch semantics (02 sections 4 and 5): validated in order on a copy, all-or-nothing.
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
  // cannot be interpreted. The retired-id set is not part of the blob and starts empty.
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
  std::set<ComponentId> retired_;
  std::uint32_t nextId_ = 1;
  Tick tick_ = 0;
  std::uint32_t clamped_ = 0;
};

}  // namespace slgt
