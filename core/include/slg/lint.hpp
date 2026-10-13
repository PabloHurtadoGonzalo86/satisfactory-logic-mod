#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "slg/netlist.hpp"
#include "slg/types.hpp"

namespace slg {

struct FeedbackReport {
  std::vector<std::vector<ComponentId>> groups;  // SCCs, ascending ids, ordered by smallest id
  std::vector<Pin> undrivenInputs;               // read 0
  std::vector<std::vector<Pin>> multiDriverNets;  // every pin of the net, ascending; merged by max
};

// Iterative Tarjan over the net-induced graph, self-edges included (01 section 5).
FeedbackReport analyze(const Netlist&);

// L_a per actuator, in ascending actuator id; empty when the cone is cyclic or holds memory (01 section 8).
std::vector<std::optional<std::uint32_t>> settlingBounds(const Netlist&);

}  // namespace slg
