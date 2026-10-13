#include "slg/lint.hpp"

#include <algorithm>

#include "internal/nets.hpp"
#include "internal/spec_rules.hpp"

namespace slg {
namespace {

constexpr std::uint32_t kNone = 0xFFFFFFFFu;

// The net-induced graph of 01 section 5, with one hub node per net so that a net with D conductors and
// R readers costs D + R edges instead of D * R. A path c -> hub -> c' is exactly the edge c -> c'.
// Nodes: components first (in slot order), then one node per net.
struct Graph {
  std::size_t components = 0;
  std::size_t netCount = 0;
  internal::PinTable table;
  internal::Nets nets;
  std::vector<std::uint32_t> outNet;        // per slot; kNone for an actuator
  std::vector<std::uint32_t> driverCount;   // per net
  std::vector<std::uint32_t> readerStart;   // per net, CSR offsets into `readers`
  std::vector<std::uint32_t> readers;       // slot of every reading input pin

  std::size_t nodeCount() const noexcept { return components + netCount; }

  std::uint32_t degree(std::uint32_t node) const noexcept {
    if (node < components) return outNet[node] == kNone ? 0u : 1u;
    const std::size_t net = node - components;
    return readerStart[net + 1] - readerStart[net];
  }

  std::uint32_t target(std::uint32_t node, std::uint32_t edge) const noexcept {
    if (node < components) return static_cast<std::uint32_t>(components) + outNet[node];
    const std::size_t net = node - components;
    return readers[readerStart[net] + edge];
  }
};

Graph buildGraph(const Netlist& netlist) {
  const auto comps = netlist.components();
  Graph g;
  g.components = comps.size();
  g.table = internal::buildPinTable(comps);
  g.nets = internal::buildNets(g.table, comps, netlist.wires());
  g.netCount = g.nets.netCount;
  g.outNet.assign(comps.size(), kNone);
  g.driverCount.assign(g.netCount, 0);
  g.readerStart.assign(g.netCount + 1, 0);

  for (std::size_t slot = 0; slot < comps.size(); ++slot) {
    if (g.table.output[slot] != 0) {
      const std::uint32_t net = g.nets.pinNet[g.table.pin(slot, kOut)];
      g.outNet[slot] = net;
      ++g.driverCount[net];
    }
    for (std::uint8_t j = 0; j < g.table.inputs[slot]; ++j) {
      ++g.readerStart[g.nets.pinNet[g.table.pin(slot, j)] + 1];
    }
  }
  for (std::size_t net = 0; net < g.netCount; ++net) g.readerStart[net + 1] += g.readerStart[net];

  g.readers.resize(g.readerStart[g.netCount]);
  std::vector<std::uint32_t> cursor(g.readerStart.begin(), g.readerStart.end() - 1);
  for (std::size_t slot = 0; slot < comps.size(); ++slot) {
    for (std::uint8_t j = 0; j < g.table.inputs[slot]; ++j) {
      g.readers[cursor[g.nets.pinNet[g.table.pin(slot, j)]]++] = static_cast<std::uint32_t>(slot);
    }
  }
  return g;
}

// Strongly connected components, emitted successors-first (reverse topological order of the condensation).
struct Sccs {
  std::vector<std::uint32_t> members;
  std::vector<std::uint32_t> start;  // per SCC, plus a final sentinel
  std::size_t count() const noexcept { return start.size() - 1; }
};

// Iterative on purpose: a chain of 10^5 components must not consume the call stack (T7).
Sccs tarjan(const Graph& g) {
  struct Frame {
    std::uint32_t node;
    std::uint32_t edge;
  };
  const std::size_t total = g.nodeCount();
  std::vector<std::uint32_t> index(total, kNone);
  std::vector<std::uint32_t> low(total, 0);
  std::vector<std::uint8_t> onStack(total, 0);
  std::vector<std::uint32_t> stack;
  std::vector<Frame> calls;
  Sccs result;
  result.start.push_back(0);
  std::uint32_t counter = 0;

  for (std::uint32_t root = 0; root < total; ++root) {
    if (index[root] != kNone) continue;
    index[root] = low[root] = counter++;
    stack.push_back(root);
    onStack[root] = 1;
    calls.push_back(Frame{root, 0});
    while (!calls.empty()) {
      const std::uint32_t node = calls.back().node;
      const std::uint32_t edge = calls.back().edge;
      if (edge < g.degree(node)) {
        calls.back().edge = edge + 1;
        const std::uint32_t next = g.target(node, edge);
        if (index[next] == kNone) {
          index[next] = low[next] = counter++;
          stack.push_back(next);
          onStack[next] = 1;
          calls.push_back(Frame{next, 0});
        } else if (onStack[next] != 0) {
          low[node] = std::min(low[node], index[next]);
        }
        continue;
      }
      if (low[node] == index[node]) {
        std::uint32_t member = kNone;
        do {
          member = stack.back();
          stack.pop_back();
          onStack[member] = 0;
          result.members.push_back(member);
        } while (member != node);
        result.start.push_back(static_cast<std::uint32_t>(result.members.size()));
      }
      calls.pop_back();
      if (!calls.empty()) {
        const std::uint32_t parent = calls.back().node;
        low[parent] = std::min(low[parent], low[node]);
      }
    }
  }
  return result;
}

// A feedback group has two or more components, or one component that reads a net it also drives.
bool isFeedback(const Graph& g, const Sccs& sccs, std::size_t scc) {
  std::size_t comps = 0;
  const std::size_t size = sccs.start[scc + 1] - sccs.start[scc];
  for (std::uint32_t k = sccs.start[scc]; k < sccs.start[scc + 1]; ++k) {
    if (sccs.members[k] < g.components) ++comps;
  }
  return comps >= 2 || (comps == 1 && size > 1);
}

std::uint32_t weightOf(const Spec& spec) noexcept {
  switch (spec.kind) {
    case Kind::Const:
    case Kind::Sensor:
    case Kind::Clock:
      return 0;
    case Kind::Gate:
    case Kind::Compare:
    case Kind::Latch:
    case Kind::Toggle:
    case Kind::Actuator:
      return 1;
    case Kind::Delay:
      return spec.p0;
  }
  return 0;
}

bool holdsMemory(Kind kind) noexcept { return kind == Kind::Latch || kind == Kind::Toggle || kind == Kind::Clock; }

}  // namespace

FeedbackReport analyze(const Netlist& netlist) {
  const auto comps = netlist.components();
  const Graph g = buildGraph(netlist);
  const Sccs sccs = tarjan(g);
  FeedbackReport report;

  for (std::size_t scc = 0; scc < sccs.count(); ++scc) {
    if (!isFeedback(g, sccs, scc)) continue;
    std::vector<ComponentId> group;
    for (std::uint32_t k = sccs.start[scc]; k < sccs.start[scc + 1]; ++k) {
      if (sccs.members[k] < g.components) group.push_back(comps[sccs.members[k]].first);
    }
    std::stable_sort(group.begin(), group.end());
    report.groups.push_back(std::move(group));
  }
  std::stable_sort(report.groups.begin(), report.groups.end(),
                   [](const std::vector<ComponentId>& l, const std::vector<ComponentId>& r) { return l.front() < r.front(); });

  std::vector<std::uint32_t> reportOfNet(g.netCount, kNone);
  for (std::size_t slot = 0; slot < comps.size(); ++slot) {
    const ComponentId id = comps[slot].first;
    for (std::uint8_t j = 0; j < g.table.inputs[slot]; ++j) {
      const std::uint32_t net = g.nets.pinNet[g.table.pin(slot, j)];
      if (g.driverCount[net] == 0) report.undrivenInputs.push_back(Pin{id, j});
      if (g.driverCount[net] >= 2) {
        if (reportOfNet[net] == kNone) {
          reportOfNet[net] = static_cast<std::uint32_t>(report.multiDriverNets.size());
          report.multiDriverNets.emplace_back();
        }
        report.multiDriverNets[reportOfNet[net]].push_back(Pin{id, j});
      }
    }
    if (g.table.output[slot] != 0) {
      const std::uint32_t net = g.outNet[slot];
      if (g.driverCount[net] >= 2) {
        if (reportOfNet[net] == kNone) {
          reportOfNet[net] = static_cast<std::uint32_t>(report.multiDriverNets.size());
          report.multiDriverNets.emplace_back();
        }
        report.multiDriverNets[reportOfNet[net]].push_back(Pin{id, kOut});
      }
    }
  }
  return report;
}

std::vector<std::optional<std::uint32_t>> settlingBounds(const Netlist& netlist) {
  const auto comps = netlist.components();
  const Graph g = buildGraph(netlist);
  const Sccs sccs = tarjan(g);
  const std::size_t total = g.nodeCount();

  // A node is tainted when anything in its cone is cyclic or holds memory (01 section 8).
  std::vector<std::uint8_t> tainted(total, 0);
  std::vector<std::uint32_t> pending;
  const auto taint = [&](std::uint32_t node) {
    if (tainted[node] == 0) {
      tainted[node] = 1;
      pending.push_back(node);
    }
  };
  for (std::size_t scc = 0; scc < sccs.count(); ++scc) {
    if (!isFeedback(g, sccs, scc)) continue;
    for (std::uint32_t k = sccs.start[scc]; k < sccs.start[scc + 1]; ++k) taint(sccs.members[k]);
  }
  for (std::size_t slot = 0; slot < comps.size(); ++slot) {
    if (holdsMemory(comps[slot].second.kind)) taint(static_cast<std::uint32_t>(slot));
  }
  while (!pending.empty()) {
    const std::uint32_t node = pending.back();
    pending.pop_back();
    for (std::uint32_t e = 0; e < g.degree(node); ++e) taint(g.target(node, e));
  }

  // Untainted nodes form a DAG, so one pass in topological order gives the maximum-weight path.
  std::vector<std::uint32_t> incoming(total, 0);
  std::vector<std::uint32_t> level(total, 0);
  for (std::size_t scc = sccs.count(); scc-- > 0;) {
    for (std::uint32_t k = sccs.start[scc]; k < sccs.start[scc + 1]; ++k) {
      const std::uint32_t node = sccs.members[k];
      if (tainted[node] != 0) continue;
      const std::uint32_t weight = node < g.components ? weightOf(comps[node].second) : 0;
      level[node] = weight + incoming[node];
      for (std::uint32_t e = 0; e < g.degree(node); ++e) {
        const std::uint32_t next = g.target(node, e);
        incoming[next] = std::max(incoming[next], level[node]);
      }
    }
  }

  std::vector<std::optional<std::uint32_t>> bounds;
  for (std::size_t slot = 0; slot < comps.size(); ++slot) {
    if (comps[slot].second.kind != Kind::Actuator) continue;
    if (tainted[slot] != 0) {
      bounds.emplace_back(std::nullopt);
    } else {
      bounds.emplace_back(level[slot]);
    }
  }
  return bounds;
}

}  // namespace slg
