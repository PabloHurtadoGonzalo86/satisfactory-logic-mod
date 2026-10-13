#include "nets.hpp"

#include <algorithm>

#include "spec_rules.hpp"

namespace slg::internal {

std::size_t findSlot(ComponentSpan comps, ComponentId id) noexcept {
  const auto it = std::lower_bound(comps.begin(), comps.end(), id,
                                   [](const std::pair<ComponentId, Spec>& c, ComponentId key) { return c.first < key; });
  if (it == comps.end() || it->first != id) return kNoSlot;
  return static_cast<std::size_t>(it - comps.begin());
}

PinTable buildPinTable(ComponentSpan comps) {
  PinTable table;
  table.first.resize(comps.size() + 1);
  table.inputs.resize(comps.size());
  table.output.resize(comps.size());
  std::uint32_t next = 0;
  for (std::size_t slot = 0; slot < comps.size(); ++slot) {
    const Spec& spec = comps[slot].second;
    table.first[slot] = next;
    table.inputs[slot] = inputCount(spec);
    table.output[slot] = hasOutput(spec.kind) ? 1 : 0;
    next += static_cast<std::uint32_t>(table.inputs[slot]) + static_cast<std::uint32_t>(table.output[slot]);
  }
  table.first[comps.size()] = next;
  table.pinCount = next;
  return table;
}

Nets buildNets(const PinTable& table, ComponentSpan comps, std::span<const Wire> wires) {
  const std::size_t pins = table.pinCount;
  std::vector<std::uint32_t> parent(pins);
  std::vector<std::uint32_t> weight(pins, 1);
  for (std::size_t i = 0; i < pins; ++i) parent[i] = static_cast<std::uint32_t>(i);

  const auto root = [&parent](std::uint32_t x) noexcept {
    while (parent[x] != x) {
      parent[x] = parent[parent[x]];
      x = parent[x];
    }
    return x;
  };

  for (const Wire& wire : wires) {
    const std::size_t slotA = findSlot(comps, wire.a.component);
    const std::size_t slotB = findSlot(comps, wire.b.component);
    if (slotA == kNoSlot || slotB == kNoSlot) continue;
    std::uint32_t ra = root(table.pin(slotA, wire.a.index));
    std::uint32_t rb = root(table.pin(slotB, wire.b.index));
    if (ra == rb) continue;
    if (weight[ra] < weight[rb]) std::swap(ra, rb);
    parent[rb] = ra;
    weight[ra] += weight[rb];
  }

  Nets nets;
  nets.pinNet.assign(pins, 0);
  std::vector<std::uint32_t> netOfRoot(pins, 0xFFFFFFFFu);
  for (std::size_t i = 0; i < pins; ++i) {
    const std::uint32_t r = root(static_cast<std::uint32_t>(i));
    if (netOfRoot[r] == 0xFFFFFFFFu) {
      netOfRoot[r] = nets.netCount++;
      nets.pinsInNet.push_back(0);
    }
    nets.pinNet[i] = netOfRoot[r];
    ++nets.pinsInNet[netOfRoot[r]];
  }
  return nets;
}

}  // namespace slg::internal
