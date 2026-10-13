// The bridge to the core: written against 02 section 2 verbatim. Reconciling the test suite with the
// real headers means editing this file only (and, if the host headers are named differently, the
// __has_include lists below).
#include "bridge.hpp"
#include "engine.hpp"

#include <span>
#include <utility>

#include "slg/lint.hpp"
#include "slg/netlist.hpp"
#include "slg/simulator.hpp"
#include "slg/types.hpp"

#if __has_include("slg/host/step_pacer.hpp")
#include "slg/host/step_pacer.hpp"
#elif __has_include("slg/host/pacer.hpp")
#include "slg/host/pacer.hpp"
#elif __has_include("slg/host/host.hpp")
#include "slg/host/host.hpp"
#else
#error "cannot find the StepPacer header under slg/host/ (edit core/tests/bridge_sim.cpp)"
#endif
#if __has_include("slg/host/frame_driver.hpp")
#include "slg/host/frame_driver.hpp"
#elif __has_include("slg/host/driver.hpp")
#include "slg/host/driver.hpp"
#elif __has_include("slg/host/host.hpp")
#include "slg/host/host.hpp"
#else
#error "cannot find the FrameDriver header under slg/host/ (edit core/tests/bridge_sim.cpp)"
#endif

namespace slgt {

namespace {

slg::Spec toSlg(const Spec& s) {
  return slg::Spec{static_cast<slg::Kind>(static_cast<std::uint8_t>(s.kind)), s.p0, s.p1, s.p2};
}
slg::Pin toSlg(Pin p) { return slg::Pin{p.component, p.index}; }
Pin fromSlg(const slg::Pin& p) { return Pin{p.component, p.index}; }
slg::Limits toSlg(const Limits& l) {
  slg::Limits r;
  r.maxComponents = l.maxComponents;
  r.maxWires = l.maxWires;
  r.maxNetPins = l.maxNetPins;
  r.maxDelayCells = l.maxDelayCells;
  r.maxDelay = l.maxDelay;
  r.maxClockPeriod = l.maxClockPeriod;
  r.maxArity = l.maxArity;
  return r;
}
Status fromSlg(slg::Status s) { return static_cast<Status>(static_cast<std::uint8_t>(s)); }

// Fills a batch from ops; false if an EditBatch::add handed out an id other than the one the op names.
bool fill(slg::EditBatch& b, const std::vector<EditOp>& ops) {
  bool matched = true;
  for (const EditOp& op : ops) {
    switch (op.kind) {
      case EditKind::Add:
        if (op.viaAdd) {
          const slg::ComponentId got = b.add(toSlg(op.spec), op.seed);
          if (got != op.id) matched = false;
        } else {
          b.addWithId(op.id, toSlg(op.spec), op.seed);
        }
        break;
      case EditKind::Remove: b.remove(op.id); break;
      case EditKind::Replace: b.replace(op.id, toSlg(op.spec), op.seed); break;
      case EditKind::Connect: b.connect(toSlg(op.a), toSlg(op.b)); break;
      case EditKind::Disconnect: b.disconnect(toSlg(op.a), toSlg(op.b)); break;
    }
  }
  return matched;
}

std::span<const std::uint8_t> view(const std::vector<std::uint8_t>& v) { return {v.data(), v.size()}; }

class SimEngine final : public Engine {
 public:
  SimEngine(const Limits& l, std::uint32_t hostStep)
      : Engine(l), sim_(std::make_unique<slg::Simulator>(toSlg(l), hostStep)) {}

  const char* label() const override { return "sim"; }
  bool isSim() const override { return true; }

  Status apply(const std::vector<EditOp>& ops) override {
    slg::EditBatch b = sim_->newBatch();
    matched_ = fill(b, ops);
    return fromSlg(sim_->apply(b));
  }
  std::size_t prepare(const std::vector<EditOp>& ops) override {
    slg::EditBatch b = sim_->newBatch();
    preparedMatched_.push_back(fill(b, ops));
    prepared_.push_back(std::move(b));
    return prepared_.size() - 1;
  }
  Status applyPrepared(std::size_t handle) override {
    matched_ = preparedMatched_[handle];
    return fromSlg(sim_->apply(prepared_[handle]));
  }
  bool lastBatchIdsMatched() const override { return matched_; }

