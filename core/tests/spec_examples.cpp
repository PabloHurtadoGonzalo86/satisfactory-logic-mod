// G1 to G10, G13 and G14 (G11 and G12 are host goldens, in t13) as tests: each example of 01 and 02,
// row by row, on both engines. They are the ground truth the other properties lean on.
#include "spec_examples.hpp"

#include "engine.hpp"
#include "gen.hpp"
#include "testkit.hpp"

namespace slgt {

const std::uint64_t kG1StateHash[5] = {0x9D16793581913B31ULL, 0x9D16723581912F4CULL, 0x9D16723581912F4CULL,
                                       0x4622C4783DE15BB9ULL, 0x4622CD783DE16B04ULL};
const unsigned kG1Nand[5] = {15, 15, 15, 0, 0};
const unsigned kG1Act[5] = {0, 15, 15, 15, 0};

Model g1Model() {
  Model m;
  m.comps = {CompDef{1, specSensor(), 0}, CompDef{2, specNot(), 15}, CompDef{3, specActuator(), 0}};
  m.wires = {makeWire(outPin(1), inPin(2, 0)), makeWire(outPin(2), inPin(3, 0))};
  m.sortAndNormalize();
  return m;
}

const std::vector<std::uint8_t>& g7Blob() {
  static const std::vector<std::uint8_t> blob = {
      0x53, 0x4C, 0x47, 0x43, 0x01, 0x00, 0x01, 0x00, 0xA0, 0x86, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x04, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,
      0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x02, 0x00, 0x00, 0x00, 0x04, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x0F,
      0x03, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x01, 0x00, 0x00, 0x00, 0xFF, 0x02, 0x00, 0x00, 0x00, 0x00,
      0x02, 0x00, 0x00, 0x00, 0xFF, 0x03, 0x00, 0x00, 0x00, 0x00,
      0x4F, 0x95, 0xAE, 0x13, 0xE3, 0x4E, 0x14, 0x3D};
  return blob;
}

Model g3Model() {
  Model m;
  m.comps = {CompDef{1, specSensor(), 0}, CompDef{2, specSensor(), 0}, CompDef{3, specGate(GateOp::And, false, 2), 0},
             CompDef{4, specDelay(2), 0}, CompDef{5, specActuator(), 0}};
  m.wires = {makeWire(outPin(1), inPin(3, 0)), makeWire(outPin(2), inPin(3, 1)), makeWire(outPin(3), inPin(4, 0)),
             makeWire(outPin(4), inPin(5, 0))};
  m.sortAndNormalize();
  return m;
}

Model g4Model() {
  const Spec nor2 = specGate(GateOp::Or, true, 2);
  Model m;
  m.comps = {CompDef{1, specSensor(), 0}, CompDef{2, specSensor(), 0}, CompDef{3, nor2, 0}, CompDef{4, nor2, 15},
             CompDef{5, specLatch(), 0}};
  m.wires = {makeWire(outPin(1), inPin(4, 0)), makeWire(outPin(3), inPin(4, 1)), makeWire(outPin(2), inPin(3, 0)),
             makeWire(outPin(4), inPin(3, 1)), makeWire(outPin(1), inPin(5, 0)), makeWire(outPin(2), inPin(5, 1))};
  m.sortAndNormalize();
  return m;
}

Model g5Model() {
  Model m;
  m.comps = {CompDef{1, specNot(), 0}, CompDef{2, specNot(), 0}};
  m.wires = {makeWire(outPin(2), inPin(1, 0)), makeWire(outPin(1), inPin(2, 0))};
  m.sortAndNormalize();
  return m;
}

Model g6Model() {
  Model m;
  m.comps = {CompDef{1, specConst(9), 0}, CompDef{2, specConst(9), 0}, CompDef{3, specActuator(), 0}};
  m.wires = {makeWire(outPin(1), inPin(3, 0)), makeWire(outPin(2), inPin(3, 0))};
  m.sortAndNormalize();
  return m;
}

Model g10Model() {
  Model m;
  m.comps = {CompDef{1, specClock(6, 3), 0}, CompDef{2, specActuator(), 0}};
  m.wires = {makeWire(outPin(1), inPin(2, 0))};
  m.sortAndNormalize();
  return m;
}

Model g13Model() {
  Model m;
  m.comps = {CompDef{1, specSensor(), 0}, CompDef{2, specNot(), 0}, CompDef{3, specNot(), 0}, CompDef{4, specNot(), 0}};
  m.wires = {makeWire(outPin(1), inPin(2, 0)), makeWire(outPin(2), inPin(3, 0)), makeWire(outPin(3), inPin(4, 0))};
  m.sortAndNormalize();
  return m;
}

Model g14Model() {
  Model m;
  m.comps = {CompDef{1, specSensor(), 0}, CompDef{2, specNot(), 15}, CompDef{3, specActuator(), 0}};
  m.wires = {makeWire(outPin(1), inPin(2, 0)), makeWire(inPin(3, 0), outPin(1)), makeWire(inPin(3, 0), outPin(2))};
  m.sortAndNormalize();
  return m;
}

char waveChar(unsigned v) {
  if (v == 0) return '_';
  if (v == 15) return '#';
  return "0123456789ABCDEF"[v & 0xFU];
}

namespace {

// state[byte] of component id in an observation, or 999 if the component is not there
unsigned stateOf(const Obs& o, ComponentId id, std::size_t byte = 0) {
  for (const BlobComp& c : o.data.comps) {
    if (c.id == id) return byte < c.state.size() ? c.state[byte] : 999U;
  }
  return 999;
}

unsigned nib(char c) { return c == 'F' ? 15U : static_cast<unsigned>(c - '0'); }

std::unique_ptr<Engine> build(Ctx& ctx, const EngineFactory& f, const Model& m, std::uint32_t hostStep = 0) {
  std::string err;
  std::unique_ptr<Engine> e = makeEngine(f, m, err, Limits{}, hostStep);
  if (e == nullptr) SLG_FAIL(ctx, err);
  return e;
}

void checkConsistent(Ctx& ctx, const Obs& o, const char* what) {
  const std::string bad = selfCheck(o);
  SLG_CHECK_MSG(ctx, bad.empty(), cat(what, ": ", bad));
}

}  // namespace

void checkG1Rows(Ctx& ctx, const EngineFactory& factory) {
  auto e = build(ctx, factory, g1Model());
  if (e == nullptr) return;
  for (unsigned tau = 0; tau < 5; ++tau) {
    const Obs o = observe(*e);
    checkConsistent(ctx, o, "G1");
    SLG_CHECK_MSG(ctx, stateOf(o, 2) == kG1Nand[tau], cat("row ", tau, ": N = ", stateOf(o, 2), " want ", kG1Nand[tau]));
    SLG_CHECK_MSG(ctx, stateOf(o, 3) == kG1Act[tau], cat("row ", tau, ": A = ", stateOf(o, 3), " want ", kG1Act[tau]));
    SLG_CHECK_MSG(ctx, o.commands.size() == 1 && o.commands[0] == kG1Act[tau], cat("row ", tau, ": commands()"));
    SLG_CHECK_MSG(ctx, o.stateHash == kG1StateHash[tau],
                  cat("row ", tau, ": stateHash ", hex(o.stateHash), " want ", hex(kG1StateHash[tau])));
    SLG_CHECK_MSG(ctx, o.tick == tau, cat("tick() ", o.tick, " in row ", tau));
    e->step({static_cast<Signal>(tau < 2 ? 0 : 15)});
  }
}

void checkG7Bytes(Ctx& ctx, const EngineFactory& factory) {
  auto e = build(ctx, factory, g1Model(), kG7HostStepMicros);
  if (e == nullptr) return;
  const std::vector<std::uint8_t> got = e->encode();
  SLG_CHECK_MSG(ctx, got.size() == 95, cat("blob size ", got.size()));
  SLG_CHECK_MSG(ctx, got == g7Blob(), cat("blob bytes differ: ", hexBytes(got, 100)));
  SLG_CHECK_MSG(ctx, e->structureHash() == kG7StructureHash, cat("structureHash ", hex(e->structureHash())));
  SLG_CHECK(ctx, e->hostStepMicros() == kG7HostStepMicros);
  SLG_CHECK(ctx, e->nextId() == 4);
}

SLG_PROPERTY(G01_lat_not, 1, "G1: sensor -> NOT -> actuator, rows and stateHash of 02 section 6.4", RunOn::Both) { checkG1Rows(ctx, factory); }

SLG_PROPERTY(G07_blob, 10, "G7: the 95-byte blob of G1, its tail and its structureHash", RunOn::Both) { checkG7Bytes(ctx, factory); }

SLG_PROPERTY(G02_ring3, 8, "G2: ring of three NOT, periods 6 (seeds F,0,F) and 2 (seeds 0,0,0), 6+2 split of the 8 states", RunOn::Both) {
  static const char* const rows[7] = {"F0F", "00F", "0FF", "0F0", "FF0", "F00", "F0F"};
  auto e = build(ctx, factory, ringModel(3, 0b101));
  if (e == nullptr) return;
  for (unsigned tau = 0; tau < 7; ++tau) {
    const Obs o = observe(*e);
    for (unsigned g = 0; g < 3; ++g) {
      SLG_CHECK_MSG(ctx, stateOf(o, 1 + g) == nib(rows[tau][g]), cat("ring3 row ", tau, " g", g + 1));
    }
    e->step({});
  }
  auto z = build(ctx, factory, ringModel(3, 0));
  if (z == nullptr) return;
  for (unsigned tau = 0; tau < 6; ++tau) {
    const Obs o = observe(*z);
    const unsigned want = tau % 2 == 0 ? 0U : 15U;
    for (unsigned g = 0; g < 3; ++g) SLG_CHECK_MSG(ctx, stateOf(o, 1 + g) == want, cat("ring3_zero row ", tau));
    z->step({});
  }
  // The 8 states form one cycle of 6 and one of 2 (theory/07 Paso 5).
  unsigned period2 = 0;
  unsigned period6 = 0;
  for (std::uint32_t mask = 0; mask < 8; ++mask) {
    auto r = build(ctx, factory, ringModel(3, mask));
    if (r == nullptr) return;
    const Obs start = observe(*r);
    unsigned p = 0;
    for (unsigned k = 1; k <= 8 && p == 0; ++k) {
      r->step({});
      if (observe(*r).data.comps == start.data.comps) p = k;
    }
    if (p == 2) ++period2;
    if (p == 6) ++period6;
  }
  SLG_CHECK_MSG(ctx, period2 == 2 && period6 == 6, cat("periods: ", period2, " states of period 2 and ", period6, " of period 6"));
}

SLG_PROPERTY(G03_and_delay2, 3, "G3: two sensors -> AND -> Delay(2) -> actuator, the table of 01 section 9.2", RunOn::Both) {
  // rows tau = 0..8: and, f1, f2, o
  static const char* const rows[9] = {"0000", "0000", "0000", "F000", "F0F0", "0FF0", "0F0F", "000F", "0000"};
  auto e = build(ctx, factory, g3Model());
  if (e == nullptr) return;
  for (unsigned tau = 0; tau < 9; ++tau) {
    const Obs o = observe(*e);
    checkConsistent(ctx, o, "G3");
    SLG_CHECK_MSG(ctx, stateOf(o, 3) == nib(rows[tau][0]) && stateOf(o, 4, 0) == nib(rows[tau][1]) &&
                           stateOf(o, 4, 1) == nib(rows[tau][2]) && stateOf(o, 5) == nib(rows[tau][3]),
                  cat("row ", tau, ": and=", stateOf(o, 3), " f1=", stateOf(o, 4, 0), " f2=", stateOf(o, 4, 1),
                      " o=", stateOf(o, 5), "; want ", rows[tau]));
    const Signal eA = (tau >= 1 && tau <= 3) ? 9 : 0;
    const Signal eB = (tau >= 2 && tau <= 4) ? 5 : 0;
    e->step({eA, eB});
  }
}

SLG_PROPERTY(G04_latch_both, 4, "G4: native Latch against two crossed NOR, S=R released together and a one-tick S pulse", RunOn::Both) {
  {   // A: S=R=F at tau 0..2
    auto e = build(ctx, factory, g4Model());
    if (e == nullptr) return;
    std::string q, qn, nat;
    for (unsigned tau = 0; tau < 10; ++tau) {
      const Obs o = observe(*e);
      q += waveChar(stateOf(o, 3));
      qn += waveChar(stateOf(o, 4));
      nat += waveChar(stateOf(o, 5));
      const Signal v = tau <= 2 ? 15 : 0;
      e->step({v, v});
    }
    SLG_CHECK_MSG(ctx, q == "____#_#_#_", cat("Q  = ", q));
    SLG_CHECK_MSG(ctx, qn == "#___#_#_#_", cat("Qn = ", qn));
    SLG_CHECK_MSG(ctx, nat == "__________", cat("q  = ", nat));
  }
  {   // B: S = F only at tau 1
    auto e = build(ctx, factory, g4Model());
    if (e == nullptr) return;
    std::string q, qn, nat;
    for (unsigned tau = 0; tau < 8; ++tau) {
      const Obs o = observe(*e);
      q += waveChar(stateOf(o, 3));
      qn += waveChar(stateOf(o, 4));
      nat += waveChar(stateOf(o, 5));
      e->step({static_cast<Signal>(tau == 1 ? 15 : 0), 0});
    }
    SLG_CHECK_MSG(ctx, q == "___#_#_#", cat("Q  = ", q));
    SLG_CHECK_MSG(ctx, qn == "##_#_#_#", cat("Qn = ", qn));
    SLG_CHECK_MSG(ctx, nat == "__######", cat("q  = ", nat));
  }
}

SLG_PROPERTY(G05_ring2, 1, "G5: two NOT, seeds (0,0): two-buffer X1 = (F,F); in place the order decides", RunOn::Both) {
  auto e = build(ctx, factory, g5Model());
  if (e == nullptr) return;
  e->step({});
  const Obs o = observe(*e);
  SLG_CHECK_MSG(ctx, stateOf(o, 1) == 15 && stateOf(o, 2) == 15, "two-buffer step from (0,0) must give (F,F)");
  if (factory.isSim) return;
  // Negative control, on the reference: in-place evaluation depends on the order (03 section 7, M01).
  ReferenceSim r1;
  ReferenceSim r2;
  const Model m = g5Model();
  r1.apply(opsForModel(m));
  r2.apply(opsForModel(m));
  StepOrder ab;
  ab.comps = {1, 2};
  ab.wires = {0, 1};
  StepOrder ba;
  ba.comps = {2, 1};
  ba.wires = {0, 1};
  r1.stepInPlace({}, ab);
  r2.stepInPlace({}, ba);
  const auto& c1 = r1.comps();
  const auto& c2 = r2.comps();
  SLG_CHECK_MSG(ctx, c1.find(1)->second.st[0] == 15 && c1.find(2)->second.st[0] == 0, "in place, order a,b: (F,0)");
  SLG_CHECK_MSG(ctx, c2.find(1)->second.st[0] == 0 && c2.find(2)->second.st[0] == 15, "in place, order b,a: (0,F)");
}

SLG_PROPERTY(G06_wired9, 1, "G6: two Const(9) on one actuator input give 9 (a saturated sum would give 15)", RunOn::Both) {
  auto e = build(ctx, factory, g6Model());
  if (e == nullptr) return;
  e->step({});
  const Obs o = observe(*e);
  SLG_CHECK_MSG(ctx, stateOf(o, 3) == 9, cat("o = ", stateOf(o, 3), " in X1"));
  SLG_CHECK(ctx, o.commands.size() == 1 && o.commands[0] == 9);
}

SLG_PROPERTY(G08_toggle_new_high, 11, "G8: a Toggle created and wired in one batch to a high net does not toggle", RunOn::Both) {
  Model m;
  m.comps = {CompDef{1, specConst(15), 0}};
  auto e = build(ctx, factory, m);
  if (e == nullptr) return;
  e->step({});
  e->step({});
  const Status s = e->apply({opAdd(2, specToggle(), 0), opConnect(outPin(1), inPin(2, 0))});
  SLG_CHECK_MSG(ctx, s == Status::Ok, cat("batch: ", statusName(s)));
  Obs o = observe(*e);
  SLG_CHECK_MSG(ctx, stateOf(o, 2, 0) == 0 && stateOf(o, 2, 1) == 1, "a new Toggle starts at q = seed, p = 1");
  for (unsigned k = 0; k < 4; ++k) {
    e->step({});
    o = observe(*e);
    SLG_CHECK_MSG(ctx, stateOf(o, 2, 0) == 0 && stateOf(o, 2, 1) == 1, cat("tick ", k, ": q = ", stateOf(o, 2, 0), " p = ", stateOf(o, 2, 1)));
  }
}

SLG_PROPERTY(G09_toggle_edit_connect, 11, "G9: connecting a wire to a created Toggle is a real 0 to 15 edge: it toggles once", RunOn::Both) {
  Model m;
  m.comps = {CompDef{1, specConst(15), 0}};
  auto e = build(ctx, factory, m);
  if (e == nullptr) return;
  e->step({});
  SLG_CHECK(ctx, e->apply({opAdd(2, specToggle(), 0)}) == Status::Ok);
  e->step({});
  Obs o = observe(*e);
  SLG_CHECK_MSG(ctx, stateOf(o, 2, 0) == 0 && stateOf(o, 2, 1) == 0, "after one tick without a wire: q = 0, p = 0");
  SLG_CHECK(ctx, e->apply({opConnect(outPin(1), inPin(2, 0))}) == Status::Ok);
  e->step({});
  o = observe(*e);
  SLG_CHECK_MSG(ctx, stateOf(o, 2, 0) == 15 && stateOf(o, 2, 1) == 1, cat("after connecting: q = ", stateOf(o, 2, 0), " p = ", stateOf(o, 2, 1)));
  for (unsigned k = 0; k < 3; ++k) {
    e->step({});
    o = observe(*e);
    SLG_CHECK_MSG(ctx, stateOf(o, 2, 0) == 15 && stateOf(o, 2, 1) == 1, "it toggles only once");
  }
}

SLG_PROPERTY(G10_clock6_3, 4, "G10: Clock(6,3), c = 0..5, output ___###___###", RunOn::Both) {
  auto e = build(ctx, factory, g10Model());
  if (e == nullptr) return;
  std::string out;
  for (unsigned tau = 0; tau < 14; ++tau) {
    const Obs o = observe(*e);
    SLG_CHECK_MSG(ctx, stateOf(o, 1, 0) + 256U * stateOf(o, 1, 1) == tau % 6, cat("c in row ", tau));
    if (tau >= 1) out += waveChar(stateOf(o, 2));   // the actuator shows the clock output one row later
    e->step({});
  }
  SLG_CHECK_MSG(ctx, out == "___###___###_", cat("output ", out));   // rows 1..13 show the output of rows 0..12
}

SLG_PROPERTY(G13_chain3, 3, "G13: the settling bound of three NOT is reached: X2 differs from the leveled fixed point, X3 is it", RunOn::Both) {
  static const char* const rows[5] = {"000", "FFF", "F00", "F0F", "F0F"};
  auto e = build(ctx, factory, g13Model());
  if (e == nullptr) return;
  for (unsigned tau = 0; tau < 5; ++tau) {
    const Obs o = observe(*e);
    for (unsigned g = 0; g < 3; ++g) {
      SLG_CHECK_MSG(ctx, stateOf(o, 2 + g) == nib(rows[tau][g]), cat("chain3 row ", tau, " g", g + 1, " = ", stateOf(o, 2 + g)));
    }
    e->step({0});
  }
}

SLG_PROPERTY(G14_mergeloop, 7, "G14: a conductor added to a net creates a loop with no wire back (01 section 5)", RunOn::Both) {
  {   // e = 0: B has period 2 and C follows it
    auto e = build(ctx, factory, g14Model());
    if (e == nullptr) return;
    std::string b;
    for (unsigned tau = 0; tau < 8; ++tau) {
      b += waveChar(stateOf(observe(*e), 2));
      e->step({0});
    }
    SLG_CHECK_MSG(ctx, b == "#_#_#_#_", cat("B with e = 0: ", b));
  }
  {   // e = 9: B drops to 0 and stays, C reads max(9, B): 15 in X1, 9 from X2
    auto e = build(ctx, factory, g14Model());
    if (e == nullptr) return;
    unsigned bs[5];
    unsigned cs[5];
    for (unsigned tau = 0; tau < 5; ++tau) {
      const Obs o = observe(*e);
      bs[tau] = stateOf(o, 2);
      cs[tau] = stateOf(o, 3);
      e->step({9});
    }
    SLG_CHECK_MSG(ctx, bs[0] == 15 && bs[1] == 0 && bs[2] == 0 && bs[3] == 0 && bs[4] == 0, "B with e = 9");
    SLG_CHECK_MSG(ctx, cs[1] == 15 && cs[2] == 9 && cs[3] == 9 && cs[4] == 9, cat("C with e = 9: ", cs[1], ",", cs[2], ",", cs[3]));
  }
}

}  // namespace slgt
