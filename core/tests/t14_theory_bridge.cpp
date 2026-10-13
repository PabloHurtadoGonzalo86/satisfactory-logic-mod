// T14: the traces of docs/theory are reproduced row for row (without shifting rows) with 1 -> 15 (03 section 3).
//  - gate-level circuits of theory/04 and 06 (latches, D and T flip-flops, edge detector, pulse extender, rings)
//    against a port of the unit-delay evaluator of check_B.py (an independent third implementation), on random
//    inputs and random initial states;
//  - the waveforms printed in the documents (04 Paso 6 flip-flop D, 06 timers, rings, divider, edge detector,
//    pulse extender, the "latido" example, 07 detector 101) as literal text;
//  - the 101 detector (Moore, one-hot) and the master-slave D flip-flop are built with the catalog (the criterion
//    that the catalog is enough).
// Not claimed: what v0 does not have (03 section 3): counters with T as an input and a simultaneous Qn, machines of
// theory/07 with primitive D flip-flops, except when they are built from gates.
// The detector 101 is checked at every row against the Moore table; the document's gate-level timing of an arbitrary
// FSM is not reproduced, only this one-hot construction.
#include <map>

#include "engine.hpp"
#include "gen.hpp"
#include "spec_examples.hpp"
#include "testkit.hpp"
#include "theory_waves.hpp"

