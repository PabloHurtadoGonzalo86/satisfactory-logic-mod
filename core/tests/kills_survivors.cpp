// Tests added because a mutant of core/tests/mutants/ survived the suite (core/tests/mutants/README.md names the
// mutant each one kills). They assert rules of the spec (or the documented contract of the core's own header) through
// observable behaviour, never through the implementation, and carry the name of the property they extend
// (T05_..., T07_..., T12_..., T13_...) so that the patch headers can name them like any other test.
// Not claimed: anything the spec leaves open; each test cites the section it rests on.
#include <algorithm>
#include <string>
#include <vector>

#include "bridge.hpp"
#include "engine.hpp"
#include "testkit.hpp"

namespace slgt {

namespace {

// The state bytes of one component, read through the independent blob parser (the only window into the state).
bool stateOf(const Engine& e, ComponentId id, std::vector<std::uint8_t>& out) {
  const Obs o = observe(e);
  if (!o.parsed) return false;
  for (const BlobComp& c : o.data.comps) {
    if (c.id == id) {
      out = c.state;
      return true;
    }
  }
  return false;
}

// 01 section 3: c' = (c + 1) mod P and the output is 15 when c >= P - H; 02 section 6.1: c is a u16, little-endian, so
// every P above 256 needs the second byte. The actuator reads the output one tick later (01 section 2.5).
void checkClockLap(Ctx& ctx, const EngineFactory& f, unsigned p, unsigned h, unsigned seed, unsigned ticks) {
  Model m;
  m.comps = {CompDef{1, specClock(p, h), u16(seed)}, CompDef{2, specActuator(), 0}};
  m.wires = {makeWire(outPin(1), inPin(2, 0))};
  m.sortAndNormalize();
  std::string err;
  auto e = makeEngine(f, m, err);
  if (e == nullptr) {
    SLG_FAIL(ctx, err);
    return;
  }
  for (unsigned t = 0; t <= ticks; ++t) {
    std::vector<std::uint8_t> clock;
    std::vector<std::uint8_t> actuator;
    if (!stateOf(*e, 1, clock) || !stateOf(*e, 2, actuator) || clock.size() != 2 || actuator.size() != 1) {
      SLG_FAIL(ctx, "cannot read the state of the clock and of its actuator");
      return;
    }
    const unsigned want = (seed + t) % p;
    const unsigned got = clock[0] + 256U * clock[1];
    const unsigned before = (seed + t + p - 1) % p;
    const unsigned wantOut = t == 0 ? 0U : (before >= p - h ? 15U : 0U);
    const bool ok = got == want && actuator[0] == wantOut;
    if (!SLG_CHECK_MSG(ctx, ok, cat("Clock(P=", p, ", H=", h, ") created at phase ", seed, ", tick ", t, ": c = ", got, " (want ", want, "), actuator ",
                                    static_cast<unsigned>(actuator[0]), " (want ", wantOut, ")"))) {
      return;
    }
    e->step({});
  }
}

using Changes = std::vector<bridge::DriverChange>;

// One call of `steps` steps that took `elapsed` microseconds; whatever it delivers is acknowledged.
Changes oneCall(bridge::Driver& d, std::uint32_t steps, std::uint32_t elapsed) {
  d.runCall(steps, elapsed);
  const Changes c = d.changes();
  for (const bridge::DriverChange& ch : c) d.acknowledge(ch.id);
  d.endCall();
  return c;
}

std::unique_ptr<bridge::Driver> driverOf(Ctx& ctx, const Model& m, const bridge::DriverOptions& opt) {
  auto d = bridge::makeDriver(Limits{}, 0, opt);
  const Status s = d->apply(opsForModel(m));
  if (s != Status::Ok) {
    SLG_FAIL(ctx, cat("FrameDriver::apply of the netlist: ", statusName(s)));
    return nullptr;
  }
  return d;
}

}  // namespace

SLG_PROPERTY(T05_clock_wide_period, 5,
             "Clock with P above 256: both bytes of the phase, seeds above 255, the wrap at P and the output threshold (01 section 3, 02 section 6.1)",
             RunOn::Both) {
  struct Lap {
    unsigned p, h, seed, ticks;
  };
  static const Lap laps[] = {{257, 3, 256, 6},    {300, 100, 255, 310}, {300, 20, 290, 40},   {512, 256, 511, 6},
                             {1000, 999, 511, 8}, {4096, 2048, 4093, 8}, {4096, 1, 4095, 4}, {256, 128, 255, 6}};
  for (const Lap& lap : laps) {
    checkClockLap(ctx, factory, lap.p, lap.h, lap.seed, lap.ticks);
    if (ctx.tooManyFailures()) return;
  }
}

// 02 section 6.3: ids ascending (so no repeats), wires "normalizados, ascendentes y con ambos pines existentes" (a < b
// strictly and no repeats). Each probe breaks one rule in an otherwise valid blob and recomputes the tail, so only the
// structural check can refuse it.
SLG_PROPERTY(T12_blob_order_invariants, 12,
             "decode refuses repeated or descending ids, a wire from a pin to itself and repeated or descending wires, tail recomputed (02 section 6.3)",
             RunOn::Both) {
  Model m;
  m.comps = {CompDef{1, specSensor(), 0}, CompDef{2, specSensor(), 0}, CompDef{3, specGate(GateOp::Or, false, 2), 0}, CompDef{4, specActuator(), 0},
             CompDef{5, specConst(3), 0},  CompDef{6, specConst(4), 0}};   // 5 and 6 carry no wire: their ids can be broken on their own
  m.wires = {makeWire(outPin(1), inPin(3, 0)), makeWire(outPin(1), inPin(3, 1)), makeWire(outPin(2), inPin(3, 1)), makeWire(outPin(3), inPin(4, 0))};
  m.sortAndNormalize();   // ascending: (1.o,3.0) (1.o,3.1) (2.o,3.1) (3.o,4.0): the first two share their lower pin
  std::string err;
  auto source = makeEngine(factory, m, err);
  if (source == nullptr) {
    SLG_FAIL(ctx, err);
    return;
  }
  const std::vector<std::uint8_t> good = source->encode();
  BlobLayout layout;
  if (!layoutOf(good, layout) || layout.compOffset.size() != 6) {
    SLG_FAIL(ctx, "cannot lay out the blob of the probe netlist");
    return;
  }
  const std::size_t wire = layout.wiresOffset;   // 10 bytes per wire: u32 id, u8 index, u32 id, u8 index
  const auto status = [&](const std::vector<std::uint8_t>& bytes) {
    auto victim = factory.make(Limits{}, 0);
    return victim->decodeDirect(bytes, Limits{});
  };
  SLG_CHECK_MSG(ctx, status(good) == Status::Ok, "the unmodified blob of the probe netlist must decode");
  const auto probe = [&](const char* what, const auto& edit) {
    std::vector<std::uint8_t> b = good;
    edit(b);
    refreshTail(b);
    const Status s = status(b);
    SLG_CHECK_MSG(ctx, s == Status::FormatCorrupt, cat(what, ": decode answered ", statusName(s), ", 02 section 6.3 says FormatCorrupt"));
  };
  const auto at = [](std::vector<std::uint8_t>& b, std::size_t offset) { return b.begin() + static_cast<std::ptrdiff_t>(offset); };
  probe("a repeated component id", [&](std::vector<std::uint8_t>& b) { std::copy(at(b, layout.compOffset[4]), at(b, layout.compOffset[4] + 4), at(b, layout.compOffset[5])); });
  probe("descending component ids", [&](std::vector<std::uint8_t>& b) {
    std::swap_ranges(at(b, layout.compOffset[4]), at(b, layout.compOffset[4] + 4), at(b, layout.compOffset[5]));
  });
  probe("a wire from a pin to itself", [&](std::vector<std::uint8_t>& b) { std::copy(at(b, wire), at(b, wire + 5), at(b, wire + 5)); });
  probe("a repeated wire", [&](std::vector<std::uint8_t>& b) { std::copy(at(b, wire), at(b, wire + 10), at(b, wire + 10)); });
  probe("descending wires with the same lower pin", [&](std::vector<std::uint8_t>& b) {
    std::swap_ranges(at(b, wire), at(b, wire + 10), at(b, wire + 10));
  });
}

// 02 section 6.2: header (32 bytes) + tail (8 bytes) is the whole blob of an empty netlist, the shortest one decode must
// accept; 02 section 6.3 asks for no byte more, so the minimum is not an error.
SLG_PROPERTY(T12_blob_empty_netlist, 12, "the shortest legal blob (no components, no wires: 32 + 8 bytes) decodes and encodes to itself (02 sections 6.2 and 6.3)",
             RunOn::Both) {
  auto empty = factory.make(Limits{}, 0);
  const std::vector<std::uint8_t> blob = empty->encode();
  SLG_CHECK_MSG(ctx, blob.size() == kBlobHeaderBytes + kBlobTailBytes, cat("the blob of an empty netlist has ", blob.size(), " bytes, want ", kBlobHeaderBytes + kBlobTailBytes));
  auto fresh = factory.make(Limits{}, 0);
  const Status s = fresh->decodeDirect(blob, Limits{});
  SLG_CHECK_MSG(ctx, s == Status::Ok, cat("decode of the blob of an empty netlist: ", statusName(s)));
  SLG_CHECK_MSG(ctx, fresh->encode() == blob, "encode(decode(b)) != b for the blob of an empty netlist");
  SLG_CHECK_MSG(ctx, fresh->nextId() == 1 && fresh->tick() == 0, "an empty netlist restores with nextId = 1 and tick = 0");
}

// lint.hpp: multiDriverNets lists "every pin of the net, ascending". 02 section 2 only says "merged by max", and T7 accepts
// any set between the drivers and all the pins; this pins the documented reading, and the same reading for every net
// (a net with two conductors and one with three in the same report).
SLG_PROPERTY(T07_multidriver_every_pin, 7, "a multi-driver net is reported with every pin it has, drivers and readers, ascending (lint.hpp)", RunOn::SimOnly) {
  Model m;   // net A: three drivers and a reader; net B: two drivers and two readers; net C: two drivers and no reader
  m.comps = {CompDef{1, specConst(5), 0},  CompDef{2, specConst(7), 0}, CompDef{3, specConst(9), 0}, CompDef{4, specConst(1), 0},
             CompDef{5, specConst(2), 0},  CompDef{6, specConst(3), 0}, CompDef{7, specConst(4), 0}, CompDef{10, specActuator(), 0},
             CompDef{11, specActuator(), 0}, CompDef{12, specNot(), 15}};
  m.wires = {makeWire(outPin(1), inPin(10, 0)), makeWire(outPin(2), inPin(10, 0)), makeWire(outPin(3), inPin(10, 0)),
             makeWire(outPin(4), inPin(11, 0)), makeWire(outPin(5), inPin(11, 0)), makeWire(inPin(11, 0), inPin(12, 0)),
             makeWire(outPin(6), outPin(7))};
  m.sortAndNormalize();
  bridge::FeedbackReport report;
  const Status s = bridge::analyze(m, Limits{}, report);
  SLG_CHECK_MSG(ctx, s == Status::Ok, cat("building the probe netlist: ", statusName(s)));
  std::vector<std::vector<Pin>> got = report.multiDriverNets;
  for (const std::vector<Pin>& net : got) SLG_CHECK_MSG(ctx, std::is_sorted(net.begin(), net.end()), "the pins of a reported net must be ascending");
  std::sort(got.begin(), got.end());
  const std::vector<std::vector<Pin>> want{{outPin(1), outPin(2), outPin(3), inPin(10, 0)},
                                           {outPin(4), outPin(5), inPin(11, 0), inPin(12, 0)},
                                           {outPin(6), outPin(7)}};
  SLG_CHECK_MSG(ctx, got == want, cat("multiDriverNets: ", got.size(), " nets reported; each must hold every pin of its net, readers included"));
}

// 02 section 2 (decision D30): add() hands out the base nextId plus the number of add() calls before it; an explicit id does
// not move that count, so one that sits where add() will arrive makes the batch fail at the add(): DuplicateId, or RetiredId
// when the explicit component was removed again. What add() returned is what the batch creates, and a refused batch changes
// nothing (I3). The ReferenceSim counts the same way, so the property runs on both engines.
SLG_PROPERTY(T02_add_returns_the_assigned_id, 2,
             "EditBatch::add returns the base nextId plus the add() calls before it; an explicit id does not move it and a collision is DuplicateId or RetiredId (02 sections 2 and 4)",
             RunOn::Both) {
  struct Case {
    const char* what;
    std::vector<EditOp> ops;   // opAdd(id, ..., true) names the id add() must return
    Status want;
    std::vector<ComponentId> ids;   // the components afterwards (Ok only)
    ComponentId nextId;
  };
  const Spec a = specActuator();
  const Spec s = specSensor();
  const Spec k = specConst(3);
  const std::vector<Case> cases = {
      {"explicit 1, then add() (which returns 1)", {opAdd(1, s, 0), opAdd(1, a, 0, true)}, Status::DuplicateId, {}, 0},
      {"add() (1), explicit 2, add() (2)", {opAdd(1, s, 0, true), opAdd(2, a, 0), opAdd(2, k, 0, true)}, Status::DuplicateId, {}, 0},
      {"explicit 2, then add() (1)", {opAdd(2, s, 0), opAdd(1, a, 0, true)}, Status::Ok, {1, 2}, 3},
      {"explicit 3, then add() (1), add() (2)", {opAdd(3, k, 0), opAdd(1, s, 0, true), opAdd(2, a, 0, true)}, Status::Ok, {1, 2, 3}, 4},
      {"add() (1), explicit 9, add() (2)", {opAdd(1, s, 0, true), opAdd(9, k, 0), opAdd(2, a, 0, true)}, Status::Ok, {1, 2, 9}, 10},
      {"explicit 1, removed, then add() (which returns 1)", {opAdd(1, s, 0), opRemove(1), opAdd(1, a, 0, true)}, Status::RetiredId, {}, 0},
      {"add() (1), removed, explicit 1", {opAdd(1, s, 0, true), opRemove(1), opAdd(1, a, 0)}, Status::RetiredId, {}, 0},
  };
  for (const Case& c : cases) {
    auto e = factory.make(Limits{}, 0);
    const Obs before = observe(*e);
    const Status got = e->apply(c.ops);
    SLG_CHECK_MSG(ctx, got == c.want, cat(c.what, ": apply answered ", statusName(got), ", want ", statusName(c.want)));
    SLG_CHECK_MSG(ctx, e->lastBatchIdsMatched(), cat(c.what, ": add() did not return the id the batch gives the new component"));
    const Obs after = observe(*e);
    if (c.want != Status::Ok) {
      SLG_CHECK_MSG(ctx, diffObs(before, after, true).empty(), cat(c.what, ": the refused batch changed the engine"));
      continue;
    }
    std::vector<ComponentId> ids;
    for (const BlobComp& comp : after.data.comps) ids.push_back(comp.id);
    SLG_CHECK_MSG(ctx, ids == c.ids && e->nextId() == c.nextId, cat(c.what, ": the batch left ", ids.size(), " components and nextId ", e->nextId()));
  }
}

// 02 section 5 (ambiguities.md B4): a wire from a pin to itself is BadPin, and a refused batch changes nothing (I3), also
// when the offending operation comes after valid ones.
SLG_PROPERTY(T11_self_wire_is_bad_pin, 11, "connecting a pin to itself is BadPin and leaves everything untouched, alone or inside a batch (02 section 5, I3)",
             RunOn::Both) {
  Model m;
  m.comps = {CompDef{1, specSensor(), 0}, CompDef{2, specNot(), 0}};
  m.sortAndNormalize();
  std::string err;
  auto e = makeEngine(factory, m, err);
  if (e == nullptr) {
    SLG_FAIL(ctx, err);
    return;
  }
  const Obs before = observe(*e);
  for (const Pin& pin : {outPin(1), inPin(2, 0), outPin(2)}) {
    const Status s = e->apply({opConnect(pin, pin)});
    SLG_CHECK_MSG(ctx, s == Status::BadPin, cat("connect of the pin ", pin.component, ".", static_cast<unsigned>(pin.index), " to itself: ", statusName(s), ", want BadPin"));
    const std::string diff = diffObs(before, observe(*e), true);
    SLG_CHECK_MSG(ctx, diff.empty(), cat("the refused self wire changed the engine: ", diff));
  }
  const Status s = e->apply({opConnect(outPin(1), inPin(2, 0)), opConnect(inPin(2, 0), inPin(2, 0))});
  SLG_CHECK_MSG(ctx, s == Status::BadPin, cat("a batch that ends with a self wire: ", statusName(s), ", want BadPin"));
  const std::string diff = diffObs(before, observe(*e), true);
  SLG_CHECK_MSG(ctx, diff.empty(), cat("the batch with a self wire was applied in part: ", diff));
}

// 02 section 4: "un lote con otra base da StaleBatch" holds for every batch, the empty one included.
SLG_PROPERTY(T11_empty_batch_stale, 11, "an empty batch taken before nextId moved is StaleBatch, one taken now is Ok (02 section 4)", RunOn::Both) {
  Model m;
  m.comps = {CompDef{1, specSensor(), 0}};
  std::string err;
  auto e = makeEngine(factory, m, err);
  if (e == nullptr) {
    SLG_FAIL(ctx, err);
    return;
  }
  const std::size_t stale = e->prepare({});
  SLG_CHECK(ctx, e->apply({opAdd(2, specActuator(), 0, true)}) == Status::Ok);
  const Obs before = observe(*e);
  const Status s = e->applyPrepared(stale);
  SLG_CHECK_MSG(ctx, s == Status::StaleBatch, cat("an empty batch taken before nextId moved: ", statusName(s), ", want StaleBatch"));
  const std::string diff = diffObs(before, observe(*e), true);
  SLG_CHECK_MSG(ctx, diff.empty(), cat("the stale empty batch changed the engine: ", diff));
  const Status now = e->applyPrepared(e->prepare({}));
  SLG_CHECK_MSG(ctx, now == Status::Ok, cat("an empty batch taken right before applying it: ", statusName(now), ", want Ok"));
}

SLG_PROPERTY(T13_driver_resolve_boundary, 13,
             "a sensor with no reading stops holding the actuators back after exactly ceil(resolveTimeoutMicros / step) executed steps, whatever elapsedMicros says (02 section 7.4)",
             RunOn::SimOnly) {
  bridge::DriverOptions opt;
  opt.settleGuard = false;
  opt.resolveTimeoutMicros = 1000000;   // hostStepMicros 0 is the default step of 100000, so the limit is 10 steps
  Model m;   // sensor 1 -> NOT 2 (born 15) -> actuator 3: with no reading the actuator goes 0 -> 15 after one step
  m.comps = {CompDef{1, specSensor(), 0}, CompDef{2, specNot(), 15}, CompDef{3, specActuator(), 0}};
  m.wires = {makeWire(outPin(1), inPin(2, 0)), makeWire(outPin(2), inPin(3, 0))};
  m.sortAndNormalize();
  auto exact = driverOf(ctx, m, opt);
  auto below = driverOf(ctx, m, opt);
  auto split = driverOf(ctx, m, opt);
  if (exact == nullptr || below == nullptr || split == nullptr) return;
  const Changes atTimeout = oneCall(*exact, 10, 0);
  SLG_CHECK_MSG(ctx, atTimeout.size() == 1 && atTimeout[0].id == 3 && atTimeout[0].previous == 0 && atTimeout[0].value == 15,
                cat("after exactly 10 executed steps the sensor is no longer unresolved and the change 3:0>15 is delivered even though the call reports no time, got ", atTimeout.size(), " changes"));
  const Changes underTimeout = oneCall(*below, 9, 0xFFFFFFFFU);
  SLG_CHECK_MSG(ctx, underTimeout.empty(), cat("9 steps are one short of the timeout and the elapsed time does not count, so the sensor is still unresolved, got ", underTimeout.size(), " changes"));
  const Changes first = oneCall(*split, 9, 0xFFFFFFFFU);
  const Changes second = oneCall(*split, 1, 0);
  SLG_CHECK_MSG(ctx, first.empty() && second.size() == 1 && second[0].id == 3,
                "the steps of the calls add up: 9 + 1 reach the timeout, and the change is delivered by the second call");
}

// frame_driver.hpp names WrongPhase for apply only; ambiguities-cpp.md A11 records that runCall and decode refuse the
// same window (a second runCall before endCall would run steps whose changes() the first call has not closed).
SLG_PROPERTY(T13_driver_phase_misuse, 13,
             "runCall and decode between runCall and endCall answer WrongPhase and change nothing; after endCall both work again (ambiguities-cpp A11)",
             RunOn::SimOnly) {
  bridge::DriverOptions opt;
  opt.settleGuard = false;
  Model m;
  m.comps = {CompDef{1, specSensor(), 0}, CompDef{2, specActuator(), 0}};
  m.wires = {makeWire(outPin(1), inPin(2, 0))};
  m.sortAndNormalize();
  auto d = driverOf(ctx, m, opt);
  if (d == nullptr) return;
  d->offer(1, 15);
  SLG_CHECK_MSG(ctx, d->runCall(1, 100000) == Status::Ok, "the first runCall");
  const Tick tick = d->simTick();
  const std::vector<std::uint8_t> simBefore = d->simEncode();
  const Status again = d->runCall(1, 100000);
  SLG_CHECK_MSG(ctx, again == Status::WrongPhase, cat("runCall between runCall and endCall: ", statusName(again), ", want WrongPhase"));
  SLG_CHECK_MSG(ctx, d->simTick() == tick && d->simEncode() == simBefore, "a refused runCall must not run a step");
  const std::vector<std::uint8_t> blob = d->encode();
  const Status decoded = d->decode(blob);
  SLG_CHECK_MSG(ctx, decoded == Status::WrongPhase, cat("decode between runCall and endCall: ", statusName(decoded), ", want WrongPhase"));
  d->endCall();
  SLG_CHECK_MSG(ctx, d->decode(blob) == Status::Ok, "decode of its own blob after endCall");
  SLG_CHECK_MSG(ctx, d->runCall(1, 100000) == Status::Ok, "runCall after endCall");
  d->endCall();
}

// ambiguities-cpp.md A11: sensors and actuators created directly in the Simulator (not through FrameDriver::apply) are
// picked up by comparing structure hashes at every runCall, apply, offer, setSensorMode and resync; a reading offered for
// such a sensor must not be dropped.
SLG_PROPERTY(T13_driver_offer_reconciles, 13,
             "offer for a sensor created behind the driver's back is kept: the sensor resolves and its actuator is delivered (ambiguities-cpp A11)",
             RunOn::SimOnly) {
  bridge::DriverOptions opt;
  opt.settleGuard = false;
  Model m;   // sensor 1 -> actuator 2, built through the driver
  m.comps = {CompDef{1, specSensor(), 0}, CompDef{2, specActuator(), 0}};
  m.wires = {makeWire(outPin(1), inPin(2, 0))};
  m.sortAndNormalize();
  auto d = driverOf(ctx, m, opt);
  if (d == nullptr) return;
  d->offer(1, 0);
  const Status edit = d->simApply({opAdd(3, specSensor(), 0), opAdd(4, specActuator(), 0), opConnect(outPin(3), inPin(4, 0))});
  SLG_CHECK_MSG(ctx, edit == Status::Ok, cat("the edit applied to the simulator itself: ", statusName(edit)));
  d->offer(3, 15);   // the driver has not seen sensor 3 yet
  const Changes c = oneCall(*d, 1, 100000);
  SLG_CHECK_MSG(ctx, c.size() == 1 && c[0].id == 4 && c[0].previous == 0 && c[0].value == 15,
                cat("the reading offered for the new sensor must resolve it and drive actuator 4 to 15 in this call, got ", c.size(), " changes"));
}

SLG_PROPERTY(T13_driver_settle_exact, 13,
             "the settling guard holds an actuator for exactly L_a steps: the call that runs step L_a delivers the change (02 section 7.5, 01 section 2.5)",
             RunOn::SimOnly) {
  for (unsigned delays = 0; delays <= 4; ++delays) {
    // sensor 1 -> Delay(1) x delays -> actuator: weights 0 + delays + 1, so L_a = delays + 1 and the command first reads 15 in X_{L_a}
    Model m;
    m.comps.push_back(CompDef{1, specSensor(), 0});
    for (unsigned i = 0; i < delays; ++i) m.comps.push_back(CompDef{2 + i, specDelay(1), 0});
    const ComponentId actuator = 2 + delays;
    m.comps.push_back(CompDef{actuator, specActuator(), 0});
    for (unsigned i = 0; i <= delays; ++i) m.wires.push_back(makeWire(outPin(1 + i), inPin(2 + i, 0)));
    m.sortAndNormalize();
    bridge::DriverOptions opt;
    opt.settleGuard = true;
    auto d = driverOf(ctx, m, opt);
    if (d == nullptr) return;
    d->offer(1, 15);
    for (unsigned call = 1; call <= delays + 3; ++call) {
      const Changes c = oneCall(*d, 1, 100000);
      const bool due = call == delays + 1;
      const bool ok = due ? (c.size() == 1 && c[0].id == actuator && c[0].previous == 0 && c[0].value == 15) : c.empty();
      SLG_CHECK_MSG(ctx, ok, cat(delays, " Delay(1) before the actuator (L_a = ", delays + 1, "), call ", call, ": ", c.size(), " changes, ",
                                 due ? "the change 0>15 was due now" : "nothing was due"));
    }
  }
}

}  // namespace slgt
