// T8: with constant samples every trace is eventually periodic: mu >= 0, lambda >= 1, X_{mu+lambda} = X_mu
// and mu + lambda <= product of the sizes of the component state spaces. Rings of n NOT with all 2^n
// seeds (theory/06: n=3 gives 2 states of period 2 and 6 of period 6, n=5 2 and 30 (10), n=7 2 and 126 (14),
// n=9 2 (2), 6 (6) and 504 (18); all mu = 0; one defect gives 2n); NOT + Delay(m) loops give 2(m+1);
// Clock(P,H) gives P; crossed NOR released from S=R gives 2; a native Latch gives 1.
// The cycle is found with Brent comparing the complete state (not a hash).
// Not claimed: a bound on mu better than |S|.
#include <map>

#include "engine.hpp"
#include "gen.hpp"
#include "spec_examples.hpp"
#include "testkit.hpp"

namespace slgt {

namespace {

struct Cycle {
  bool found = false;
  std::uint64_t mu = 0;
  std::uint64_t lambda = 0;
};

std::vector<std::uint8_t> stateOfEngine(const Engine& e) {
  BlobData d;
  if (!parseBlob(e.encode(), d)) return {0xDE, 0xAD};
  return stateStream(d);
}

std::unique_ptr<Engine> fresh(const EngineFactory& f, const Model& m, const BlobData* start, std::string& err) {
  auto e = makeEngine(f, m, err);
  if (e != nullptr && start != nullptr && injectState(*e, *start) != Status::Ok) {
    err = "a legal start state was rejected by decode";
    return nullptr;
  }
  return e;
}

// Brent's algorithm on the sequence of complete states; false in found when no repetition shows up within cap steps.
Cycle brent(const EngineFactory& f, const Model& m, const BlobData* start, const std::vector<Signal>& samples,
            std::uint64_t cap, std::string& err) {
  Cycle c;
  auto hare = fresh(f, m, start, err);
  if (hare == nullptr) return c;
  std::vector<std::uint8_t> tortoise = stateOfEngine(*hare);
  std::uint64_t power = 1;
  std::uint64_t lam = 1;
  hare->step(samples);
  std::uint64_t steps = 1;
  while (stateOfEngine(*hare) != tortoise) {
    if (power == lam) {
      tortoise = stateOfEngine(*hare);
      power *= 2;
      lam = 0;
    }
    if (steps++ >= cap) return c;
    hare->step(samples);
    ++lam;
  }
  auto t = fresh(f, m, start, err);
  auto h = fresh(f, m, start, err);
  if (t == nullptr || h == nullptr) return c;
  for (std::uint64_t i = 0; i < lam; ++i) h->step(samples);
  std::uint64_t mu = 0;
  while (stateOfEngine(*t) != stateOfEngine(*h)) {
    t->step(samples);
    h->step(samples);
    ++mu;
    if (mu > cap) return c;
  }
  c.found = true;
  c.mu = mu;
  c.lambda = lam;
  return c;
}

std::uint64_t stateProduct(const Model& m) {
  constexpr std::uint64_t kCap = std::uint64_t{1} << 62;
  std::uint64_t p = 1;
  for (const CompDef& c : m.comps) {
    const std::uint64_t s = stateSpaceSize(c.spec);
    if (p > kCap / s) return kCap;
    p *= s;
  }
  return p;
}

void checkRings(Ctx& ctx, const EngineFactory& f) {
  struct Expect { unsigned n; std::map<std::uint64_t, unsigned> census; };
  const Expect table[] = {{3, {{2, 2}, {6, 6}}}, {5, {{2, 2}, {10, 30}}}, {7, {{2, 2}, {14, 126}}}, {9, {{2, 2}, {6, 6}, {18, 504}}}};
  for (const Expect& ex : table) {
    std::map<std::uint64_t, unsigned> census;
    for (std::uint32_t mask = 0; mask < (1U << ex.n); ++mask) {
      const Model m = ringModel(ex.n, mask);
      std::string err;
      const Cycle c = brent(f, m, nullptr, {}, 8 * (std::uint64_t{1} << ex.n) + 64, err);
      if (!c.found) {
        SLG_FAIL(ctx, cat("ring ", ex.n, " seeds ", mask, ": no repetition of the state within 8 * 2^n steps ", err));
        return;
      }
      ++census[c.lambda];
      SLG_CHECK_MSG(ctx, c.mu == 0, cat("ring ", ex.n, " seeds ", mask, ": mu = ", c.mu, " (theory/06: every state of a ring is on its cycle)"));
      SLG_CHECK_MSG(ctx, c.mu + c.lambda <= (std::uint64_t{1} << ex.n), "mu + lambda exceeds the size of the state space");
      // a single defect gives period 2n: gate i is in defect when it equals its predecessor (a NOT would differ)
      unsigned defects = 0;
      for (unsigned i = 0; i < ex.n; ++i) {
        const unsigned prev = (i + ex.n - 1) % ex.n;
        if (((mask >> i) & 1U) == ((mask >> prev) & 1U)) ++defects;
      }
      if (defects == 1) SLG_CHECK_MSG(ctx, c.lambda == 2ULL * ex.n, cat("ring ", ex.n, " seeds ", mask, " has one defect but period ", c.lambda));
    }
    SLG_CHECK_MSG(ctx, census == ex.census, [&] {
      std::string s = cat("ring of ", ex.n, " NOT: periods found (period:states) ");
      for (const auto& [p, k] : census) s += cat(p, ":", k, " ");
      return s;
    }());
  }
}

void checkLoops(Ctx& ctx, const EngineFactory& f, SplitMix64& rng) {
  std::string err;
  for (unsigned mm = 1; mm <= 12; ++mm) {   // NOT + Delay(m), from the settled chain: NOT = 15, every slot 15
    Model m;
    m.comps = {CompDef{1, specNot(), 15}, CompDef{2, specDelay(mm), 15}};
    m.wires = {makeWire(outPin(1), inPin(2, 0)), makeWire(outPin(2), inPin(1, 0))};
    m.sortAndNormalize();
    const Cycle c = brent(f, m, nullptr, {}, 100000, err);
    SLG_CHECK_MSG(ctx, c.found && c.lambda == 2ULL * (mm + 1) && c.mu == 0, cat("NOT + Delay(", mm, "): period ", c.lambda, ", mu ", c.mu, " (want ", 2 * (mm + 1), ")"));
  }
  for (unsigned p = 2; p <= 40; ++p) {   // Clock(P,H) alone, then with an actuator on its output
    const unsigned h = 1 + rng.pick(p - 1);
    const unsigned phase = rng.pick(p);
    Model m;
    m.comps = {CompDef{1, specClock(p, h), u16(phase)}};
    Cycle c = brent(f, m, nullptr, {}, 10000, err);
    SLG_CHECK_MSG(ctx, c.found && c.lambda == p && c.mu == 0, cat("Clock(", p, ",", h, ") phase ", phase, ": period ", c.lambda, ", mu ", c.mu));
    m.comps.push_back(CompDef{2, specActuator(), 0});
    m.wires = {makeWire(outPin(1), inPin(2, 0))};
    m.sortAndNormalize();
    c = brent(f, m, nullptr, {}, 10000, err);
    SLG_CHECK_MSG(ctx, c.found && c.lambda == p && c.mu <= 1, cat("Clock(", p, ",", h, ") + actuator: period ", c.lambda, ", mu ", c.mu));
  }
  {   // the crossed NOR pair released from S=R=1 (state (0,0), both inputs 0) oscillates with period 2; the native latch holds
    Model m = g4Model();
    for (CompDef& c : m.comps) {
      if (c.id == 4) c.seed = 0;
    }
    Cycle c = brent(f, m, nullptr, {0, 0}, 100, err);
    SLG_CHECK_MSG(ctx, c.found && c.lambda == 2 && c.mu == 0, cat("crossed NOR from (0,0): period ", c.lambda, ", mu ", c.mu));
    Model latch;
    latch.comps = {CompDef{1, specSensor(), 0}, CompDef{2, specSensor(), 0}, CompDef{3, specLatch(), 0}};
    latch.wires = {makeWire(outPin(1), inPin(3, 0)), makeWire(outPin(2), inPin(3, 1))};
    latch.sortAndNormalize();
    c = brent(f, latch, nullptr, {0, 0}, 100, err);
    SLG_CHECK_MSG(ctx, c.found && c.lambda == 1 && c.mu == 0, cat("native Latch: period ", c.lambda, ", mu ", c.mu));
  }
}

}  // namespace

SLG_PROPERTY(T08_periodicity, 8, "eventually periodic with constant inputs; ring census of theory/06; loops, clocks and latches", RunOn::Both) {
  checkRings(ctx, factory);
  SplitMix64 rng(ctx.caseSeed(0));
  checkLoops(ctx, factory, rng);
  unsigned skipped = 0;
  forEachCase(ctx, ctx.cases(), [&](std::uint64_t, SplitMix64& r) {
    GenConfig cfg;
    cfg.maxComps = 3 + r.pick(6);
    Model m;
    std::uint64_t bound = 0;
    for (unsigned attempt = 0; attempt < 8; ++attempt) {   // redraw circuits whose state space is too large to enumerate
      m = r.coin() ? genModel(r, cfg) : genAnyModel(r);
      bound = stateProduct(m);
      if (bound <= (std::uint64_t{1} << 16)) break;
    }
    if (bound > (std::uint64_t{1} << 16)) {
      ++skipped;
      return;
    }
    std::vector<Signal> samples;
    for (std::size_t i = 0; i < sensorIdsOf(m).size(); ++i) samples.push_back(u8(r.pick(16)));
    std::string err;
    const BlobData start = randomLegalState(r, m);
    const Cycle c = brent(factory, m, &start, samples, 4 * bound + 64, err);
    if (!c.found) {
      SLG_FAIL(ctx, cat("no repetition of the complete state within the product of the state spaces (", bound, " states) ", err, "\n    ", formatLnet(m)));
      return;
    }
    SLG_CHECK_MSG(ctx, c.lambda >= 1 && c.mu + c.lambda <= bound, cat("mu = ", c.mu, ", lambda = ", c.lambda, " exceed the bound ", bound, "\n    ", formatLnet(m)));
  });
  if (skipped > 0) ctx.note(cat(skipped, " random circuits had a state space above 2^16 and were skipped"));
}

}  // namespace slgt