  Status step(const std::vector<Signal>& samples) override {
    return fromSlg(sim_->step(std::span<const slg::Signal>(samples.data(), samples.size())));
  }
  Tick tick() const override { return sim_->tick(); }
  std::uint32_t nextId() const override { return sim_->netlist().nextId(); }
  std::uint32_t hostStepMicros() const override { return sim_->hostStepMicros(); }
  std::uint32_t clampedSamples() const override { return sim_->clampedSamples(); }
  std::vector<ComponentId> sensorIds() const override {
    const auto s = sim_->sensorIds();
    return std::vector<ComponentId>(s.begin(), s.end());
  }
  std::vector<ComponentId> actuatorIds() const override {
    const auto s = sim_->actuatorIds();
    return std::vector<ComponentId>(s.begin(), s.end());
  }
  std::vector<Signal> commands() const override {
    const auto s = sim_->commands();
    return std::vector<Signal>(s.begin(), s.end());
  }
  std::uint64_t stateHash() const override { return sim_->stateHash(); }
  std::uint64_t structureHash() const override { return sim_->netlist().structureHash(); }
  std::vector<std::uint8_t> encode() const override { return sim_->encode(); }

  Status loadBlob(const std::vector<std::uint8_t>& bytes) override {
    auto fresh = std::make_unique<slg::Simulator>(toSlg(limits_), 0);
    const slg::Status s = slg::Simulator::decode(view(bytes), toSlg(limits_), *fresh);
    if (s == slg::Status::Ok) sim_ = std::move(fresh);
    return fromSlg(s);
  }
  Status decodeDirect(const std::vector<std::uint8_t>& bytes, const Limits& lim) override {
    return fromSlg(slg::Simulator::decode(view(bytes), toSlg(lim), *sim_));
  }

