#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "slg/netlist.hpp"
#include "slg/simulator.hpp"
#include "slg/types.hpp"

// Implementation detail of FrameDriver, public only so that the driver can hold one by value (the same reason
// slg::detail::Slot sits in simulator.hpp).
namespace slg::host::detail {

inline constexpr std::size_t kNoSlot = static_cast<std::size_t>(-1);

// The net-induced graph of 01 section 5: one node per component (slot = position in the ascending id list) and one
// hub per net, so that a net with D drivers and R readers costs D + R edges. The same graph settlingBounds walks.
class ConeGraph {
 public:
  ConeGraph() = default;
  explicit ConeGraph(const Netlist& netlist);

  std::size_t slotOf(ComponentId id) const noexcept;  // kNoSlot when absent

  // marks[slot] = 1 for every component that is in `sources` or has one of them among its ancestors, so an
  // actuator is marked exactly when its cone (itself plus its ancestors) holds a source.
  std::vector<std::uint8_t> descendantsOf(std::span<const std::size_t> sources) const;

 private:
  std::vector<ComponentId> ids_;
  std::vector<std::uint32_t> outNet_;      // per slot; 0xFFFFFFFF when the component has no output pin
  std::vector<std::uint32_t> readerStart_; // per net, CSR offsets into readers_, plus a final sentinel
  std::vector<std::uint32_t> readers_;     // slot of every component with an input pin on the net
};

struct ComponentSignature {
  ComponentId id = 0;
  std::uint64_t hash = 0;
};

// Per component, by ascending id: a hash of everything that decides what the component computes from the next tick
// on, namely its id, spec, state bytes and, for each input pin, the ids of the output pins that drive its net. Two
// signatures of the same id differ exactly when the component was created, replaced (even with the same spec and
// another seed) or rewired between the two readings.
std::vector<ComponentSignature> signaturesOf(const Simulator& sim);

}  // namespace slg::host::detail
