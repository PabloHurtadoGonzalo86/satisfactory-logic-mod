// The leveled evaluator of 01 section 8, written independently of any simulator: levels l(c) and the
// zero-delay fixed point z for netlists whose net-induced graph is acyclic and made only of Const,
// Sensor, Gate, Compare, Delay and Actuator.
#pragma once

#include <map>
#include <vector>

#include "netutil.hpp"
#include "tmodel.hpp"

namespace slgt {

struct ConeInfo {
  std::vector<ComponentId> order;           // drivers before readers
  std::map<ComponentId, unsigned> level;    // l(c) = w(c) + max l(c')
  unsigned maxLevel = 0;                    // L
  NetAnalysis nets;
};

unsigned weightOf(const Spec& s) noexcept;   // Const, Sensor, Clock 0; Delay d; the rest 1
// false when the model has a cycle in the induced graph or a kind outside the theorem
bool analyzeCone(const Model& m, ConeInfo& out);

// z of every component under constant sensor values: the state bytes the component holds at the fixed
// point (empty for Const and Sensor; every Delay slot equal to its input). Values above 15 saturate.
std::map<ComponentId, std::vector<std::uint8_t>> fixedPoint(const Model& m, const ConeInfo& ci,
                                                            const std::map<ComponentId, Signal>& sensorValues);

}  // namespace slgt
