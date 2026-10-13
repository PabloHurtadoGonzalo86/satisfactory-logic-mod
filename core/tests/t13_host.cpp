// T13: the host layer.
//  StepPacer: steps <= max, 0 <= acc < step and the conservation of 02 section 7.3 (G12), against an exact
//  model of the accumulator; fullness() against its closed form.
//  FrameDriver: the burst of G11, n steps in one call == n calls of one step for constant Level sensors (same
//  stateHash), Pulse differs by design, retention of the last good Level value, the timeout of unresolved sensors
//  in executed steps, the withholding by cone, the holds of the settling guard (after an edit, when a sensor
//  resolves), pending deliveries, resync, the phase rule, readings above 15, the v1 blob of the driver byte for
//  byte with its strict decode and its pairing with the simulator, adopting a loaded simulator, the loop of
//  02 section 7.1 with the pacer and the driver together, and a model of the whole policy.
// Not claimed: that the world sees the same thing under another grouping of calls.
#include <algorithm>
#include <array>
#include <map>
#include <optional>
#include <set>
#include <string>

#include "blob.hpp"
#include "bridge.hpp"
#include "cone.hpp"
#include "engine.hpp"
#include "fnv.hpp"
#include "gen.hpp"
#include "netutil.hpp"
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


void checkPacerForHost(Ctx& ctx) {
  // 02 section 7.3: a world whose hostStepMicros was never fixed (0) runs at the default step, not at 1 us
  SLG_CHECK(ctx, bridge::effectiveStepMicros(0) == 100000);
  SLG_CHECK(ctx, bridge::effectiveStepMicros(33333) == 33333);
  auto unset = bridge::makePacerForHost(0, 4);
  SLG_CHECK_MSG(ctx, unset->stepMicros() == 100000, cat("StepPacer::forHostStep(0, 4) steps every ", unset->stepMicros(), " us, want 100000"));
  SLG_CHECK_MSG(ctx, unset->advance(250000, false) == 2 && unset->accumulatedMicros() == 50000, "forHostStep(0, 4): dt = 250000 gives 2 steps and leaves 50000 us");
  auto fixed = bridge::makePacerForHost(33333, 2);
  SLG_CHECK(ctx, fixed->stepMicros() == 33333);
  SLG_CHECK_MSG(ctx, fixed->advance(100000, false) == 2 && fixed->droppedSteps() == 1 && fixed->accumulatedMicros() == 1, "forHostStep(33333, 2): dt = 100000 is 3 steps and 1 us: 2 run, 1 dropped");
}

// ---- driver ---------------------------------------------------------------------------------------------------

using Changes = std::vector<bridge::DriverChange>;
using Offers = std::vector<std::pair<ComponentId, Signal>>;

Changes callAndAck(bridge::Driver& d, std::uint32_t steps, std::uint32_t elapsed, const Offers& offers, bool ack = true,
                   Status* st = nullptr) {
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

// sensor 1 -> NOT 2 (born 15) -> actuator 3 (born 0): with the sensor at its default 0 the actuator goes 0 -> 15 after
// one step, and with a reading of 15 it settles back to 0 (L_a = 2).
Model notChain() {
  Model m;
  m.comps = {CompDef{1, specSensor(), 0}, CompDef{2, specNot(), 15}, CompDef{3, specActuator(), 0}};
  m.wires = {makeWire(outPin(1), inPin(2, 0)), makeWire(outPin(2), inPin(3, 0))};
  m.sortAndNormalize();
  return m;
}

// sensor -> buffer -> actuator, twice: ids 1, 2, 3 and 5, 6, 7 (L_a = 2 each)
Model twoBuffers() {
  Model m;
  const Spec buffer = specGate(GateOp::Or, false, 1);
  m.comps = {CompDef{1, specSensor(), 0}, CompDef{2, buffer, 0}, CompDef{3, specActuator(), 0},
             CompDef{5, specSensor(), 0}, CompDef{6, buffer, 0}, CompDef{7, specActuator(), 0}};
  m.wires = {makeWire(outPin(1), inPin(2, 0)), makeWire(outPin(2), inPin(3, 0)), makeWire(outPin(5), inPin(6, 0)), makeWire(outPin(6), inPin(7, 0))};
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
  // Pulse offered on every call is a pulse every call: it differs from the single burst by design (B9)
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
    Offers offers;
    for (ComponentId id : sensors) offers.emplace_back(id, u8(rng.pick(16)));
    const unsigned n = 1 + rng.pick(12);
    bridge::DriverOptions opt;
    opt.settleGuard = rng.coin();
    auto one = driverFor(ctx, m, opt);
    auto many = driverFor(ctx, m, opt);
    auto chunks = driverFor(ctx, m, opt);
    if (one == nullptr || many == nullptr || chunks == nullptr) return;
    callAndAck(*one, n, n * 100000, offers);
    for (unsigned i = 0; i < n; ++i) callAndAck(*many, 1, 100000, i == 0 ? offers : Offers{});
    unsigned left = n;
    bool first = true;
    while (left > 0) {
      const unsigned k = 1 + rng.pick(left);
      callAndAck(*chunks, k, k * 100000, first ? offers : Offers{});
      first = false;
      left -= k;
    }
    if (one->simStateHash() != many->simStateHash() || one->simStateHash() != chunks->simStateHash() ||
        one->simEncode() != many->simEncode() || one->simEncode() != chunks->simEncode()) {
      SLG_FAIL(ctx, cat(n, " steps in one call, in ", n, " calls of one step and in random chunks gave different states (Level sensors, constant readings)\n    ", formatLnet(m)));
    }
  });
}

