// T5: the algebra of the catalog against closed forms written differently from the interpreters:
// net fusion (commutative, associative, idempotent, identity 0, t(a max b) = t(a) or t(b); 16^3),
// Gate(op, inv, k) for k = 1..3, Compare and Subtract (16^2), Latch, Toggle, Clock (P <= 12, every H and
// phase), Delay(d) = chain of d Delay(1) (d <= 8), plus validate / restSeed / the factories (core only).
// Not claimed: anything outside those domains.
#include "bridge.hpp"
#include "engine.hpp"
#include "gen.hpp"
#include "testkit.hpp"

namespace slgt {

namespace {

unsigned stateByte(const Engine& e, ComponentId id, std::size_t byte = 0) {
  BlobData d;
  if (!parseBlob(e.encode(), d)) return 9999;
  for (const BlobComp& c : d.comps) {
    if (c.id == id) return byte < c.state.size() ? c.state[byte] : 9998U;
  }
  return 9997;
}

unsigned truth(unsigned v) { return v > 0 ? 1U : 0U; }

// ---- fusion ---------------------------------------------------------------------------------------------

void checkFusion(Ctx& ctx, const EngineFactory& f) {
  for (unsigned a = 0; a < 16; ++a) {
    for (unsigned b = 0; b < 16; ++b) {
      for (unsigned c = 0; c < 16; ++c) {
        // Const a, b, c and an actuator and a digitizer on the same net, wired in one of four topologies
        Model m;
        m.comps = {CompDef{1, specConst(a), 0}, CompDef{2, specConst(b), 0}, CompDef{3, specConst(c), 0},
                   CompDef{4, specActuator(), 0}, CompDef{5, specGate(GateOp::Or, false, 1), 0}};
        const unsigned topology = (a * 5 + b * 3 + c) % 4;
        switch (topology) {
          case 0:
            m.wires = {makeWire(outPin(1), inPin(4, 0)), makeWire(outPin(2), inPin(4, 0)), makeWire(outPin(3), inPin(4, 0)),
                       makeWire(inPin(4, 0), inPin(5, 0))};
            break;
          case 1:   // a chain of outputs tied together, the reader at the end
            m.wires = {makeWire(outPin(1), outPin(2)), makeWire(outPin(2), outPin(3)), makeWire(outPin(3), inPin(4, 0)),
                       makeWire(inPin(4, 0), inPin(5, 0))};
            break;
          case 2:   // the readers first, the drivers hang off the digitizer's pin
            m.wires = {makeWire(inPin(4, 0), inPin(5, 0)), makeWire(outPin(3), inPin(5, 0)), makeWire(outPin(1), inPin(5, 0)),
                       makeWire(outPin(2), inPin(4, 0))};
            break;
          default:  // a ring of wires (a cycle in the wire graph)
            m.wires = {makeWire(outPin(1), outPin(2)), makeWire(outPin(2), outPin(3)), makeWire(outPin(3), outPin(1)),
                       makeWire(outPin(1), inPin(4, 0)), makeWire(outPin(2), inPin(5, 0))};
            break;
        }
        m.sortAndNormalize();
        std::string err;
        auto e = makeEngine(f, m, err);
        if (e == nullptr) {
          SLG_FAIL(ctx, err);
          return;
        }
        e->step({});
        const unsigned want = std::max(a, std::max(b, c));
        const unsigned gotAct = stateByte(*e, 4);
        const unsigned gotDig = stateByte(*e, 5);
        if (gotAct != want || gotDig != (truth(a) | truth(b) | truth(c)) * 15U) {
          SLG_FAIL(ctx, cat("fusion of (", a, ",", b, ",", c, ") in topology ", topology, ": actuator ", gotAct, " (want ", want,
                            "), digitizer ", gotDig, " (want ", (truth(a) | truth(b) | truth(c)) * 15U, ")"));
          return;
        }
      }
    }
  }
}

// ---- gates ------------------------------------------------------------------------------------------------

void checkGates(Ctx& ctx, const EngineFactory& f) {
  for (unsigned op = 0; op < 3; ++op) {
    for (unsigned inv = 0; inv < 2; ++inv) {
      for (unsigned k = 1; k <= 3; ++k) {
        Model m;
        for (unsigned i = 1; i <= k; ++i) m.comps.push_back(CompDef{i, specSensor(), 0});
        m.comps.push_back(CompDef{k + 1, specGate(static_cast<GateOp>(op), inv != 0, k), 0});
        for (unsigned i = 1; i <= k; ++i) m.wires.push_back(makeWire(outPin(i), inPin(k + 1, i - 1)));
        m.sortAndNormalize();
        std::string err;
        auto e = makeEngine(f, m, err);
        if (e == nullptr) {
          SLG_FAIL(ctx, err);
          return;
        }
        unsigned total = 1;
        for (unsigned i = 0; i < k; ++i) total *= 16;
        for (unsigned code = 0; code < total; ++code) {
          std::vector<Signal> in(k);
          unsigned truthy = 0;
          for (unsigned i = 0; i < k; ++i) {
            in[i] = u8((code >> (4 * i)) & 0xFU);
            truthy += truth(in[i]);
          }
          e->step(in);
          const bool g = op == 0 ? truthy == k : op == 1 ? truthy > 0 : (truthy % 2) == 1;
          const unsigned want = (g != (inv != 0)) ? 15U : 0U;
          const unsigned got = stateByte(*e, k + 1);
          if (got != want) {
            std::string inputs;
            for (Signal v : in) inputs += cat(static_cast<unsigned>(v), " ");
            SLG_FAIL(ctx, cat("Gate(op=", op, ", inv=", inv, ", k=", k, ") on inputs ", inputs, "gave ", got, ", want ", want));
            return;
          }
        }
      }
    }
  }
}

// ---- compare / subtract -------------------------------------------------------------------------------------

void checkCompare(Ctx& ctx, const EngineFactory& f) {
  for (unsigned mode = 0; mode < 2; ++mode) {
    Model m;
    m.comps = {CompDef{1, specSensor(), 0}, CompDef{2, specSensor(), 0},
               CompDef{3, specCompare(static_cast<CompareMode>(mode)), 0}};
    m.wires = {makeWire(outPin(1), inPin(3, 0)), makeWire(outPin(2), inPin(3, 1))};
    m.sortAndNormalize();
    std::string err;
    auto e = makeEngine(f, m, err);
    if (e == nullptr) {
      SLG_FAIL(ctx, err);
      return;
    }
    for (unsigned a = 0; a < 16; ++a) {
      for (unsigned b = 0; b < 16; ++b) {
        e->step({u8(a), u8(b)});
        const unsigned want = mode == 0 ? (b <= a ? a : 0U) : (a > b ? a - b : 0U);
        const unsigned got = stateByte(*e, 3);
        if (got != want) {
          SLG_FAIL(ctx, cat(mode == 0 ? "Compare" : "Subtract", " A=", a, " B=", b, " gave ", got, ", want ", want));
          return;
        }
      }
    }
  }
}

// ---- latch --------------------------------------------------------------------------------------------------

void checkLatch(Ctx& ctx, const EngineFactory& f) {
  for (unsigned q0 : {0U, 15U}) {
    for (unsigned s = 0; s < 16; ++s) {
      for (unsigned r = 0; r < 16; ++r) {
        Model m;
        m.comps = {CompDef{1, specSensor(), 0}, CompDef{2, specSensor(), 0}, CompDef{3, specLatch(), u16(q0)}};
        m.wires = {makeWire(outPin(1), inPin(3, 0)), makeWire(outPin(2), inPin(3, 1))};
        m.sortAndNormalize();
        std::string err;
        auto e = makeEngine(f, m, err);
        if (e == nullptr) {
          SLG_FAIL(ctx, err);
          return;
        }
        e->step({u8(s), u8(r)});
        const unsigned want = r > 0 ? 0U : (s > 0 ? 15U : q0);
        const unsigned got = stateByte(*e, 3);
        if (got != want) {
          SLG_FAIL(ctx, cat("Latch q=", q0, " S=", s, " R=", r, " gave ", got, ", want ", want, r > 0 && s > 0 ? " (S=R=1: reset dominates)" : ""));
          return;
        }
      }
    }
  }
}

// ---- toggle ---------------------------------------------------------------------------------------------------

void checkToggle(Ctx& ctx, const EngineFactory& f) {
  static const unsigned alphabet[3] = {0, 7, 15};
  for (unsigned q0 : {0U, 15U}) {
    for (unsigned code = 0; code < 729; ++code) {
      std::vector<unsigned> seq(6);
      unsigned c = code;
      for (unsigned& v : seq) {
        v = alphabet[c % 3];
        c /= 3;
      }
      Model m;
      m.comps = {CompDef{1, specSensor(), 0}, CompDef{2, specToggle(), u16(q0)}};
      m.wires = {makeWire(outPin(1), inPin(2, 0))};
      m.sortAndNormalize();
      std::string err;
      auto e = makeEngine(f, m, err);
      if (e == nullptr) {
        SLG_FAIL(ctx, err);
        return;
      }
      unsigned edges = 0;
      bool prev = true;   // p starts at 1: the first tick can never see an edge
      for (unsigned i = 0; i < seq.size(); ++i) {
        e->step({u8(seq[i])});
        const bool t = seq[i] > 0;
        if (t && !prev) ++edges;
        prev = t;
        const unsigned wantQ = (edges % 2 == 0) ? q0 : 15 - q0;
        const unsigned gotQ = stateByte(*e, 2, 0);
        const unsigned gotP = stateByte(*e, 2, 1);
        if (gotQ != wantQ || gotP != (t ? 1U : 0U)) {
          std::string s;
          for (unsigned v : seq) s += cat(v, " ");
          SLG_FAIL(ctx, cat("Toggle q0=", q0, " inputs ", s, "after ", i + 1, " ticks: (q,p) = (", gotQ, ",", gotP, "), want (", wantQ, ",", t ? 1U : 0U, ")"));
          return;
        }
      }
    }
  }
}

// ---- clock ----------------------------------------------------------------------------------------------------

void checkClock(Ctx& ctx, const EngineFactory& f) {
  for (unsigned p = 2; p <= 12; ++p) {
    for (unsigned h = 1; h < p; ++h) {
      for (unsigned phase = 0; phase < p; ++phase) {
        Model m;
        m.comps = {CompDef{1, specClock(p, h), u16(phase)}, CompDef{2, specActuator(), 0}};
        m.wires = {makeWire(outPin(1), inPin(2, 0))};
        m.sortAndNormalize();
        std::string err;
        auto e = makeEngine(f, m, err);
        if (e == nullptr) {
          SLG_FAIL(ctx, err);
          return;
        }
        for (unsigned t = 0; t <= p + 2; ++t) {
          const unsigned c = (phase + t) % p;
          const unsigned gotC = stateByte(*e, 1, 0) + 256U * stateByte(*e, 1, 1);
          const unsigned gotO = stateByte(*e, 2);
          const unsigned prevC = (phase + t + p - 1) % p;
          const unsigned wantO = t == 0 ? 0U : (prevC >= p - h ? 15U : 0U);
          if (gotC != c || gotO != wantO) {
            SLG_FAIL(ctx, cat("Clock(P=", p, ", H=", h, ") phase ", phase, " at tick ", t, ": c = ", gotC, " (want ", c, "), actuator ", gotO,
                              " (want ", wantO, ")"));
            return;
          }
          e->step({});
        }
      }
    }
  }
}

// ---- delay = chain of delay(1) -----------------------------------------------------------------------------------

void checkDelayChain(Ctx& ctx, const EngineFactory& f, SplitMix64& rng) {
  for (unsigned d = 1; d <= 8; ++d) {
    for (unsigned rep = 0; rep < 3; ++rep) {
      const unsigned seed = rng.pick(16);
      Model a;
      a.comps = {CompDef{1, specSensor(), 0}, CompDef{2, specDelay(d), u16(seed)}, CompDef{3, specActuator(), 0}};
      a.wires = {makeWire(outPin(1), inPin(2, 0)), makeWire(outPin(2), inPin(3, 0))};
      a.sortAndNormalize();
      Model b;   // sensor 1, chain 2..d+1, actuator d+2
      b.comps.push_back(CompDef{1, specSensor(), 0});
      for (unsigned i = 0; i < d; ++i) b.comps.push_back(CompDef{2 + i, specDelay(1), u16(seed)});
      b.comps.push_back(CompDef{2 + d, specActuator(), 0});
      for (unsigned i = 0; i <= d; ++i) b.wires.push_back(makeWire(outPin(1 + i), inPin(2 + i, 0)));
      b.sortAndNormalize();
      std::string err;
      auto ea = makeEngine(f, a, err);
      auto eb = makeEngine(f, b, err);
      if (ea == nullptr || eb == nullptr) {
        SLG_FAIL(ctx, err);
        return;
      }
      for (unsigned t = 0; t < 24; ++t) {
        const Obs oa = observe(*ea);
        const Obs ob = observe(*eb);
        bool same = oa.commands == ob.commands;
        // slot f_i of Delay(d) (f1 oldest) is the state of chain element d + 1 - i
        for (unsigned i = 1; i <= d && same; ++i) {
          same = stateByte(*ea, 2, i - 1) == stateByte(*eb, 2 + (d - i), 0);
        }
        if (!same) {
          SLG_FAIL(ctx, cat("Delay(", d, ") and a chain of ", d, " Delay(1) diverge at tick ", t, " (seed ", seed, ")"));
          return;
        }
        const Signal in = u8(rng.pick(16));
        ea->step({in});
        eb->step({in});
      }
    }
  }
}

// ---- spec rules through the core's public functions ------------------------------------------------------------

void checkSpecRules(Ctx& ctx) {
  static const unsigned params[] = {0, 1, 2, 3, 4, 7, 8, 9, 12, 15, 16, 17, 255, 256, 257, 300, 4095, 4096, 4097, 5000, 5001, 65535};
  static const unsigned seeds[] = {0, 1, 2, 3, 7, 15, 16, 17, 255, 4095, 4096};
  Limits small;
  small.maxArity = 3;
  small.maxDelay = 4;
  small.maxClockPeriod = 6;
  Limits raised;   // caps above the catalog ranges: the caps rule, not the ranges of 01 section 3
  raised.maxArity = 12;
  raised.maxDelay = 300;
  raised.maxClockPeriod = 5000;
  const Limits* const limits[3] = {nullptr, &small, &raised};
  for (const Limits* lim : limits) {
    const Limits l = lim == nullptr ? Limits{} : *lim;
    for (unsigned kind = 0; kind <= 10; ++kind) {
      for (unsigned p0 : params) {
        for (unsigned p1 : params) {
          for (unsigned p2 : params) {
            if ((p0 > 5 && p0 != 255 && p0 != 256 && p0 != 300 && p0 != 4096 && p0 != 5001 && p0 != 16) && (p1 > 5 || p2 > 5)) continue;   // keep the grid affordable
            for (unsigned seed : seeds) {
              const Spec s{static_cast<Kind>(kind), u16(p0), u16(p1), u16(p2)};
              const Status want = checkSpec(s, seed, l);
              const Status got = bridge::validateSpec(s, seed, l);
              if (got != want) {
                SLG_FAIL(ctx, cat("validate(kind ", kind, ", p=", p0, ",", p1, ",", p2, ", seed ", seed, ", maxArity ", static_cast<unsigned>(l.maxArity),
                                  ", maxDelay ", l.maxDelay, ", maxClockPeriod ", l.maxClockPeriod, ") returned ", statusName(got), " but the spec says ",
                                  statusName(want)));
                return;
              }
            }
          }
        }
      }
    }
  }
  for (unsigned kind = 1; kind <= 9; ++kind) {
    for (unsigned p0 : {0U, 1U, 2U, 3U, 7U}) {
      for (unsigned p1 : {0U, 1U, 2U}) {
        for (unsigned p2 : {0U, 1U, 3U}) {
          const Spec s{static_cast<Kind>(kind), u16(p0), u16(p1), u16(p2)};
          if (checkSpec(s, 0, Limits{}) != Status::Ok) continue;
          SLG_CHECK_MSG(ctx, bridge::restSeedOf(s) == restSeed(s), cat("restSeed of kind ", kind, " (", p0, ",", p1, ",", p2, "): ", bridge::restSeedOf(s)));
        }
      }
    }
  }
  for (const bridge::FactoryCase& c : bridge::factoryCases()) {
    SLG_CHECK_MSG(ctx, c.got == c.expected,
                  cat(c.call, " returned kind ", static_cast<unsigned>(c.got.kind), " p=(", c.got.p0, ",", c.got.p1, ",", c.got.p2,
                      "), the spec of 02 section 6.1 says kind ", static_cast<unsigned>(c.expected.kind), " p=(", c.expected.p0, ",",
                      c.expected.p1, ",", c.expected.p2, ")"));
  }
}

void passed(Ctx& ctx) { SLG_CHECK(ctx, true); }

}  // namespace

SLG_PROPERTY(T05_component_tables, 5, "fusion algebra, Gate k<=3, Compare/Subtract, Latch, Toggle, Clock, Delay chains, validate/restSeed",
             RunOn::Both) {
  checkFusion(ctx, factory);
  passed(ctx);
  if (ctx.tooManyFailures()) return;
  checkGates(ctx, factory);
  passed(ctx);
  if (ctx.tooManyFailures()) return;
  checkCompare(ctx, factory);
  checkLatch(ctx, factory);
  checkToggle(ctx, factory);
  passed(ctx);
  if (ctx.tooManyFailures()) return;
  checkClock(ctx, factory);
  SplitMix64 rng(ctx.caseSeed(0));
  checkDelayChain(ctx, factory, rng);
  passed(ctx);
  if (factory.isSim) checkSpecRules(ctx);
}

}  // namespace slgt
