#pragma once

#include <cstdint>

#include "slg/host/fullness.hpp"

namespace slg::host {

// Conservation: sum(dt) = pausedMicros + (sum(steps) + droppedSteps) * stepMicros + accumulatedMicros,
// with 0 <= accumulatedMicros < stepMicros.
class StepPacer {
 public:
  StepPacer(std::uint32_t stepMicros, std::uint32_t maxStepsPerCall) noexcept;  // both clamped to >= 1

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
