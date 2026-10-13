// T4: in those cones with constant samples X_tau = X_{tau+1} for tau >= tau0 + L; and every Delay(d)
// satisfies out(tau + d) = in(tau) (checked on arbitrary netlists, cycles included).
// Not claimed: oscillating sensors give oscillating outputs, which is not a failure.
#include "cone.hpp"
#include "engine.hpp"
#include "gen.hpp"
#include "testkit.hpp"

namespace slgt {

SLG_PROPERTY(T04_quiescence_and_fifo, 4, "cones are quiescent after tau0 + L; Delay(d) is an exact FIFO of length d", RunOn::Both) {
  forEachCase(ctx, ctx.cases(), [&](std::uint64_t, SplitMix64& rng) {
    // ---- quiescence ----
    {
      DagConfig cfg;
      cfg.maxComps = 5 + rng.pick(12);
      const Model m = genDag(rng, cfg);
      ConeInfo ci;
      if (!SLG_CHECK_MSG(ctx, analyzeCone(m, ci), "generator bug: not an acyclic cone")) return;
      const unsigned L = ci.maxLevel;
      const unsigned tau0 = rng.pick(5);
      const unsigned rows = tau0 + 2 * L + 17;
      const std::vector<ComponentId> sensors = sensorIdsOf(m);
      auto frames = genFrames(rng, sensors.size(), rows, SampleStyle::Random);
      for (std::size_t i = 0; i < sensors.size(); ++i) {
        const Signal v = u8(rng.pick(16));
        for (unsigned t = tau0; t < rows; ++t) frames[t][i] = v;
      }
      std::string err;
      auto e = makeEngine(factory, m, err);
      if (e == nullptr) {
        SLG_FAIL(ctx, err);
        return;
      }
      if (rng.coin() && injectState(*e, randomLegalState(rng, m)) != Status::Ok) {
        SLG_FAIL(ctx, "a legal random state was rejected by decode");
        return;
      }
      Obs prev;
      for (unsigned t = 0; t < rows; ++t) {
        const Obs o = observe(*e);
        if (t > tau0 + L && stateStream(o.data) != stateStream(prev.data)) {
          SLG_FAIL(ctx, cat("X_", t, " != X_", t - 1, " although tau0 + L = ", tau0 + L, "\n    ", formatLnet(m)));
          return;
        }
        prev = o;
        if (t + 1 < rows) e->step(frames[t]);
      }
    }
    // ---- FIFO on arbitrary netlists ----
    {
      const Model m = genAnyModel(rng);
      const unsigned rows = 10 + rng.pick(14);
      const auto frames = genFrames(rng, sensorIdsOf(m).size(), rows, genStyle(rng));
      std::string err;
      auto e = makeEngine(factory, m, err);
      if (e == nullptr) {
        SLG_FAIL(ctx, err);
        return;
      }
      if (rng.coin() && injectState(*e, randomLegalState(rng, m)) != Status::Ok) {
        SLG_FAIL(ctx, "a legal random state was rejected by decode");
        return;
      }
      std::vector<Obs> obs;
      for (unsigned t = 0; t < rows; ++t) {
        obs.push_back(observe(*e));
        if (t + 1 < rows) e->step(frames[t]);
      }
      for (const CompDef& c : m.comps) {
        if (c.spec.kind != Kind::Delay) continue;
        const unsigned d = c.spec.p0;
        for (unsigned t = 0; t + d < rows; ++t) {
          const Signal in = netValueFrom(obs[t], inPin(c.id, 0), frames[t]);
          const BlobComp* later = nullptr;
          for (const BlobComp& b : obs[t + d].data.comps) {
            if (b.id == c.id) later = &b;
          }
          if (later == nullptr || later->state[0] != in) {
            SLG_FAIL(ctx, cat("Delay ", c.id, " (d = ", d, "): out(", t + d, ") = ", later == nullptr ? 999U : static_cast<unsigned>(later->state[0]),
                              " but in(", t, ") = ", static_cast<unsigned>(in), "\n    ", formatLnet(m)));
            return;
          }
        }
      }
    }
  });
}

}  // namespace slgt