namespace slgt {

namespace {

// ---- a port of Circuit (check_B.py): unit-delay gates over 0/1 -----------------------------------------------

struct TGate {
  std::string name;
  std::string op;
  std::vector<std::string> src;
};

struct TCircuit {
  std::string name;
  std::vector<std::string> inputs;
  std::vector<TGate> gates;
};

std::vector<TCircuit> circuits() {
  const std::vector<TGate> ffCore = {
      {"nCLK", "NOT", {"CLK"}},        {"nD", "NOT", {"D"}},           {"a1", "NAND", {"D", "nCLK"}},
      {"b1", "NAND", {"nD", "nCLK"}},  {"M", "NAND", {"a1", "Mn"}},    {"Mn", "NAND", {"b1", "M"}},
      {"a2", "NAND", {"M", "CLK"}},    {"b2", "NAND", {"Mn", "CLK"}},  {"Q", "NAND", {"a2", "Qn"}},
      {"Qn", "NAND", {"b2", "Q"}}};
  std::vector<TCircuit> r;
  r.push_back({"ring of two NOT", {}, {{"A", "NOT", {"B"}}, {"B", "NOT", {"A"}}}});
  r.push_back({"SR latch with NOR", {"S", "R"}, {{"Q", "NOR", {"R", "Qn"}}, {"Qn", "NOR", {"S", "Q"}}}});
  r.push_back({"SR latch with NAND", {"Sn", "Rn"}, {{"Q", "NAND", {"Sn", "Qn"}}, {"Qn", "NAND", {"Rn", "Q"}}}});
  r.push_back({"D latch", {"D", "E"},
               {{"nD", "NOT", {"D"}}, {"a", "NAND", {"D", "E"}}, {"b", "NAND", {"nD", "E"}}, {"Q", "NAND", {"a", "Qn"}}, {"Qn", "NAND", {"b", "Q"}}}});
  r.push_back({"master-slave D flip-flop", {"D", "CLK"}, ffCore});
  std::vector<TGate> tff = {{"D", "XOR", {"T", "Q"}}};
  tff.insert(tff.end(), ffCore.begin(), ffCore.end());
  r.push_back({"T flip-flop", {"T", "CLK"}, tff});
  r.push_back({"edge detector", {"x"},
               {{"nx", "NOT", {"x"}}, {"SUBIDA", "AND", {"x", "nx"}}, {"BAJADA", "NOR", {"x", "nx"}}, {"AMBOS", "XNOR", {"x", "nx"}}}});
  r.push_back({"pulse extender", {"x"},
               {{"b1", "BUF", {"x"}}, {"b2", "BUF", {"b1"}}, {"b3", "BUF", {"b2"}}, {"ANCHO", "OR", {"x", "b1", "b2", "b3"}}}});
  r.push_back({"ring of three NOT", {}, {{"g1", "NOT", {"g3"}}, {"g2", "NOT", {"g1"}}, {"g3", "NOT", {"g2"}}}});
  r.push_back({"ring of five NOT", {}, {{"g1", "NOT", {"g5"}}, {"g2", "NOT", {"g1"}}, {"g3", "NOT", {"g2"}}, {"g4", "NOT", {"g3"}}, {"g5", "NOT", {"g4"}}}});
  return r;
}

int theoryOp(const std::string& op, const std::vector<int>& a) {
  int ones = 0;
  for (int v : a) ones += v;
  const int n = static_cast<int>(a.size());
  if (op == "NOT") return 1 - a[0];
  if (op == "BUF") return a[0];
  if (op == "AND") return ones == n ? 1 : 0;
  if (op == "OR") return ones > 0 ? 1 : 0;
  if (op == "XOR") return ones % 2;
  if (op == "NAND") return ones == n ? 0 : 1;
  if (op == "NOR") return ones > 0 ? 0 : 1;
  return 1 - (ones % 2);   // XNOR
}

// states[t] for t = 0..xs.size(): s(t+1) = f(s(t), x(t)) -- check_B.py `Circuit.run`
std::vector<std::vector<int>> theoryRun(const TCircuit& c, std::vector<int> state, const std::vector<std::vector<int>>& xs) {
  std::map<std::string, std::size_t> inIdx;
  std::map<std::string, std::size_t> gIdx;
  for (std::size_t i = 0; i < c.inputs.size(); ++i) inIdx[c.inputs[i]] = i;
  for (std::size_t i = 0; i < c.gates.size(); ++i) gIdx[c.gates[i].name] = i;
  std::vector<std::vector<int>> states{state};
  for (const std::vector<int>& x : xs) {
    std::vector<int> next;
    for (const TGate& g : c.gates) {
      std::vector<int> args;
      for (const std::string& s : g.src) {
        const auto it = inIdx.find(s);
        args.push_back(it != inIdx.end() ? x[it->second] : states.back()[gIdx[s]]);
      }
      next.push_back(theoryOp(g.op, args));
    }
    states.push_back(next);
  }
  return states;
}

struct Built {
  Model model;
  std::map<std::string, ComponentId> id;
  ComponentId actuator = 0;   // watches one gate when asked to (weight 1: its command is that gate one row later)
};

Built buildCircuit(const TCircuit& c, const std::vector<int>& state0, bool bufAsDelay, int watchGate = -1) {
  Built b;
  ComponentId next = 1;
  for (const std::string& in : c.inputs) {
    b.id[in] = next;
    b.model.comps.push_back(CompDef{next, specSensor(), 0});
    ++next;
  }
  for (std::size_t i = 0; i < c.gates.size(); ++i) {
    const TGate& g = c.gates[i];
    b.id[g.name] = next;
    const unsigned k = u32(g.src.size());
    Spec s;
    if (g.op == "NOT") s = specNot();
    else if (g.op == "BUF") s = bufAsDelay ? specDelay(1) : specGate(GateOp::Or, false, 1);
    else if (g.op == "AND") s = specGate(GateOp::And, false, k);
    else if (g.op == "OR") s = specGate(GateOp::Or, false, k);
    else if (g.op == "XOR") s = specGate(GateOp::Xor, false, k);
    else if (g.op == "NAND") s = specGate(GateOp::And, true, k);
    else if (g.op == "NOR") s = specGate(GateOp::Or, true, k);
    else s = specGate(GateOp::Xor, true, k);
    b.model.comps.push_back(CompDef{next, s, u16(state0[i] != 0 ? 15U : 0U)});
    ++next;
  }
  for (const TGate& g : c.gates) {
    for (std::size_t j = 0; j < g.src.size(); ++j) b.model.wires.push_back(makeWire(outPin(b.id[g.src[j]]), inPin(b.id[g.name], u32(j))));
  }
  if (watchGate >= 0) {
    b.actuator = next;
    b.model.comps.push_back(CompDef{next, specActuator(), 0});
    b.model.wires.push_back(makeWire(outPin(b.id[c.gates[static_cast<std::size_t>(watchGate)].name]), inPin(next, 0)));
  }
  b.model.sortAndNormalize();
  return b;
}

unsigned gateState(const Obs& o, ComponentId id) {
  for (const BlobComp& c : o.data.comps) {
    if (c.id == id) return c.state[0];
  }
  return 999;
}

// ---- running a model and writing each watched state as a wave --------------------------------------------------

struct Watch {
  const char* label;
  ComponentId id;
  std::size_t byte = 0;
  bool clockOut = false;   // report the clock's output instead of its phase
};

std::map<std::string, std::string> runWaves(Ctx& ctx, const EngineFactory& f, const Model& m, const std::vector<Watch>& watch,
                                            unsigned rows, const std::vector<std::string>& inputWaves) {
  std::map<std::string, std::string> out;
  std::string err;
  auto e = makeEngine(f, m, err);
  if (e == nullptr) {
    SLG_FAIL(ctx, err);
    return out;
  }
  for (unsigned t = 0; t < rows; ++t) {
    const Obs o = observe(*e);
    for (const Watch& w : watch) {
      unsigned v = 999;
      for (const BlobComp& c : o.data.comps) {
        if (c.id != w.id) continue;
        if (w.clockOut) {
          const unsigned phase = c.state[0] + 256U * c.state[1];
          v = phase >= static_cast<unsigned>(c.spec.p0 - c.spec.p1) ? 15U : 0U;
        } else {
          v = c.state[w.byte];
        }
      }
      out[w.label] += v == 0 || v == 15 ? waveChar(v) : '?';
    }
    std::vector<Signal> samples;
    for (const std::string& wv : inputWaves) samples.push_back(t < wv.size() && wv[t] == '#' ? Signal{15} : Signal{0});
    if (t + 1 < rows) e->step(samples);
  }
  return out;
}

void expectWave(Ctx& ctx, const std::map<std::string, std::string>& got, const char* label, const std::string& want, const char* what) {
  const auto it = got.find(label);
  SLG_CHECK_MSG(ctx, it != got.end() && it->second == want,
                cat(what, " ", label, ": got  ", it == got.end() ? std::string("(missing)") : it->second, "\n      want ", want));
}

std::string waveNamed(const TheoryWave* waves, std::size_t n, const char* label) {
  for (std::size_t i = 0; i < n; ++i) {
    if (std::string(waves[i].label) == label) return waves[i].wave;
  }
  return "";
}
#define WAVE(arr, label) waveNamed(arr, sizeof(arr) / sizeof(arr[0]), label)

// ---- the literal waveforms ---------------------------------------------------------------------------------------

void checkDffWave(Ctx& ctx, const EngineFactory& f) {
  // initial state of check_B.powerup_dff(0): nCLK=1 nD=1 a1=1 b1=0 M=0 Mn=1 a2=1 b2=1 Q=0 Qn=1
  const std::vector<TCircuit> all = circuits();
  const TCircuit& dff = all[4];
  const std::vector<int> s0 = {1, 1, 1, 0, 0, 1, 1, 1, 0, 1};
  // the same start reached the way theory/04 does: 6 steps with CLK=0, 6 with CLK=1, 6 with CLK=0, D = 0, from all zeros
  std::vector<std::vector<int>> seq;
  for (unsigned i = 0; i < 18; ++i) seq.push_back({0, i >= 6 && i < 12 ? 1 : 0});
  SLG_CHECK_MSG(ctx, theoryRun(dff, std::vector<int>(10, 0), seq).back() == s0, "powerup_dff(0) as derived by the port of Circuit");
  for (bool delayBuf : {false}) {
    const Built b = buildCircuit(dff, s0, delayBuf);
    const std::string clk = WAVE(kWaveDff, "CLK");
    const std::string d = WAVE(kWaveDff, "D");
    const std::vector<Watch> watch = {{"M", b.id.at("M")}, {"Q", b.id.at("Q")}, {"Qn", b.id.at("Qn")}};
    // sensors: D = id 1, CLK = id 2 (input order of the circuit)
    const auto got = runWaves(ctx, f, b.model, watch, 48, {d, clk});
    for (const Watch& w : watch) expectWave(ctx, got, w.label, WAVE(kWaveDff, w.label), "04 Paso 6, flip-flop D:");
  }
}

void checkTimers(Ctx& ctx, const EngineFactory& f) {
  const unsigned pairs[4][2] = {{8, 4}, {8, 2}, {4, 2}, {2, 1}};
  for (unsigned i = 0; i < 4; ++i) {
    Model m;
    m.comps = {CompDef{1, specClock(pairs[i][0], pairs[i][1]), 0}};
    const auto got = runWaves(ctx, f, m, {{"CLK", 1, 0, true}}, 24, {});
    expectWave(ctx, got, "CLK", kWaveTimers[i].wave, cat("06 Paso 3, ", kWaveTimers[i].label, ":").c_str());
  }
}

void checkRingWave(Ctx& ctx, const EngineFactory& f) {
  const auto got = runWaves(ctx, f, ringModel(3, 0b101), {{"g1", 1}, {"g2", 2}, {"g3", 3}}, 16, {});
  for (const char* g : {"g1", "g2", "g3"}) expectWave(ctx, got, g, WAVE(kWaveRing3, g), "06 Paso 2, ring of 3 (seeds 1,0,1):");
}

void checkDivider(Ctx& ctx, const EngineFactory& f) {
  Model m;
  m.comps = {CompDef{1, specClock(4, 2), 0}, CompDef{2, specToggle(), 0}, CompDef{3, specToggle(), 0}, CompDef{4, specToggle(), 0}};
  m.wires = {makeWire(outPin(1), inPin(2, 0)), makeWire(outPin(2), inPin(3, 0)), makeWire(outPin(3), inPin(4, 0))};
  m.sortAndNormalize();
  const auto got = runWaves(ctx, f, m, {{"CLK", 1, 0, true}, {"Z1", 2, 0}, {"Z2", 3, 0}, {"Z3", 4, 0}}, 48, {});
  for (const char* g : {"CLK", "Z1", "Z2", "Z3"}) expectWave(ctx, got, g, WAVE(kWaveDivider, g), "06 Paso 4, divider with Toggle:");
}

void checkEdge(Ctx& ctx, const EngineFactory& f) {
  const std::vector<TCircuit> all = circuits();
  const TCircuit& edge = all[6];
  // check_C starts the gates at 0 except nx, which "arranca en 1"
  const Built b = buildCircuit(edge, {1, 0, 0, 0}, false);
  const std::string x = WAVE(kWaveEdge, "x");
  const std::vector<Watch> watch = {{"nx", b.id.at("nx")}, {"SUBIDA", b.id.at("SUBIDA")}, {"BAJADA", b.id.at("BAJADA")}, {"AMBOS", b.id.at("AMBOS")}};
  const auto got = runWaves(ctx, f, b.model, watch, 24, {x});
  for (const Watch& w : watch) expectWave(ctx, got, w.label, WAVE(kWaveEdge, w.label), "06 Paso 6, edge detector:");
}

void checkExtender(Ctx& ctx, const EngineFactory& f) {
  const std::vector<TCircuit> all = circuits();
  const TCircuit& ext = all[7];
  for (bool delayBuf : {false, true}) {
    const Built b = buildCircuit(ext, {0, 0, 0, 0}, delayBuf);
    const std::string x = WAVE(kWaveExtender, "x");
    const auto got = runWaves(ctx, f, b.model, {{"ANCHO", b.id.at("ANCHO")}}, 26, {x});
    expectWave(ctx, got, "ANCHO", WAVE(kWaveExtender, "ANCHO"), delayBuf ? "06 Paso 7, extender with Delay(1) as BUF:" : "06 Paso 7, extender with a digitizer as BUF:");
  }
}

void checkLatido(Ctx& ctx, const EngineFactory& f) {
  Model m;
  m.comps = {CompDef{1, specClock(2, 1), 0}, CompDef{2, specToggle(), 0}, CompDef{3, specToggle(), 0}, CompDef{4, specToggle(), 0},
             CompDef{5, specNot(), 15}, CompDef{6, specGate(GateOp::And, false, 2), 0}, CompDef{7, specDelay(1), 0},
             CompDef{8, specGate(GateOp::Or, false, 1), 0}, CompDef{9, specDelay(1), 0}, CompDef{10, specGate(GateOp::Or, false, 4), 0}};
  m.wires = {makeWire(outPin(1), inPin(2, 0)), makeWire(outPin(2), inPin(3, 0)), makeWire(outPin(3), inPin(4, 0)),
             makeWire(outPin(4), inPin(5, 0)), makeWire(outPin(4), inPin(6, 0)), makeWire(outPin(5), inPin(6, 1)),
             makeWire(outPin(6), inPin(7, 0)), makeWire(outPin(7), inPin(8, 0)), makeWire(outPin(8), inPin(9, 0)),
             makeWire(outPin(6), inPin(10, 0)), makeWire(outPin(7), inPin(10, 1)), makeWire(outPin(8), inPin(10, 2)),
             makeWire(outPin(9), inPin(10, 3))};
  m.sortAndNormalize();
  const auto got = runWaves(ctx, f, m, {{"CLK", 1, 0, true}, {"Z1", 2, 0}, {"Z2", 3, 0}, {"Z3", 4, 0}, {"PULSO", 6, 0}, {"ANCHO", 10, 0}}, 56, {});
  for (const char* g : {"CLK", "Z1", "Z2", "Z3", "PULSO", "ANCHO"}) expectWave(ctx, got, g, WAVE(kWaveLatido, g), "06 \"latido\" example:");
}

// Moore detector of 101, one-hot, built with the catalog: each state bit is one gate, which is also the register
// that holds it, so one tick is one cycle of the machine. Under the one-hot invariant not-S2 = S0 + S1 + S3, so a
// register nS2 = x + S0 + S2 keeps S1' = x . nS2 to a single gate (a wired OR cannot be used here: a driver that
// feeds two readers puts both in the same net).
void checkDetector101(Ctx& ctx, const EngineFactory& f) {
  Model m;   // x = 1; S0..S3 = 2..5; nS2 = 6
  m.comps = {CompDef{1, specSensor(), 0},
             CompDef{2, specGate(GateOp::Or, true, 3), 15},    // S0' = NOR(x, S1, S3)
             CompDef{3, specGate(GateOp::And, false, 2), 0},   // S1' = x . nS2
             CompDef{4, specGate(GateOp::Or, true, 3), 0},     // S2' = NOR(x, S0, S2)
             CompDef{5, specGate(GateOp::And, false, 2), 0},   // S3' = x . S2
             CompDef{6, specGate(GateOp::Or, false, 3), 15}};  // nS2' = x + S0 + S2
  m.wires = {makeWire(outPin(1), inPin(2, 0)), makeWire(outPin(3), inPin(2, 1)), makeWire(outPin(5), inPin(2, 2)),
             makeWire(outPin(1), inPin(3, 0)), makeWire(outPin(6), inPin(3, 1)),
             makeWire(outPin(1), inPin(4, 0)), makeWire(outPin(2), inPin(4, 1)), makeWire(outPin(4), inPin(4, 2)),
             makeWire(outPin(1), inPin(5, 0)), makeWire(outPin(4), inPin(5, 1)),
             makeWire(outPin(1), inPin(6, 0)), makeWire(outPin(2), inPin(6, 1)), makeWire(outPin(4), inPin(6, 2))};
  m.sortAndNormalize();
  const std::string x = WAVE(kWaveSeq101, "x");
  const auto got = runWaves(ctx, f, m, {{"S0", 2}, {"S1", 3}, {"S2", 4}, {"S3", 5}}, 14, {x});
  // the state of the Moore table at each t, S(t): S0 S1 S2 S3 S2 S3 S1 S2 S3 S2 S0 S1 S2 S3
  static const unsigned table[14] = {0, 1, 2, 3, 2, 3, 1, 2, 3, 2, 0, 1, 2, 3};
  for (unsigned s = 0; s < 4; ++s) {
    std::string want;
    for (unsigned t = 0; t < 14; ++t) want += table[t] == s ? '#' : '_';
    expectWave(ctx, got, cat("S", s).c_str(), want, "07 detector 101, one-hot state:");
  }
  expectWave(ctx, got, "S3", WAVE(kWaveSeq101, "Y Moore"), "07 detector 101, Moore output:");
  // the Mealy output is the Moore output one row earlier (the register that holds it is S3 itself)
  const std::string moore = WAVE(kWaveSeq101, "Y Moore");
  const std::string mealy = WAVE(kWaveSeq101, "Y Mealy");
  SLG_CHECK_MSG(ctx, moore.substr(1) + "_" == mealy, "document check: Mealy(t) = Moore(t + 1) in theory/07");
}

// ---- random gate-level circuits against the port ------------------------------------------------------------------

void checkRandomCircuits(Ctx& ctx, const EngineFactory& f, std::uint64_t cases) {
  const std::vector<TCircuit> all = circuits();
  forEachCase(ctx, cases, [&](std::uint64_t, SplitMix64& rng) {
    const TCircuit& c = all[rng.pick(static_cast<unsigned>(all.size()))];
    std::vector<int> state0(c.gates.size(), 0);
    if (rng.coin()) {
      for (int& v : state0) v = rng.coin() ? 1 : 0;
    }
    const unsigned rows = 12 + rng.pick(30);
    std::vector<std::vector<int>> xs;
    std::vector<int> level(c.inputs.size(), 0);
    for (unsigned t = 0; t + 1 < rows; ++t) {
      for (int& v : level) {
        if (rng.chance(1, 3)) v = rng.coin() ? 1 : 0;
      }
      xs.push_back(level);
    }
    const bool delayBuf = rng.coin();
    const int watch = static_cast<int>(rng.pick(static_cast<unsigned>(c.gates.size())));
    const Built b = buildCircuit(c, state0, delayBuf, watch);
    const auto states = theoryRun(c, state0, xs);
    std::string err;
    auto e = makeEngine(f, b.model, err);
    if (e == nullptr) {
      SLG_FAIL(ctx, err);
      return;
    }
    SLG_CHECK(ctx, true);
    for (unsigned t = 0; t < rows; ++t) {
      const Obs o = observe(*e);
      const std::string bad = selfCheck(o);
      if (!bad.empty()) {
        SLG_FAIL(ctx, cat(c.name, ": row ", t, ": ", bad));
        return;
      }
      // the actuator is one more register: its command in row t is the watched gate in row t - 1 (0, its seed, in row 0)
      const unsigned wantCmd = t == 0 ? 0U : (states[t - 1][static_cast<std::size_t>(watch)] != 0 ? 15U : 0U);
      if (o.commands.size() != 1 || o.commands[0] != wantCmd) {
        SLG_FAIL(ctx, cat(c.name, ": the command of an actuator watching gate ", c.gates[static_cast<std::size_t>(watch)].name, " is ",
                          o.commands.empty() ? 999U : static_cast<unsigned>(o.commands[0]), " in row ", t, ", want ", wantCmd));
        return;
      }
      for (std::size_t g = 0; g < c.gates.size(); ++g) {
        const unsigned want = states[t][g] != 0 ? 15U : 0U;
        if (gateState(o, b.id.at(c.gates[g].name)) != want) {
          SLG_FAIL(ctx, cat(c.name, ": gate ", c.gates[g].name, " is ", gateState(o, b.id.at(c.gates[g].name)), " at row ", t, ", the unit-delay model of check_B.py says ", want,
                            delayBuf ? " (BUF as Delay(1))" : "\n    ", formatLnet(b.model)));
          return;
        }
      }
      if (t + 1 < rows) {
        std::vector<Signal> samples;
        for (int v : xs[t]) samples.push_back(v != 0 ? Signal{15} : Signal{0});
        e->step(samples);
      }
    }
  });
}

}  // namespace

SLG_PROPERTY(T14_theory_bridge, 14, "docs/theory traces reproduce row for row; D flip-flop and detector 101 built from the catalog", RunOn::Both) {
  checkRandomCircuits(ctx, factory, ctx.cases());
  checkDffWave(ctx, factory);
  checkTimers(ctx, factory);
  checkRingWave(ctx, factory);
  checkDivider(ctx, factory);
  checkEdge(ctx, factory);
  checkExtender(ctx, factory);
  checkLatido(ctx, factory);
  checkDetector101(ctx, factory);
}

}  // namespace slgt
