// T1: the trace (states and commands) is the same in the Simulator, in the reference with true two-buffer
// evaluation in any permutation (a new one every tick), and in a netlist rebuilt by inserting in any order
// with the same ids. Negative control: evaluating in place with two orders must differ (G5).
// Not claimed: anything about in-place update.
#include "engine.hpp"
#include "gen.hpp"
#include "testkit.hpp"

namespace slgt {

namespace {

StepOrder randomOrder(const ReferenceSim& r, SplitMix64& rng) {
  StepOrder o;
  for (const auto& [id, c] : r.comps()) o.comps.push_back(id);
  rng.shuffle(o.comps);
  for (std::size_t i = 0; i < r.wires().size(); ++i) o.wires.push_back(i);
  rng.shuffle(o.wires);
  return o;
}

// Builds the model into an empty engine through a different insertion history; returns "" or why not.
std::string buildByHistory(Engine& e, const Model& m, SplitMix64& rng, unsigned variant) {
  auto adds = [&](const std::vector<CompDef>& comps) {
    std::vector<EditOp> ops;
    for (const CompDef& c : comps) ops.push_back(opAdd(c.id, c.spec, c.seed));
    rng.shuffle(ops);
    return ops;
  };
  auto wireOps = [&](const std::vector<Wire>& wires) {
    std::vector<EditOp> ops;
    for (const Wire& w : wires) ops.push_back(rng.coin() ? opConnect(w.a, w.b) : opConnect(w.b, w.a));
    rng.shuffle(ops);
    return ops;
  };
  auto run = [&](std::vector<EditOp> ops, const char* what) -> std::string {
    if (ops.empty()) return "";
    const Status s = e.apply(ops);
    return s == Status::Ok ? std::string() : cat(what, " batch rejected: ", statusName(s));
  };
  if (variant == 0) {
    std::vector<EditOp> ops = adds(m.comps);
    const std::vector<EditOp> w = wireOps(m.wires);
    ops.insert(ops.end(), w.begin(), w.end());
    return run(std::move(ops), "shuffled one-batch build");
  }
  // several batches: ascending id chunks (a later batch may not add ids below the nextId it started from)
  const std::size_t n = m.comps.size();
  std::size_t pos = 0;
  while (pos < n) {
    const std::size_t take = 1 + rng.pick(4);
    std::vector<CompDef> chunk(m.comps.begin() + static_cast<std::ptrdiff_t>(pos),
                               m.comps.begin() + static_cast<std::ptrdiff_t>(std::min(n, pos + take)));
    pos += chunk.size();
    const std::string why = run(adds(chunk), "chunk");
    if (!why.empty()) return why;
  }
  std::vector<Wire> rest = m.wires;
  rng.shuffle(rest);
  while (!rest.empty()) {
    const std::size_t take = 1 + rng.pick(6);
    std::vector<Wire> part(rest.begin(), rest.begin() + static_cast<std::ptrdiff_t>(std::min(rest.size(), take)));
    rest.erase(rest.begin(), rest.begin() + static_cast<std::ptrdiff_t>(part.size()));
    const std::string why = run(wireOps(part), "wire");
    if (!why.empty()) return why;
  }
  if (variant == 2) {
    // leave scaffolding behind: a spurious component wired to real pins and removed again, and a wire
    // connected and disconnected; the surviving netlist is the same, only the ids handed out differ
    std::vector<Pin> pins;
    for (const CompDef& c : m.comps) {
      for (unsigned j = 0; j < inputCount(c.spec); ++j) pins.push_back(inPin(c.id, j));
      if (hasOutput(c.spec)) pins.push_back(outPin(c.id));
    }
    const ComponentId spurious = e.nextId() + rng.pick(3);
    std::vector<EditOp> ops;
    ops.push_back(opAdd(spurious, specGate(GateOp::Xor, false, 2), 0));
    if (!pins.empty()) {
      ops.push_back(opConnect(outPin(spurious), pins[rng.pick(static_cast<unsigned>(pins.size()))]));
      ops.push_back(opConnect(inPin(spurious, 1), pins[rng.pick(static_cast<unsigned>(pins.size()))]));
    }
    ops.push_back(opRemove(spurious));
    std::string why = run(std::move(ops), "scaffolding");
    if (!why.empty()) return why;
    for (unsigned attempt = 0; attempt < 6 && pins.size() >= 2; ++attempt) {
      const Pin a = pins[rng.pick(static_cast<unsigned>(pins.size()))];
      const Pin b = pins[rng.pick(static_cast<unsigned>(pins.size()))];
      if (a == b) continue;
      bool exists = false;
      for (const Wire& w : m.wires) exists = exists || w == makeWire(a, b);
      if (exists) continue;
      why = run({opConnect(a, b)}, "temporary wire");
      if (!why.empty()) return why;
      why = run({opDisconnect(b, a)}, "temporary wire removal");
      if (!why.empty()) return why;
      break;
    }
  }
  return "";
}

std::string t1Run(const EngineFactory& f, const Model& m, const std::vector<std::vector<Signal>>& frames,
                  const BlobData* state, std::uint64_t permSeed, unsigned variant) {
  SplitMix64 rng(permSeed);
  std::string err;
  auto a = makeEngine(f, m, err);
  if (a == nullptr) return err;
  auto b = f.make(Limits{}, 0);
  err = buildByHistory(*b, m, rng, variant);
  if (!err.empty()) return cat("rebuilding with the same ids (history ", variant, "): ", err);
  ReferenceSim r;
  ReferenceSim r0;
  r.apply(opsForModel(m));
  r0.apply(opsForModel(m));
  if (state != nullptr) {
    const Status sa = injectState(*a, *state);
    const Status sb = injectState(*b, *state);
    if (sa != Status::Ok || sb != Status::Ok) return cat("a legal random state was rejected by decode: ", statusName(sa), "/", statusName(sb));
    BlobData d = *state;
    d.nextId = r.nextId();
    r.load(d);
    r0.load(d);
  }
  for (std::size_t t = 0; t < frames.size(); ++t) {
    const Obs oa = observe(*a);
    const Obs ob = observe(*b);
    const std::string bad = selfCheck(oa);
    if (!bad.empty()) return cat("row ", t, ": ", bad);
    std::string d = diffObs(oa, observeRef(r), false);
    if (!d.empty()) return cat("row ", t, ": ", a->label(), " vs the permuted two-buffer reference: ", d);
    d = diffObs(oa, observeRef(r0), false);
    if (!d.empty()) return cat("row ", t, ": ", a->label(), " vs the canonical-order reference: ", d);
    d = diffObs(oa, ob, false);
    if (!d.empty()) return cat("row ", t, ": ", a->label(), " vs the rebuilt netlist (history ", variant, "): ", d);
    if (oa.tick != t) return cat("tick() = ", oa.tick, " at row ", t);
    if (t + 1 == frames.size()) break;
    const StepOrder order = randomOrder(r, rng);
    if (a->step(frames[t]) != Status::Ok || b->step(frames[t]) != Status::Ok) return "step rejected a well-formed frame";
    r.step(frames[t], &order);
    r0.step(frames[t]);
  }
  return "";
}

}  // namespace

SLG_PROPERTY(T01_order_independence, 1, "trace is independent of evaluation order, wire order and insertion history; in place differs",
             RunOn::Both) {
  forEachCase(ctx, ctx.cases(), [&](std::uint64_t, SplitMix64& rng) {
    const Model m = genAnyModel(rng);
    const unsigned rows = 8 + rng.pick(14);
    const std::size_t sensors = sensorIdsOf(m).size();
    const auto frames = genFrames(rng, sensors, rows, genStyle(rng));
    const bool randomState = rng.coin();
    BlobData st;
    if (randomState) st = randomLegalState(rng, m);
    const std::uint64_t permSeed = rng.next();
    const unsigned variant = rng.pick(3);
    const std::string why = t1Run(factory, m, frames, randomState ? &st : nullptr, permSeed, variant);
    SLG_CHECK(ctx, true);
    if (!why.empty()) {
      SLG_FAIL(ctx, cat(why, randomState ? "\n    (started from a random legal state, which lnet cannot express)" : ""));
      if (!randomState) {
        reportScenario(ctx, "T1", scenarioFromFrames(m, frames), [&](const Scenario& s) {
          return !t1Run(factory, s.net, framesOf(s), nullptr, permSeed, variant).empty();
        });
      }
    }
  });

  if (!factory.isSim) {
    // Negative control (03 section 2): in-place evaluation with two different orders must differ somewhere.
    bool differs = false;
    for (std::uint64_t k = 0; k < 400 && !differs; ++k) {
      SplitMix64 rng(ctx.caseSeed(1000000 + k));
      const Model m = genAnyModel(rng);
      if (m.comps.size() < 2) continue;
      ReferenceSim r1;
      ReferenceSim r2;
      r1.apply(opsForModel(m));
      r2.apply(opsForModel(m));
      const BlobData st = randomLegalState(rng, m);
      BlobData d = st;
      d.nextId = r1.nextId();
      r1.load(d);
      r2.load(d);
      const auto frames = genFrames(rng, sensorIdsOf(m).size(), 6, SampleStyle::Random);
      for (std::size_t t = 0; t < frames.size() && !differs; ++t) {
        r1.stepInPlace(frames[t], randomOrder(r1, rng));
        r2.stepInPlace(frames[t], randomOrder(r2, rng));
        differs = observeRef(r1).data.comps != observeRef(r2).data.comps;
      }
    }
    SLG_CHECK_MSG(ctx, differs, "negative control: in-place evaluation never depended on the order in 400 random circuits");
  }
}

}  // namespace slgt