void checkRetention(Ctx& ctx) {
  bridge::DriverOptions opt;
  opt.settleGuard = false;
  Model m;   // L=1 Level -> A2; P=3 Pulse -> A4
  m.comps = {CompDef{1, specSensor(), 0}, CompDef{2, specActuator(), 0}, CompDef{3, specSensor(), 0}, CompDef{4, specActuator(), 0}};
  m.wires = {makeWire(outPin(1), inPin(2, 0)), makeWire(outPin(3), inPin(4, 0))};
  m.sortAndNormalize();
  // a Level keeps its last good value, a Pulse without a reading is 0 (02 section 7.4)
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

// First call (1-based) whose changes() is not empty, or 0.
unsigned firstDeliveryCall(bridge::Driver& d, unsigned maxCalls, std::uint32_t steps, std::uint32_t elapsed) {
  for (unsigned call = 1; call <= maxCalls; ++call) {
    if (!callAndAck(d, steps, elapsed, {}).empty()) return call;
  }
  return 0;
}

// 02 section 7.4: an unresolved sensor holds the changes back until its first reading or until the calls have run
// ceil(resolveTimeoutMicros / step) steps. The timeout is a count of executed steps: no time enters the driver.
void checkResolveTimeout(Ctx& ctx) {
  bridge::DriverOptions opt;
  opt.settleGuard = false;
  opt.resolveTimeoutMicros = 1000000;   // 10 steps of the default 100000 us
  const Model m = notChain();
  for (const std::uint32_t elapsed : {0U, 1U, 100000U, 4000000000U, 0xFFFFFFFFU}) {
    auto d = driverFor(ctx, m, opt);
    if (d == nullptr) return;
    const unsigned call = firstDeliveryCall(*d, 30, 1, elapsed);
    SLG_CHECK_MSG(ctx, call == 10 && d->simTick() == 10,
                  cat("calls of one step, ", elapsed, " us each: the first delivery came at call ", call, " (tick ", d->simTick(), "); the timeout counts 10 executed steps, whatever the time"));
  }
  {   // the steps of one call are all credited, the calls are not what is counted
    auto d = driverFor(ctx, m, opt);
    if (d == nullptr) return;
    const Changes nine = callAndAck(*d, 9, 0xFFFFFFFFU, {});
    SLG_CHECK_MSG(ctx, nine.empty(), cat("9 steps of a timeout of 10: changes ", changesText(nine)));
    const Changes tenth = callAndAck(*d, 1, 0, {});
    SLG_CHECK_MSG(ctx, equalChanges(tenth, {{3, 0, 15}}), cat("the 10th step resolves the sensor: changes ", changesText(tenth), "want 3:0>15"));
  }
  {   // a hitch: the pacer lets 4 steps through after 5 s, and 4 steps are all that a call can credit
    auto d = driverFor(ctx, m, opt);
    if (d == nullptr) return;
    const Changes hitch = callAndAck(*d, 4, 5000000, {});
    SLG_CHECK_MSG(ctx, hitch.empty(), cat("a call of 4 steps that took 5 s must not time out a sensor of 10 steps: changes ", changesText(hitch)));
  }
  {   // the step is the world's: 250000 us per step makes the timeout 4 steps
    auto d = driverFor(ctx, m, opt, 250000);
    if (d == nullptr) return;
    const unsigned call = firstDeliveryCall(*d, 30, 1, 100000);
    SLG_CHECK_MSG(ctx, call == 4, cat("hostStepMicros = 250000: first delivery at call ", call, ", want 4 (1000000 / 250000)"));
  }
  {   // rounded up: 1050000 us of 100000 is 11 steps, not 10
    bridge::DriverOptions o = opt;
    o.resolveTimeoutMicros = 1050000;
    auto d = driverFor(ctx, m, o);
    if (d == nullptr) return;
    const unsigned call = firstDeliveryCall(*d, 30, 1, 100000);
    SLG_CHECK_MSG(ctx, call == 11, cat("a timeout of 1050000 us at 100000 us per step: first delivery at call ", call, ", want 11"));
  }
  {   // no timeout: the first call lets everything through
    bridge::DriverOptions o = opt;
    o.resolveTimeoutMicros = 0;
    auto d = driverFor(ctx, m, o);
    if (d == nullptr) return;
    const Changes c = callAndAck(*d, 1, 0, {});
    SLG_CHECK_MSG(ctx, equalChanges(c, {{3, 0, 15}}), cat("resolveTimeoutMicros = 0: changes ", changesText(c), "want 3:0>15 at once"));
  }
  {   // a first reading resolves at once
    auto d = driverFor(ctx, m, opt);
    if (d == nullptr) return;
    callAndAck(*d, 1, 100000, {});
    const Changes c = callAndAck(*d, 3, 300000, {{1, 0}});
    SLG_CHECK_MSG(ctx, !c.empty(), "a first reading resolves the sensor: the actuator that changed must be delivered");
  }
  {   // runCall(0): no step, no credit; Level readings are kept and Pulse readings dropped (B9)
    auto d = driverFor(ctx, m, opt);
    if (d == nullptr) return;
    for (unsigned i = 0; i < 40; ++i) callAndAck(*d, 0, 0xFFFFFFFFU, {});
    SLG_CHECK_MSG(ctx, d->simTick() == 0, "runCall(0, ...) must not run a step");
    const unsigned call = firstDeliveryCall(*d, 30, 1, 0);
    SLG_CHECK_MSG(ctx, call == 10, cat("40 calls without steps must credit nothing: first delivery at call ", call, ", want 10"));
  }
  {
    bridge::DriverOptions o;
    o.settleGuard = false;
    auto d = driverFor(ctx, burstModel(), o);
    if (d == nullptr) return;
    d->setSensorMode(2, true);
    Changes c = callAndAck(*d, 0, 100000, {{1, 7}, {2, 12}});
    SLG_CHECK_MSG(ctx, c.empty() && d->simTick() == 0, cat("runCall(0, ...) with readings: changes ", changesText(c), "(want none), tick ", d->simTick()));
    c = callAndAck(*d, 1, 100000, {});
    SLG_CHECK_MSG(ctx, equalChanges(c, {{3, 0, 7}}), cat("the Level offered in a call without steps is kept, the Pulse is dropped: changes ", changesText(c), "want 3:0>7"));
  }
}

// 02 section 7.4: only the actuators whose cone holds an unresolved sensor are held back.
void checkWithholdOnlyTheCone(Ctx& ctx) {
  bridge::DriverOptions opt;
  opt.settleGuard = false;
  opt.resolveTimeoutMicros = 1000000;   // 10 steps
  Model m;   // sensor 1 -> actuator 2; sensor 3 -> NOT 4 (born 15) -> actuator 5
  m.comps = {CompDef{1, specSensor(), 0}, CompDef{2, specActuator(), 0}, CompDef{3, specSensor(), 0}, CompDef{4, specNot(), 15}, CompDef{5, specActuator(), 0}};
  m.wires = {makeWire(outPin(1), inPin(2, 0)), makeWire(outPin(3), inPin(4, 0)), makeWire(outPin(4), inPin(5, 0))};
  m.sortAndNormalize();
  auto d = driverFor(ctx, m, opt);
  if (d == nullptr) return;
  Changes c = callAndAck(*d, 1, 100000, {{1, 9}});
  SLG_CHECK_MSG(ctx, equalChanges(c, {{2, 0, 9}}), cat("sensor 3 has no reading but is not in the cone of actuator 2: changes ", changesText(c), "want 2:0>9"));
  for (unsigned call = 2; call <= 9; ++call) {
    c = callAndAck(*d, 1, 100000, {});
    SLG_CHECK_MSG(ctx, c.empty(), cat("call ", call, ": actuator 5 has an unresolved sensor in its cone, changes ", changesText(c)));
  }
  c = callAndAck(*d, 1, 100000, {});
  SLG_CHECK_MSG(ctx, equalChanges(c, {{5, 0, 15}}), cat("call 10 (the timeout of sensor 3): changes ", changesText(c), "want 5:0>15"));
  // a sensor that feeds nothing, added later, must not freeze the world while it waits for its reading
  SLG_CHECK_MSG(ctx, d->apply({opAdd(7, specSensor(), 0)}) == Status::Ok, "adding an unwired sensor");
  c = callAndAck(*d, 1, 100000, {{1, 3}});
  SLG_CHECK_MSG(ctx, equalChanges(c, {{2, 9, 3}}), cat("an unwired unresolved sensor holds nothing back: changes ", changesText(c), "want 2:9>3"));
}

// 02 section 7.4: when a sensor goes from unresolved to resolved the actuators of its cone wait L_a steps, so the
// move from the default it fed to its real value is not written to the world.
void checkResolveHold(Ctx& ctx) {
  for (const bool guard : {true, false}) {
    bridge::DriverOptions opt;
    opt.settleGuard = guard;
    opt.resolveTimeoutMicros = 1000000;
    auto d = driverFor(ctx, notChain(), opt);
    if (d == nullptr) return;
    for (unsigned call = 1; call <= 3; ++call) {
      const Changes c = callAndAck(*d, 1, 100000, {});
      SLG_CHECK_MSG(ctx, c.empty(), cat("guard ", guard, ", call ", call, " (no reading yet): changes ", changesText(c)));
    }
    // the reading is 15: the NOT settles to 0, which is where the world already is, but it first goes through 15
    std::vector<Changes> seen;
    for (unsigned call = 4; call <= 8; ++call) seen.push_back(callAndAck(*d, 1, 100000, call == 4 ? Offers{{1, 15}} : Offers{}));
    if (guard) {
      for (unsigned i = 0; i < seen.size(); ++i) {
        SLG_CHECK_MSG(ctx, seen[i].empty(), cat("with the guard the 0 > 15 > 0 of the resolution must not reach the world, call ", 4 + i, ": ", changesText(seen[i])));
      }
    } else {
      SLG_CHECK_MSG(ctx, equalChanges(seen[0], {{3, 0, 15}}) && equalChanges(seen[1], {{3, 15, 0}}),
                    cat("without the guard the transient is delivered (the premise): ", changesText(seen[0]), "then ", changesText(seen[1])));
    }
  }
}

// 02 section 7.5: after an apply only the actuators whose cone changed wait L_a steps.
void checkEditHolds(Ctx& ctx) {
  {   // a stream of edits that touch nobody's cone starves nobody
    bridge::DriverOptions opt;
    Model m;
    m.comps = {CompDef{1, specSensor(), 0}, CompDef{2, specGate(GateOp::Or, false, 1), 0}, CompDef{3, specActuator(), 0}};
    m.wires = {makeWire(outPin(1), inPin(2, 0)), makeWire(outPin(2), inPin(3, 0))};
    m.sortAndNormalize();
    auto d = driverFor(ctx, m, opt);
    if (d == nullptr) return;
    for (unsigned call = 1; call <= 4; ++call) {
      SLG_CHECK_MSG(ctx, d->apply({opAdd(99 + call, specConst(0), 0)}) == Status::Ok, "adding a Const nobody reads");
      const Changes c = callAndAck(*d, 1, 100000, call == 1 ? Offers{{1, 15}} : Offers{});
      const bool due = call == 2;
      SLG_CHECK_MSG(ctx, due ? equalChanges(c, {{3, 0, 15}}) : c.empty(), cat("an unrelated Const added before every call, call ", call, ": changes ", changesText(c), due ? "want 3:0>15 (L_a = 2)" : "want none"));
    }
  }
  // Replacing a component by itself with another seed resets its state: the cone changes although ids, specs and wires do not.
  for (const bool guard : {true, false}) {
    bridge::DriverOptions opt;
    opt.settleGuard = guard;
    auto d = driverFor(ctx, twoBuffers(), opt);
    if (d == nullptr) return;
    for (unsigned call = 1; call <= 6; ++call) callAndAck(*d, 1, 100000, {{1, 15}, {5, 15}});
    const std::vector<Signal> settled = d->simCommands();
    SLG_CHECK_MSG(ctx, settled.size() == 2 && settled[0] == 15 && settled[1] == 15, "both buffers settle at 15 before the edit");
    SLG_CHECK_MSG(ctx, d->apply({opReplace(6, specGate(GateOp::Or, false, 1), 0)}) == Status::Ok, "replace of buffer 6 by itself with seed 0");
    std::vector<Changes> seen;
    for (unsigned call = 1; call <= 4; ++call) seen.push_back(callAndAck(*d, 1, 100000, {{1, 0}, {5, 15}}));
    if (guard) {
      SLG_CHECK_MSG(ctx, seen[0].empty() && equalChanges(seen[1], {{3, 15, 0}}) && seen[2].empty() && seen[3].empty(),
                    cat("guard: actuator 7 sits out the glitch of its reset buffer and actuator 3, whose cone did not change, is delivered on time: ",
                        changesText(seen[0]), "| ", changesText(seen[1]), "| ", changesText(seen[2]), "| ", changesText(seen[3])));
    } else {
      SLG_CHECK_MSG(ctx, equalChanges(seen[0], {{7, 15, 0}}) && equalChanges(seen[1], {{3, 15, 0}, {7, 0, 15}}),
                    cat("without the guard the reset glitch is delivered (the premise): ", changesText(seen[0]), "| ", changesText(seen[1])));
    }
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
  // resync forgets what was acknowledged: every actuator comes again, and its previous value is kUnconfirmed (255)
  d->resync();
  c = callAndAck(*d, 1, 100000, {}, true);
  SLG_CHECK_MSG(ctx, c.size() == 2 && c[0].id == 3 && c[1].id == 4, cat("after resync every actuator is delivered again: ", changesText(c)));
  SLG_CHECK_MSG(ctx, c.size() == 2 && c[0].previous == 255 && c[1].previous == 255, cat("after resync previous is 255 (kUnconfirmed, outside 0..15): ", changesText(c)));
  // apply is refused between runCall and endCall
  d->runCall(1, 100000);
  const std::vector<std::uint8_t> netBefore = d->simEncode();
  const Status s = d->apply({opAdd(9, specSensor(), 0)});
  SLG_CHECK_MSG(ctx, s == Status::WrongPhase, cat("apply inside a call: ", statusName(s)));
  SLG_CHECK_MSG(ctx, d->simEncode() == netBefore, "a refused apply must leave the netlist alone");
  d->endCall();
  SLG_CHECK_MSG(ctx, d->apply({opAdd(9, specSensor(), 0)}) == Status::Ok, "apply between calls is allowed");
}

// A reading is saturated once, where it is taken; clampedSamples() counts readings, not the steps they stay in force.
void checkReadingsAboveFifteen(Ctx& ctx) {
  bridge::DriverOptions opt;
  opt.settleGuard = false;
  Model m = burstModel();   // 1 Level -> 3, 2 Pulse -> 4
  auto d = driverFor(ctx, m, opt);
  if (d == nullptr) return;
  d->setSensorMode(2, true);
  Changes c = callAndAck(*d, 4, 400000, {{1, 200}, {2, 255}});
  SLG_CHECK_MSG(ctx, d->simClamped() == 2, cat("two readings above 15 in a call of 4 steps count 2 (one each), not ", d->simClamped()));
  SLG_CHECK_MSG(ctx, equalChanges(c, {{3, 0, 15}}), cat("the Level saturates to 15 and the Pulse is over after its step: changes ", changesText(c)));
  callAndAck(*d, 3, 300000, {});
  SLG_CHECK_MSG(ctx, d->simClamped() == 2, cat("the retained reading is saturated: 3 more steps must not count, clampedSamples = ", d->simClamped()));
  callAndAck(*d, 1, 100000, {{1, 200}});
  SLG_CHECK_MSG(ctx, d->simClamped() == 3, cat("a new reading above 15 counts again: clampedSamples = ", d->simClamped()));
  const std::vector<std::uint8_t> blob = d->encode();
  SLG_CHECK_MSG(ctx, blob.size() > 43 && blob[42] == 15, "the retained value in the driver blob is saturated (a Level in 0..15)");
}

// A Pulse carries no value: switching a sensor between modes forgets the reading, so a live driver and one restored
// from its blob (which saves no value for a Pulse) do not diverge.
void checkModeSwitch(Ctx& ctx) {
  bridge::DriverOptions opt;
  opt.settleGuard = false;
  Model m;
  m.comps = {CompDef{1, specSensor(), 0}, CompDef{2, specActuator(), 0}};
  m.wires = {makeWire(outPin(1), inPin(2, 0))};
  m.sortAndNormalize();
  for (const bool fromPulse : {true, false}) {
    auto live = driverFor(ctx, m, opt);
    if (live == nullptr) return;
    if (fromPulse) {
      live->setSensorMode(1, true);
      callAndAck(*live, 1, 100000, {{1, 9}});   // a Pulse of 9
      live->setSensorMode(1, false);
    } else {
      callAndAck(*live, 1, 100000, {{1, 9}});   // a Level of 9
      live->setSensorMode(1, true);
      live->setSensorMode(1, false);
    }
    const std::vector<std::uint8_t> blob = live->encode();
    Status ds = Status::Ok;
    auto restored = bridge::makeDriverFromBlob(live->simEncode(), Limits{}, opt, ds);
    if (restored == nullptr) {
      SLG_FAIL(ctx, cat("decode of the simulator blob: ", statusName(ds)));
      return;
    }
    SLG_CHECK_MSG(ctx, restored->decode(blob) == Status::Ok && restored->encode() == blob, "the blob of a driver whose sensor changed mode restores to itself");
    // both are Level sensors with no reading now: the 9 is gone from the live driver as it is from the blob
    callAndAck(*live, 2, 200000, {});
    callAndAck(*restored, 2, 200000, {});
    SLG_CHECK_MSG(ctx, live->simCommands() == restored->simCommands() && live->simCommands().size() == 1 && live->simCommands()[0] == 0,
                  cat(fromPulse ? "Pulse" : "Level", " -> Level: the live and the restored driver must both feed 0, the live one has ", live->simCommands().empty() ? 999U : static_cast<unsigned>(live->simCommands()[0]),
                      " and the restored one ", restored->simCommands().empty() ? 999U : static_cast<unsigned>(restored->simCommands()[0])));
  }
  {   // and it shows in what the world is told
    auto d = driverFor(ctx, m, opt);
    if (d == nullptr) return;
    callAndAck(*d, 1, 100000, {{1, 9}});
    d->setSensorMode(1, true);
    d->setSensorMode(1, false);
    const Changes c = callAndAck(*d, 1, 100000, {});
    SLG_CHECK_MSG(ctx, equalChanges(c, {{2, 9, 0}}), cat("Level -> Pulse -> Level forgets the reading of 9: changes ", changesText(c), "want 2:9>0"));
  }
}

// ---- the blob of the driver (02 section 7.5) ---------------------------------------------------------------------

struct SensorRec {
  ComponentId id;
  unsigned retained;
  unsigned mode;
};
struct ActuatorRec {
  ComponentId id;
  unsigned confirmed;
  std::uint32_t hold;
};

// The v1 blob written from the spec text: "SLGD", u16 version, u64 tick, u64 stateHash, u64 structureHash of the paired
// simulator blob, u32 sensors, u32 actuators, u32 id + u8 retained + u8 mode per sensor, u32 id + u8 confirmed + u32
// hold per actuator, FNV-1a tail.
std::vector<std::uint8_t> driverBlobFor(const std::vector<std::uint8_t>& simBlob, const std::vector<SensorRec>& sensors,
                                        const std::vector<ActuatorRec>& actuators) {
  BlobData bd;
  if (!parseBlob(simBlob, bd)) return {};
  ByteWriter w;
  for (const char ch : {'S', 'L', 'G', 'D'}) w.u8(u8(static_cast<unsigned char>(ch)));
  w.u16(1);
  w.u64(bd.tick);
  w.u64(stateHashOf(bd));
  w.u64(structureHashOf(bd));
  w.u32(u32(sensors.size()));
  w.u32(u32(actuators.size()));
  for (const SensorRec& s : sensors) {
    w.u32(s.id);
    w.u8(u8(s.retained));
    w.u8(u8(s.mode));
  }
  for (const ActuatorRec& a : actuators) {
    w.u32(a.id);
    w.u8(u8(a.confirmed));
    w.u32(a.hold);
  }
  const std::uint64_t tail = fnv1a64(w.bytes().data(), w.bytes().size());
  w.u64(tail);
  return w.bytes();
}

std::vector<std::uint8_t> patchedWord(std::vector<std::uint8_t> b, std::size_t at, unsigned width, std::uint64_t value) {
  for (unsigned i = 0; i < width; ++i) b[at + i] = u8((value >> (8 * i)) & 0xFFU);
  refreshTail(b);
  return b;
}

Status decodeOn(const std::vector<std::uint8_t>& simBlob, const bridge::DriverOptions& opt, const std::vector<std::uint8_t>& driverBlob) {
  Status ds = Status::Ok;
  auto probe = bridge::makeDriverFromBlob(simBlob, Limits{}, opt, ds);
  if (probe == nullptr) return ds;
  return probe->decode(driverBlob);
}

// offsets in a blob with two sensors and two actuators
constexpr std::size_t kVersionAt = 4;
constexpr std::size_t kTickAt = 6;
constexpr std::size_t kStateHashAt = 14;
constexpr std::size_t kStructureHashAt = 22;
constexpr std::size_t kSensorCountAt = 30;
constexpr std::size_t kSensor0 = 38;   // id, retained (+4), mode (+5)
constexpr std::size_t kSensor1 = 44;
constexpr std::size_t kActuator0 = 50;  // id, confirmed (+4), hold (+5)
constexpr std::size_t kActuator1 = 59;

void checkDriverBlobFormat(Ctx& ctx) {
  bridge::DriverOptions opt;   // the guard is on: the holds of a fresh netlist are in the blob
  Model m;
  m.comps = {CompDef{1, specSensor(), 0}, CompDef{2, specActuator(), 0}, CompDef{3, specSensor(), 0}, CompDef{4, specActuator(), 0}};
  m.wires = {makeWire(outPin(1), inPin(2, 0)), makeWire(outPin(3), inPin(4, 0))};
  m.sortAndNormalize();
  auto d = driverFor(ctx, m, opt);
  if (d == nullptr) return;
  d->setSensorMode(3, true);
  // a created actuator waits L_a = 1 steps (sensor weight 0 + actuator weight 1)
  std::vector<std::uint8_t> got = d->encode();
  std::vector<std::uint8_t> want = driverBlobFor(d->simEncode(), {{1, 0, 0}, {3, 0, 1}}, {{2, 0, 1}, {4, 0, 1}});
  SLG_CHECK_MSG(ctx, got == want, cat("blob of a fresh driver:\n    got  ", hexBytes(got, 90), "\n    want ", hexBytes(want, 90)));
  // one call: the Level keeps 9, a Pulse keeps nothing, the hold ran out, only actuator 2 is acknowledged
  d->offer(1, 9);
  d->offer(3, 12);
  SLG_CHECK(ctx, d->runCall(1, 100000) == Status::Ok);
  const Changes c = d->changes();
  SLG_CHECK_MSG(ctx, equalChanges(c, {{2, 0, 9}, {4, 0, 12}}), cat("the call: changes ", changesText(c)));
  d->acknowledge(2);
  d->endCall();
  const std::vector<std::uint8_t> simBlob = d->simEncode();
  got = d->encode();
  want = driverBlobFor(simBlob, {{1, 9, 0}, {3, 0, 1}}, {{2, 9, 0}, {4, 0, 0}});
  SLG_CHECK_MSG(ctx, got == want, cat("blob after a call:\n    got  ", hexBytes(got, 90), "\n    want ", hexBytes(want, 90)));
  if (got != want || got.size() != kActuator1 + 9 + 8) return;

  SLG_CHECK_MSG(ctx, decodeOn(simBlob, opt, got) == Status::Ok, "the blob decodes against the simulator it was saved with");
  // strict decode: each of these breaks one rule of the blob and recomputes the tail, so only the rule can refuse it
  struct Bad {
    const char* what;
    std::vector<std::uint8_t> blob;
    Status want;
  };
  const std::uint64_t holdCap = 8192ULL + 65536ULL;
  const std::vector<Bad> bad = {
      {"a Level retained 16", patchedWord(got, kSensor0 + 4, 1, 16), Status::FormatCorrupt},
      {"a Level retained 255", patchedWord(got, kSensor0 + 4, 1, 255), Status::FormatCorrupt},
      {"a Pulse retained 1", patchedWord(got, kSensor1 + 4, 1, 1), Status::FormatCorrupt},
      {"a sensor mode 2", patchedWord(got, kSensor0 + 5, 1, 2), Status::FormatCorrupt},
      {"a confirmed value 16", patchedWord(got, kActuator0 + 4, 1, 16), Status::FormatCorrupt},
      {"a hold above the longest possible path", patchedWord(got, kActuator1 + 5, 4, holdCap + 1), Status::FormatCorrupt},
      {"a hold of 0xFFFFFFFF", patchedWord(got, kActuator0 + 5, 4, 0xFFFFFFFFULL), Status::FormatCorrupt},
      {"another tick", patchedWord(got, kTickAt, 8, readU64(got.data() + kTickAt) + 1), Status::FormatCorrupt},
      {"another stateHash", patchedWord(got, kStateHashAt, 8, readU64(got.data() + kStateHashAt) ^ 1ULL), Status::FormatCorrupt},
      {"another structureHash", patchedWord(got, kStructureHashAt, 8, readU64(got.data() + kStructureHashAt) ^ 1ULL), Status::FormatCorrupt},
      {"a sensor count of 3", patchedWord(got, kSensorCountAt, 4, 3), Status::FormatCorrupt},
      {"another sensor id", patchedWord(got, kSensor1, 4, 5), Status::FormatCorrupt},
      {"another actuator id", patchedWord(got, kActuator0, 4, 6), Status::FormatCorrupt},
      {"version 0", patchedWord(got, kVersionAt, 2, 0), Status::FormatCorrupt},
      {"version 2", patchedWord(got, kVersionAt, 2, 2), Status::FormatTooNew},
  };
  for (const Bad& b : bad) {
    const Status s = decodeOn(simBlob, opt, b.blob);
    SLG_CHECK_MSG(ctx, s == b.want, cat(b.what, ": ", statusName(s), ", want ", statusName(b.want)));
  }
  {   // a trailing byte, even with a good tail over it
    std::vector<std::uint8_t> longer(got.begin(), got.end() - 8);
    longer.push_back(0);
    longer.resize(longer.size() + 8);
    refreshTail(longer);
    SLG_CHECK_MSG(ctx, decodeOn(simBlob, opt, longer) == Status::FormatCorrupt, "a trailing byte in the blob");
  }
  {   // the largest hold, and confirmed = 255, are in the domain and come back byte for byte
    std::vector<std::uint8_t> edge = patchedWord(got, kActuator1 + 5, 4, holdCap);
    edge = patchedWord(edge, kActuator0 + 4, 1, 255);
    Status ds = Status::Ok;
    auto probe = bridge::makeDriverFromBlob(simBlob, Limits{}, opt, ds);
    SLG_CHECK_MSG(ctx, probe != nullptr && probe->decode(edge) == Status::Ok && probe->encode() == edge, "a hold at the cap and a confirmed value of 255 decode and re-encode unchanged");
  }
  // the pair is checked, not just the ids: a driver blob of another moment is refused
  d->offer(1, 4);
  SLG_CHECK(ctx, d->runCall(1, 100000) == Status::Ok);
  d->endCall();
  const std::vector<std::uint8_t> laterSim = d->simEncode();
  SLG_CHECK_MSG(ctx, decodeOn(laterSim, opt, got) == Status::FormatCorrupt, "a driver blob of tick 1 against the simulator of tick 2 (a torn save)");
  SLG_CHECK_MSG(ctx, decodeOn(simBlob, opt, d->encode()) == Status::FormatCorrupt, "a driver blob of tick 2 against the simulator of tick 1");
}

// The round trip of its own blob, its refusal of corruption, and what a load leaves unresolved.
void checkDriverBlobRoundTrip(Ctx& ctx) {
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

// A saved hold is kept as saved, honoured with the guard (and never shortened by the hold of a sensor that resolves:
// the larger of the two wins) and ignored without it.
void checkRestoredHold(Ctx& ctx) {
  bridge::DriverOptions on;
  bridge::DriverOptions off;
  off.settleGuard = false;
  Model m;
  m.comps = {CompDef{1, specSensor(), 0}, CompDef{2, specActuator(), 0}, CompDef{3, specSensor(), 0}, CompDef{4, specActuator(), 0}};
  m.wires = {makeWire(outPin(1), inPin(2, 0)), makeWire(outPin(3), inPin(4, 0))};
  m.sortAndNormalize();
  auto d = driverFor(ctx, m, on);
  if (d == nullptr) return;
  const std::vector<std::uint8_t> simBlob = d->simEncode();
  const std::vector<std::uint8_t> held = patchedWord(d->encode(), kActuator0 + 5, 4, 50);   // L_a is 1; the hold saved is 50
  for (const bool guard : {true, false}) {
    Status ds = Status::Ok;
    auto r = bridge::makeDriverFromBlob(simBlob, Limits{}, guard ? on : off, ds);
    if (r == nullptr) {
      SLG_FAIL(ctx, cat("decode of the simulator blob: ", statusName(ds)));
      return;
    }
    SLG_CHECK_MSG(ctx, r->decode(held) == Status::Ok && r->encode() == held, cat("guard ", guard, ": a hold of 50 is kept as saved"));
    const Changes c = callAndAck(*r, 1, 100000, {{1, 9}, {3, 0}});
    if (guard) SLG_CHECK_MSG(ctx, c.empty(), cat("guard on: the hold of 50 steps must not be cut short by the L_a = 1 of the sensor that resolves: changes ", changesText(c)));
    else SLG_CHECK_MSG(ctx, equalChanges(c, {{2, 0, 9}}), cat("guard off: a saved hold is ignored: changes ", changesText(c), "want 2:0>9"));
    const std::vector<std::uint8_t> after = r->encode();
    SLG_CHECK_MSG(ctx, after.size() > kActuator0 + 5 && after[kActuator0 + 5] == 49, cat("guard ", guard, ": the hold counts down with the steps that run (50 -> 49)"));
  }
}

// A torn pair that differs only by a wire: same ids, same tick, same state, other structure.
void checkTornByWire(Ctx& ctx) {
  bridge::DriverOptions opt;
  Model m = burstModel();
  m.comps.push_back(CompDef{5, specConst(0), 0});
  m.sortAndNormalize();
  auto d = driverFor(ctx, m, opt);
  if (d == nullptr) return;
  const std::vector<std::uint8_t> before = d->encode();
  const std::vector<std::uint8_t> simBefore = d->simEncode();
  SLG_CHECK_MSG(ctx, d->apply({opConnect(outPin(5), inPin(3, 0))}) == Status::Ok, "wiring a Const to an actuator");
  const std::vector<std::uint8_t> simAfter = d->simEncode();
  const std::vector<std::uint8_t> after = d->encode();
  SLG_CHECK_MSG(ctx, decodeOn(simAfter, opt, after) == Status::Ok && decodeOn(simBefore, opt, before) == Status::Ok, "each blob decodes against its own simulator");
  SLG_CHECK_MSG(ctx, decodeOn(simAfter, opt, before) == Status::FormatCorrupt, "the driver blob from before a wire edit against the simulator after it, same tick and same state");
}

// 02 section 7.6: a save during a settling window keeps the hold, and the restored world delivers no transient either.
void checkSaveDuringSettling(Ctx& ctx) {
  bridge::DriverOptions opt;   // guard on
  Model m;   // sensor 15 -> NOT -> NOT -> actuator born 15: 15, 0, 15 on the way (01 section 6.3)
  m.comps = {CompDef{1, specSensor(), 0}, CompDef{2, specNot(), 15}, CompDef{3, specNot(), 15}, CompDef{4, specActuator(), 15}};
  m.wires = {makeWire(outPin(1), inPin(2, 0)), makeWire(outPin(2), inPin(3, 0)), makeWire(outPin(3), inPin(4, 0))};
  m.sortAndNormalize();
  auto live = driverFor(ctx, m, opt);
  if (live == nullptr) return;
  Changes c = callAndAck(*live, 1, 100000, {{1, 15}});
  SLG_CHECK_MSG(ctx, c.empty(), cat("call 1: ", changesText(c)));
  const std::vector<std::uint8_t> blob = live->encode();
  const std::vector<std::uint8_t> simBlob = live->simEncode();
  // one sensor (id 1), one actuator (id 4): the hold sits at kSensor1 + 5 in this smaller blob
  const std::size_t holdAt = 38 + 6 + 4 + 1;
  SLG_CHECK_MSG(ctx, blob.size() == 38 + 6 + 9 + 8 && blob[holdAt] == 2 && blob[holdAt + 1] == 0, cat("the hold of the actuator (L_a = 3, one step run) is saved: ", hexBytes(blob, 70)));
  Status ds = Status::Ok;
  auto restored = bridge::makeDriverFromBlob(simBlob, Limits{}, opt, ds);
  if (restored == nullptr) {
    SLG_FAIL(ctx, cat("decode of the simulator blob: ", statusName(ds)));
    return;
  }
  SLG_CHECK_MSG(ctx, restored->decode(blob) == Status::Ok && restored->encode() == blob, "the driver restored mid-settling saves the same blob (the hold included)");
  unsigned deliveries = 0;
  for (unsigned call = 2; call <= 12; ++call) {
    const Offers offers = {{1, 15}};
    deliveries += static_cast<unsigned>(callAndAck(*live, 1, 100000, offers).size());
    deliveries += static_cast<unsigned>(callAndAck(*restored, 1, 100000, offers).size());
  }
  SLG_CHECK_MSG(ctx, deliveries == 0, cat("the 15, 0, 15 of the settling must reach the world through neither driver: ", deliveries, " deliveries"));
  SLG_CHECK_MSG(ctx, live->simCommands() == restored->simCommands() && live->simCommands()[0] == 15, "both end with the actuator at 15");
}

// 02 section 7.6: to adopt a loaded simulator, bind a new driver to it.
void checkAdoptingASimulator(Ctx& ctx) {
  bridge::DriverOptions opt;
  opt.settleGuard = false;
  Model m = burstModel();
  auto d = driverFor(ctx, m, opt);
  if (d == nullptr) return;
  callAndAck(*d, 1, 100000, {{1, 15}, {2, 0}}, false);   // delivered, never acknowledged: pending in the world's eyes
  const std::vector<std::uint8_t> drv = d->encode();
  const std::vector<std::uint8_t> sim = d->simEncode();
  const Changes live = callAndAck(*d, 1, 100000, {}, false);
  SLG_CHECK_MSG(ctx, equalChanges(live, {{3, 0, 15}}), cat("the running driver offers the pending change again: ", changesText(live)));
  // the world is replaced by the save: a NEW driver on the decoded simulator, then its own blob
  SLG_CHECK_MSG(ctx, d->adopt(sim) == Status::Ok, "adopt the simulator blob");
  SLG_CHECK_MSG(ctx, d->decode(drv) == Status::Ok, "decode the driver blob into the new driver");
  const Changes first = callAndAck(*d, 1, 100000, {}, false);
  SLG_CHECK_MSG(ctx, first.empty(), cat("after adoption every sensor is unresolved again: changes ", changesText(first)));
  const Changes second = callAndAck(*d, 1, 100000, {{1, 15}, {2, 0}}, false);
  SLG_CHECK_MSG(ctx, equalChanges(second, {{3, 0, 15}}), cat("the pending change survives the save and is offered with the first readings: ", changesText(second)));
  // a blob that is not a simulator leaves the pair alone
  const std::vector<std::uint8_t> before = d->simEncode();
  SLG_CHECK_MSG(ctx, d->adopt({1, 2, 3}) == Status::FormatCorrupt && d->simEncode() == before, "adopting garbage changes nothing");
}

// ---- the 7.1 loop ------------------------------------------------------------------------------------------------

// StepPacer and FrameDriver together, as the adapter calls them every frame: dt in microseconds, steps from the pacer
// only, no call when it gives 0. The timeout of an unresolved sensor is 50 steps whatever the frames look like, and
// every microsecond of dt is accounted for through sim.tick() (02 section 7.3).
void checkFrameLoop(Ctx& ctx, std::uint64_t cases) {
  forEachCase(ctx, cases, [&](std::uint64_t, SplitMix64& rng) {
    bridge::DriverOptions opt;
    opt.settleGuard = false;   // the default timeout of 5 s: 50 steps at 100000 us
    auto d = driverFor(ctx, notChain(), opt);
    if (d == nullptr) return;
    auto pacer = bridge::makePacerForHost(0, 4);
    const unsigned style = rng.pick(4);
    static const std::uint32_t mixed[] = {0, 1, 16667, 16667, 33333, 100000, 250000, 0xFFFFFFFFU};
    std::uint64_t sumDt = 0;
    std::uint64_t firstDelivery = 0;
    for (unsigned frame = 0; frame < 6000 && firstDelivery == 0; ++frame) {
      std::uint32_t dt = 16667;
      bool paused = false;
      if (style == 1) dt = mixed[rng.pick(8)];
      if (style == 2) dt = rng.chance(1, 25) ? 5000000U : 16667U;
      if (style == 3) {
        dt = rng.chance(1, 4) ? 33333U : 16667U;
        paused = rng.chance(1, 8);
      }
      sumDt += dt;
      const std::uint32_t steps = pacer->advance(dt, paused);
      if (steps > 4) {
        SLG_FAIL(ctx, cat("the pacer gave ", steps, " steps in a frame"));
        return;
      }
      if (steps != 0) {
        const Status s = d->runCall(steps, dt);
        const Changes c = d->changes();
        for (const bridge::DriverChange& ch : c) d->acknowledge(ch.id);
        d->endCall();
        if (s != Status::Ok) {
          SLG_FAIL(ctx, cat("runCall: ", statusName(s)));
          return;
        }
        if (!c.empty()) firstDelivery = d->simTick();
      }
      if (sumDt != pacer->pausedMicros() + (d->simTick() + pacer->droppedSteps()) * pacer->stepMicros() + pacer->accumulatedMicros()) {
        SLG_FAIL(ctx, cat("frame ", frame, ": sum of dt ", sumDt, " is not paused ", pacer->pausedMicros(), " + (tick ", d->simTick(), " + dropped ", pacer->droppedSteps(),
                          ") * ", pacer->stepMicros(), " + acc ", pacer->accumulatedMicros()));
        return;
      }
    }
    // the call that credits the 50th step is the first that can deliver: its tick is 50 to 53 (a call has at most 4 steps)
    if (!SLG_CHECK_MSG(ctx, firstDelivery >= 50 && firstDelivery <= 53,
                       cat("style ", style, ": the unresolved sensor timed out at tick ", firstDelivery, ", want 50 to 53 (50 executed steps, whatever the frame times)"))) {
      return;
    }
  });
}

// ---- a model of the policy of 02 section 7 --------------------------------------------------------------------------

// The cone of an actuator: itself plus its ancestors over the net-induced graph (01 section 8), from the test's own nets.
std::set<ComponentId> coneOf(const NetAnalysis& na, ComponentId actuator) {
  std::map<ComponentId, std::vector<ComponentId>> preds;
  for (const auto& [from, to] : na.edges) preds[to].push_back(from);
  std::set<ComponentId> cone{actuator};
  std::vector<ComponentId> todo{actuator};
  while (!todo.empty()) {
    const ComponentId c = todo.back();
    todo.pop_back();
    for (const ComponentId p : preds[c]) {
      if (cone.insert(p).second) todo.push_back(p);
    }
  }
  return cone;
}

// L_a of 01 section 8: none when the cone has a cycle or a Latch, Toggle or Clock.
std::optional<unsigned> boundOf(const Model& m, const NetAnalysis& na, const std::set<ComponentId>& cone, ComponentId actuator) {
  for (const ComponentId id : cone) {
    const Kind k = m.find(id)->spec.kind;
    if (k == Kind::Latch || k == Kind::Toggle || k == Kind::Clock) return std::nullopt;
  }
  std::map<ComponentId, std::vector<ComponentId>> succ;
  std::map<ComponentId, std::vector<ComponentId>> preds;
  std::map<ComponentId, unsigned> indegree;
  for (const ComponentId id : cone) indegree[id] = 0;
  for (const auto& [from, to] : na.edges) {
    if (cone.count(from) == 0 || cone.count(to) == 0) continue;
    succ[from].push_back(to);
    preds[to].push_back(from);
    ++indegree[to];
  }
  std::vector<ComponentId> ready;
  for (const auto& [id, deg] : indegree) {
    if (deg == 0) ready.push_back(id);
  }
  std::map<ComponentId, unsigned> level;
  std::size_t done = 0;
  while (!ready.empty()) {
    const ComponentId c = ready.back();
    ready.pop_back();
    ++done;
    unsigned best = 0;
    for (const ComponentId p : preds[c]) best = std::max(best, level[p]);
    level[c] = weightOf(m.find(c)->spec) + best;
    for (const ComponentId s : succ[c]) {
      if (--indegree[s] == 0) ready.push_back(s);
    }
  }
  if (done != cone.size()) return std::nullopt;
  return level[actuator];
}

// Everything that decides what the cone computes from now on: members, specs, states and who drives each input net.
std::string describeCone(const NetAnalysis& na, const ReferenceSim& ref, const std::set<ComponentId>& cone) {
  std::string s;
  for (const ComponentId id : cone) {
    const ReferenceSim::Comp& comp = ref.comps().at(id);
    s += cat(id, "/", static_cast<unsigned>(comp.spec.kind), "/", comp.spec.p0, "/", comp.spec.p1, "/", comp.spec.p2, "/");
    for (const std::uint16_t v : comp.st) s += cat(v, ",");
    for (unsigned j = 0; j < inputCount(comp.spec); ++j) {
      s += cat("<", j, ":");
      for (const Pin& drv : na.nets[na.netOf.at(inPin(id, j))].drivers) s += cat(drv.component, ",");
    }
    s += ";";
  }
  return s;
}

struct PolicyFacts {
  Model net;
  NetAnalysis na;
  std::map<ComponentId, std::set<ComponentId>> cone;           // per actuator
  std::map<ComponentId, std::optional<unsigned>> bound;        // per actuator
  std::map<ComponentId, std::string> description;              // per actuator

  void rebuild(const Model& m, const ReferenceSim& ref) {
    net = m;
    na = analyzeNets(m);
    cone.clear();
    bound.clear();
    description.clear();
    for (const CompDef& c : m.comps) {
      if (c.spec.kind != Kind::Actuator) continue;
      cone[c.id] = coneOf(na, c.id);
      bound[c.id] = boundOf(m, na, cone[c.id], c.id);
      description[c.id] = describeCone(na, ref, cone[c.id]);
    }
  }
};

// Random calls (steps, readings that may be missing for a while, pulses, acknowledgements, resyncs) and edits between
// them (an actuator, a sensor wired into the middle of the net, a removed sensor, a component replaced by itself with
// another seed), against a model of 02 sections 7.2, 7.4 and 7.5 written on top of the reference: timeouts in steps,
// the withholding by cone, the hold armed when a sensor resolves and by the edits that change a cone, the guard on and off.
void checkDriverPolicy(Ctx& ctx, std::uint64_t cases) {
  forEachCase(ctx, cases, [&](std::uint64_t, SplitMix64& rng) {
    Model cur = genAnyModel(rng);
    bridge::DriverOptions opt;
    opt.settleGuard = rng.coin();
    static const std::uint32_t timeouts[] = {0, 100000, 250000, 300000, 1000000};
    opt.resolveTimeoutMicros = timeouts[rng.pick(5)];
    const std::uint64_t limit = (std::uint64_t{opt.resolveTimeoutMicros} + 99999) / 100000;
    const unsigned offerPct = std::array<unsigned, 3>{15, 50, 100}[rng.pick(3)];
    auto d = driverFor(ctx, cur, opt);
    if (d == nullptr) return;
    ReferenceSim ref;
    ref.apply(opsForModel(cur));

    std::map<ComponentId, bool> pulse, resolved;
    std::map<ComponentId, Signal> held;
    std::map<ComponentId, std::uint64_t> waited;
    std::map<ComponentId, int> confirmed;   // -1: unknown after a resync
    std::map<ComponentId, std::uint32_t> hold;
    PolicyFacts facts;
    facts.rebuild(cur, ref);
    for (const ComponentId id : sensorIdsOf(cur)) {
      pulse[id] = rng.coin();
      held[id] = 0;
      resolved[id] = false;
      waited[id] = 0;
      d->setSensorMode(id, pulse[id]);
    }
    for (const CompDef& c : cur.comps) {
      if (c.spec.kind != Kind::Actuator) continue;
      confirmed[c.id] = c.seed;
      hold[c.id] = opt.settleGuard ? facts.bound[c.id].value_or(0) : 0;   // every actuator is new to the driver
    }

    const unsigned calls = 6 + rng.pick(14);
    for (unsigned call = 0; call < calls; ++call) {
      if (call > 0 && rng.chance(25, 100)) {
        std::vector<EditOp> ops;
        const unsigned kind = rng.pick(4);
        const ComponentId fresh = ref.nextId();
        std::vector<ComponentId> sources, readers, sensors, stateful;
        for (const auto& [id, comp] : ref.comps()) {
          if (hasOutput(comp.spec)) sources.push_back(id);
          if (inputCount(comp.spec) > 0) readers.push_back(id);
          if (comp.spec.kind == Kind::Sensor) sensors.push_back(id);
          if (comp.spec.kind != Kind::Sensor && comp.spec.kind != Kind::Const) stateful.push_back(id);
        }
        const unsigned seed = rng.pick(16);
        if (kind == 0 && !sources.empty()) {
          ops = {opAdd(fresh, specActuator(), seed), opConnect(outPin(sources[rng.pick(static_cast<unsigned>(sources.size()))]), inPin(fresh, 0))};
        } else if (kind == 1 && !readers.empty()) {
          const ComponentId reader = readers[rng.pick(static_cast<unsigned>(readers.size()))];
          const unsigned pin = rng.pick(inputCount(ref.comps().at(reader).spec));
          ops = {opAdd(fresh, specSensor(), 0), opConnect(outPin(fresh), inPin(reader, pin))};
        } else if (kind == 2 && !sensors.empty()) {
          ops = {opRemove(sensors[rng.pick(static_cast<unsigned>(sensors.size()))])};
        } else if (kind == 3 && !stateful.empty()) {
          const ComponentId target = stateful[rng.pick(static_cast<unsigned>(stateful.size()))];
          const Spec spec = ref.comps().at(target).spec;
          ops = {opReplace(target, spec, genSeed(rng, spec))};
        }
        if (!ops.empty()) {
          facts.rebuild(cur, ref);   // the states move with every step: the cones are described at the moment of the edit
          const std::map<ComponentId, std::string> before = facts.description;
          const Status sd = d->apply(ops);
          const ApplyResult sr = ref.apply(ops);
          if (sd != Status::Ok || sr.status != Status::Ok) {
            SLG_FAIL(ctx, cat("edit ", kind, " between calls: driver ", statusName(sd), ", reference ", statusName(sr.status), "\n    ", formatLnet(cur)));
            return;
          }
          if (ops[0].kind == EditKind::Add) {
            cur.comps.push_back(CompDef{ops[0].id, ops[0].spec, ops[0].seed});
            cur.wires.push_back(makeWire(ops[1].a, ops[1].b));
          } else if (ops[0].kind == EditKind::Remove) {
            const ComponentId gone = ops[0].id;
            std::erase_if(cur.comps, [gone](const CompDef& c) { return c.id == gone; });
            std::erase_if(cur.wires, [gone](const Wire& w) { return w.a.component == gone || w.b.component == gone; });
            pulse.erase(gone);
            held.erase(gone);
            resolved.erase(gone);
            waited.erase(gone);
          }
          cur.sortAndNormalize();
          facts.rebuild(cur, ref);
          if (ops[0].kind == EditKind::Add && ops[0].spec.kind == Kind::Sensor) {
            pulse[ops[0].id] = rng.coin();
            held[ops[0].id] = 0;
            resolved[ops[0].id] = false;
            waited[ops[0].id] = 0;
            d->setSensorMode(ops[0].id, pulse[ops[0].id]);
          }
          if (ops[0].kind == EditKind::Add && ops[0].spec.kind == Kind::Actuator) confirmed[ops[0].id] = static_cast<int>(ops[0].seed);
          // an actuator is held L_a more steps when its cone is new or is not what it was
          for (const auto& [a, desc] : facts.description) {
            const auto was = before.find(a);
            if (opt.settleGuard && (was == before.end() || was->second != desc) && facts.bound[a].has_value()) {
              hold[a] = std::max(hold[a], *facts.bound[a]);
            }
          }
        }
      }

      const unsigned steps = rng.chance(1, 20) ? 0U : 1 + rng.pick(4);
      std::map<ComponentId, Signal> offered;
      for (const auto& [id, isPulse] : pulse) {
        (void)isPulse;
        if (rng.chance(offerPct, 100)) {
          const Signal v = u8(rng.pick(16));
          d->offer(id, v);
          offered[id] = v;
        }
      }
      // the call, in the order of 02 section 7: readings and timeouts, holds, steps, what the world is told
      std::set<ComponentId> flipped;
      for (auto& [id, isResolved] : resolved) {
        if (offered.count(id) != 0) {
          if (!pulse[id]) held[id] = offered[id];
          if (!isResolved) flipped.insert(id);
          isResolved = true;
        } else if (!isResolved) {
          waited[id] += steps;
          if (waited[id] >= limit) {
            isResolved = true;
            flipped.insert(id);
          }
        }
      }
      if (opt.settleGuard) {
        for (const auto& [a, members] : facts.cone) {
          if (!facts.bound.at(a).has_value()) continue;
          for (const ComponentId s : flipped) {
            if (members.count(s) != 0) hold[a] = std::max(hold[a], *facts.bound.at(a));
          }
        }
      }
      for (unsigned j = 0; j < steps; ++j) {
        std::vector<Signal> samples;
        for (const ComponentId id : ref.sensorIds()) {
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
      SLG_CHECK(ctx, d->runCall(steps, steps * 100000) == Status::Ok);
      const Changes got = d->changes();
      const std::vector<ComponentId> ids = ref.actuatorIds();
      const std::vector<Signal> finals = ref.commands();
      std::string want;
      bool ok = true;
      std::size_t gi = 0;
      for (std::size_t k = 0; k < ids.size(); ++k) {
        std::uint32_t& h = hold[ids[k]];
        h = h > steps ? h - steps : 0;
        bool withheld = false;
        for (const ComponentId s : facts.cone.at(ids[k])) {
          const auto r = resolved.find(s);
          if (r != resolved.end() && !r->second) withheld = true;
        }
        if (withheld || (opt.settleGuard && h != 0)) continue;
        const int conf = confirmed[ids[k]];
        if (conf == static_cast<int>(finals[k])) continue;
        want += cat(ids[k], ":", conf < 0 ? 255U : static_cast<unsigned>(conf), ">", static_cast<unsigned>(finals[k]), " ");
        if (gi >= got.size() || got[gi].id != ids[k] || got[gi].value != finals[k] || got[gi].previous != (conf < 0 ? Signal{255} : static_cast<Signal>(conf))) ok = false;
        ++gi;
      }
      if (gi != got.size()) ok = false;
      if (!ok) {
        SLG_FAIL(ctx, cat("call ", call, " (", steps, " steps, guard ", opt.settleGuard, ", timeout ", limit, " steps): changes ", changesText(got), "but the model of 02 section 7 says ",
                          want.empty() ? std::string("(none)") : want, "\n    ", formatLnet(cur)));
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
        SLG_FAIL(ctx, cat("call ", call, ": the simulator behind the driver is not where the model says (tick ", d->simTick(), " vs ", ref.tick(), ")\n    ", formatLnet(cur)));
        return;
      }
      SLG_CHECK(ctx, true);
    }
  });
}

// The old model, kept as it was: guard off, every sensor offered in the first call.
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

SLG_PROPERTY(T13_host_layer, 13, "StepPacer conservation and G12, fullness, FrameDriver burst (G11), Level equivalence, retention, guard, edits, pending deliveries",
             RunOn::SimOnly) {
  checkPacerExample(ctx);
  checkPacerRandom(ctx, ctx.cases());
  checkPacerForHost(ctx);
  checkFullness(ctx);
  checkBurst(ctx);
  checkLevelEquivalence(ctx, ctx.cases(1, 2));
  checkDriverModel(ctx, ctx.cases());
  checkRetention(ctx);
  checkResolveTimeout(ctx);
  checkWithholdOnlyTheCone(ctx);
  checkResolveHold(ctx);
  checkSettlingGuard(ctx);
  checkEditHolds(ctx);
  checkPendingResyncPhase(ctx);
  checkReadingsAboveFifteen(ctx);
  checkModeSwitch(ctx);
}

SLG_PROPERTY(T13_host_blob, 13, "FrameDriver blob v1 byte for byte, its strict decode, torn pairs, a save during settling, adopting a loaded simulator (02 sections 7.5 and 7.6)",
             RunOn::SimOnly) {
  checkDriverBlobRoundTrip(ctx);
  checkDriverBlobFormat(ctx);
  checkRestoredHold(ctx);
  checkTornByWire(ctx);
  checkSaveDuringSettling(ctx);
  checkAdoptingASimulator(ctx);
}

SLG_PROPERTY(T13_host_policy, 13, "StepPacer + FrameDriver in the loop of 02 section 7.1, and the driver against a model of its policy with unresolved sensors, the guard and edits",
             RunOn::SimOnly) {
  checkFrameLoop(ctx, ctx.cases(1, 2));
  checkDriverPolicy(ctx, ctx.cases());
}

}  // namespace slgt
