#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "slg/netlist.hpp"
#include "slg/types.hpp"

namespace slg::detail {

// Per-component evaluation record, rebuilt whenever the structure changes.
struct Slot {
  Kind kind = Kind::Const;
  std::uint8_t mode = 0;  // GateOp or CompareMode
  std::uint8_t invert = 0;
  std::uint8_t inputs = 0;
  std::uint16_t p0 = 0;
  std::uint16_t p1 = 0;
  std::uint32_t pinFirst = 0;
  std::uint32_t stateOffset = 0;
  std::uint32_t stateSize = 0;
};

}  // namespace slg::detail

namespace slg {

class Simulator {  // single thread
 public:
  explicit Simulator(Limits limits = {}, std::uint32_t hostStepMicros = 0);  // empty, tick 0

  EditBatch newBatch() const;                  // captures nextId
  Status apply(const EditBatch&);              // only between ticks
  Status step(std::span<const Signal> samples);  // ONE tick; samples[i] feeds sensorIds()[i]

  const Netlist& netlist() const noexcept { return netlist_; }
  Tick tick() const noexcept { return tick_; }
  std::uint32_t hostStepMicros() const noexcept { return hostStepMicros_; }  // opaque to F; saved with the world
  std::span<const ComponentId> sensorIds() const noexcept { return sensorIds_; }
  std::span<const ComponentId> actuatorIds() const noexcept { return actuatorIds_; }
  std::span<const Signal> commands() const noexcept { return commands_; }  // o of each actuator in X_tick
  std::uint32_t clampedSamples() const noexcept { return clamped_; }       // diagnostic; in no hash
  std::uint64_t stateHash() const noexcept;

  std::vector<std::uint8_t> encode() const;
  static Status decode(std::span<const std::uint8_t> bytes, const Limits& limits, Simulator& out);

 private:
  void rebuildPlan();
  void refreshCommands() noexcept;

  Netlist netlist_;
  std::uint32_t hostStepMicros_ = 0;
  Tick tick_ = 0;
  std::uint32_t clamped_ = 0;

  std::vector<std::uint8_t> state_;  // X_tick, canonical bytes (02 section 6.1) in id order
  std::vector<std::uint8_t> next_;   // write side of the two-buffer tick
  std::vector<detail::Slot> slots_;
  std::vector<std::uint32_t> pinNet_;     // dense net id per pin
  std::vector<Signal> netValue_;          // scratch, one value per net
  std::vector<Signal> sensorOut_;         // scratch, one value per slot
  std::vector<ComponentId> sensorIds_;
  std::vector<ComponentId> actuatorIds_;
  std::vector<std::uint32_t> sensorSlot_;
  std::vector<std::uint32_t> actuatorSlot_;
  std::vector<Signal> commands_;
};

}  // namespace slg
