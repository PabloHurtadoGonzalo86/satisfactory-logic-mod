#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "slg/host/fullness.hpp"
#include "slg/netlist.hpp"
#include "slg/simulator.hpp"
#include "slg/types.hpp"

namespace slg::host {

enum class SensorMode : std::uint8_t { Level, Pulse };

inline constexpr Signal kUnconfirmed = 0xFF;  // ActuatorChange::previous when nothing was acknowledged

struct ActuatorChange {
  ComponentId id;
  Signal previous;  // last acknowledged, or kUnconfirmed
  Signal value;
};

struct DriverOptions {
  std::uint32_t resolveTimeoutMicros = 5'000'000;
  bool settleGuard = true;
};

// The whole policy of one host call: one sample, n steps, final values out (02 section 7).
class FrameDriver {
 public:
  explicit FrameDriver(Simulator& sim, DriverOptions options = {});

  Status apply(const EditBatch& batch);  // WrongPhase between runCall and endCall
  void setSensorMode(ComponentId sensor, SensorMode mode);
  void resync();                                    // forget acknowledged values
  void offer(ComponentId sensor, Signal reading);   // only for successful reads
  Status runCall(std::uint32_t steps, std::uint32_t elapsedMicros);  // ONE sample, n steps
  std::span<const ActuatorChange> changes() const noexcept { return changes_; }  // final values only
  void acknowledge(ComponentId actuator);           // the world accepted the write
  void endCall();

  std::vector<std::uint8_t> encode() const;
  Status decode(std::span<const std::uint8_t> bytes);

 private:
  struct SensorState {
    ComponentId id = 0;
    SensorMode mode = SensorMode::Level;
    Signal retained = 0;  // last good reading
    bool pending = false;
    Signal pendingValue = 0;
    bool unresolved = true;
    std::uint64_t waitedMicros = 0;
  };
  struct ActuatorState {
    ComponentId id = 0;
    Signal confirmed = kUnconfirmed;
    std::uint32_t hold = 0;  // steps still to run before this actuator may deliver
  };

  // Sensors and actuators as the simulator has them now, keeping what the driver already knows of each.
  std::vector<SensorState> currentSensors() const;
  std::vector<ActuatorState> currentActuators() const;
  void reconcile();
  void syncIfStale();
  SensorState* findSensor(ComponentId id) noexcept;

  Simulator* sim_;
  DriverOptions options_;
  std::vector<SensorState> sensors_;
  std::vector<ActuatorState> actuators_;
  std::vector<ActuatorChange> changes_;
  std::vector<Signal> samples_;
  std::uint64_t seenStructure_ = 0;
  bool inCall_ = false;
};

}  // namespace slg::host
