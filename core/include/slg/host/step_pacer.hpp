#pragma once

#include <cstdint>

#include "slg/host/fullness.hpp"

namespace slg::host {

inline constexpr std::uint32_t kDefaultStepMicros = 100'000;  // one "redstone tick" (02 section 7.3)
inline constexpr std::uint32_t kDefaultMaxStepsPerCall = 4;

// The step a world runs at: its hostStepMicros, or the default when the world never fixed one (0 = unset).
// The pacer and the FrameDriver's resolve timeout both read the step through this one function.
constexpr std::uint32_t effectiveStepMicros(std::uint32_t hostStepMicros) noexcept {
  return hostStepMicros == 0 ? kDefaultStepMicros : hostStepMicros;
}

// Conservation: sum(dt) = pausedMicros + (sum(steps) + droppedSteps) * stepMicros + accumulatedMicros,
// with 0 <= accumulatedMicros < stepMicros.
class StepPacer {
 public:
  StepPacer(std::uint32_t stepMicros, std::uint32_t maxStepsPerCall) noexcept;  // both clamped to >= 1

  // The pacer of a world: Simulator::hostStepMicros() goes in as it is, and 0 means "unset", not "1 us".
  static StepPacer forHostStep(std::uint32_t hostStepMicros,
                               std::uint32_t maxStepsPerCall = kDefaultMaxStepsPerCall) noexcept {
    return StepPacer(effectiveStepMicros(hostStepMicros), maxStepsPerCall);
  }

  std::uint32_t advance(std::uint32_t elapsedMicros, bool paused) noexcept;  // steps to run now

  std::uint64_t droppedSteps() const noexcept { return dropped_; }
  std::uint64_t pausedMicros() const noexcept { return pausedMicros_; }
  std::uint64_t accumulatedMicros() const noexcept { return accumulated_; }
  std::uint32_t stepMicros() const noexcept { return stepMicros_; }
  std::uint32_t maxStepsPerCall() const noexcept { return maxSteps_; }

 private:
  std::uint32_t stepMicros_;
  std::uint32_t maxSteps_;
  std::uint64_t accumulated_ = 0;
  std::uint64_t dropped_ = 0;
  std::uint64_t pausedMicros_ = 0;
};

}  // namespace slg::host
