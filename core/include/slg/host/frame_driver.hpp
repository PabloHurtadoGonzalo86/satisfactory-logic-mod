#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "slg/host/cone_graph.hpp"
#include "slg/host/fullness.hpp"
#include "slg/host/step_pacer.hpp"
#include "slg/netlist.hpp"
#include "slg/simulator.hpp"
#include "slg/types.hpp"

namespace slg::host {

enum class SensorMode : std::uint8_t { Level, Pulse };

// ActuatorChange::previous when nothing was acknowledged (a fresh driver after a load, or after resync()). It is
// outside the Signal domain 0..15: never use `previous` as an index.
inline constexpr Signal kUnconfirmed = 0xFF;

struct ActuatorChange {
  ComponentId id;
  Signal previous;  // last acknowledged, or kUnconfirmed
  Signal value;
};

struct DriverOptions {
  // Counted in executed steps, never in wall time: ceil(resolveTimeoutMicros / step) with step =
  // effectiveStepMicros(sim.hostStepMicros()) (02 section 7.4). 0 resolves every sensor in the first call.
  std::uint32_t resolveTimeoutMicros = 5'000'000;
  bool settleGuard = true;
};

// The whole policy of one host call: one sample, n steps, final values out (02 section 7). The driver is a pure
// function of its calls (offers, steps) and of the edits it is given; it reads no clock.
// A driver is bound to one Simulator for its whole life: to adopt a simulator decoded from a save, build a new
// FrameDriver on it and decode() the driver blob (02 section 7.6).
class FrameDriver {
 public:
  explicit FrameDriver(Simulator& sim, DriverOptions options = {});

  Status apply(const EditBatch& batch);  // WrongPhase between runCall and endCall
  void setSensorMode(ComponentId sensor, SensorMode mode);  // a change of mode forgets the retained reading
  void resync();                                    // forget acknowledged values
  void offer(ComponentId sensor, Signal reading);   // only for successful reads; above 15 saturates to 15
  // ONE sample, `steps` steps. elapsedMicros is informational: no decision depends on it. runCall(0, ...) runs no
  // step and credits no timeout; it retains Level readings and drops Pulse readings, but it still opens the call
  // (changes() may list pending deliveries and endCall() is required).
  Status runCall(std::uint32_t steps, std::uint32_t elapsedMicros);
  std::span<const ActuatorChange> changes() const noexcept { return changes_; }  // final values only
  void acknowledge(ComponentId actuator);           // the world accepted the write
  void endCall();

  // Blob v1 (02 section 7.5). decode() accepts only the blob of the simulator this driver is bound to, as it is now.
  std::vector<std::uint8_t> encode() const;
  Status decode(std::span<const std::uint8_t> bytes);

 private:
  struct SensorState {
    ComponentId id = 0;
    SensorMode mode = SensorMode::Level;
    Signal retained = 0;  // last good reading, 0..15; always 0 for a Pulse
    bool pending = false;
    Signal pendingValue = 0;  // as offered: the one step that takes it is the one that counts it when above 15
    bool unresolved = true;
    std::uint64_t waitedSteps = 0;
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
  void refreshStructure();
  void syncIfStale();
  SensorState* findSensor(ComponentId id) noexcept;
  std::uint64_t timeoutSteps() const noexcept;
  // hold = max(hold, L_a) for every actuator marked in `marks` (one flag per graph slot) whose bound is defined.
  void armHold(const std::vector<std::uint8_t>& marks);

  Simulator* sim_;
  DriverOptions options_;
  std::vector<SensorState> sensors_;
  std::vector<ActuatorState> actuators_;
  std::vector<ActuatorChange> changes_;
  std::vector<Signal> samples_;
  std::uint64_t seenStructure_ = 0;
  detail::ConeGraph graph_;
  std::vector<std::optional<std::uint32_t>> bounds_;  // L_a per actuator, ascending id; empty with the guard off
  bool inCall_ = false;
};

}  // namespace slg::host
