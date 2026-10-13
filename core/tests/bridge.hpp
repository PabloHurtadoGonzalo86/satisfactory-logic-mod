// The one place where the test suite touches the core's public API (02 section 2). Everything the
// property tests need from slg:: (lint, pacer, frame driver) is exposed here through slgt types; the
// Simulator itself is reached through Engine (engine.hpp). When the core is not linked
// (bridge_nosim.cpp) available() is false and the tests that need it report "skipped".
#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "tmodel.hpp"

namespace slgt::bridge {

bool available();

// ---- spec rules (02 section 2: validate, restSeed, the makeXxx factories) ---------------------------------

Status validateSpec(const Spec& s, unsigned seed, const Limits& lim);
unsigned restSeedOf(const Spec& s);
// The specs the public factories return for a fixed list of literal arguments, paired with the spec
// the test expects (so the test side does not depend on the factories' parameter types).
struct FactoryCase {
  const char* call;
  Spec got;
  Spec expected;
};
std::vector<FactoryCase> factoryCases();

// ---- Netlist used on its own (structure only, seeds ignored) ---------------------------------------------------

struct NetlistInfo {
  Status status = Status::Ok;
  std::uint64_t structureHash = 0;
  std::uint32_t nextId = 0;
  std::vector<std::pair<ComponentId, Spec>> comps;
  std::vector<Wire> wires;
};
// Netlist::apply of one batch (adds then connects) on an empty Netlist.
NetlistInfo netlistDirect(const Model& m);

// ---- lint (02 section 2, 01 section 5 and 8) ---------------------------------------------------------

struct FeedbackReport {
  std::vector<std::vector<ComponentId>> groups;
  std::vector<Pin> undrivenInputs;
  std::vector<std::vector<Pin>> multiDriverNets;
};
// Builds the model's structure in a Simulator under lim, then runs analyze() on its netlist.
Status analyze(const Model& m, const Limits& lim, FeedbackReport& out);
// settlingBounds(netlist): one entry per actuator (ascending id), empty when the bound does not apply.
Status settlingBounds(const Model& m, const Limits& lim, std::vector<std::optional<std::uint32_t>>& out);

// ---- host: StepPacer and fullness (02 section 7.3, 2) --------------------------------------------------

class Pacer {
 public:
  virtual ~Pacer() = default;
  virtual std::uint32_t advance(std::uint32_t elapsedMicros, bool paused) = 0;
  virtual std::uint64_t droppedSteps() const = 0;
  virtual std::uint64_t pausedMicros() const = 0;
  virtual std::uint64_t accumulatedMicros() const = 0;
  virtual std::uint32_t stepMicros() const = 0;
};
std::unique_ptr<Pacer> makePacer(std::uint32_t stepMicros, std::uint32_t maxStepsPerCall);
// StepPacer::forHostStep: the pacer of a world whose Simulator::hostStepMicros() is the argument (0 = unset).
std::unique_ptr<Pacer> makePacerForHost(std::uint32_t hostStepMicros, std::uint32_t maxStepsPerCall);
// effectiveStepMicros: the step a world with this hostStepMicros runs at.
std::uint32_t effectiveStepMicros(std::uint32_t hostStepMicros);
std::int64_t fullness(std::int64_t num, std::int64_t den);

// ---- host: FrameDriver (02 section 7) -----------------------------------------------------------------

struct DriverChange {
  ComponentId id = 0;
  Signal previous = 0;
  Signal value = 0;
  friend bool operator==(const DriverChange&, const DriverChange&) = default;
};

struct DriverOptions {
  std::uint32_t resolveTimeoutMicros = 5'000'000;
  bool settleGuard = true;
};

// A FrameDriver together with the Simulator it drives.
class Driver {
 public:
  virtual ~Driver() = default;
  virtual Status apply(const std::vector<EditOp>& ops) = 0;
  virtual void setSensorMode(ComponentId sensor, bool pulse) = 0;
  virtual void resync() = 0;
  virtual void offer(ComponentId sensor, Signal reading) = 0;
  virtual Status runCall(std::uint32_t steps, std::uint32_t elapsedMicros) = 0;
  virtual std::vector<DriverChange> changes() const = 0;
  virtual void acknowledge(ComponentId actuator) = 0;
  virtual void endCall() = 0;
  virtual std::vector<std::uint8_t> encode() const = 0;
  virtual Status decode(const std::vector<std::uint8_t>& blob) = 0;
  // the driven Simulator
  virtual Tick simTick() const = 0;
  virtual std::uint64_t simStateHash() const = 0;
  virtual std::vector<std::uint8_t> simEncode() const = 0;
  virtual std::vector<Signal> simCommands() const = 0;
  virtual std::uint32_t simClamped() const = 0;
  // An edit applied to the driven Simulator itself, behind the driver's back (the driver learns of it by comparing structure hashes).
  virtual Status simApply(const std::vector<EditOp>& ops) = 0;
  // 02 section 7.6: decode the simulator blob into a NEW Simulator and bind a NEW FrameDriver to it (same options); the
  // old pair is dropped. On a non-Ok status nothing changes. The driver blob is not restored here: call decode() after.
  virtual Status adopt(const std::vector<std::uint8_t>& simBlob) = 0;
};
std::unique_ptr<Driver> makeDriver(const Limits& lim, std::uint32_t hostStepMicros, const DriverOptions& opt);
// A driver whose Simulator is decoded from a blob (the "after load" situation). nullptr if decode fails.
std::unique_ptr<Driver> makeDriverFromBlob(const std::vector<std::uint8_t>& simBlob, const Limits& lim,
                                           const DriverOptions& opt, Status& decodeStatus);

}  // namespace slgt::bridge
