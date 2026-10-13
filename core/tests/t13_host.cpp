// T13: the host layer.
//  StepPacer: steps <= max, 0 <= acc < step and the conservation of 02 section 7.3 (G12), against an exact
//  model of the accumulator; fullness() against its closed form.
//  FrameDriver: the burst of G11, n steps in one call == n calls of one step for constant Level sensors (same
//  stateHash), Pulse differs by design, retention of the last good Level value, unresolved sensors, the
//  settling guard, pending deliveries, resync, the phase rule, and the round trip of its own blob.
// Not claimed: that the world sees the same thing under another grouping of calls.
#include <algorithm>
#include <array>
#include <map>

#include "bridge.hpp"
#include "engine.hpp"
#include "gen.hpp"
#include "spec_examples.hpp"
#include "testkit.hpp"

namespace slgt {

namespace {

// ---- pacer --------------------------------------------------------------------------------------------------

void checkPacerExample(Ctx& ctx) {
  auto p = bridge::makePacer(100000, 4);
  const std::uint32_t dts[3] = {250000, 30000, 530000};
  const std::uint32_t wantSteps[3] = {2, 0, 4};
  std::uint64_t sumDt = 0;
  std::uint64_t sumSteps = 0;
  for (unsigned i = 0; i < 3; ++i) {
    const std::uint32_t n = p->advance(dts[i], false);
    sumDt += dts[i];
    sumSteps += n;
    SLG_CHECK_MSG(ctx, n == wantSteps[i], cat("G12 call ", i, ": ", n, " steps, want ", wantSteps[i]));
  }
  SLG_CHECK_MSG(ctx, p->droppedSteps() == 2, cat("G12 dropped ", p->droppedSteps(), ", want 2"));
  SLG_CHECK_MSG(ctx, p->pausedMicros() == 0, "G12 paused");
  const std::uint64_t acc = sumDt - p->pausedMicros() - (sumSteps + p->droppedSteps()) * 100000;
  SLG_CHECK_MSG(ctx, acc == 10000 && sumDt == 810000, cat("G12 acc ", acc, ", want 10000 (sum of dt ", sumDt, ")"));
  // a pause adds to pausedMicros, runs nothing and does not accumulate
  auto q = bridge::makePacer(100000, 4);
  SLG_CHECK(ctx, q->advance(50000, false) == 0);
  SLG_CHECK(ctx, q->advance(900000, true) == 0);
  SLG_CHECK_MSG(ctx, q->pausedMicros() == 900000 && q->droppedSteps() == 0, "paused time is not accumulated and is not dropped");
  SLG_CHECK_MSG(ctx, q->advance(50000, false) == 1, "after a pause the accumulator resumes where it was (50000 + 50000 = one step)");
}

void checkPacerRandom(Ctx& ctx, std::uint64_t cases) {
  forEachCase(ctx, cases, [&](std::uint64_t, SplitMix64& rng) {
    static const std::uint32_t steps[] = {1, 2, 7, 33333, 100000, 250000};
    static const std::uint32_t maxes[] = {1, 2, 4, 10};
    const std::uint32_t step = steps[rng.pick(6)];
    const std::uint32_t max = maxes[rng.pick(4)];
    auto p = bridge::makePacer(step, max);
    std::uint64_t acc = 0;
    std::uint64_t dropped = 0;
    std::uint64_t paused = 0;
    std::uint64_t sumDt = 0;
    std::uint64_t sumSteps = 0;
    for (unsigned i = 0; i < 40; ++i) {
      std::uint32_t dt;
      switch (rng.pick(4)) {
        case 0: dt = u32(rng.pick(2U * step + 1)); break;
        case 1: dt = u32(rng.below(10ULL * step + 1)); break;
        case 2: dt = u32(rng.next()); break;
        default: dt = i == 0 ? 0U : u32(rng.pick(1000)); break;
      }
      const bool isPaused = rng.chance(1, 5);
      const std::uint32_t n = p->advance(dt, isPaused);
      sumDt += dt;
      std::uint32_t want = 0;
      if (isPaused) {
        paused += dt;
      } else {
        acc += dt;
        std::uint64_t k = acc / step;
        if (k > max) {
          dropped += k - max;
          k = max;
        }
        acc %= step;
        want = u32(k);
      }
      sumSteps += want;
      if (n != want || p->droppedSteps() != dropped || p->pausedMicros() != paused) {
        SLG_FAIL(ctx, cat("StepPacer(step ", step, ", max ", max, ") call ", i, " dt ", dt, isPaused ? " paused" : "", ": steps ", n, " dropped ", p->droppedSteps(),
                          " paused ", p->pausedMicros(), "; model says ", want, " ", dropped, " ", paused));
        return;
      }
      if (n > max || acc >= step || sumDt != paused + (sumSteps + dropped) * step + acc) {
        SLG_FAIL(ctx, "the conservation law of 02 section 7.3 is broken");
        return;
      }
    }
  });
  auto zero = bridge::makePacer(0, 0);   // clamped to at least 1, 02 section 2
  const std::uint32_t n = zero->advance(5, false);
  SLG_CHECK_MSG(ctx, n == 1 && zero->droppedSteps() == 4, cat("StepPacer(0, 0) must behave as (1, 1): advance(5) gave ", n, " steps and dropped ", zero->droppedSteps()));
}

void checkFullness(Ctx& ctx) {
  SLG_CHECK(ctx, bridge::fullness(50, 200) == 4);
  SLG_CHECK(ctx, bridge::fullness(200, 200) == 15);
  SLG_CHECK(ctx, bridge::fullness(1, 200) == 1);
  SLG_CHECK(ctx, bridge::fullness(0, 200) == 0);
  SLG_CHECK(ctx, bridge::fullness(-5, 200) == 0);
  SLG_CHECK(ctx, bridge::fullness(5, 0) == 0);
  SLG_CHECK(ctx, bridge::fullness(5, -3) == 0);
  SLG_CHECK(ctx, bridge::fullness(500, 200) == 15);
  SplitMix64 rng(ctx.caseSeed(77));
  for (unsigned i = 0; i < 4000; ++i) {
    const std::int64_t num = static_cast<std::int64_t>(rng.below(2000001)) - 50000;
    const std::int64_t den = static_cast<std::int64_t>(rng.below(300000)) - 1000;
    std::int64_t want = 0;
    if (num > 0 && den > 0) want = std::min<std::int64_t>(15, 1 + 14 * num / den);
    const std::int64_t got = bridge::fullness(num, den);
    if (got != want) {
      SLG_FAIL(ctx, cat("fullness(", num, ", ", den, ") = ", got, ", want ", want));
      return;
    }
  }
}

// ---- driver ---------------------------------------------------------------------------------------------------

using Changes = std::vector<bridge::DriverChange>;

Changes callAndAck(bridge::Driver& d, std::uint32_t steps, std::uint32_t elapsed,
                   const std::vector<std::pair<ComponentId, Signal>>& offers, bool ack = true, Status* st = nullptr) {
  for (const auto& [id, v] : offers) d.offer(id, v);
  const Status s = d.runCall(steps, elapsed);
  if (st != nullptr) *st = s;
  const Changes c = d.changes();
  if (ack) {
    for (const bridge::DriverChange& ch : c) d.acknowledge(ch.id);
  }
  d.endCall();
  return c;
}

std::string changesText(const Changes& c) {
  std::string s;
  for (const bridge::DriverChange& ch : c) s += cat(ch.id, ":", static_cast<unsigned>(ch.previous), ">", static_cast<unsigned>(ch.value), " ");
  return s.empty() ? std::string("(none)") : s;
}

bool equalChanges(const Changes& c, const std::vector<std::array<unsigned, 3>>& want) {
  if (c.size() != want.size()) return false;
  for (std::size_t i = 0; i < c.size(); ++i) {
    if (c[i].id != want[i][0] || c[i].previous != want[i][1] || c[i].value != want[i][2]) return false;
  }
  return true;
}

std::unique_ptr<bridge::Driver> driverFor(Ctx& ctx, const Model& m, bridge::DriverOptions opt, std::uint32_t hostStep = 0) {
  auto d = bridge::makeDriver(Limits{}, hostStep, opt);
  const Status s = d->apply(opsForModel(m));
  if (s != Status::Ok) {
    SLG_FAIL(ctx, cat("FrameDriver::apply of the netlist: ", statusName(s)));
    return nullptr;
  }
  return d;
}

Model burstModel() {
  Model m;   // L=1 (Level), P=2 (Pulse), A1=3 <- L, A2=4 <- P
  m.comps = {CompDef{1, specSensor(), 0}, CompDef{2, specSensor(), 0}, CompDef{3, specActuator(), 0}, CompDef{4, specActuator(), 0}};
  m.wires = {makeWire(outPin(1), inPin(3, 0)), makeWire(outPin(2), inPin(4, 0))};
  m.sortAndNormalize();
  return m;
}

void checkBurst(Ctx& ctx) {
  bridge::DriverOptions opt;
  opt.settleGuard = false;
  // G11: one call of 3 steps; the pulse into the world is invisible
  auto burst = driverFor(ctx, burstModel(), opt);
  if (burst == nullptr) return;
  burst->setSensorMode(2, true);
  Changes c = callAndAck(*burst, 3, 300000, {{1, 15}, {2, 15}});
  SLG_CHECK_MSG(ctx, equalChanges(c, {{3, 0, 15}}), cat("G11, one call of 3 steps: changes ", changesText(c), "want 3:0>15"));
  SLG_CHECK(ctx, burst->simTick() == 3);
  // three calls of one step: the world sees A2 rise and fall, the final state is the same X3
  auto single = driverFor(ctx, burstModel(), opt);
  if (single == nullptr) return;
  single->setSensorMode(2, true);
  c = callAndAck(*single, 1, 100000, {{1, 15}, {2, 15}});
  SLG_CHECK_MSG(ctx, equalChanges(c, {{3, 0, 15}, {4, 0, 15}}), cat("G11, call 1 of 3: changes ", changesText(c)));
  c = callAndAck(*single, 1, 100000, {});
  SLG_CHECK_MSG(ctx, equalChanges(c, {{4, 15, 0}}), cat("G11, call 2 of 3: changes ", changesText(c), "want 4:15>0"));
  c = callAndAck(*single, 1, 100000, {});
  SLG_CHECK_MSG(ctx, c.empty(), cat("G11, call 3 of 3: changes ", changesText(c)));
  SLG_CHECK_MSG(ctx, single->simStateHash() == burst->simStateHash() && single->simTick() == burst->simTick(),
                "G11: three calls of one step end in the same X3 as one call of three");
  // Pulse offered on every call is a pulse every call: it differs from the single burst by design
  auto every = driverFor(ctx, burstModel(), opt);
  if (every == nullptr) return;
  every->setSensorMode(2, true);
  for (unsigned i = 0; i < 3; ++i) callAndAck(*every, 1, 100000, {{1, 15}, {2, 15}});
  SLG_CHECK_MSG(ctx, every->simStateHash() != burst->simStateHash(), "a Pulse re-offered on each of three calls must differ from one call of three steps");
}

void checkLevelEquivalence(Ctx& ctx, std::uint64_t cases) {
  forEachCase(ctx, cases, [&](std::uint64_t, SplitMix64& rng) {
    const Model m = genAnyModel(rng);
    const std::vector<ComponentId> sensors = sensorIdsOf(m);
    std::vector<std::pair<ComponentId, Signal>> offers;
    for (ComponentId id : sensors) offers.emplace_back(id, u8(rng.pick(16)));
    const unsigned n = 1 + rng.pick(12);
    bridge::DriverOptions opt;
    opt.settleGuard = rng.coin();
    auto one = driverFor(ctx, m, opt);
    auto many = driverFor(ctx, m, opt);
    auto chunks = driverFor(ctx, m, opt);
    if (one == nullptr || many == nullptr || chunks == nullptr) return;
    callAndAck(*one, n, n * 100000, offers);
    for (unsigned i = 0; i < n; ++i) callAndAck(*many, 1, 100000, i == 0 ? offers : std::vector<std::pair<ComponentId, Signal>>{});
    unsigned left = n;
    bool first = true;
    while (left > 0) {
      const unsigned k = 1 + rng.pick(left);
      callAndAck(*chunks, k, k * 100000, first ? offers : std::vector<std::pair<ComponentId, Signal>>{});
      first = false;
      left -= k;
    }
    if (one->simStateHash() != many->simStateHash() || one->simStateHash() != chunks->simStateHash() ||
        one->simEncode() != many->simEncode() || one->simEncode() != chunks->simEncode()) {
      SLG_FAIL(ctx, cat(n, " steps in one call, in ", n, " calls of one step and in random chunks gave different states (Level sensors, constant readings)\n    ", formatLnet(m)));
    }
  });
}

void checkRetentionAndUnresolved(Ctx& ctx) {
  bridge::DriverOptions opt;
  opt.settleGuard = false;
  opt.resolveTimeoutMicros = 1000000;
  Model m;   // L=1 Level -> A2; P=3 Pulse -> A4
  m.comps = {CompDef{1, specSensor(), 0}, CompDef{2, specActuator(), 0}, CompDef{3, specSensor(), 0}, CompDef{4, specActuator(), 0}};
  m.wires = {makeWire(outPin(1), inPin(2, 0)), makeWire(outPin(3), inPin(4, 0))};
  m.sortAndNormalize();
  // retention: a Level keeps its last good value, a Pulse without a reading is 0 (02 section 7.4)
  {
    auto d = driverFor(ctx, m, opt);
    if (d == nullptr) return;
    d->setSensorMode(3, true);
    callAndAck(*d, 2, 200000, {{1, 9}, {3, 12}});
    callAndAck(*d, 3, 300000, {});
    const std::vector<Signal> cmd = d->simCommands();
    SLG_CHECK_MSG(ctx, cmd.size() == 2 && cmd[0] == 9 && cmd[1] == 0,
                  cat("without a new reading the Level must keep 9 and the Pulse must be 0; actuators hold ", cmd.empty() ? 999U : static_cast<unsigned>(cmd[0]), ",",
                      cmd.size() < 2 ? 999U : static_cast<unsigned>(cmd[1])));
  }
  // unresolved: a new sensor reads 0 and, while it has no reading, changes() is empty although the steps run
  {
    Model n;
    n.comps = {CompDef{1, specSensor(), 0}, CompDef{2, specNot(), 15}, CompDef{3, specActuator(), 0}};
    n.wires = {makeWire(outPin(1), inPin(2, 0)), makeWire(outPin(2), inPin(3, 0))};
    n.sortAndNormalize();
    auto d = driverFor(ctx, n, opt);
    if (d == nullptr) return;
    Changes c = callAndAck(*d, 2, 200000, {});
    SLG_CHECK_MSG(ctx, c.empty() && d->simTick() == 2, cat("an unresolved sensor: changes ", changesText(c), "(want none) and the steps must still run (tick ", d->simTick(), ")"));
    c = callAndAck(*d, 2, 200000, {});
    SLG_CHECK_MSG(ctx, c.empty(), "still unresolved after 400000 us of a 1000000 us timeout");
    bool delivered = false;
    for (unsigned i = 0; i < 6 && !delivered; ++i) delivered = !callAndAck(*d, 1, 300000, {}).empty();
    SLG_CHECK_MSG(ctx, delivered, "after resolveTimeoutMicros of adapter time the unresolved sensors must stop holding the actuators back");
    // a first reading resolves at once
    auto e = driverFor(ctx, n, opt);
    if (e == nullptr) return;
    callAndAck(*e, 1, 100000, {});
    c = callAndAck(*e, 3, 300000, {{1, 0}});
    SLG_CHECK_MSG(ctx, !c.empty(), "a first reading resolves the sensor: the actuator that changed must be delivered");
  }
}

void checkSettlingGuard(Ctx& ctx) {
  // 01 section 6.3: sensor 15 -> NOT -> NOT -> actuator, everything born at 15; the actuator would read 15, 15, 0, 15
  Model m;
  m.comps = {CompDef{1, specSensor(), 0}, CompDef{2, specNot(), 15}, CompDef{3, specNot(), 15}, CompDef{4, specActuator(), 15}};
  m.wires = {makeWire(outPin(1), inPin(2, 0)), makeWire(outPin(2), inPin(3, 0)), makeWire(outPin(3), inPin(4, 0))};
  m.sortAndNormalize();
  for (bool guard : {true, false}) {
    bridge::DriverOptions opt;
    opt.settleGuard = guard;
    auto d = driverFor(ctx, m, opt);
    if (d == nullptr) return;
    d->offer(1, 15);
    bool sawDrop = false;
    unsigned deliveries = 0;
    for (unsigned i = 0; i < 8; ++i) {
      const Changes c = callAndAck(*d, 1, 100000, {});
      deliveries += static_cast<unsigned>(c.size());
      for (const bridge::DriverChange& ch : c) sawDrop = sawDrop || (ch.id == 4 && ch.value == 0);
    }
    if (guard) SLG_CHECK_MSG(ctx, !sawDrop && deliveries == 0, "with the settling guard the transient 15, 0, 15 must never reach the world");
    else SLG_CHECK_MSG(ctx, sawDrop, "without the guard the transient 0 is delivered (the premise of the guard)");
  }
}

void checkPendingResyncPhase(Ctx& ctx) {
  bridge::DriverOptions opt;
  opt.settleGuard = false;
  Model m = burstModel();
  auto d = driverFor(ctx, m, opt);
  if (d == nullptr) return;
  // an unacknowledged change is offered again in the next call, with the same previous value
  Changes c = callAndAck(*d, 1, 100000, {{1, 15}, {2, 0}}, false);
  SLG_CHECK_MSG(ctx, equalChanges(c, {{3, 0, 15}}), cat("first delivery: ", changesText(c)));
  c = callAndAck(*d, 1, 100000, {}, false);
  SLG_CHECK_MSG(ctx, equalChanges(c, {{3, 0, 15}}), cat("not acknowledged, it must come again with the same previous: ", changesText(c)));
  c = callAndAck(*d, 1, 100000, {}, true);
  c = callAndAck(*d, 1, 100000, {}, true);
  SLG_CHECK_MSG(ctx, c.empty(), cat("acknowledged, nothing is pending: ", changesText(c)));
  // resync forgets what was acknowledged: every actuator comes again
  d->resync();
  c = callAndAck(*d, 1, 100000, {}, true);
  SLG_CHECK_MSG(ctx, c.size() == 2 && c[0].id == 3 && c[1].id == 4, cat("after resync every actuator is delivered again: ", changesText(c)));
  // apply is refused between runCall and endCall
  d->runCall(1, 100000);
  const std::vector<std::uint8_t> netBefore = d->simEncode();
  const Status s = d->apply({opAdd(9, specSensor(), 0)});
  SLG_CHECK_MSG(ctx, s == Status::WrongPhase, cat("apply inside a call: ", statusName(s)));
  SLG_CHECK_MSG(ctx, d->simEncode() == netBefore, "a refused apply must leave the netlist alone");
  d->endCall();
  SLG_CHECK_MSG(ctx, d->apply({opAdd(9, specSensor(), 0)}) == Status::Ok, "apply between calls is allowed");
}

void checkDriverBlob(Ctx& ctx) {
  bridge::DriverOptions opt;
  opt.settleGuard = false;
  Model m = burstModel();
  m.comps.push_back(CompDef{5, specSensor(), 0});
  m.comps.push_back(CompDef{6, specActuator(), 0});
  m.wires.push_back(makeWire(outPin(5), inPin(6, 0)));
  m.sortAndNormalize();
  auto live = driverFor(ctx, m, opt);
  if (live == nullptr) return;
  live->setSensorMode(2, true);
  callAndAck(*live, 2, 200000, {{1, 7}, {2, 15}, {5, 11}});
  Changes pending = callAndAck(*live, 1, 100000, {{1, 9}}, false);   // not acknowledged: pending across the save
  const std::vector<std::uint8_t> drvBlob = live->encode();
  const std::vector<std::uint8_t> simBlob = live->simEncode();
  Status ds = Status::Ok;
  auto restored = bridge::makeDriverFromBlob(simBlob, Limits{}, opt, ds);
  if (restored == nullptr) {
    SLG_FAIL(ctx, cat("decode of the simulator blob: ", statusName(ds)));
    return;
  }
  const Status s = restored->decode(drvBlob);
  SLG_CHECK_MSG(ctx, s == Status::Ok, cat("FrameDriver::decode of its own blob: ", statusName(s)));
  SLG_CHECK_MSG(ctx, restored->encode() == drvBlob, "encode(decode(b)) != b for the FrameDriver blob");
  // corruption of its blob is refused (a FNV-1a tail, 02 section 7.5)
  for (std::size_t i = 0; i < drvBlob.size(); ++i) {
    std::vector<std::uint8_t> b = drvBlob;
    b[i] = u8(b[i] ^ 0x5AU);
    Status ds2 = Status::Ok;
    auto probe = bridge::makeDriverFromBlob(simBlob, Limits{}, opt, ds2);
    if (probe != nullptr && probe->decode(b) == Status::Ok) {
      SLG_FAIL(ctx, cat("a FrameDriver blob with byte ", i, " changed was accepted"));
      break;
    }
  }
  for (std::size_t len = 0; len < drvBlob.size(); ++len) {
    const std::vector<std::uint8_t> b(drvBlob.begin(), drvBlob.begin() + static_cast<std::ptrdiff_t>(len));
    Status ds2 = Status::Ok;
    auto probe = bridge::makeDriverFromBlob(simBlob, Limits{}, opt, ds2);
    if (probe != nullptr && probe->decode(b) == Status::Ok) {
      SLG_FAIL(ctx, cat("a FrameDriver blob truncated to ", len, " bytes was accepted"));
      break;
    }
  }
  // after a load every sensor is unresolved: no deliveries until a reading, though the pending change is not lost
  Changes c = callAndAck(*restored, 1, 100000, {}, false);
  SLG_CHECK_MSG(ctx, c.empty(), cat("after a load the sensors are unresolved and changes() is empty: ", changesText(c)));
  restored->endCall();
  // the uninterrupted driver, given no readings either, evolves like the restored one (a Level keeps its saved value)
  callAndAck(*live, 1, 100000, {}, false);
  SLG_CHECK_MSG(ctx, live->simStateHash() == restored->simStateHash(), "a Level restored from the blob must keep its saved value (and a Pulse start at 0)");
  c = callAndAck(*restored, 1, 100000, {{1, 9}, {2, 0}, {5, 11}}, false);
  bool pendingBack = false;
  for (const bridge::DriverChange& ch : c) {
    for (const bridge::DriverChange& p : pending) pendingBack = pendingBack || (ch.id == p.id && ch.previous == p.previous);
  }
  SLG_CHECK_MSG(ctx, pending.empty() || pendingBack, cat("the change pending at the save must be delivered again after the first readings: ", changesText(c)));
}

// Random calls (steps, readings, pulses, acknowledgements, resyncs, actuators created between calls) against a
// model of 02 section 7 written on top of the reference. The settling guard is off (the oracle's model has none)
// and every sensor is offered in the first call (so none is unresolved, whose timing the spec leaves open).
void checkDriverModel(Ctx& ctx, std::uint64_t cases) {
  forEachCase(ctx, cases, [&](std::uint64_t, SplitMix64& rng) {
    const Model m = genAnyModel(rng);
    bridge::DriverOptions opt;
    opt.settleGuard = false;
    auto d = driverFor(ctx, m, opt);
    if (d == nullptr) return;
    ReferenceSim ref;
    ref.apply(opsForModel(m));
    std::map<ComponentId, bool> pulse;
    std::map<ComponentId, Signal> held;
    for (ComponentId id : sensorIdsOf(m)) {
      pulse[id] = rng.coin();
      held[id] = 0;
      d->setSensorMode(id, pulse[id]);
    }
    std::map<ComponentId, int> confirmed;   // -1: unknown after a resync
    for (const CompDef& c : m.comps) {
      if (c.spec.kind == Kind::Actuator) confirmed[c.id] = c.seed;
    }
    const unsigned calls = 5 + rng.pick(10);
    for (unsigned call = 0; call < calls; ++call) {
      if (call > 0 && rng.chance(15, 100)) {   // an actuator created between calls starts confirmed at its seed
        std::vector<ComponentId> sources;
        for (const auto& [id, comp] : ref.comps()) {
          if (hasOutput(comp.spec)) sources.push_back(id);
        }
        const ComponentId id = ref.nextId();
        const unsigned seed = rng.pick(16);
        std::vector<EditOp> ops{opAdd(id, specActuator(), seed)};
        if (!sources.empty()) ops.push_back(opConnect(outPin(sources[rng.pick(static_cast<unsigned>(sources.size()))]), inPin(id, 0)));
        const Status sd = d->apply(ops);
        const ApplyResult sr = ref.apply(ops);
        if (sd != Status::Ok || sr.status != Status::Ok) {
          SLG_FAIL(ctx, cat("creating an actuator between calls: driver ", statusName(sd), ", reference ", statusName(sr.status)));
          return;
        }
        confirmed[id] = static_cast<int>(seed);
      }
      const unsigned steps = 1 + rng.pick(4);
      std::map<ComponentId, Signal> offered;
      for (const auto& [id, isPulse] : pulse) {
        (void)isPulse;
        if (call == 0 || rng.coin()) {
          const Signal v = u8(rng.pick(16));
          d->offer(id, v);
          offered[id] = v;
          held[id] = v;
        }
      }
      for (unsigned j = 0; j < steps; ++j) {
        std::vector<Signal> samples;
        for (ComponentId id : ref.sensorIds()) {
          if (pulse[id]) samples.push_back(j == 0 && offered.count(id) != 0 ? offered[id] : Signal{0});
          else samples.push_back(held[id]);
        }
        ref.step(samples);
      }
      const bool forceAll = rng.chance(1, 8);
      if (forceAll) {
        d->resync();
        for (auto& [id, v] : confirmed) v = -1;
      }
      d->runCall(steps, steps * 100000);
      const Changes got = d->changes();
      std::vector<ComponentId> ids = ref.actuatorIds();
      const std::vector<Signal> finals = ref.commands();
      std::string want;
      bool ok = true;
      std::size_t gi = 0;
      for (std::size_t k = 0; k < ids.size(); ++k) {
        const int conf = confirmed[ids[k]];
        if (conf == static_cast<int>(finals[k])) continue;
        want += cat(ids[k], ":", conf < 0 ? std::string("?") : cat(conf), ">", static_cast<unsigned>(finals[k]), " ");
        if (gi >= got.size() || got[gi].id != ids[k] || got[gi].value != finals[k] ||
            (conf >= 0 && got[gi].previous != static_cast<Signal>(conf))) {
          ok = false;
        }
        ++gi;
      }
      if (gi != got.size()) ok = false;
      if (!ok) {
        SLG_FAIL(ctx, cat("call ", call, " (", steps, " steps): changes ", changesText(got), "but the model of 02 section 7 says ", want.empty() ? std::string("(none)") : want, "\n    ", formatLnet(m)));
        d->endCall();
        return;
      }
      for (const bridge::DriverChange& ch : got) {
        if (rng.chance(7, 10)) {
          d->acknowledge(ch.id);
          confirmed[ch.id] = ch.value;
        }
      }
      d->endCall();
      if (d->simStateHash() != ref.stateHash() || d->simTick() != ref.tick()) {
        SLG_FAIL(ctx, cat("call ", call, ": the simulator behind the driver is not where the model says (tick ", d->simTick(), " vs ", ref.tick(), ")\n    ", formatLnet(m)));
        return;
      }
      SLG_CHECK(ctx, true);
    }
  });
}

}  // namespace

SLG_PROPERTY(G11_burst3, 13, "G11: one sample and 3 steps in one call, against three calls of one step (02 section 7.2)", RunOn::SimOnly) {
  checkBurst(ctx);
}

SLG_PROPERTY(G12_pacer, 13, "G12: dt = 250000, 30000, 530000 give 2, 0 and 4 steps, 2 dropped, acc = 10000 (02 section 7.3)", RunOn::SimOnly) {
  checkPacerExample(ctx);
}

SLG_PROPERTY(T13_host_layer, 13, "StepPacer conservation and G12, fullness, FrameDriver burst (G11), Level equivalence, retention, guard, blob",
             RunOn::SimOnly) {
  checkPacerExample(ctx);
  checkPacerRandom(ctx, ctx.cases());
  checkFullness(ctx);
  checkBurst(ctx);
  checkLevelEquivalence(ctx, ctx.cases(1, 2));
  checkDriverModel(ctx, ctx.cases());
  checkRetentionAndUnresolved(ctx);
  checkSettlingGuard(ctx);
  checkPendingResyncPhase(ctx);
  checkDriverBlob(ctx);
}

}  // namespace slgt
