#include "slg/host/frame_driver.hpp"

#include <algorithm>

#include "../internal/bytes.hpp"
#include "../internal/fnv.hpp"
#include "slg/lint.hpp"

namespace slg::host {
namespace {

constexpr std::uint16_t kDriverFormatVersion = 1;
constexpr std::size_t kDriverHeaderBytes = 14;  // magic, u16 version, u32 sensors, u32 actuators
constexpr std::size_t kTailBytes = 8;
constexpr std::uint64_t kSensorRecordBytes = 6;    // u32 id, u8 retained, u8 mode
constexpr std::uint64_t kActuatorRecordBytes = 5;  // u32 id, u8 confirmed
constexpr std::uint8_t kDriverMagic[4] = {0x53, 0x4C, 0x47, 0x44};  // "SLGD"

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

void FrameDriver::reconcile() {
  sensors_ = currentSensors();
  actuators_ = currentActuators();
  seenStructure_ = sim_->netlist().structureHash();
}

void FrameDriver::syncIfStale() {
  if (sim_->netlist().structureHash() != seenStructure_) reconcile();
}

FrameDriver::SensorState* FrameDriver::findSensor(ComponentId id) noexcept {
  const auto it = std::lower_bound(sensors_.begin(), sensors_.end(), id,
                                   [](const SensorState& s, ComponentId key) { return s.id < key; });
  return (it != sensors_.end() && it->id == id) ? &*it : nullptr;
}

Status FrameDriver::apply(const EditBatch& batch) {
  if (inCall_) return Status::WrongPhase;
  syncIfStale();
  const Status status = sim_->apply(batch);
  if (status != Status::Ok || batch.empty()) return status;

  reconcile();
  const auto bounds = settlingBounds(sim_->netlist());
  for (std::size_t i = 0; i < actuators_.size() && i < bounds.size(); ++i) {
    actuators_[i].hold = (options_.settleGuard && bounds[i].has_value()) ? *bounds[i] : 0;
  }
  return Status::Ok;
}

void FrameDriver::setSensorMode(ComponentId sensor, SensorMode mode) {
  syncIfStale();
  if (SensorState* state = findSensor(sensor)) state->mode = mode;
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

Status FrameDriver::runCall(std::uint32_t steps, std::uint32_t elapsedMicros) {
  if (inCall_) return Status::WrongPhase;
  syncIfStale();

  for (SensorState& sensor : sensors_) {
    if (sensor.pending) {
      sensor.retained = sensor.pendingValue;
      sensor.unresolved = false;
    } else if (sensor.unresolved) {
      sensor.waitedMicros += elapsedMicros;
      if (sensor.waitedMicros >= options_.resolveTimeoutMicros) sensor.unresolved = false;
    }
  }

  samples_.assign(sensors_.size(), 0);
  for (std::uint32_t step = 0; step < steps; ++step) {
    for (std::size_t i = 0; i < sensors_.size(); ++i) {
      const SensorState& sensor = sensors_[i];
      if (sensor.mode == SensorMode::Level) {
        samples_[i] = sensor.retained;
      } else {
        samples_[i] = (step == 0 && sensor.pending) ? sensor.pendingValue : Signal{0};
      }
    }
    const Status status = sim_->step(samples_);
    if (status != Status::Ok) return status;
  }
  if (steps > 0) {
    for (SensorState& sensor : sensors_) sensor.pending = false;
  }

  bool anyUnresolved = false;
  for (const SensorState& sensor : sensors_) anyUnresolved = anyUnresolved || sensor.unresolved;

  changes_.clear();
  const auto commands = sim_->commands();
  for (std::size_t i = 0; i < actuators_.size(); ++i) {
    ActuatorState& actuator = actuators_[i];
    actuator.hold = actuator.hold > steps ? actuator.hold - steps : 0;
    if (anyUnresolved || actuator.hold != 0) continue;
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
  out.u32(static_cast<std::uint32_t>(sensors.size()));
  out.u32(static_cast<std::uint32_t>(actuators.size()));
  for (const SensorState& sensor : sensors) {
    out.u32(sensor.id);
    out.u8(sensor.mode == SensorMode::Pulse ? Signal{0} : sensor.retained);  // a pulse never carries a value over
    out.u8(static_cast<std::uint8_t>(sensor.mode));
  }
  for (const ActuatorState& actuator : actuators) {
    out.u32(actuator.id);
    out.u8(actuator.confirmed);
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
  std::uint32_t sensorCount = 0;
  std::uint32_t actuatorCount = 0;
  if (!r.u32(sensorCount) || !r.u32(actuatorCount)) return Status::FormatCorrupt;
  if (sensorCount * kSensorRecordBytes + actuatorCount * kActuatorRecordBytes != r.remaining()) {
    return Status::FormatCorrupt;
  }
  // The blob is tied to the simulator it was saved with: a different set of ids is a mismatched pair.
  if (sensorCount != sim_->sensorIds().size() || actuatorCount != sim_->actuatorIds().size()) {
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
    if (state.mode == SensorMode::Pulse && retained != 0) return Status::FormatCorrupt;
    state.retained = retained;
    sensors.push_back(state);
  }

  std::vector<ActuatorState> actuators;
  actuators.reserve(actuatorCount);
  for (std::uint32_t i = 0; i < actuatorCount; ++i) {
    std::uint32_t id = 0;
    std::uint8_t confirmed = 0;
    if (!r.u32(id) || !r.u8(confirmed)) return Status::FormatCorrupt;
    if (id != sim_->actuatorIds()[i] || (confirmed > 15 && confirmed != kUnconfirmed)) return Status::FormatCorrupt;
    ActuatorState state;
    state.id = id;
    state.confirmed = confirmed;
    actuators.push_back(state);
  }

  sensors_ = std::move(sensors);
  actuators_ = std::move(actuators);
  changes_.clear();
  seenStructure_ = sim_->netlist().structureHash();
  return Status::Ok;
}

}  // namespace slg::host
