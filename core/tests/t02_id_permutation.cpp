// T2: for every bijection phi of ids, the trace of phi(N) is phi(trace of N). The ids handed out by add()
// in a different order are covered by the dense variant. Not claimed: coordinates (the core never gets any).
#include <algorithm>
#include <map>

#include "engine.hpp"
#include "gen.hpp"
#include "testkit.hpp"

namespace slgt {

namespace {

using IdMap = std::map<ComponentId, ComponentId>;

Model mapModel(const Model& m, const IdMap& phi) {
  Model r;
  for (const CompDef& c : m.comps) r.comps.push_back(CompDef{phi.find(c.id)->second, c.spec, c.seed});
  for (const Wire& w : m.wires) {
    r.wires.push_back(makeWire(Pin{phi.find(w.a.component)->second, w.a.index}, Pin{phi.find(w.b.component)->second, w.b.index}));
  }
  r.sortAndNormalize();
  return r;
}

BlobData mapState(const BlobData& d, const IdMap& phi) {
  BlobData r = d;
  r.comps.clear();
  for (const BlobComp& c : d.comps) r.comps.push_back(BlobComp{phi.find(c.id)->second, c.spec, c.state});
  std::stable_sort(r.comps.begin(), r.comps.end(), [](const BlobComp& a, const BlobComp& b) { return a.id < b.id; });
  r.wires.clear();
  for (const Wire& w : d.wires) {
    r.wires.push_back(makeWire(Pin{phi.find(w.a.component)->second, w.a.index}, Pin{phi.find(w.b.component)->second, w.b.index}));
  }
  std::stable_sort(r.wires.begin(), r.wires.end());
  return r;
}

IdMap makePhi(SplitMix64& rng, const Model& m, unsigned variant) {
  IdMap phi;
  const std::size_t n = m.comps.size();
  std::vector<ComponentId> pool;
  if (variant == 0) {          // sparse
    for (ComponentId i = 1; i <= 3 * n + 12; ++i) pool.push_back(i);
  } else if (variant == 1) {   // dense, built with add() in the permuted order
    for (ComponentId i = 1; i <= n; ++i) pool.push_back(i);
  } else {                     // the very top of the id space
    for (ComponentId i = 0; i < 40; ++i) pool.push_back(0xFFFFFFFEU - i);
  }
  rng.shuffle(pool);
  for (std::size_t i = 0; i < n; ++i) phi[m.comps[i].id] = pool[i];
  return phi;
}

std::string t2Run(const EngineFactory& f, const Model& m, const std::vector<std::vector<Signal>>& frames,
                  const BlobData* state, std::uint64_t seed, unsigned variant) {
  SplitMix64 rng(seed);
  const IdMap phi = makePhi(rng, m, variant);
  IdMap inv;
  for (const auto& [a, b] : phi) inv[b] = a;
  const Model mp = mapModel(m, phi);
  std::string err;
  auto a = makeEngine(f, m, err);
  if (a == nullptr) return err;
  auto b = f.make(Limits{}, 0);
  if (variant == 1) {
    // add() hands out base + position: create the components in the order of their new ids
    std::vector<EditOp> ops;
    for (const CompDef& c : mp.comps) ops.push_back(opAdd(c.id, c.spec, c.seed, true));
    for (const Wire& w : mp.wires) ops.push_back(opConnect(w.a, w.b));
    const Status s = b->apply(ops);
    if (s != Status::Ok || !b->lastBatchIdsMatched()) return cat("building phi(N) with add(): ", statusName(s), b->lastBatchIdsMatched() ? "" : " (ids not as promised)");
  } else {
    const Status s = b->apply(opsForModel(mp));
    if (s != Status::Ok) return cat("building phi(N): ", statusName(s));
  }
  if (state != nullptr) {
    if (injectState(*a, *state) != Status::Ok || injectState(*b, mapState(*state, phi)) != Status::Ok) {
      return "a legal random state was rejected by decode";
    }
  }
  for (std::size_t t = 0; t < frames.size(); ++t) {
    const Obs oa = observe(*a);
    const Obs ob = observe(*b);
    const std::string bad = selfCheck(ob);
    if (!bad.empty()) return cat("row ", t, ": ", bad);
    const BlobData expect = mapState(oa.data, phi);
    if (expect.comps != ob.data.comps) {
      return cat("row ", t, ": states of phi(N) are not phi(states of N):\n      expected ", describeState(expect), "\n      got      ",
                 describeState(ob.data));
    }
    if (expect.wires != ob.data.wires) return cat("row ", t, ": wires of phi(N) are not phi(wires of N)");
    for (std::size_t i = 0; i < oa.actuators.size(); ++i) {
      const ComponentId mapped = phi.find(oa.actuators[i])->second;
      const auto it = std::find(ob.actuators.begin(), ob.actuators.end(), mapped);
      if (it == ob.actuators.end() || ob.commands[static_cast<std::size_t>(it - ob.actuators.begin())] != oa.commands[i]) {
        return cat("row ", t, ": the command of actuator ", oa.actuators[i], " is not the command of actuator ", mapped);
      }
    }
    if (t + 1 == frames.size()) break;
    std::map<ComponentId, Signal> byNewId;
    for (std::size_t i = 0; i < oa.sensors.size(); ++i) byNewId[phi.find(oa.sensors[i])->second] = frames[t][i];
    std::vector<Signal> samplesB;
    for (ComponentId id : ob.sensors) samplesB.push_back(byNewId[id]);
    if (a->step(frames[t]) != Status::Ok || b->step(samplesB) != Status::Ok) return "step rejected a well-formed frame";
  }
  return "";
}

}  // namespace

SLG_PROPERTY(T02_id_permutation, 2, "trace of phi(N) is phi of the trace of N, for sparse, dense (add) and top-of-range ids",
             RunOn::Both) {
  forEachCase(ctx, ctx.cases(), [&](std::uint64_t, SplitMix64& rng) {
    const Model m = genAnyModel(rng);
    const unsigned rows = 8 + rng.pick(12);
    const auto frames = genFrames(rng, sensorIdsOf(m).size(), rows, genStyle(rng));
    const bool randomState = rng.coin();
    BlobData st;
    if (randomState) st = randomLegalState(rng, m);
    const std::uint64_t seed = rng.next();
    const unsigned variant = rng.pick(3);
    const std::string why = t2Run(factory, m, frames, randomState ? &st : nullptr, seed, variant);
    SLG_CHECK(ctx, true);
    if (!why.empty()) {
      SLG_FAIL(ctx, cat(why, "\n    (id variant ", variant, randomState ? ", random legal start state" : "", ")"));
      if (!randomState) {
        reportScenario(ctx, "T2", scenarioFromFrames(m, frames), [&](const Scenario& s) {
          return !t2Run(factory, s.net, framesOf(s), nullptr, seed, variant).empty();
        });
      }
    }
  });
}

}  // namespace slgt
