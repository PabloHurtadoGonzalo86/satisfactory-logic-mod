#include "cone.hpp"

#include <algorithm>

namespace slgt {

unsigned weightOf(const Spec& s) noexcept {
  switch (s.kind) {
    case Kind::Const: case Kind::Sensor: case Kind::Clock: return 0;
    case Kind::Delay: return s.p0;
    default: return 1;
  }
}

bool analyzeCone(const Model& m, ConeInfo& out) {
  ConeInfo ci;
  for (const CompDef& c : m.comps) {
    if (c.spec.kind == Kind::Latch || c.spec.kind == Kind::Toggle || c.spec.kind == Kind::Clock) return false;
  }
  ci.nets = analyzeNets(m);
  std::map<ComponentId, std::vector<ComponentId>> readersOf;
  std::map<ComponentId, unsigned> indegree;
  std::map<ComponentId, std::vector<ComponentId>> driversOf;
  for (const CompDef& c : m.comps) indegree[c.id] = 0;
  for (const auto& [a, b] : ci.nets.edges) {
    readersOf[a].push_back(b);
    driversOf[b].push_back(a);
    ++indegree[b];
  }
  std::vector<ComponentId> ready;
  for (const auto& [id, d] : indegree) {
    if (d == 0) ready.push_back(id);
  }
  while (!ready.empty()) {
    const ComponentId c = ready.back();
    ready.pop_back();
    ci.order.push_back(c);
    for (ComponentId r : readersOf[c]) {
      if (--indegree[r] == 0) ready.push_back(r);
    }
  }
  if (ci.order.size() != m.comps.size()) return false;
  for (ComponentId c : ci.order) {
    unsigned best = 0;
    for (ComponentId d : driversOf[c]) best = std::max(best, ci.level[d]);
    ci.level[c] = weightOf(m.find(c)->spec) + best;
    ci.maxLevel = std::max(ci.maxLevel, ci.level[c]);
  }
  out = std::move(ci);
  return true;
}

std::map<ComponentId, std::vector<std::uint8_t>> fixedPoint(const Model& m, const ConeInfo& ci,
                                                            const std::map<ComponentId, Signal>& sensorValues) {
  std::map<ComponentId, unsigned> out;   // output value of each component at the fixed point
  std::map<ComponentId, std::vector<std::uint8_t>> z;
  for (ComponentId id : ci.order) {
    const Spec& s = m.find(id)->spec;
    if (s.kind == Kind::Const) {
      out[id] = s.p0;
      z[id] = {};
      continue;
    }
    if (s.kind == Kind::Sensor) {
      const auto it = sensorValues.find(id);
      out[id] = it == sensorValues.end() ? 0U : std::min<unsigned>(it->second, 15U);
      z[id] = {};
      continue;
    }
    const unsigned n = inputCount(s);
    std::vector<unsigned> u(n, 0);
    for (unsigned j = 0; j < n; ++j) {
      const NetInfo& net = ci.nets.nets[ci.nets.netOf.find(inPin(id, j))->second];
      for (const Pin& d : net.drivers) u[j] = std::max(u[j], out[d.component]);
    }
    std::vector<std::uint8_t> st;
    switch (s.kind) {
      case Kind::Actuator: st = {u8(u[0])}; break;
      case Kind::Gate: {
        unsigned truthy = 0;
        for (unsigned v : u) truthy += v > 0 ? 1U : 0U;
        const bool g = s.p0 == 0 ? truthy == n : s.p0 == 1 ? truthy > 0 : (truthy % 2) == 1;
        st = {static_cast<std::uint8_t>((g != (s.p1 != 0)) ? 15 : 0)};
        break;
      }
      case Kind::Compare: st = {u8(s.p0 == 0 ? (u[1] <= u[0] ? u[0] : 0U) : (u[0] > u[1] ? u[0] - u[1] : 0U))}; break;
      case Kind::Delay: st.assign(s.p0, u8(u[0])); break;
      default: break;
    }
    out[id] = st.empty() ? 0U : st[0];
    z[id] = st;
  }
  return z;
}

}  // namespace slgt
