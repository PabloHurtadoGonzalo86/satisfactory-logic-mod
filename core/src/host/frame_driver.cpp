#include "slg/host/frame_driver.hpp"

#include <algorithm>
#include <utility>

#include "../internal/bytes.hpp"
#include "../internal/fnv.hpp"
#include "slg/lint.hpp"

namespace slg::host {
namespace {

constexpr std::uint16_t kDriverFormatVersion = 1;
// magic, u16 version, u64 tick, u64 stateHash, u64 structureHash, u32 sensors, u32 actuators
constexpr std::size_t kDriverHeaderBytes = 38;
constexpr std::size_t kTailBytes = 8;
constexpr std::uint64_t kSensorRecordBytes = 6;    // u32 id, u8 retained, u8 mode
constexpr std::uint64_t kActuatorRecordBytes = 9;  // u32 id, u8 confirmed, u32 hold
constexpr std::uint8_t kDriverMagic[4] = {0x53, 0x4C, 0x47, 0x44};  // "SLGD"
constexpr Signal kMaxSignal = 15;

}  // namespace

FrameDriver::FrameDriver(Simulator& sim, DriverOptions options) : sim_(&sim), options_(options) { reconcile(); }

std::vector<FrameDriver::SensorState> FrameDriver::currentSensors() const {
  std::vector<SensorState> sensors;
  sensors.reserve(sim_->sensorIds().size());
  for (const ComponentId id : sim_->sensorIds()) {
    SensorState state;
    state.id = id;
    const auto known = std::lower_bound(sensors_.begin(), sensors_.end(), id,
                                        [](const SensorState& s, ComponentId key) { return s.id < key; });
    if (known != sensors_.end() && known->id == id) state = *known;
    sensors.push_back(state);
  }
  return sensors;
}

std::vector<FrameDriver::ActuatorState> FrameDriver::currentActuators() const {
  const auto ids = sim_->actuatorIds();
  const auto commands = sim_->commands();
  std::vector<ActuatorState> actuators;
  actuators.reserve(ids.size());
  for (std::size_t i = 0; i < ids.size(); ++i) {
    ActuatorState state;
    state.id = ids[i];
    state.confirmed = commands[i];  // a created actuator is never written to (02 section 7.5)
    const auto known = std::lower_bound(actuators_.begin(), actuators_.end(), ids[i],
                                        [](const ActuatorState& a, ComponentId key) { return a.id < key; });
    if (known != actuators_.end() && known->id == ids[i]) state = *known;
    actuators.push_back(state);
  }
  return actuators;
}

void FrameDriver::refreshStructure() {
  seenStructure_ = sim_->netlist().structureHash();
  graph_ = detail::ConeGraph(sim_->netlist());
  bounds_ = options_.settleGuard ? settlingBounds(sim_->netlist()) : std::vector<std::optional<std::uint32_t>>{};
}

void FrameDriver::reconcile() {
  sensors_ = currentSensors();
  actuators_ = currentActuators();
  refreshStructure();
}

void FrameDriver::syncIfStale() {
  if (sim_->netlist().structureHash() != seenStructure_) reconcile();
}

FrameDriver::SensorState* FrameDriver::findSensor(ComponentId id) noexcept {
  const auto it = std::lower_bound(sensors_.begin(), sensors_.end(), id,
                                   [](const SensorState& s, ComponentId key) { return s.id < key; });
  return (it != sensors_.end() && it->id == id) ? &*it : nullptr;
}

std::uint64_t FrameDriver::timeoutSteps() const noexcept {
  const std::uint64_t step = effectiveStepMicros(sim_->hostStepMicros());
  return (std::uint64_t{options_.resolveTimeoutMicros} + step - 1) / step;
}

void FrameDriver::armHold(const std::vector<std::uint8_t>& marks) {
  for (std::size_t i = 0; i < actuators_.size() && i < bounds_.size(); ++i) {
    if (!bounds_[i].has_value()) continue;
    const std::size_t slot = graph_.slotOf(actuators_[i].id);
    if (slot == detail::kNoSlot || marks[slot] == 0) continue;
    actuators_[i].hold = std::max(actuators_[i].hold, *bounds_[i]);
  }
}

Status FrameDriver::apply(const EditBatch& batch) {
  if (inCall_) return Status::WrongPhase;
  syncIfStale();
  const bool guard = options_.settleGuard && !batch.empty();
  std::vector<detail::ComponentSignature> before;
  if (guard) before = detail::signaturesOf(*sim_);

  const Status status = sim_->apply(batch);
  if (status != Status::Ok || batch.empty()) return status;
  reconcile();
  if (!guard) return Status::Ok;

  // Only an actuator whose cone changed has a new transient to hide: its cone holds a component that is new, was
  // replaced or reset, or has other drivers than before.
  const std::vector<detail::ComponentSignature> after = detail::signaturesOf(*sim_);
  std::vector<std::size_t> changed;
  std::size_t b = 0;
  for (std::size_t slot = 0; slot < after.size(); ++slot) {
    while (b < before.size() && before[b].id < after[slot].id) ++b;
    const bool same = b < before.size() && before[b].id == after[slot].id && before[b].hash == after[slot].hash;
    if (!same) changed.push_back(slot);
  }
  if (!changed.empty()) armHold(graph_.descendantsOf(changed));
  return Status::Ok;
}

void FrameDriver::setSensorMode(ComponentId sensor, SensorMode mode) {
  syncIfStale();
  SensorState* state = findSensor(sensor);
  if (state == nullptr || state->mode == mode) return;
  state->mode = mode;
  state->retained = 0;  // a Pulse carries no value over, and a Level that was a Pulse has none to retain
}

void FrameDriver::resync() {
  syncIfStale();
  for (ActuatorState& actuator : actuators_) actuator.confirmed = kUnconfirmed;
}

void FrameDriver::offer(ComponentId sensor, Signal reading) {
  SensorState* state = findSensor(sensor);
  if (state == nullptr) {
    syncIfStale();
    state = findSensor(sensor);
  }
  if (state == nullptr) return;
  state->pending = true;
  state->pendingValue = reading;
}

Status FrameDriver::runCall(std::uint32_t steps, std::uint32_t /*elapsedMicros*/) {
  if (inCall_) return Status::WrongPhase;
  syncIfStale();

  // 1. Readings and the timeout, in executed steps. A sensor resolves with its first reading or when it has waited
  // ceil(resolveTimeoutMicros / step) steps without one.
  const std::uint64_t limit = timeoutSteps();
  std::vector<std::size_t> resolved;
  for (SensorState& sensor : sensors_) {
    bool resolves = false;
    if (sensor.pending) {
      if (sensor.mode == SensorMode::Level) sensor.retained = std::min(sensor.pendingValue, kMaxSignal);
      resolves = sensor.unresolved;
    } else if (sensor.unresolved) {
      sensor.waitedSteps += steps;
      resolves = sensor.waitedSteps >= limit;
    }
    if (resolves) {
      sensor.unresolved = false;
      resolved.push_back(graph_.slotOf(sensor.id));
    }
  }
  // 2. A sensor that stops being unresolved moves its cone from the default it fed to the real value: that
  // transient starts with the first step below, so the hold starts here.
  if (options_.settleGuard && !resolved.empty()) armHold(graph_.descendantsOf(resolved));

  // 3. The steps: one sample for the whole call. A reading above 15 enters the simulator once, in the first step
  // that uses it, so that clampedSamples() counts the reading and not the steps it stays in force.
  samples_.assign(sensors_.size(), 0);
  for (std::uint32_t step = 0; step < steps; ++step) {
    for (std::size_t i = 0; i < sensors_.size(); ++i) {
      const SensorState& sensor = sensors_[i];
      const bool first = step == 0 && sensor.pending;
      if (sensor.mode == SensorMode::Level) {
        samples_[i] = (first && sensor.pendingValue > kMaxSignal) ? sensor.pendingValue : sensor.retained;
      } else {
        samples_[i] = first ? sensor.pendingValue : Signal{0};
      }
    }
    const Status status = sim_->step(samples_);
    if (status != Status::Ok) return status;
  }
  for (SensorState& sensor : sensors_) sensor.pending = false;  // a Pulse reading that met no step is dropped

  // 4. What the world is told: final values only, and not for an actuator whose cone still holds an unresolved
  // sensor or that is still settling.
  std::vector<std::size_t> waiting;
  for (const SensorState& sensor : sensors_) {
    if (sensor.unresolved) waiting.push_back(graph_.slotOf(sensor.id));
  }
  const std::vector<std::uint8_t> withheld = waiting.empty() ? std::vector<std::uint8_t>{} : graph_.descendantsOf(waiting);

  changes_.clear();
  const auto commands = sim_->commands();
  for (std::size_t i = 0; i < actuators_.size(); ++i) {
    ActuatorState& actuator = actuators_[i];
    actuator.hold = actuator.hold > steps ? actuator.hold - steps : 0;
    if (!withheld.empty() && withheld[graph_.slotOf(actuator.id)] != 0) continue;
    if (options_.settleGuard && actuator.hold != 0) continue;
    if (actuator.confirmed != commands[i]) changes_.push_back(ActuatorChange{actuator.id, actuator.confirmed, commands[i]});
  }
  inCall_ = true;
  return Status::Ok;
}

void FrameDriver::acknowledge(ComponentId actuator) {
  if (!inCall_) return;
  const auto change = std::lower_bound(changes_.begin(), changes_.end(), actuator,
                                       [](const ActuatorChange& c, ComponentId key) { return c.id < key; });
  if (change == changes_.end() || change->id != actuator) return;
  const auto state = std::lower_bound(actuators_.begin(), actuators_.end(), actuator,
                                      [](const ActuatorState& a, ComponentId key) { return a.id < key; });
  if (state != actuators_.end() && state->id == actuator) state->confirmed = change->value;
}

void FrameDriver::endCall() {
  inCall_ = false;
  changes_.clear();
}

std::vector<std::uint8_t> FrameDriver::encode() const {
  const std::vector<SensorState> sensors = currentSensors();
  const std::vector<ActuatorState> actuators = currentActuators();

  std::vector<std::uint8_t> bytes;
  bytes.reserve(kDriverHeaderBytes + kTailBytes + sensors.size() * kSensorRecordBytes +
                actuators.size() * kActuatorRecordBytes);
  internal::ByteWriter out(bytes);
  for (const std::uint8_t m : kDriverMagic) out.u8(m);
  out.u16(kDriverFormatVersion);
  // The blob belongs to the simulator state it was saved with: a pair taken at different moments is refused.
  out.u64(sim_->tick());
  out.u64(sim_->stateHash());
  out.u64(sim_->netlist().structureHash());
  out.u32(static_cast<std::uint32_t>(sensors.size()));
  out.u32(static_cast<std::uint32_t>(actuators.size()));
  for (const SensorState& sensor : sensors) {
    out.u32(sensor.id);
    out.u8(sensor.retained);
    out.u8(static_cast<std::uint8_t>(sensor.mode));
  }
  for (const ActuatorState& actuator : actuators) {
    out.u32(actuator.id);
    out.u8(actuator.confirmed);
    out.u32(actuator.hold);
  }
  internal::Fnv64 tail;
  for (const std::uint8_t b : bytes) tail.byte(b);
  out.u64(tail.value());
  return bytes;
}

Status FrameDriver::decode(std::span<const std::uint8_t> bytes) {
  using internal::ByteReader;
  if (inCall_) return Status::WrongPhase;

  if (bytes.size() < 4) return Status::FormatCorrupt;
  for (std::size_t i = 0; i < 4; ++i) {
    if (bytes[i] != kDriverMagic[i]) return Status::FormatCorrupt;
  }
  if (bytes.size() < 6) return Status::FormatCorrupt;
  {
    std::uint16_t version = 0;
    ByteReader versionReader(bytes.subspan(4));
    versionReader.u16(version);
    if (version > kDriverFormatVersion) return Status::FormatTooNew;
    if (version != kDriverFormatVersion) return Status::FormatCorrupt;
  }
  if (bytes.size() < kDriverHeaderBytes + kTailBytes) return Status::FormatCorrupt;

  const auto body = bytes.first(bytes.size() - kTailBytes);
  {
    internal::Fnv64 check;
    for (const std::uint8_t b : body) check.byte(b);
    std::uint64_t stored = 0;
    ByteReader tailReader(bytes.last(kTailBytes));
    tailReader.u64(stored);
    if (stored != check.value()) return Status::FormatCorrupt;
  }

  ByteReader r(body.subspan(6));
  std::uint64_t tick = 0;
  std::uint64_t stateHash = 0;
  std::uint64_t structureHash = 0;
  std::uint32_t sensorCount = 0;
  std::uint32_t actuatorCount = 0;
  if (!r.u64(tick) || !r.u64(stateHash) || !r.u64(structureHash) || !r.u32(sensorCount) || !r.u32(actuatorCount)) {
    return Status::FormatCorrupt;
  }
  if (sensorCount * kSensorRecordBytes + actuatorCount * kActuatorRecordBytes != r.remaining()) {
    return Status::FormatCorrupt;
  }
  // The blob is tied to the simulator it was saved with: another set of ids, or the same ids at another tick, in
  // another state or with other wires, is a pair taken at different moments.
  if (sensorCount != sim_->sensorIds().size() || actuatorCount != sim_->actuatorIds().size()) {
    return Status::FormatCorrupt;
  }
  if (tick != sim_->tick() || stateHash != sim_->stateHash() || structureHash != sim_->netlist().structureHash()) {
    return Status::FormatCorrupt;
  }

  std::vector<SensorState> sensors;
  sensors.reserve(sensorCount);
  for (std::uint32_t i = 0; i < sensorCount; ++i) {
    std::uint32_t id = 0;
    std::uint8_t retained = 0;
    std::uint8_t mode = 0;
    if (!r.u32(id) || !r.u8(retained) || !r.u8(mode)) return Status::FormatCorrupt;
    if (id != sim_->sensorIds()[i] || mode > static_cast<std::uint8_t>(SensorMode::Pulse)) {
      return Status::FormatCorrupt;
    }
    SensorState state;
    state.id = id;
    state.mode = static_cast<SensorMode>(mode);
    // A Level holds a reading in 0..15; a Pulse holds none.
    if (retained > (state.mode == SensorMode::Pulse ? Signal{0} : kMaxSignal)) return Status::FormatCorrupt;
    state.retained = retained;
    sensors.push_back(state);
  }

  // A hold is at most the longest path any cone of this simulator can have (components plus Delay slots), whatever
  // the bound of the cone is today: an edit that shortens a cone leaves the hold it armed before.
  const Limits& limits = sim_->netlist().limits();
  const std::uint64_t holdCap = std::uint64_t{limits.maxComponents} + limits.maxDelayCells;
  std::vector<ActuatorState> actuators;
  actuators.reserve(actuatorCount);
  for (std::uint32_t i = 0; i < actuatorCount; ++i) {
    std::uint32_t id = 0;
    std::uint8_t confirmed = 0;
    std::uint32_t hold = 0;
    if (!r.u32(id) || !r.u8(confirmed) || !r.u32(hold)) return Status::FormatCorrupt;
    if (id != sim_->actuatorIds()[i] || (confirmed > kMaxSignal && confirmed != kUnconfirmed) || hold > holdCap) {
      return Status::FormatCorrupt;
    }
    ActuatorState state;
    state.id = id;
    state.confirmed = confirmed;
    state.hold = hold;
    actuators.push_back(state);
  }

  sensors_ = std::move(sensors);
  actuators_ = std::move(actuators);
  changes_.clear();
  refreshStructure();
  return Status::Ok;
}

}  // namespace slg::host
