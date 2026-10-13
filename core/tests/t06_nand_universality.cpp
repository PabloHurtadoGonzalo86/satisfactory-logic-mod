// T6: all 256 Boolean functions of 3 variables are built from three Sensors, NAND = Gate(And, inv, 2)
// and one actuator with NAND depth <= 5; in the 8 constant assignments (readings 15, and also 1..14) the
// command at tau >= L (L <= 6) is 15*f. The construction is found by a breadth-first closure from the
// three projections, computed here.
// Not claimed: a minimum number of gates, functions of nibbles.
#include <algorithm>
#include <map>
#include <optional>
#include <set>

#include "bridge.hpp"
#include "engine.hpp"
#include "gen.hpp"
#include "testkit.hpp"

namespace slgt {

namespace {

struct Nand {
  unsigned depth = 0;   // longest chain of NAND gates ending in this function's gate
  unsigned a = 0, b = 0;
};

unsigned nandOf(unsigned a, unsigned b) { return (~(a & b)) & 0xFFU; }

// truth table: bit i is f at the assignment (x, y, z) = (bit 2, bit 1, bit 0) of i
unsigned projection(unsigned var) {
  unsigned t = 0;
  for (unsigned i = 0; i < 8; ++i) t |= ((i >> (2 - var)) & 1U) << i;
  return t;
}

std::map<unsigned, Nand> closure() {
  std::map<unsigned, Nand> reached;
  std::set<unsigned> cum{projection(0), projection(1), projection(2)};
  for (unsigned level = 1; level <= 8; ++level) {
    std::map<unsigned, Nand> fresh;
    for (unsigned a : cum) {
      for (unsigned b : cum) {
        const unsigned f = nandOf(a, b);
        if (reached.count(f) == 0 && fresh.count(f) == 0) fresh[f] = Nand{level, a, b};
      }
    }
    if (fresh.empty()) break;
    for (const auto& [f, n] : fresh) {
      reached[f] = n;
      cum.insert(f);
    }
  }
  return reached;
}

struct Built {
  Model model;
  unsigned depth = 0;
};

Built build(const std::map<unsigned, Nand>& table, unsigned f, unsigned nandSeed) {
  const auto isProjection = [](unsigned t) { return t == projection(0) || t == projection(1) || t == projection(2); };
  // The target is always built as a NAND gate (an actuator wired to a bare sensor would show the raw reading, not 15),
  // so a projection target gets its own gate while its operands refer to the sensors.
  std::set<unsigned> needed{f};
  std::vector<unsigned> stack{table.find(f)->second.a, table.find(f)->second.b};
  while (!stack.empty()) {
    const unsigned t = stack.back();
    stack.pop_back();
    if (isProjection(t) || !needed.insert(t).second) continue;
    stack.push_back(table.find(t)->second.a);
    stack.push_back(table.find(t)->second.b);
  }
  std::vector<unsigned> order(needed.begin(), needed.end());
  std::stable_sort(order.begin(), order.end(), [&](unsigned x, unsigned y) {
    const unsigned dx = table.find(x)->second.depth;
    const unsigned dy = table.find(y)->second.depth;
    return dx != dy ? dx < dy : x < y;
  });
  Built b;
  std::map<unsigned, ComponentId> sensorId;
  std::map<unsigned, ComponentId> gateId;
  for (unsigned v = 0; v < 3; ++v) {
    sensorId[projection(v)] = v + 1;
    b.model.comps.push_back(CompDef{v + 1, specSensor(), 0});
  }
  ComponentId next = 4;
  for (unsigned t : order) {
    gateId[t] = next;
    b.model.comps.push_back(CompDef{next, specGate(GateOp::And, true, 2), u16(nandSeed)});
    ++next;
  }
  const auto source = [&](unsigned t) { return isProjection(t) ? sensorId[t] : gateId[t]; };
  for (unsigned t : order) {
    const Nand& n = table.find(t)->second;
    b.model.wires.push_back(makeWire(outPin(source(n.a)), inPin(gateId[t], 0)));
    b.model.wires.push_back(makeWire(outPin(source(n.b)), inPin(gateId[t], 1)));
  }
  b.model.comps.push_back(CompDef{next, specActuator(), 0});
  b.model.wires.push_back(makeWire(outPin(gateId[f]), inPin(next, 0)));
  b.model.sortAndNormalize();
  b.depth = table.find(f)->second.depth;
  return b;
}

}  // namespace

SLG_PROPERTY(T06_nand_universality, 6, "all 256 functions of 3 variables from NAND with depth <= 5, run on every constant assignment", RunOn::Both) {
  const std::map<unsigned, Nand> table = closure();
  SLG_CHECK_MSG(ctx, table.size() == 256, cat("the closure reached only ", table.size(), " of 256 functions"));
  unsigned deepest = 0;
  for (const auto& [f, n] : table) deepest = std::max(deepest, n.depth);
  SLG_CHECK_MSG(ctx, deepest <= 5, cat("deepest NAND depth is ", deepest, ", the spec says <= 5"));
  SplitMix64 rng(ctx.caseSeed(0));
  for (unsigned f = 0; f < 256 && !ctx.tooManyFailures(); ++f) {
    if (table.count(f) == 0) continue;
    for (unsigned nandSeed : {0U, 15U}) {
      const Built b = build(table, f, nandSeed);
      const unsigned L = b.depth + 1;
      SLG_CHECK_MSG(ctx, L <= 6, cat("function ", f, " needs L = ", L));
      std::string err;
      auto e = makeEngine(factory, b.model, err);
      if (e == nullptr) {
        SLG_FAIL(ctx, err);
        return;
      }
      if (factory.isSim) {
        std::vector<std::optional<std::uint32_t>> bounds;
        if (bridge::settlingBounds(b.model, Limits{}, bounds) == Status::Ok) {
          SLG_CHECK_MSG(ctx, bounds.size() == 1 && bounds[0].has_value() && *bounds[0] == L,
                        cat("settlingBounds of function ", f, ": ", bounds.size() == 1 && bounds[0].has_value() ? cat(*bounds[0]) : std::string("none"), ", want ", L));
        } else {
          SLG_FAIL(ctx, "settlingBounds could not analyse the netlist");
        }
      }
      for (unsigned variant = 0; variant < 2; ++variant) {
        for (unsigned i = 0; i < 8; ++i) {
          std::vector<Signal> in(3);
          for (unsigned v = 0; v < 3; ++v) {
            const bool on = ((i >> (2 - v)) & 1U) != 0;
            in[v] = on ? (variant == 0 ? Signal{15} : u8(1 + rng.pick(14))) : Signal{0};
          }
          const unsigned want = ((f >> i) & 1U) != 0 ? 15U : 0U;
          for (unsigned t = 0; t < L + 3; ++t) {
            const std::vector<Signal> cmd = e->commands();
            if (t >= L && (cmd.size() != 1 || cmd[0] != want)) {
              SLG_FAIL(ctx, cat("function ", f, " (nand seed ", nandSeed, ") at assignment ", i, " with readings ", static_cast<unsigned>(in[0]), ",",
                                static_cast<unsigned>(in[1]), ",", static_cast<unsigned>(in[2]), ": command ", cmd.empty() ? 999U : static_cast<unsigned>(cmd[0]),
                                " at tau = ", t, " (L = ", L, "), want ", want, "\n    ", formatLnet(b.model)));
              return;
            }
            e->step(in);
          }
        }
      }
    }
  }
}

}  // namespace slgt