 private:
  std::unique_ptr<slg::Simulator> sim_;
  std::vector<slg::EditBatch> prepared_;
  std::vector<bool> preparedMatched_;
  bool matched_ = true;
};

std::unique_ptr<Engine> makeSim(const Limits& l, std::uint32_t hostStep) { return std::make_unique<SimEngine>(l, hostStep); }

}  // namespace

const EngineFactory* simFactory() {
  static const EngineFactory f{"sim", true, &makeSim};
  return &f;
}

namespace bridge {

bool available() { return true; }

NetlistInfo netlistDirect(const Model& m) {
  NetlistInfo info;
  slg::Simulator scratch;                    // only to obtain a batch whose base nextId is 1, like an empty Netlist
  slg::EditBatch b = scratch.newBatch();
  fill(b, opsForModel(m));
  slg::Netlist nl;
  info.status = fromSlg(nl.apply(b));
  info.structureHash = nl.structureHash();
  info.nextId = nl.nextId();
  for (const auto& [id, spec] : nl.components()) info.comps.emplace_back(id, Spec{static_cast<Kind>(static_cast<std::uint8_t>(spec.kind)), spec.p0, spec.p1, spec.p2});
  for (const slg::Wire& w : nl.wires()) info.wires.push_back(Wire{fromSlg(w.a), fromSlg(w.b)});
  return info;
}

Status validateSpec(const Spec& s, unsigned seed, const Limits& lim) {
  return fromSlg(slg::validate(toSlg(s), static_cast<std::uint16_t>(seed), toSlg(lim)));
}

unsigned restSeedOf(const Spec& s) { return slg::restSeed(toSlg(s)); }

namespace {
Spec specFromSlg(const slg::Spec& s) { return Spec{static_cast<Kind>(static_cast<std::uint8_t>(s.kind)), s.p0, s.p1, s.p2}; }
}  // namespace

std::vector<FactoryCase> factoryCases() {
  using slg::GateOp;
  using slg::CompareMode;
  return {
      {"makeConst(0)", specFromSlg(slg::makeConst(0)), specConst(0)},
      {"makeConst(9)", specFromSlg(slg::makeConst(9)), specConst(9)},
      {"makeConst(15)", specFromSlg(slg::makeConst(15)), specConst(15)},
      {"makeSensor()", specFromSlg(slg::makeSensor()), specSensor()},
      {"makeActuator()", specFromSlg(slg::makeActuator()), specActuator()},
      {"makeNot()", specFromSlg(slg::makeNot()), specNot()},
      {"makeGate(And, true, 3)", specFromSlg(slg::makeGate(GateOp::And, true, 3)), specGate(slgt::GateOp::And, true, 3)},
      {"makeGate(Or, false, 2)", specFromSlg(slg::makeGate(GateOp::Or, false, 2)), specGate(slgt::GateOp::Or, false, 2)},
      {"makeGate(Xor, false, 8)", specFromSlg(slg::makeGate(GateOp::Xor, false, 8)), specGate(slgt::GateOp::Xor, false, 8)},
      {"makeGate(Or, true, 1)", specFromSlg(slg::makeGate(GateOp::Or, true, 1)), specNot()},
      {"makeDelay(1)", specFromSlg(slg::makeDelay(1)), specDelay(1)},
      {"makeDelay(5)", specFromSlg(slg::makeDelay(5)), specDelay(5)},
      {"makeDelay(255)", specFromSlg(slg::makeDelay(255)), specDelay(255)},
      {"makeCompare(Compare)", specFromSlg(slg::makeCompare(CompareMode::Compare)), specCompare(slgt::CompareMode::Compare)},
      {"makeCompare(Subtract)", specFromSlg(slg::makeCompare(CompareMode::Subtract)), specCompare(slgt::CompareMode::Subtract)},
      {"makeLatch()", specFromSlg(slg::makeLatch()), specLatch()},
      {"makeToggle()", specFromSlg(slg::makeToggle()), specToggle()},
      {"makeClock(8, 4)", specFromSlg(slg::makeClock(8, 4)), specClock(8, 4)},
      {"makeClock(6, 3)", specFromSlg(slg::makeClock(6, 3)), specClock(6, 3)},
      {"makeClock(4096, 4095)", specFromSlg(slg::makeClock(4096, 4095)), specClock(4096, 4095)},
  };
}

namespace {

Status buildStructure(slg::Simulator& sim, const Model& m) {
  slg::EditBatch b = sim.newBatch();
  for (const CompDef& c : m.comps) b.addWithId(c.id, toSlg(c.spec), c.seed);
  for (const Wire& w : m.wires) b.connect(toSlg(w.a), toSlg(w.b));
  return fromSlg(sim.apply(b));
}

}  // namespace

Status analyze(const Model& m, const Limits& lim, FeedbackReport& out) {
  slg::Simulator sim(toSlg(lim));
  const Status s = buildStructure(sim, m);
  if (s != Status::Ok) return s;
  const slg::FeedbackReport r = slg::analyze(sim.netlist());
  FeedbackReport f;
  f.groups = r.groups;
  for (const slg::Pin& p : r.undrivenInputs) f.undrivenInputs.push_back(fromSlg(p));
  for (const auto& net : r.multiDriverNets) {
    std::vector<Pin> pins;
    for (const slg::Pin& p : net) pins.push_back(fromSlg(p));
    f.multiDriverNets.push_back(std::move(pins));
  }
  out = std::move(f);
  return Status::Ok;
}

Status settlingBounds(const Model& m, const Limits& lim, std::vector<std::optional<std::uint32_t>>& out) {
  slg::Simulator sim(toSlg(lim));
  const Status s = buildStructure(sim, m);
  if (s != Status::Ok) return s;
  out = slg::settlingBounds(sim.netlist());
  return Status::Ok;
}

namespace {

class SimPacer final : public Pacer {
 public:
  SimPacer(std::uint32_t step, std::uint32_t max) : p_(step, max) {}
  explicit SimPacer(slg::host::StepPacer p) : p_(p) {}
  std::uint32_t advance(std::uint32_t elapsedMicros, bool paused) override { return p_.advance(elapsedMicros, paused); }
  std::uint64_t droppedSteps() const override { return p_.droppedSteps(); }
  std::uint64_t pausedMicros() const override { return p_.pausedMicros(); }
  std::uint64_t accumulatedMicros() const override { return p_.accumulatedMicros(); }
  std::uint32_t stepMicros() const override { return p_.stepMicros(); }

 private:
  slg::host::StepPacer p_;
};

slg::host::DriverOptions toSlgOptions(const DriverOptions& o) {
  slg::host::DriverOptions r;
  r.resolveTimeoutMicros = o.resolveTimeoutMicros;
  r.settleGuard = o.settleGuard;
  return r;
}

class SimDriver final : public Driver {
 public:
  SimDriver(std::unique_ptr<slg::Simulator> sim, const DriverOptions& opt)
      : options_(toSlgOptions(opt)), sim_(std::move(sim)), drv_(std::make_unique<slg::host::FrameDriver>(*sim_, options_)) {}

