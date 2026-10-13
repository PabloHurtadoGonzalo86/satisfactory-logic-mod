// T11, exact statuses: what the spec answers to an edit that has more than one thing wrong, or whose wrongness is a
// question of which rule is judged first. Every expected value is the one the oracle gives (ambiguities.md B2, B3, B4,
// B5; decisions D28 to D30) and nothing here accepts "either of two":
//  - arity, Delay length and Clock period above their `Limits` cap are LimitExceeded, whatever the catalog range says,
//    and the cap is judged after the malformed parameters and before the seed or the Clock's high width;
//  - disconnect judges its pins like connect: UnknownComponent, then BadPin (a pin onto itself and the output of an
//    Actuator included), and only then UnknownWire; pin a is judged completely before pin b;
//  - the limits of a batch judge the netlist it leaves, after the last operation, so a batch that overshoots and repairs
//    is valid and a batch whose operation fails answers that operation's status, not a limit.
// Both engines run them: the reference interpreter checks the table, the core is checked against the table.
// Not claimed: the status of a batch with two failing operations when the spec does not order them (the first one wins).
#include <string>
#include <vector>

#include "engine.hpp"
#include "testkit.hpp"

namespace slgt {

namespace {

struct Probe {
  std::string what;
  Limits limits;
  std::vector<EditOp> setup;   // applied first, must be accepted
  std::vector<EditOp> ops;
  Status want = Status::Ok;
};

void runProbes(Ctx& ctx, const EngineFactory& f, const std::vector<Probe>& probes) {
  for (const Probe& p : probes) {
    auto e = f.make(p.limits, 0);
    if (!p.setup.empty() && e->apply(p.setup) != Status::Ok) {
      SLG_FAIL(ctx, cat(p.what, ": the setup batch was rejected"));
      continue;
    }
    const Obs before = observe(*e);
    const Status got = e->apply(p.ops);
    SLG_CHECK_MSG(ctx, got == p.want, cat(p.what, ": apply answered ", statusName(got), ", the spec says ", statusName(p.want)));
    SLG_CHECK_MSG(ctx, e->lastBatchIdsMatched(), cat(p.what, ": EditBatch::add handed out an unexpected id"));
    if (p.want != Status::Ok) {
      const std::string diff = diffObs(before, observe(*e), true);
      SLG_CHECK_MSG(ctx, diff.empty(), cat(p.what, ": the refused batch changed the engine: ", diff));
    }
  }
}

// const 1 -> NOT 2 -> actuator 3 with the wire 1.o - 2.0 only; nextId = 4
std::vector<EditOp> smallNet() {
  return {opAdd(1, specConst(15), 0), opAdd(2, specNot(), 0), opAdd(3, specActuator(), 0), opConnect(outPin(1), inPin(2, 0))};
}

Limits limitsWith(unsigned arity, unsigned delay, unsigned period) {
  Limits l;
  l.maxArity = u8(arity);
  l.maxDelay = u16(delay);
  l.maxClockPeriod = u16(period);
  return l;
}

// one spec through an explicit-id add, an add() and a replace of the existing component 1
void specProbes(std::vector<Probe>& out, const char* what, const Limits& lim, const Spec& spec, unsigned seed, Status want) {
  Probe viaId;
  viaId.what = cat(what, " (addWithId)");
  viaId.limits = lim;
  viaId.ops = {opAdd(1, spec, seed)};
  viaId.want = want;
  out.push_back(viaId);
  Probe viaAdd = viaId;
  viaAdd.what = cat(what, " (add)");
  viaAdd.ops = {opAdd(1, spec, seed, true)};
  out.push_back(viaAdd);
  Probe viaReplace = viaId;
  viaReplace.what = cat(what, " (replace)");
  viaReplace.setup = {opAdd(1, specConst(0), 0)};
  viaReplace.ops = {opReplace(1, spec, seed)};
  out.push_back(viaReplace);
}

}  // namespace

SLG_PROPERTY(T11_param_caps_are_limit_exceeded, 11,
             "arity, Delay length and Clock period above their cap are LimitExceeded, ahead of the seed and of the high width, behind malformed parameters (ambiguities B2)",
             RunOn::Both) {
  std::vector<Probe> probes;
  const Limits dflt;
  const Limits small = limitsWith(3, 4, 6);        // caps below the catalog ranges
  const Limits raised = limitsWith(12, 300, 5000);  // caps above them
  // default caps: the catalog range and the cap coincide, and it is still the cap that answers
  for (unsigned d : {256U, 257U, 300U, 1000U, 65535U}) specProbes(probes, cat("Delay(", d, ") at the default cap").c_str(), dflt, specDelay(d), 0, Status::LimitExceeded);
  for (unsigned k : {9U, 10U, 16U, 255U, 256U, 65535U}) specProbes(probes, cat("Gate k=", k, " at the default cap").c_str(), dflt, specGate(GateOp::Or, true, k), 15, Status::LimitExceeded);
  for (unsigned p : {4097U, 4098U, 5000U, 65535U}) {
    for (unsigned h : {1U, p - 1}) specProbes(probes, cat("Clock(", p, ",", h, ") at the default cap").c_str(), dflt, specClock(p, h), 0, Status::LimitExceeded);
    for (unsigned h : {0U, p, 65535U}) specProbes(probes, cat("Clock(", p, ",", h, ") (bad high width) at the default cap").c_str(), dflt, specClock(p, h), 0, Status::LimitExceeded);
  }
  specProbes(probes, "Delay(255) is the longest by default", dflt, specDelay(255), 0, Status::Ok);
  specProbes(probes, "Gate k=8 is the widest by default", dflt, specGate(GateOp::Xor, false, 8), 0, Status::Ok);
  specProbes(probes, "Clock(4096,1) is the longest period by default", dflt, specClock(4096, 1), 4095, Status::Ok);
  // caps below the catalog ranges: every value above the cap, also above the range, is the same answer
  for (unsigned d : {5U, 6U, 100U, 255U, 256U, 300U, 65535U}) specProbes(probes, cat("Delay(", d, ") with maxDelay 4").c_str(), small, specDelay(d), 0, Status::LimitExceeded);
  for (unsigned k : {4U, 5U, 8U, 9U, 255U, 65535U}) specProbes(probes, cat("Gate k=", k, " with maxArity 3").c_str(), small, specGate(GateOp::And, false, k), 0, Status::LimitExceeded);
  for (unsigned h : {0U, 3U, 6U, 7U, 8U, 65535U}) specProbes(probes, cat("Clock(7,", h, ") with maxClockPeriod 6").c_str(), small, specClock(7, h), 0, Status::LimitExceeded);
  specProbes(probes, "Delay(4) with maxDelay 4", small, specDelay(4), 0, Status::Ok);
  specProbes(probes, "Gate k=3 with maxArity 3", small, specGate(GateOp::And, false, 3), 0, Status::Ok);
  specProbes(probes, "Clock(6,3) with maxClockPeriod 6", small, specClock(6, 3), 0, Status::Ok);
  specProbes(probes, "Clock(6,6) with maxClockPeriod 6: the period is allowed, the high width is not", small, specClock(6, 6), 0, Status::BadSpec);
  specProbes(probes, "Clock(6,0) with maxClockPeriod 6", small, specClock(6, 0), 0, Status::BadSpec);
  // caps above the catalog ranges: the cap is the only bound
  for (unsigned d : {256U, 300U}) specProbes(probes, cat("Delay(", d, ") with maxDelay 300").c_str(), raised, specDelay(d), 0, Status::Ok);
  for (unsigned d : {301U, 1000U, 65535U}) specProbes(probes, cat("Delay(", d, ") with maxDelay 300").c_str(), raised, specDelay(d), 0, Status::LimitExceeded);
  for (unsigned k : {9U, 12U}) specProbes(probes, cat("Gate k=", k, " with maxArity 12").c_str(), raised, specGate(GateOp::Or, false, k), 0, Status::Ok);
  for (unsigned k : {13U, 255U}) specProbes(probes, cat("Gate k=", k, " with maxArity 12").c_str(), raised, specGate(GateOp::Or, false, k), 0, Status::LimitExceeded);
  specProbes(probes, "Clock(5000,2500) with maxClockPeriod 5000", raised, specClock(5000, 2500), 4999, Status::Ok);
  specProbes(probes, "Clock(5000,5000) with maxClockPeriod 5000", raised, specClock(5000, 5000), 0, Status::BadSpec);
  specProbes(probes, "Clock(5001,6000) with maxClockPeriod 5000", raised, specClock(5001, 6000), 0, Status::LimitExceeded);
  specProbes(probes, "Clock(5001,3) with maxClockPeriod 5000", raised, specClock(5001, 3), 0, Status::LimitExceeded);
  // the cap is judged before the seed: a seed outside its domain does not hide it
  specProbes(probes, "Delay(300) with a seed of 200", dflt, specDelay(300), 200, Status::LimitExceeded);
  specProbes(probes, "Gate k=9 with the seed 7", dflt, specGate(GateOp::Or, false, 9), 7, Status::LimitExceeded);
  specProbes(probes, "Clock(5000,2) with the seed 60000", dflt, specClock(5000, 2), 60000, Status::LimitExceeded);
  // malformed parameters are judged before the cap
  Spec s = specGate(GateOp::Or, false, 300);
  s.p0 = 3;
  specProbes(probes, "Gate op 3 with k=300", dflt, s, 0, Status::BadSpec);
  s = specGate(GateOp::Xor, true, 9);
  s.p1 = 2;
  specProbes(probes, "Gate inv 2 with k=9", dflt, s, 0, Status::BadSpec);
  s = specGate(GateOp::Xor, true, 0);
  specProbes(probes, "Gate k=0", dflt, s, 0, Status::BadSpec);
  s = specDelay(300);
  s.p1 = 1;
  specProbes(probes, "Delay(300) with an unused parameter", dflt, s, 0, Status::BadSpec);
  s = specDelay(65535);
  s.p2 = 1;
  specProbes(probes, "Delay(65535) with an unused parameter", dflt, s, 0, Status::BadSpec);
  specProbes(probes, "Delay(0)", dflt, specDelay(0), 0, Status::BadSpec);
  s = specClock(5000, 2);
  s.p2 = 1;
  specProbes(probes, "Clock(5000,2) with an unused parameter", dflt, s, 0, Status::BadSpec);
  specProbes(probes, "Clock(1,1): period below 2", dflt, specClock(1, 1), 0, Status::BadSpec);
  specProbes(probes, "Clock(0,0): period below 2", dflt, specClock(0, 0), 0, Status::BadSpec);
  specProbes(probes, "Clock(7,3) with maxClockPeriod 6 and a bad unused parameter", small, [] { Spec c = specClock(7, 3); c.p2 = 1; return c; }(), 0, Status::BadSpec);
  runProbes(ctx, factory, probes);
}

SLG_PROPERTY(T11_disconnect_judges_pins_like_connect, 11,
             "disconnect answers UnknownComponent, then BadPin (a pin onto itself, an Actuator output), then UnknownWire, pin a before pin b, as connect does (ambiguities B4)",
             RunOn::Both) {
  struct Wire2 {
    const char* what;
    Pin a, b;
    Status want;
  };
  const Wire2 wires[] = {
      {"a names a missing component", outPin(9), inPin(2, 0), Status::UnknownComponent},
      {"b names a missing component", outPin(1), inPin(9, 0), Status::UnknownComponent},
      {"both name missing components", outPin(8), inPin(9, 0), Status::UnknownComponent},
      {"a missing, b with a bad pin", outPin(9), inPin(2, 5), Status::UnknownComponent},
      {"a with a bad pin, b missing", inPin(2, 5), outPin(9), Status::BadPin},
      {"a with a bad pin", inPin(2, 5), outPin(1), Status::BadPin},
      {"b with a bad pin", outPin(1), inPin(2, 5), Status::BadPin},
      {"a is the output of an Actuator", outPin(3), outPin(1), Status::BadPin},
      {"b is the output of an Actuator", outPin(1), outPin(3), Status::BadPin},
      {"an input pin onto itself", inPin(2, 0), inPin(2, 0), Status::BadPin},
      {"an output pin onto itself", outPin(1), outPin(1), Status::BadPin},
      {"a pin that does not exist onto itself", inPin(2, 7), inPin(2, 7), Status::BadPin},
      {"a missing pin of a missing component onto itself", inPin(9, 0), inPin(9, 0), Status::UnknownComponent},
  };
  std::vector<Probe> probes;
  for (const Wire2& w : wires) {
    Probe dis;
    dis.what = cat("disconnect: ", w.what);
    dis.setup = smallNet();
    dis.ops = {opDisconnect(w.a, w.b)};
    dis.want = w.want;
    probes.push_back(dis);
    Probe con = dis;
    con.what = cat("connect: ", w.what);
    con.ops = {opConnect(w.a, w.b)};
    probes.push_back(con);
    Probe late = dis;   // after valid operations, so the failure is not the first operation of the batch
    late.what = cat("disconnect after valid operations: ", w.what);
    late.ops = {opAdd(4, specSensor(), 0), opConnect(outPin(4), inPin(3, 0)), opDisconnect(outPin(4), inPin(3, 0)), opDisconnect(w.a, w.b)};
    probes.push_back(late);
  }
  Probe p;
  p.setup = smallNet();
  p.what = "disconnect of a wire that does not exist between valid pins";
  p.ops = {opDisconnect(outPin(1), inPin(3, 0))};
  p.want = Status::UnknownWire;
  probes.push_back(p);
  p.what = "disconnect twice";
  p.ops = {opDisconnect(outPin(1), inPin(2, 0)), opDisconnect(outPin(1), inPin(2, 0))};
  probes.push_back(p);
  p.what = "disconnect twice, the second one with the pins swapped";
  p.ops = {opDisconnect(outPin(1), inPin(2, 0)), opDisconnect(inPin(2, 0), outPin(1))};
  probes.push_back(p);
  p.what = "a component removed and, in the same batch, one of its wires disconnected (a first)";
  p.ops = {opRemove(2), opDisconnect(inPin(2, 0), outPin(1))};
  p.want = Status::UnknownComponent;
  probes.push_back(p);
  p.what = "a component removed and, in the same batch, one of its wires disconnected (b second)";
  p.ops = {opRemove(1), opDisconnect(inPin(2, 0), outPin(1))};
  probes.push_back(p);
  p.what = "a component added, wired, removed, and its wire disconnected";
  p.ops = {opAdd(4, specConst(3), 0), opConnect(outPin(4), inPin(3, 0)), opRemove(4), opDisconnect(outPin(4), inPin(3, 0))};
  probes.push_back(p);
  p.what = "disconnect of a pin that the batch replaced away";
  p.ops = {opDisconnect(outPin(1), inPin(2, 0)), opReplace(2, specConst(1), 0), opDisconnect(outPin(1), inPin(2, 0))};
  p.want = Status::BadPin;
  probes.push_back(p);
  p.what = "disconnect, valid, in either order of the pins";
  p.ops = {opDisconnect(inPin(2, 0), outPin(1))};
  p.want = Status::Ok;
  probes.push_back(p);
  p.what = "disconnect then connect the same wire";
  p.ops = {opDisconnect(outPin(1), inPin(2, 0)), opConnect(inPin(2, 0), outPin(1))};
  probes.push_back(p);
  runProbes(ctx, factory, probes);
}

SLG_PROPERTY(T11_limits_judge_the_final_netlist, 11,
             "a batch may overshoot a count limit and repair it; a failing operation answers its own status, not a limit; the limits bite on what the batch leaves (ambiguities B5)",
             RunOn::Both) {
  std::vector<Probe> probes;
  Probe p;
  p.limits.maxComponents = 2;
  p.what = "maxComponents: three adds and a remove leave two";
  p.ops = {opAdd(1, specSensor(), 0), opAdd(2, specSensor(), 0), opAdd(3, specSensor(), 0), opRemove(2)};
  p.want = Status::Ok;
  probes.push_back(p);
  p.what = "maxComponents: three adds leave three";
  p.ops = {opAdd(1, specSensor(), 0), opAdd(2, specSensor(), 0), opAdd(3, specSensor(), 0)};
  p.want = Status::LimitExceeded;
  probes.push_back(p);
  p.what = "maxComponents: an operation fails after the overshoot, so its status is the answer";
  p.ops = {opAdd(1, specSensor(), 0), opAdd(2, specSensor(), 0), opAdd(3, specSensor(), 0), opConnect(inPin(3, 0), outPin(1))};
  p.want = Status::BadPin;
  probes.push_back(p);
  p.what = "maxComponents: the overshoot comes after the failing operation";
  p.ops = {opAdd(1, specSensor(), 0), opRemove(7), opAdd(2, specSensor(), 0), opAdd(3, specSensor(), 0)};
  p.want = Status::UnknownComponent;
  probes.push_back(p);
  p.limits = Limits{};
  p.limits.maxWires = 1;
  p.setup = {opAdd(1, specConst(1), 0), opAdd(2, specActuator(), 0), opAdd(3, specActuator(), 0)};
  p.what = "maxWires: two wires and a disconnect leave one";
  p.ops = {opConnect(outPin(1), inPin(2, 0)), opConnect(outPin(1), inPin(3, 0)), opDisconnect(outPin(1), inPin(2, 0))};
  p.want = Status::Ok;
  probes.push_back(p);
  p.what = "maxWires: two wires leave two";
  p.ops = {opConnect(outPin(1), inPin(2, 0)), opConnect(outPin(1), inPin(3, 0))};
  p.want = Status::LimitExceeded;
  probes.push_back(p);
  p.limits = Limits{};
  p.limits.maxNetPins = 3;
  p.setup = {opAdd(1, specConst(1), 0), opAdd(2, specActuator(), 0), opAdd(3, specActuator(), 0), opAdd(4, specActuator(), 0)};
  p.what = "maxNetPins: a net of four pins and a disconnect leave three";
  p.ops = {opConnect(outPin(1), inPin(2, 0)), opConnect(outPin(1), inPin(3, 0)), opConnect(outPin(1), inPin(4, 0)), opDisconnect(outPin(1), inPin(4, 0))};
  p.want = Status::Ok;
  probes.push_back(p);
  p.what = "maxNetPins: a net of four pins";
  p.ops = {opConnect(outPin(1), inPin(2, 0)), opConnect(outPin(1), inPin(3, 0)), opConnect(outPin(1), inPin(4, 0))};
  p.want = Status::LimitExceeded;
  probes.push_back(p);
  p.limits = Limits{};
  p.limits.maxDelayCells = 4;
  p.setup = {};
  p.what = "maxDelayCells: two Delay(3) and a remove leave three cells";
  p.ops = {opAdd(1, specDelay(3), 0), opAdd(2, specDelay(3), 0), opRemove(1)};
  p.want = Status::Ok;
  probes.push_back(p);
  p.what = "maxDelayCells: a replace that lengthens a Delay past the total";
  p.setup = {opAdd(1, specDelay(3), 0)};
  p.ops = {opReplace(1, specDelay(5), 0)};
  p.want = Status::LimitExceeded;
  probes.push_back(p);
  runProbes(ctx, factory, probes);
}

}  // namespace slgt
