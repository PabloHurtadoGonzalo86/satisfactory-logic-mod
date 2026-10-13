// T11: edits at the tick boundary.
//  - apply(batch) + step == rebuilding the edited netlist with the state transplanted (survivors untouched,
//    new components and replaced ones at their seed) + step;
//  - an invalid batch or one beyond the limits leaves everything identical (netlist, state, tick, nextId,
//    hashes, counters) and answers the status the spec names;
//  - connect then remove of the same component is valid, remove then add of the same id is RetiredId,
//    replace keeps ids and wires, stale batches are refused, nextId never goes back, the limits bite;
//  - input frames: a frame of the wrong length is BadFrame, a sample above 15 saturates and is counted (I8).
// G8 and G9 (the Toggle edges) are the tests G08 and G09 of spec_examples.cpp.
// Not claimed: out-of-memory behaviour.
#include <map>
#include <set>

#include "bridge.hpp"
#include "engine.hpp"
#include "gen.hpp"
#include "testkit.hpp"

namespace slgt {

namespace {

GenState stateOfEngine(const Engine& e) {
  GenState gs;
  const Obs o = observe(e);
  for (const BlobComp& c : o.data.comps) gs.comps[c.id] = c.spec;
  gs.wires.insert(o.data.wires.begin(), o.data.wires.end());
  gs.nextId = e.nextId();
  return gs;
}

std::string unchanged(const Obs& before, const Obs& after) {
  const std::string d = diffObs(before, after, true);
  return d.empty() ? std::string() : cat("a rejected batch changed the engine: ", d);
}

// ---- A: transplant equivalence ------------------------------------------------------------------------------

std::string transplantRun(const EngineFactory& f, SplitMix64& rng) {
  const Model m = genAnyModel(rng);
  std::string err;
  auto e = makeEngine(f, m, err);
  if (e == nullptr) return err;
  ReferenceSim r;
  r.apply(opsForModel(m));
  if (rng.coin()) {
    BlobData st = randomLegalState(rng, m);
    if (injectState(*e, st) != Status::Ok) return "a legal random state was rejected by decode";
    st.nextId = r.nextId();
    r.load(st);
  }
  GenState gs = GenState::fromModel(m);
  const unsigned rows = 10 + rng.pick(14);
  for (unsigned t = 0; t < rows; ++t) {
    if (rng.chance(40, 100)) {
      const std::vector<EditOp> batch = genBatch(rng, gs, 4);
      const Obs pre = observe(*e);
      std::map<ComponentId, BlobComp> before;
      for (const BlobComp& c : pre.data.comps) before[c.id] = c;
      std::map<ComponentId, std::pair<Spec, unsigned>> fresh;   // added or replaced in this batch and still there
      for (const EditOp& op : batch) {
        if (op.kind == EditKind::Add || op.kind == EditKind::Replace) fresh[op.id] = {op.spec, op.seed};
        if (op.kind == EditKind::Remove) fresh.erase(op.id);
      }
      const ApplyResult rr = r.apply(batch);
      if (rr.status != Status::Ok) return cat("generator bug: the reference rejected a generated batch (", describe(rr), ")");
      const Status se = e->apply(batch);
      if (se != Status::Ok) return cat("row ", t, ": a valid batch was rejected: ", statusName(se));
      if (!e->lastBatchIdsMatched()) return cat("row ", t, ": EditBatch::add handed out an unexpected id");
      BlobData tr;
      tr.tick = e->tick();
      tr.nextId = r.nextId();
      tr.hostStepMicros = e->hostStepMicros();
      for (const auto& [id, c] : r.comps()) {
        BlobComp b;
        b.id = id;
        b.spec = c.spec;
        const auto fr = fresh.find(id);
        if (fr != fresh.end()) b.state = initialStateBytes(fr->second.first, fr->second.second);
        else b.state = before.find(id)->second.state;
        tr.comps.push_back(std::move(b));
      }
      tr.wires.assign(r.wires().begin(), r.wires().end());
      auto e2 = f.make(Limits{}, 0);
      const Status sl = e2->loadBlob(encodeBlob(tr));
      if (sl != Status::Ok) return cat("row ", t, ": the transplanted blob was rejected: ", statusName(sl));
      const std::string d = diffObs(observe(*e), observe(*e2), true);
      if (!d.empty()) return cat("row ", t, ": apply(batch) differs from the rebuilt netlist with transplanted state: ", d);
      const std::string d2 = diffObs(observe(*e), observeRef(r), true);
      if (!d2.empty()) return cat("row ", t, ": the engine differs from the reference after the batch: ", d2);
      std::vector<Signal> samples;
      for (std::size_t i = 0; i < e->sensorIds().size(); ++i) samples.push_back(u8(rng.pick(16)));
      e->step(samples);
      e2->step(samples);
      r.step(samples);
      const std::string d3 = diffObs(observe(*e), observe(*e2), true);
      if (!d3.empty()) return cat("row ", t, ": the step after the batch differs from the step of the rebuilt netlist: ", d3);
      if (!diffObs(observe(*e), observeRef(r), true).empty()) return cat("row ", t, ": the step after the batch differs from the reference");
    } else {
      std::vector<Signal> samples;
      for (std::size_t i = 0; i < e->sensorIds().size(); ++i) samples.push_back(u8(rng.pick(16)));
      e->step(samples);
      r.step(samples);
    }
  }
  return "";
}

// ---- B: faults leave everything untouched ----------------------------------------------------------------------

std::string faultRun(const EngineFactory& f, SplitMix64& rng, Fault fault) {
  const Model m = genAnyModel(rng);
  std::string err;
  auto e = makeEngine(f, m, err);
  if (e == nullptr) return err;
  ReferenceSim r;
  r.apply(opsForModel(m));
  for (unsigned t = 0; t < 3; ++t) {
    std::vector<Signal> samples;
    for (std::size_t i = 0; i < e->sensorIds().size(); ++i) samples.push_back(u8(rng.pick(16)));
    e->step(samples);
    r.step(samples);
  }
  GenState gs = stateOfEngine(*e);
  std::vector<EditOp> ops;
  if (!genFaultyBatch(rng, gs, fault, ops)) return "";
  const ApplyResult want = expectedForFault(fault);
  const ApplyResult refGot = ReferenceSim(r).apply(ops);
  if (!compatible(want, refGot.status) && refGot.status != Status::Ok) {
    return cat("generator bug: the reference answers ", describe(refGot), " to a ", faultName(fault), " batch, expected ", describe(want));
  }
  if (refGot.status == Status::Ok) return cat("generator bug: the ", faultName(fault), " batch is valid for the reference");
  const Obs before = observe(*e);
  const Status got = e->apply(ops);
  if (!compatible(want, got)) {
    return cat(faultName(fault), " batch: the core answered ", statusName(got), ", the spec says ", describe(want));
  }
  const Obs after = observe(*e);
  return unchanged(before, after);
}

// ---- the small fixed scenarios ------------------------------------------------------------------------------------

void checkOrderings(Ctx& ctx, const EngineFactory& f) {
  Model m;
  m.comps = {CompDef{1, specSensor(), 0}, CompDef{2, specNot(), 15}, CompDef{3, specActuator(), 0}};
  std::string err;
  // connect then remove of the same component is valid; the other order is not
  {
    auto e = makeEngine(f, m, err);
    if (e == nullptr) { SLG_FAIL(ctx, err); return; }
    const Status s = e->apply({opConnect(outPin(1), inPin(2, 0)), opRemove(2)});
    SLG_CHECK_MSG(ctx, s == Status::Ok, cat("connect then remove of the same component must be valid, got ", statusName(s)));
    const Obs o = observe(*e);
    SLG_CHECK_MSG(ctx, o.data.wires.empty() && o.data.comps.size() == 2, "remove must take its wires with it");
    const Status s2 = e->apply({opConnect(outPin(1), inPin(2, 0))});
    SLG_CHECK_MSG(ctx, s2 == Status::UnknownComponent || s2 == Status::RetiredId, cat("connecting to a removed component: ", statusName(s2)));
  }
  {
    auto e = makeEngine(f, m, err);
    if (e == nullptr) { SLG_FAIL(ctx, err); return; }
    const Obs before = observe(*e);
    const Status s = e->apply({opRemove(2), opConnect(outPin(1), inPin(2, 0))});
    SLG_CHECK_MSG(ctx, s == Status::UnknownComponent || s == Status::RetiredId, cat("remove then connect to it: ", statusName(s)));
    SLG_CHECK_MSG(ctx, unchanged(before, observe(*e)).empty(), "remove then connect must leave everything untouched");
  }
  // remove then add of the same id is RetiredId, in the same batch and in the next one
  {
    auto e = makeEngine(f, m, err);
    if (e == nullptr) { SLG_FAIL(ctx, err); return; }
    Obs before = observe(*e);
    Status s = e->apply({opRemove(2), opAdd(2, specNot(), 0)});
    SLG_CHECK_MSG(ctx, s == Status::RetiredId, cat("remove then add of the same id in one batch: ", statusName(s)));
    SLG_CHECK_MSG(ctx, unchanged(before, observe(*e)).empty(), "RetiredId must leave everything untouched");
    SLG_CHECK(ctx, e->apply({opRemove(2)}) == Status::Ok);
    before = observe(*e);
    s = e->apply({opAdd(2, specNot(), 0)});
    SLG_CHECK_MSG(ctx, s == Status::RetiredId, cat("add of a retired id in a later batch: ", statusName(s)));
    SLG_CHECK_MSG(ctx, e->nextId() == 4, cat("removing an id must not move nextId (", e->nextId(), ")"));
    SLG_CHECK(ctx, e->apply({opAdd(4, specSensor(), 0, true)}) == Status::Ok && e->lastBatchIdsMatched());
    SLG_CHECK_MSG(ctx, e->nextId() == 5, "nextId follows the highest id handed out");
  }
  // replace keeps the id and the wires, resets the state to the new seed, and refuses to orphan a wired pin
  {
    Model w;
    w.comps = {CompDef{1, specConst(15), 0}, CompDef{2, specToggle(), 15}, CompDef{3, specDelay(3), 9}, CompDef{4, specGate(GateOp::And, false, 3), 0}};
    w.wires = {makeWire(outPin(1), inPin(2, 0)), makeWire(outPin(1), inPin(3, 0)), makeWire(outPin(1), inPin(4, 2))};
    w.sortAndNormalize();
    auto e = makeEngine(f, w, err);
    if (e == nullptr) { SLG_FAIL(ctx, err); return; }
    e->step({});
    e->step({});
    SLG_CHECK(ctx, e->apply({opReplace(2, specToggle(), 0), opReplace(3, specDelay(5), 4)}) == Status::Ok);
    Obs o = observe(*e);
    bool ok = o.data.wires.size() == 3;
    for (const BlobComp& c : o.data.comps) {
      if (c.id == 2) ok = ok && c.state == std::vector<std::uint8_t>({0, 1});
      if (c.id == 3) ok = ok && c.state == std::vector<std::uint8_t>({4, 4, 4, 4, 4});
    }
    SLG_CHECK_MSG(ctx, ok, cat("replace must keep the wires and reset the state to the seed: ", describeState(o.data)));
    const Obs before = observe(*e);
    const Status s = e->apply({opReplace(4, specGate(GateOp::And, false, 2), 0)});   // pin 2 of the AND is wired
    SLG_CHECK_MSG(ctx, s == Status::BadPin, cat("replace that drops a wired pin: ", statusName(s)));
    SLG_CHECK_MSG(ctx, unchanged(before, observe(*e)).empty(), "BadPin must leave everything untouched");
    SLG_CHECK(ctx, e->apply({opDisconnect(outPin(1), inPin(4, 2)), opReplace(4, specGate(GateOp::And, false, 2), 0)}) == Status::Ok);
  }
}

void checkStaleAndExhaustion(Ctx& ctx, const EngineFactory& f) {
  std::string err;
  Model m;
  m.comps = {CompDef{1, specSensor(), 0}};
  auto e = makeEngine(f, m, err);
  if (e == nullptr) { SLG_FAIL(ctx, err); return; }
  const std::size_t stale = e->prepare({opAdd(2, specActuator(), 0, true)});
  SLG_CHECK(ctx, e->apply({opAdd(2, specLatch(), 0, true)}) == Status::Ok);
  const Obs before = observe(*e);
  const Status s = e->applyPrepared(stale);
  SLG_CHECK_MSG(ctx, s == Status::StaleBatch, cat("a batch taken before nextId moved: ", statusName(s)));
  SLG_CHECK_MSG(ctx, unchanged(before, observe(*e)).empty(), "StaleBatch must leave everything untouched");
  const std::size_t fine = e->prepare({opAdd(3, specActuator(), 0, true)});
  SLG_CHECK_MSG(ctx, e->applyPrepared(fine) == Status::Ok, "a batch taken right before applying is not stale");

  // nextId = 0xFFFFFFFF: add() gives LimitExceeded, an explicit 0xFFFFFFFF is refused too
  auto x = f.make(Limits{}, 0);
  SLG_CHECK(ctx, x->apply({opAdd(0xFFFFFFFEU, specConst(3), 0)}) == Status::Ok);
  SLG_CHECK_MSG(ctx, x->nextId() == 0xFFFFFFFFU, cat("nextId after adding id 0xFFFFFFFE: ", x->nextId()));
  const Obs b2 = observe(*x);
  const Status s2 = x->apply({opAdd(0, specConst(1), 0, true)});
  SLG_CHECK_MSG(ctx, s2 == Status::LimitExceeded, cat("add() with nextId = 0xFFFFFFFF: ", statusName(s2)));
  SLG_CHECK_MSG(ctx, unchanged(b2, observe(*x)).empty(), "LimitExceeded must leave everything untouched");
  SLG_CHECK_MSG(ctx, x->apply({opAdd(0xFFFFFFFFU, specConst(1), 0)}) != Status::Ok, "id 0xFFFFFFFF is outside 1..0xFFFFFFFE");
  SLG_CHECK_MSG(ctx, unchanged(b2, observe(*x)).empty(), "the refused id must leave everything untouched");
}

void checkLimits(Ctx& ctx, const EngineFactory& f) {
  auto check = [&](const char* what, Limits lim, const std::vector<EditOp>& setup, const std::vector<EditOp>& ok,
                   const std::vector<EditOp>& tooMuch, ApplyResult want) {
    auto e = f.make(lim, 0);
    if (e->apply(setup) != Status::Ok) {
      SLG_FAIL(ctx, cat(what, ": the setup batch was rejected"));
      return;
    }
    const Status s = e->apply(ok);
    SLG_CHECK_MSG(ctx, s == Status::Ok, cat(what, ": exactly at the limit must be accepted, got ", statusName(s)));
    auto g = f.make(lim, 0);
    g->apply(setup);
    g->apply(ok);
    const Obs before = observe(*g);
    const Status s2 = g->apply(tooMuch);
    SLG_CHECK_MSG(ctx, compatible(want, s2), cat(what, ": one past the limit answered ", statusName(s2), ", expected ", describe(want)));
    SLG_CHECK_MSG(ctx, unchanged(before, observe(*g)).empty(), cat(what, ": the batch beyond the limit changed the engine"));
  };
  ApplyResult limit;
  limit.status = Status::LimitExceeded;
  ApplyResult paramLimit;
  paramLimit.status = Status::BadSpec;
  paramLimit.soft = ApplyResult::Soft::ParamLimit;
  Limits l;
  l = Limits{};
  l.maxComponents = 3;
  check("maxComponents", l, {opAdd(1, specSensor(), 0)}, {opAdd(2, specSensor(), 0), opAdd(3, specSensor(), 0)}, {opAdd(4, specSensor(), 0)}, limit);
  l = Limits{};
  l.maxWires = 2;
  check("maxWires", l, {opAdd(1, specConst(1), 0), opAdd(2, specActuator(), 0), opAdd(3, specActuator(), 0), opAdd(4, specActuator(), 0)},
        {opConnect(outPin(1), inPin(2, 0)), opConnect(outPin(1), inPin(3, 0))}, {opConnect(outPin(1), inPin(4, 0))}, limit);
  l = Limits{};
  l.maxNetPins = 4;
  check("maxNetPins", l, {opAdd(1, specConst(1), 0), opAdd(2, specActuator(), 0), opAdd(3, specActuator(), 0), opAdd(4, specActuator(), 0), opAdd(5, specActuator(), 0)},
        {opConnect(outPin(1), inPin(2, 0)), opConnect(outPin(1), inPin(3, 0)), opConnect(outPin(1), inPin(4, 0))}, {opConnect(outPin(1), inPin(5, 0))}, limit);
  l = Limits{};
  l.maxDelayCells = 5;
  check("maxDelayCells", l, {opAdd(1, specDelay(3), 0)}, {opAdd(2, specDelay(2), 0)}, {opAdd(3, specDelay(1), 0)}, limit);
  l = Limits{};
  l.maxDelay = 4;
  check("maxDelay", l, {}, {opAdd(1, specDelay(4), 0)}, {opAdd(2, specDelay(5), 0)}, paramLimit);
  l = Limits{};
  l.maxArity = 3;
  check("maxArity", l, {}, {opAdd(1, specGate(GateOp::And, false, 3), 0)}, {opAdd(2, specGate(GateOp::And, false, 4), 0)}, paramLimit);
  l = Limits{};
  l.maxClockPeriod = 6;
  check("maxClockPeriod", l, {}, {opAdd(1, specClock(6, 3), 0)}, {opAdd(2, specClock(7, 3), 0)}, paramLimit);
}

void checkFrames(Ctx& ctx, const EngineFactory& f) {
  std::string err;
  Model m;
  m.comps = {CompDef{1, specSensor(), 0}, CompDef{2, specActuator(), 0}, CompDef{3, specSensor(), 0}};
  m.wires = {makeWire(outPin(1), inPin(2, 0))};
  m.sortAndNormalize();
  auto e = makeEngine(f, m, err);
  if (e == nullptr) { SLG_FAIL(ctx, err); return; }
  e->step({3, 4});
  const Obs before = observe(*e);
  for (const std::vector<Signal>& bad : {std::vector<Signal>{}, std::vector<Signal>{1}, std::vector<Signal>{1, 2, 3}}) {
    const Status s = e->step(bad);
    SLG_CHECK_MSG(ctx, s == Status::BadFrame, cat("a frame of ", bad.size(), " samples for 2 sensors: ", statusName(s)));
    SLG_CHECK_MSG(ctx, unchanged(before, observe(*e)).empty(), "BadFrame must leave everything untouched (tick and counters included)");
  }
  SLG_CHECK(ctx, e->step({200, 255}) == Status::Ok);
  Obs o = observe(*e);
  SLG_CHECK_MSG(ctx, o.clamped == 2, cat("clampedSamples() after two oversized samples: ", o.clamped));
  SLG_CHECK_MSG(ctx, o.commands.size() == 1 && o.commands[0] == 15, cat("a sample of 200 must act as 15, the actuator holds ", o.commands.empty() ? 999U : static_cast<unsigned>(o.commands[0])));
  e->step({0, 0});
  o = observe(*e);
  SLG_CHECK_MSG(ctx, o.clamped == 2, "clampedSamples() only grows with oversized samples");
}

// Netlist::apply on its own: the structure (ascending ids, normalized sorted wires, nextId) and a structureHash that
// does not depend on seeds and equals the FNV of 02 section 6.2 computed here.
void checkNetlistAlone(Ctx& ctx, std::uint64_t cases) {
  forEachCase(ctx, cases, [&](std::uint64_t, SplitMix64& rng) {
    Model m = genAnyModel(rng);
    const bridge::NetlistInfo a = bridge::netlistDirect(m);
    if (!SLG_CHECK_MSG(ctx, a.status == Status::Ok, cat("Netlist::apply of a valid batch: ", statusName(a.status)))) return;
    std::vector<std::pair<ComponentId, Spec>> comps;
    for (const CompDef& c : m.comps) comps.emplace_back(c.id, c.spec);
    SLG_CHECK_MSG(ctx, a.comps == comps, "Netlist::components() is not the ascending list of the batch's components");
    SLG_CHECK_MSG(ctx, a.wires == m.wires, "Netlist::wires() is not the normalized, sorted, unique list of the batch's wires");
    SLG_CHECK_MSG(ctx, a.nextId == m.nextIdHint(), cat("Netlist::nextId() = ", a.nextId, ", want ", m.nextIdHint()));
    SLG_CHECK_MSG(ctx, a.structureHash == structureHashOf(blobFromModel(m)),
                  cat("structureHash ", hex(a.structureHash), ", the FNV of the structure is ", hex(structureHashOf(blobFromModel(m)))));
    for (CompDef& c : m.comps) c.seed = u16(genSeed(rng, c.spec));
    SLG_CHECK_MSG(ctx, bridge::netlistDirect(m).structureHash == a.structureHash, "structureHash must not depend on the seeds");
  });
}

}  // namespace

SLG_PROPERTY(T11_edit_semantics, 11, "batches are atomic at the tick boundary; survivors keep their state; faults answer the spec's status and change nothing",
             RunOn::Both) {
  forEachCase(ctx, ctx.cases(), [&](std::uint64_t, SplitMix64& rng) {
    const std::string why = transplantRun(factory, rng);
    if (!SLG_CHECK_MSG(ctx, why.empty(), why)) return;
    const Fault fault = static_cast<Fault>(rng.pick(static_cast<unsigned>(Fault::kCount)));
    const std::string why2 = faultRun(factory, rng, fault);
    SLG_CHECK_MSG(ctx, why2.empty(), why2);
  });
  if (factory.isSim) checkNetlistAlone(ctx, ctx.cases(1, 2));
  checkOrderings(ctx, factory);
  checkStaleAndExhaustion(ctx, factory);
  checkLimits(ctx, factory);
  checkFrames(ctx, factory);
}

}  // namespace slgt
