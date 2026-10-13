#include "slg/host/step_pacer.hpp"

namespace slg::host {

StepPacer::StepPacer(std::uint32_t stepMicros, std::uint32_t maxStepsPerCall) noexcept
    : stepMicros_(stepMicros == 0 ? 1 : stepMicros), maxSteps_(maxStepsPerCall == 0 ? 1 : maxStepsPerCall) {}

std::uint32_t StepPacer::advance(std::uint32_t elapsedMicros, bool paused) noexcept {
  if (paused) {
    pausedMicros_ += elapsedMicros;
    return 0;
  }
  accumulated_ += elapsedMicros;
  std::uint64_t steps = accumulated_ / stepMicros_;
  accumulated_ %= stepMicros_;
  // The backlog is discarded, not repaid: logical time stretches under load and never spirals.
  if (steps > maxSteps_) {
    dropped_ += steps - maxSteps_;
    steps = maxSteps_;
  }
  return static_cast<std::uint32_t>(steps);
}

}  // namespace slg::host
