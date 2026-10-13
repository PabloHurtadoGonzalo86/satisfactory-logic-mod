#include "netutil.hpp"

#include <algorithm>
#include <set>

namespace slgt {

namespace {

class Dsu {
 public:
  explicit Dsu(std::size_t n) : parent_(n) {
    for (std::size_t i = 0; i < n; ++i) parent_[i] = i;
  }
  std::size_t find(std::size_t x) {
    while (parent_[x] != x) {
      parent_[x] = parent_[parent_[x]];
      x = parent_[x];
    }
    return x;
  }
  void join(std::size_t a, std::size_t b) { parent_[find(a)] = find(b); }

 private:
  std::vector<std::size_t> parent_;
};

}  // namespace

NetAnalysis analyzeNets(const Model& m) {
  std::vector<Pin> all;
  for (const CompDef& c : m.comps) {
    for (unsigned j = 0; j < inputCount(c.spec); ++j) all.push_back(inPin(c.id, j));
    if (hasOutput(c.spec)) all.push_back(outPin(c.id));
  }
  std::stable_sort(all.begin(), all.end());
  std::map<Pin, std::size_t> index;
  for (std::size_t i = 0; i < all.size(); ++i) index[all[i]] = i;
  Dsu dsu(all.size());
  for (const Wire& w : m.wires) {
    const auto a = index.find(w.a);
    const auto b = index.find(w.b);
    if (a != index.end() && b != index.end()) dsu.join(a->second, b->second);
  }
  std::map<std::size_t, std::size_t> rootToNet;
  NetAnalysis na;
  for (std::size_t i = 0; i < all.size(); ++i) {
    const std::size_t r = dsu.find(i);
    auto it = rootToNet.find(r);
    if (it == rootToNet.end()) {
      it = rootToNet.emplace(r, na.nets.size()).first;
      na.nets.emplace_back();
    }
    NetInfo& n = na.nets[it->second];
    n.pins.push_back(all[i]);
    if (all[i].index == kOut) n.drivers.push_back(all[i]); else n.readers.push_back(all[i]);
    na.netOf[all[i]] = it->second;
  }
  std::set<std::pair<ComponentId, ComponentId>> edges;
  for (const NetInfo& n : na.nets) {
    for (const Pin& d : n.drivers) {
      for (const Pin& r : n.readers) edges.insert({d.component, r.component});
    }
  }
  na.edges.assign(edges.begin(), edges.end());
  return na;
}

bool inducedGraphIsAcyclic(const NetAnalysis& na, const Model& m) {
  std::map<ComponentId, std::vector<ComponentId>> adj;
  std::map<ComponentId, unsigned> indeg;
  for (const CompDef& c : m.comps) indeg[c.id] = 0;
  for (const auto& [a, b] : na.edges) {
    adj[a].push_back(b);
    ++indeg[b];
  }
  std::vector<ComponentId> ready;
  for (const auto& [id, d] : indeg) {
    if (d == 0) ready.push_back(id);
  }
  std::size_t seen = 0;
  while (!ready.empty()) {
    const ComponentId c = ready.back();
    ready.pop_back();
    ++seen;
    for (ComponentId n : adj[c]) {
      if (--indeg[n] == 0) ready.push_back(n);
    }
  }
  return seen == m.comps.size();
}

}  // namespace slgt