  Status apply(const std::vector<EditOp>& ops) override {
    slg::EditBatch b = sim_->newBatch();
    fill(b, ops);
    return fromSlg(drv_->apply(b));
  }
  void setSensorMode(ComponentId sensor, bool pulse) override {
    drv_->setSensorMode(sensor, pulse ? slg::host::SensorMode::Pulse : slg::host::SensorMode::Level);
  }
  void resync() override { drv_->resync(); }
  void offer(ComponentId sensor, Signal reading) override { drv_->offer(sensor, reading); }
  Status runCall(std::uint32_t steps, std::uint32_t elapsedMicros) override {
    return fromSlg(drv_->runCall(steps, elapsedMicros));
  }
  std::vector<DriverChange> changes() const override {
    std::vector<DriverChange> r;
    for (const slg::host::ActuatorChange& c : drv_->changes()) r.push_back(DriverChange{c.id, c.previous, c.value});
    return r;
  }
  void acknowledge(ComponentId actuator) override { drv_->acknowledge(actuator); }
  void endCall() override { drv_->endCall(); }
  std::vector<std::uint8_t> encode() const override { return drv_->encode(); }
  Status decode(const std::vector<std::uint8_t>& blob) override { return fromSlg(drv_->decode(view(blob))); }
  Tick simTick() const override { return sim_->tick(); }
  std::uint64_t simStateHash() const override { return sim_->stateHash(); }
  std::vector<std::uint8_t> simEncode() const override { return sim_->encode(); }
  std::vector<Signal> simCommands() const override {
    const auto s = sim_->commands();
    return std::vector<Signal>(s.begin(), s.end());
  }
  std::uint32_t simClamped() const override { return sim_->clampedSamples(); }
  Status simApply(const std::vector<EditOp>& ops) override {
    slg::EditBatch b = sim_->newBatch();
    fill(b, ops);
    return fromSlg(sim_->apply(b));
  }
  Status adopt(const std::vector<std::uint8_t>& simBlob) override {
    const slg::Limits limits = sim_->netlist().limits();
    auto fresh = std::make_unique<slg::Simulator>(limits, 0);
    const slg::Status s = slg::Simulator::decode(view(simBlob), limits, *fresh);
    if (s != slg::Status::Ok) return fromSlg(s);
    drv_.reset();   // the old driver points into the old simulator
    sim_ = std::move(fresh);
    drv_ = std::make_unique<slg::host::FrameDriver>(*sim_, options_);
    return Status::Ok;
  }

 private:
  slg::host::DriverOptions options_;
  std::unique_ptr<slg::Simulator> sim_;
  std::unique_ptr<slg::host::FrameDriver> drv_;
};

}  // namespace

std::unique_ptr<Pacer> makePacer(std::uint32_t stepMicros, std::uint32_t maxStepsPerCall) {
  return std::make_unique<SimPacer>(stepMicros, maxStepsPerCall);
}

std::unique_ptr<Pacer> makePacerForHost(std::uint32_t hostStepMicros, std::uint32_t maxStepsPerCall) {
  return std::make_unique<SimPacer>(slg::host::StepPacer::forHostStep(hostStepMicros, maxStepsPerCall));
}

std::uint32_t effectiveStepMicros(std::uint32_t hostStepMicros) { return slg::host::effectiveStepMicros(hostStepMicros); }

std::int64_t fullness(std::int64_t num, std::int64_t den) {
  return static_cast<std::int64_t>(slg::host::fullness(num, den));
}

std::unique_ptr<Driver> makeDriver(const Limits& lim, std::uint32_t hostStepMicros, const DriverOptions& opt) {
  return std::make_unique<SimDriver>(std::make_unique<slg::Simulator>(toSlg(lim), hostStepMicros), opt);
}

std::unique_ptr<Driver> makeDriverFromBlob(const std::vector<std::uint8_t>& simBlob, const Limits& lim,
                                           const DriverOptions& opt, Status& decodeStatus) {
  auto sim = std::make_unique<slg::Simulator>(toSlg(lim), 0);
  const slg::Status s = slg::Simulator::decode(view(simBlob), toSlg(lim), *sim);
  decodeStatus = fromSlg(s);
  if (s != slg::Status::Ok) return nullptr;
  return std::make_unique<SimDriver>(std::move(sim), opt);
}

}  // namespace bridge
}  // namespace slgt
