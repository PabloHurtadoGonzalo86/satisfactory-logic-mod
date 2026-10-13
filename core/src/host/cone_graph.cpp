#include "slg/host/cone_graph.hpp"

#include <algorithm>

#include "../internal/fnv.hpp"
#include "../internal/nets.hpp"
#include "../internal/spec_rules.hpp"

namespace slg::host::detail {
namespace {

constexpr std::uint32_t kNone = 0xFFFFFFFFu;
constexpr std::size_t kSimulatorHeaderBytes = 32;    // 02 section 6.2
constexpr std::size_t kSimulatorRecordBytes = 11;    // id, kind, p0..p2; the state follows

}  // namespace

ConeGraph::ConeGraph(const Netlist& netlist) {
  const auto comps = netlist.components();
  const internal::PinTable table = internal::buildPinTable(comps);
  const internal::Nets nets = internal::buildNets(table, comps, netlist.wires());

  ids_.reserve(comps.size());
  outNet_.assign(comps.size(), kNone);
  readerStart_.assign(static_cast<std::size_t>(nets.netCount) + 1, 0);
  for (std::size_t slot = 0; slot < comps.size(); ++slot) {
    ids_.push_back(comps[slot].first);
    if (table.output[slot] != 0) outNet_[slot] = nets.pinNet[table.pin(slot, kOut)];
    for (std::uint8_t j = 0; j < table.inputs[slot]; ++j) ++readerStart_[nets.pinNet[table.pin(slot, j)] + 1];
  }
  for (std::size_t net = 0; net < nets.netCount; ++net) readerStart_[net + 1] += readerStart_[net];

  readers_.resize(readerStart_[nets.netCount]);
  std::vector<std::uint32_t> cursor(readerStart_.begin(), readerStart_.end() - 1);
  for (std::size_t slot = 0; slot < comps.size(); ++slot) {
    for (std::uint8_t j = 0; j < table.inputs[slot]; ++j) {
      readers_[cursor[nets.pinNet[table.pin(slot, j)]]++] = static_cast<std::uint32_t>(slot);
    }
  }
}

std::size_t ConeGraph::slotOf(ComponentId id) const noexcept {
  const auto it = std::lower_bound(ids_.begin(), ids_.end(), id);
  if (it == ids_.end() || *it != id) return kNoSlot;
  return static_cast<std::size_t>(it - ids_.begin());
}

std::vector<std::uint8_t> ConeGraph::descendantsOf(std::span<const std::size_t> sources) const {
  std::vector<std::uint8_t> marks(ids_.size(), 0);
  std::vector<std::uint8_t> netDone(readerStart_.empty() ? 0 : readerStart_.size() - 1, 0);
  std::vector<std::size_t> pending;
  for (const std::size_t source : sources) {
    if (source < marks.size() && marks[source] == 0) {
      marks[source] = 1;
      pending.push_back(source);
    }
  }
  while (!pending.empty()) {
    const std::size_t slot = pending.back();
    pending.pop_back();
    const std::uint32_t net = outNet_[slot];
    if (net == kNone || netDone[net] != 0) continue;
    netDone[net] = 1;
    for (std::uint32_t k = readerStart_[net]; k < readerStart_[net + 1]; ++k) {
      const std::size_t reader = readers_[k];
      if (marks[reader] == 0) {
        marks[reader] = 1;
        pending.push_back(reader);
      }
    }
  }
  return marks;
}

std::vector<ComponentSignature> signaturesOf(const Simulator& sim) {
  const auto comps = sim.netlist().components();
  const internal::PinTable table = internal::buildPinTable(comps);
  const internal::Nets nets = internal::buildNets(table, comps, sim.netlist().wires());

  // The ids of the drivers of each net, in ascending order because the slots are.
  std::vector<internal::Fnv64> drivers(nets.netCount);
  for (std::size_t slot = 0; slot < comps.size(); ++slot) {
    if (table.output[slot] != 0) drivers[nets.pinNet[table.pin(slot, kOut)]].u32(comps[slot].first);
  }

  // The Simulator shows its per-component state only through encode() (02 section 6.2): records in ascending id,
  // each the fixed part followed by the state bytes of its kind.
  const std::vector<std::uint8_t> blob = sim.encode();
  std::size_t offset = kSimulatorHeaderBytes;

  std::vector<ComponentSignature> signatures;
  signatures.reserve(comps.size());
  for (std::size_t slot = 0; slot < comps.size(); ++slot) {
    const Spec& spec = comps[slot].second;
    internal::Fnv64 h;
    h.u32(comps[slot].first);
    h.byte(static_cast<std::uint8_t>(spec.kind));
    h.u16(spec.p0);
    h.u16(spec.p1);
    h.u16(spec.p2);
    offset += kSimulatorRecordBytes;
    const std::size_t size = internal::stateSize(spec);
    for (std::size_t k = 0; k < size && offset + k < blob.size(); ++k) h.byte(blob[offset + k]);
    offset += size;
    for (std::uint8_t j = 0; j < table.inputs[slot]; ++j) h.u64(drivers[nets.pinNet[table.pin(slot, j)]].value());
    signatures.push_back(ComponentSignature{comps[slot].first, h.value()});
  }
  return signatures;
}

}  // namespace slg::host::detail
