// T3: the settling theorem of 01 section 8 on every acyclic cone of Const, Sensor, Gate, Compare, Delay.
//  (a) two arbitrary legal initial states and the same samples: the state of each c agrees for tau >= l(c);
//  (b) constant samples from tau0: the state of c is z(c) for tau >= tau0 + l(c), z the leveled evaluation.
// Not claimed: a tight bound, absence of glitches, anything with Latch, Toggle, Clock or cycles.
#include "cone.hpp"
#include "engine.hpp"
#include "gen.hpp"
#include "testkit.hpp"

namespace slgt {

namespace {

const std::vector<std::uint8_t>* stateBytes(const Obs& o, ComponentId id) {
  for (const BlobComp& c : o.data.comps) {
    if (c.id == id) return &c.state;
  }
  return nullptr;
}

}  // namespace

SLG_PROPERTY(T03_settling_theorem, 3, "memory vanishes after l(c) ticks and constant inputs reach the leveled fixed point", RunOn::Both) {
  forEachCase(ctx, ctx.cases(), [&](std::uint64_t, SplitMix64& rng) {
    DagConfig cfg;
    cfg.maxComps = 6 + rng.pick(14);
    const Model m = genDag(rng, cfg);
    ConeInfo ci;
    if (!SLG_CHECK_MSG(ctx, analyzeCone(m, ci), cat("generator bug: not an acyclic cone\n", formatLnet(m)))) return;
    const std::vector<ComponentId> sensors = sensorIdsOf(m);
    const unsigned L = ci.maxLevel;
    const BlobData s1 = randomLegalState(rng, m);
    const BlobData s2 = randomLegalState(rng, m);
    std::string err;

    // (a) same samples (arbitrary, changing), two different states
    {
      const unsigned rows = L + 9;
      const auto frames = genFrames(rng, sensors.size(), rows, SampleStyle::Random);
      auto a = makeEngine(factory, m, err);
      auto b = makeEngine(factory, m, err);
      if (a == nullptr || b == nullptr) {
        SLG_FAIL(ctx, err);
        return;
      }
      if (injectState(*a, s1) != Status::Ok || injectState(*b, s2) != Status::Ok) {
        SLG_FAIL(ctx, "a legal random state was rejected by decode");
        return;
      }
      for (unsigned t = 0; t < rows; ++t) {
        const Obs oa = observe(*a);
        const Obs ob = observe(*b);
        for (const CompDef& c : m.comps) {
          if (t < ci.level[c.id]) continue;
          const auto* x = stateBytes(oa, c.id);
          const auto* y = stateBytes(ob, c.id);
          if (x == nullptr || y == nullptr || *x != *y) {
            SLG_FAIL(ctx, cat("(a) the state of component ", c.id, " (", kindToken(c.spec.kind), ", l = ", ci.level[c.id],
                              ") still depends on the initial state at tau = ", t, " (L = ", L, ")\n    ", formatLnet(m)));
            return;
          }
        }
        if (t + 1 < rows) {
          a->step(frames[t]);
          b->step(frames[t]);
        }
      }
    }

    // (b) constant samples from tau0
    {
      const unsigned tau0 = rng.pick(4);
      const unsigned rows = tau0 + L + 9;
      auto frames = genFrames(rng, sensors.size(), rows, SampleStyle::Random);
      std::map<ComponentId, Signal> star;
      for (std::size_t i = 0; i < sensors.size(); ++i) {
        const Signal v = u8(rng.pick(16));
        star[sensors[i]] = v;
        for (unsigned t = tau0; t < rows; ++t) frames[t][i] = v;
      }
      const auto z = fixedPoint(m, ci, star);
      for (const BlobData* start : {&s1, &s2}) {
        auto e = makeEngine(factory, m, err);
        if (e == nullptr || injectState(*e, *start) != Status::Ok) {
          SLG_FAIL(ctx, err.empty() ? "a legal random state was rejected by decode" : err);
          return;
        }
        for (unsigned t = 0; t < rows; ++t) {
          const Obs o = observe(*e);
          for (const CompDef& c : m.comps) {
            if (c.spec.kind == Kind::Const || c.spec.kind == Kind::Sensor || t < tau0 + ci.level[c.id]) continue;
            const auto* x = stateBytes(o, c.id);
            if (x == nullptr || *x != z.find(c.id)->second) {
              SLG_FAIL(ctx, cat("(b) component ", c.id, " (", kindToken(c.spec.kind), ", l = ", ci.level[c.id], ") is ",
                                x == nullptr ? std::string("missing") : hexBytes(*x), " at tau = ", t, " (tau0 = ", tau0,
                                ") but z = ", hexBytes(z.find(c.id)->second), "\n    ", formatLnet(m)));
              return;
            }
          }
          if (t + 1 < rows) e->step(frames[t]);
        }
      }
    }
  });
}

}  // namespace slgt
