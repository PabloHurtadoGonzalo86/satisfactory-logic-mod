#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

#include "slg/netlist.hpp"
#include "slg/types.hpp"

namespace slg::internal {

using ComponentSpan = std::span<const std::pair<ComponentId, Spec>>;

inline constexpr std::size_t kNoSlot = static_cast<std::size_t>(-1);

// Slot = position in the ascending component list.
std::size_t findSlot(ComponentSpan comps, ComponentId id) noexcept;

// Dense numbering of every pin: the inputs of a slot first, then its output pin if it has one.
struct PinTable {
  std::vector<std::uint32_t> first;  // per slot, plus a final sentinel
  std::vector<std::uint8_t> inputs;  // per slot
  std::vector<std::uint8_t> output;  // per slot, 1 if the kind has an output pin
  std::uint32_t pinCount = 0;

  std::uint32_t pin(std::size_t slot, std::uint8_t index) const noexcept {
    const std::uint32_t offset = index == kOut ? inputs[slot] : index;
    return first[slot] + offset;
  }
};

PinTable buildPinTable(ComponentSpan comps);

// Connected components of the pin graph (01 section 2.2), numbered 0..netCount-1 by lowest pin.
struct Nets {
  std::vector<std::uint32_t> pinNet;      // per pin
  std::vector<std::uint32_t> pinsInNet;   // per net
  std::uint32_t netCount = 0;
};

Nets buildNets(const PinTable& table, ComponentSpan comps, std::span<const Wire> wires);

}  // namespace slg::internal
