// T7: analyze().groups is exactly the set of components that lie on a closed path of the net-induced graph
// (self-edges and loops created by merging included), grouped by strongly connected component; the
// undriven inputs and the multi-driver nets are exactly those of the netlist. The reference is the
// O(n^3) transitive closure on n <= 40, then G14, then a chain and a ring of 10^5 components analysed in a
// child process whose stack is limited to 256 KiB (the Tarjan of the core must be iterative).
// Not claimed: cost beyond what the benchmark measures.
#include <algorithm>
#include <cstdlib>
#include <map>
#include <optional>
#include <set>

#include "bridge.hpp"
#include "cone.hpp"
#include "engine.hpp"
#include "gen.hpp"
#include "netutil.hpp"
#include "spec_examples.hpp"
#include "testkit.hpp"

namespace slgt {

namespace {

using Groups = std::vector<std::vector<ComponentId>>;

struct ExpectedLint {
  std::vector<std::vector<ComponentId>> groups;        // each ascending, ordered by first id
  std::vector<Pin> undriven;                           // ascending
  std::vector<std::pair<std::vector<Pin>, std::vector<Pin>>> multi;   // (drivers, all pins) of each net with >= 2 drivers
};

ExpectedLint expectedLint(const Model& m) {
  ExpectedLint e;
  const NetAnalysis na = analyzeNets(m);
  const std::size_t n = m.comps.size();
  std::map<ComponentId, std::size_t> index;
  for (std::size_t i = 0; i < n; ++i) index[m.comps[i].id] = i;
  std::vector<std::vector<bool>> reach(n, std::vector<bool>(n, false));
  for (const auto& [a, b] : na.edges) reach[index[a]][index[b]] = true;
  for (std::size_t k = 0; k < n; ++k) {
    for (std::size_t i = 0; i < n; ++i) {
      if (!reach[i][k]) continue;
      for (std::size_t j = 0; j < n; ++j) {
        if (reach[k][j]) reach[i][j] = true;
      }
    }
  }
  std::vector<bool> done(n, false);
  for (std::size_t i = 0; i < n; ++i) {
    if (!reach[i][i] || done[i]) continue;
    std::vector<ComponentId> g;
    for (std::size_t j = 0; j < n; ++j) {
      if (j == i || (reach[i][j] && reach[j][i])) {
        g.push_back(m.comps[j].id);
        done[j] = true;
      }
    }
    e.groups.push_back(g);
  }
  std::stable_sort(e.groups.begin(), e.groups.end());
  for (const NetInfo& net : na.nets) {
    if (net.drivers.empty()) {
      for (const Pin& r : net.readers) e.undriven.push_back(r);
    }
    if (net.drivers.size() >= 2) e.multi.emplace_back(net.drivers, net.pins);
  }
  std::stable_sort(e.undriven.begin(), e.undriven.end());
  return e;
}

std::string pinsText(const std::vector<Pin>& v) {
  std::string s;
  for (const Pin& p : v) s += cat(p.component, p.index == kOut ? std::string(".o") : cat(".", static_cast<unsigned>(p.index)), " ");
  return s;
}

std::string groupsText(const std::vector<std::vector<ComponentId>>& gs) {
  std::string s;
  for (const auto& g : gs) {
    s += "{";
    for (ComponentId c : g) s += cat(c, " ");
    s += "} ";
  }
  return s.empty() ? std::string("(none)") : s;
}

// "" if the core's report matches the closure of the reference
std::string compareLint(const Model& m, const bridge::FeedbackReport& got, std::string& orderNote) {
  const ExpectedLint want = expectedLint(m);
  std::vector<std::vector<ComponentId>> groups = got.groups;
  for (const auto& g : groups) {
    if (!std::is_sorted(g.begin(), g.end()) || std::adjacent_find(g.begin(), g.end()) != g.end()) {
      return cat("a group is not strictly ascending: ", groupsText({g}));
    }
  }
  if (!std::is_sorted(groups.begin(), groups.end())) orderNote = "groups are not ordered by their first id";
  std::stable_sort(groups.begin(), groups.end());
  if (groups != want.groups) return cat("groups: got ", groupsText(groups), "want ", groupsText(want.groups));
  std::vector<Pin> undriven = got.undrivenInputs;
  if (!std::is_sorted(undriven.begin(), undriven.end())) orderNote = "undrivenInputs are not in ascending pin order";
  std::stable_sort(undriven.begin(), undriven.end());
  if (undriven != want.undriven) return cat("undrivenInputs: got ", pinsText(undriven), "| want ", pinsText(want.undriven));
  if (got.multiDriverNets.size() != want.multi.size()) {
    return cat("multiDriverNets: ", got.multiDriverNets.size(), " nets reported, ", want.multi.size(), " have two or more drivers");
  }
  // Each reported net must contain the drivers of exactly one such net and nothing outside it (02 does not say
  // whether the reader pins are listed too, so both readings are accepted).
  std::vector<bool> used(want.multi.size(), false);
  for (std::vector<Pin> net : got.multiDriverNets) {
    std::stable_sort(net.begin(), net.end());
    bool found = false;
    for (std::size_t k = 0; k < want.multi.size() && !found; ++k) {
      if (used[k]) continue;
      const std::vector<Pin>& drivers = want.multi[k].first;
      const std::vector<Pin>& pins = want.multi[k].second;
      const bool hasDrivers = std::includes(net.begin(), net.end(), drivers.begin(), drivers.end());
      const bool inside = std::includes(pins.begin(), pins.end(), net.begin(), net.end());
      if (hasDrivers && inside) {
        used[k] = found = true;
      }
    }
    if (!found) return cat("multiDriverNets: a reported net (", pinsText(net), ") is not a multi-driver net of the netlist");
  }
  return "";
}

// Positive control for the stack limit: genuine recursion that the compiler cannot turn into a loop (the call goes
// through a volatile pointer and its result is used afterwards), a frame per call, 2*10^5 deep.
std::uint64_t deepRecursion(std::uint64_t n, volatile std::uint8_t* sink);
std::uint64_t (*volatile deepFn)(std::uint64_t, volatile std::uint8_t*) = &deepRecursion;
std::uint64_t deepRecursion(std::uint64_t n, volatile std::uint8_t* sink) {
  volatile std::uint8_t pad[128];
  pad[0] = u8(n);
  *sink = pad[0];
  if (n == 0) return 0;
  return deepFn(n - 1, sink) + pad[0];
}

// L_a of 01 section 8 for every actuator (ascending id): defined only when the cone of a is acyclic and made of
// Const, Sensor, Gate, Compare, Delay and a; l(c) = w(c) + max over the drivers of c.
std::vector<std::optional<std::uint32_t>> expectedBounds(const Model& m) {
  const NetAnalysis na = analyzeNets(m);
  const std::size_t n = m.comps.size();
  std::map<ComponentId, std::size_t> index;
  for (std::size_t i = 0; i < n; ++i) index[m.comps[i].id] = i;
  std::vector<std::vector<bool>> reach(n, std::vector<bool>(n, false));
  for (const auto& [a, b] : na.edges) reach[index[a]][index[b]] = true;
  for (std::size_t k = 0; k < n; ++k) {
    for (std::size_t i = 0; i < n; ++i) {
      if (!reach[i][k]) continue;
      for (std::size_t j = 0; j < n; ++j) {
        if (reach[k][j]) reach[i][j] = true;
      }
    }
  }
  std::vector<std::optional<std::uint32_t>> out;
  for (std::size_t a = 0; a < n; ++a) {
    if (m.comps[a].spec.kind != Kind::Actuator) continue;
    std::vector<std::size_t> cone{a};
    for (std::size_t c = 0; c < n; ++c) {
      if (c != a && reach[c][a]) cone.push_back(c);
    }
    bool ok = true;
    for (std::size_t c : cone) {
      const Kind k = m.comps[c].spec.kind;
      if (reach[c][c] || k == Kind::Latch || k == Kind::Toggle || k == Kind::Clock) ok = false;
    }
    if (!ok) {
      out.push_back(std::nullopt);
      continue;
    }
    std::map<std::size_t, std::uint32_t> level;   // memoised by repeated sweeps (the cone is acyclic)
    std::size_t resolved = 0;
    while (resolved < cone.size()) {
      for (std::size_t c : cone) {
        if (level.count(c) != 0) continue;
        std::uint32_t best = 0;
        bool ready = true;
        for (const auto& [from, to] : na.edges) {
          if (index[to] != c) continue;
          const auto it = level.find(index[from]);
          if (it == level.end()) { ready = false; break; }
          best = std::max(best, it->second);
        }
        if (!ready) continue;
        level[c] = weightOf(m.comps[c].spec) + best;
        ++resolved;
      }
    }
    out.emplace_back(level[a]);
  }
  return out;
}

bool bigChecks(std::size_t n, std::string& why) {
  Limits lim;
  lim.maxComponents = u32(n + 16);
  lim.maxWires = u32(n + 16);
  {
    const Model chain = genChain(u32(n));
    bridge::FeedbackReport rep;
    const Status s = bridge::analyze(chain, lim, rep);
    if (s != Status::Ok) {
      why = cat("building the chain of ", n, ": ", statusName(s));
      return false;
    }
    if (!rep.groups.empty() || !rep.undrivenInputs.empty() || !rep.multiDriverNets.empty()) {
      why = cat("a chain has no feedback, undriven input or multi-driver net, but the report has ", rep.groups.size(), "/",
                rep.undrivenInputs.size(), "/", rep.multiDriverNets.size());
      return false;
    }
  }
  {
    const Model ring = ringModel(u32(n), 0xFFFFFFFFU);
    bridge::FeedbackReport rep;
    const Status s = bridge::analyze(ring, lim, rep);
    if (s != Status::Ok) {
      why = cat("building the ring of ", n, ": ", statusName(s));
      return false;
    }
    if (rep.groups.size() != 1 || rep.groups[0].size() != n || !rep.undrivenInputs.empty() || !rep.multiDriverNets.empty() ||
        !std::is_sorted(rep.groups[0].begin(), rep.groups[0].end())) {
      why = cat("a ring of ", n, " NOT gates is one group of ", n, " components, but the report has ", rep.groups.size(), " groups");
      return false;
    }
  }
  return true;
}

}  // namespace

SLG_PROPERTY(T07_lint, 7, "feedback groups = components on closed paths of the net-induced graph; undriven and multi-driver nets", RunOn::Both) {
  // The reference closure on the spec's own example first (G14: the group is {B}).
  {
    const Groups justB{{2}};
    const Groups ring3{{1, 2, 3}};
    const ExpectedLint g14 = expectedLint(g14Model());
    SLG_CHECK_MSG(ctx, g14.groups == justB, cat("reference closure on G14: ", groupsText(g14.groups)));
    SLG_CHECK_MSG(ctx, expectedLint(ringModel(3, 0)).groups == ring3, "reference closure on a ring of 3");
    SLG_CHECK_MSG(ctx, expectedLint(genChain(5)).groups.empty(), "reference closure on a chain");
    SLG_CHECK(ctx, expectedLint(Model{}).groups.empty());
  }
  if (!factory.isSim) {
    // On the reference side only the closure itself is exercised: random models must agree with an independent
    // reading of "components of a closed path" through repeated squaring-free reachability (self-consistency).
    forEachCase(ctx, ctx.cases(1, 4), [&](std::uint64_t, SplitMix64& rng) {
      const Model m = genAnyModel(rng);
      const ExpectedLint e = expectedLint(m);
      const NetAnalysis na = analyzeNets(m);
      std::set<ComponentId> inGroups;
      for (const auto& g : e.groups) inGroups.insert(g.begin(), g.end());
      if (inducedGraphIsAcyclic(na, m)) SLG_CHECK_MSG(ctx, inGroups.empty(), "an acyclic induced graph must have no group");
      else SLG_CHECK_MSG(ctx, !inGroups.empty(), "a cyclic induced graph must have a group");
    });
    return;
  }
  {
    const Groups justB{{2}};
    bridge::FeedbackReport rep;
    SLG_CHECK(ctx, bridge::analyze(g14Model(), Limits{}, rep) == Status::Ok);
    SLG_CHECK_MSG(ctx, rep.groups == justB, cat("G14 groups: ", groupsText(rep.groups), "(want {2}: the NOT is its own feedback through the merged net)"));
    SLG_CHECK_MSG(ctx, rep.multiDriverNets.size() == 1, "G14 has one multi-driver net (A and B drive in(C,0))");
  }
  forEachCase(ctx, ctx.cases(), [&](std::uint64_t, SplitMix64& rng) {
    GenConfig cfg;
    cfg.maxComps = 4 + rng.pick(36);
    cfg.minComps = 2;
    cfg.sparseIds = rng.coin();
    cfg.extraDriverPct = rng.pick(40);
    const Model m = rng.chance(3, 4) ? genModel(rng, cfg) : genAnyModel(rng);
    bridge::FeedbackReport rep;
    const Status s = bridge::analyze(m, Limits{}, rep);
    if (s != Status::Ok) {
      SLG_FAIL(ctx, cat("building the netlist: ", statusName(s)));
      return;
    }
    std::string note;
    const std::string why = compareLint(m, rep, note);
    if (!SLG_CHECK_MSG(ctx, why.empty(), cat(why, "\n    ", formatLnet(m)))) return;
    if (!note.empty()) ctx.note(note + " (02 section 2 only says each group is ascending)");
    std::vector<std::optional<std::uint32_t>> bounds;
    if (bridge::settlingBounds(m, Limits{}, bounds) != Status::Ok) {
      SLG_FAIL(ctx, "settlingBounds could not analyse the netlist");
      return;
    }
    const std::vector<std::optional<std::uint32_t>> wantBounds = expectedBounds(m);
    if (!SLG_CHECK_MSG(ctx, bounds == wantBounds, [&] {
          std::string t = "settlingBounds (per actuator, ascending id): got ";
          for (const auto& b : bounds) t += b.has_value() ? cat(*b, " ") : std::string("none ");
          t += "want ";
          for (const auto& b : wantBounds) t += b.has_value() ? cat(*b, " ") : std::string("none ");
          return t + "\n    " + formatLnet(m);
        }())) {
      return;
    }
  });
  // 10^5 components under a 256 KiB stack
  std::size_t n = 100000;
  if (const char* e = std::getenv("SLG_T7_CHAIN")) {
    const std::size_t v = static_cast<std::size_t>(std::strtoull(e, nullptr, 10));
    if (v >= 10) n = v;
  }
  std::string why;
  std::string detail;
  if (canIsolate()) {
    // the limit must really bite: recursion this deep cannot survive 256 KiB, or the check above proves nothing
    volatile std::uint8_t sink = 0;
    std::string crash;
    const bool survived = runIsolated([&sink] { return deepFn(200000, &sink) != 12345; }, 256 * 1024, crash);
    SLG_CHECK_MSG(ctx, !survived, "positive control: a 200000-frame recursion survived a 256 KiB stack, so the stack limit does not bite");
  }
  const bool ok = runIsolated([&] { return bigChecks(n, detail); }, 256 * 1024, why);
  SLG_CHECK_MSG(ctx, ok, cat("chain/ring of ", n, " components in a child with a 256 KiB stack: ", why, detail.empty() ? std::string() : cat(" (", detail, ")")));
}

}  // namespace slgt
