#include "gen.hpp"

#include <algorithm>

#include "netutil.hpp"

namespace slgt {

// ---- specs, seeds -------------------------------------------------------------------------------------

Spec genSpecOfKind(SplitMix64& rng, Kind k, bool allowWide) {
  switch (k) {
    case Kind::Const: {
      const unsigned r = rng.pick(4);
      return specConst(r == 0 ? 0U : r == 1 ? 15U : rng.pick(16));
    }
    case Kind::Sensor: return specSensor();
    case Kind::Actuator: return specActuator();
    case Kind::Latch: return specLatch();
    case Kind::Toggle: return specToggle();
    case Kind::Gate: {
      unsigned arity;
      if (allowWide && rng.chance(8, 100)) {
        arity = 5 + rng.pick(4);
      } else {
        const unsigned r = rng.pick(100);
        arity = r < 20 ? 1U : r < 60 ? 2U : r < 85 ? 3U : 4U;
      }
      return specGate(static_cast<GateOp>(rng.pick(3)), rng.coin(), arity);
    }
    case Kind::Delay: return specDelay(rng.chance(80, 100) ? 1 + rng.pick(4) : 5 + rng.pick(8));
    case Kind::Compare: return specCompare(static_cast<CompareMode>(rng.pick(2)));
    case Kind::Clock: {
      const unsigned p = 2 + rng.pick(8);
      return specClock(p, 1 + rng.pick(p - 1));
    }
  }
  return specConst(0);
}

Spec genSpec(SplitMix64& rng, bool allowWide) {
  const unsigned r = rng.pick(100);
  Kind k;
  if (r < 28) k = Kind::Gate;
  else if (r < 38) k = Kind::Delay;
  else if (r < 46) k = Kind::Compare;
  else if (r < 54) k = Kind::Latch;
  else if (r < 60) k = Kind::Toggle;
  else if (r < 65) k = Kind::Clock;
  else if (r < 73) k = Kind::Const;
  else if (r < 87) k = Kind::Sensor;
  else k = Kind::Actuator;
  return genSpecOfKind(rng, k, allowWide);
}

unsigned genSeed(SplitMix64& rng, const Spec& s) {
  switch (s.kind) {
    case Kind::Const: case Kind::Sensor: return 0;
    case Kind::Gate: case Kind::Latch: case Kind::Toggle: return rng.coin() ? 15U : 0U;
    case Kind::Actuator: case Kind::Compare: case Kind::Delay: {
      const unsigned r = rng.pick(4);
      return r == 0 ? 0U : r == 1 ? 15U : rng.pick(16);
    }
    case Kind::Clock: return rng.pick(s.p0);
  }
  return 0;
}

// ---- models ---------------------------------------------------------------------------------------------

static std::vector<ComponentId> makeIds(SplitMix64& rng, unsigned n, bool sparse) {
  std::vector<ComponentId> ids;
  ComponentId cur = sparse ? 1 + rng.pick(1000) : 1;
  for (unsigned i = 0; i < n; ++i) {
    ids.push_back(cur);
    cur += sparse ? 1 + rng.pick(7) : 1;
  }
  return ids;
}

Model genModel(SplitMix64& rng, const GenConfig& cfg) {
  const unsigned n = cfg.minComps + rng.pick(cfg.maxComps - cfg.minComps + 1);
  Model m;
  const std::vector<ComponentId> ids = makeIds(rng, n, cfg.sparseIds);
  for (unsigned i = 0; i < n; ++i) {
    CompDef c;
    c.id = ids[i];
    c.spec = genSpec(rng, rng.chance(cfg.wideGatePct, 100));
    c.seed = u16(genSeed(rng, c.spec));
    m.comps.push_back(c);
  }
  if (n >= 2 && rng.chance(85, 100)) {   // most circuits have an input and an output
    m.comps[0].spec = specSensor();
    m.comps[0].seed = 0;
    m.comps[n - 1].spec = specActuator();
    m.comps[n - 1].seed = u16(genSeed(rng, m.comps[n - 1].spec));
  }
  std::vector<unsigned> drivers;
  for (unsigned i = 0; i < n; ++i) {
    if (hasOutput(m.comps[i].spec)) drivers.push_back(i);
  }
  auto addWire = [&](Pin a, Pin b) {
    if (a == b) return;
    m.wires.push_back(makeWire(a, b));
  };
  if (!drivers.empty()) {
    for (unsigned i = 0; i < n; ++i) {
      const CompDef& c = m.comps[i];
      const unsigned inputs = inputCount(c.spec);
      for (unsigned j = 0; j < inputs; ++j) {
        if (rng.chance(cfg.unconnectedPct, 100)) continue;
        unsigned d = drivers[rng.pick(static_cast<unsigned>(drivers.size()))];
        while (!cfg.allowSelfLoops && m.comps[d].id == c.id && drivers.size() > 1) {
          d = drivers[rng.pick(static_cast<unsigned>(drivers.size()))];
        }
        addWire(inPin(c.id, j), outPin(m.comps[d].id));
        if (rng.chance(cfg.extraDriverPct, 100)) {
          addWire(inPin(c.id, j), outPin(m.comps[drivers[rng.pick(static_cast<unsigned>(drivers.size()))]].id));
        }
        if (cfg.allowSelfLoops && hasOutput(c.spec) && rng.chance(4, 100)) addWire(inPin(c.id, j), outPin(c.id));
        if (rng.chance(5, 100)) {   // join two readers
          const CompDef& o = m.comps[rng.pick(n)];
          if (inputCount(o.spec) > 0) addWire(inPin(c.id, j), inPin(o.id, rng.pick(inputCount(o.spec))));
        }
      }
    }
    if (drivers.size() >= 2 && rng.chance(10, 100)) {   // two outputs tied together
      addWire(outPin(m.comps[drivers[rng.pick(static_cast<unsigned>(drivers.size()))]].id),
              outPin(m.comps[drivers[rng.pick(static_cast<unsigned>(drivers.size()))]].id));
    }
  }
  m.sortAndNormalize();
  return m;
}

Model ringModel(unsigned n, std::uint32_t seedMask) {
  Model m;
  for (unsigned i = 1; i <= n; ++i) {
    const bool high = i > 32 || ((seedMask >> (i - 1)) & 1U) != 0;   // beyond 32 gates the mask has no bits: seed 15
    m.comps.push_back(CompDef{i, specNot(), u16(high ? 15U : 0U)});
  }
  for (unsigned i = 1; i <= n; ++i) {
    const unsigned prev = i == 1 ? n : i - 1;
    m.wires.push_back(makeWire(outPin(prev), inPin(i, 0)));
  }
  m.sortAndNormalize();
  return m;
}

Model genRing(SplitMix64& rng, unsigned n, bool randomSeeds) {
  return ringModel(n, randomSeeds ? static_cast<std::uint32_t>(rng.next()) : 0xFFFFFFFFU);
}

Model genWide(SplitMix64& rng, unsigned arity) {
  Model m;
  const unsigned sources = 3 + rng.pick(6);
  for (unsigned i = 1; i <= sources; ++i) {
    if (rng.coin()) m.comps.push_back(CompDef{i, specSensor(), 0});
    else m.comps.push_back(CompDef{i, specConst(rng.pick(16)), 0});
  }
  const ComponentId g = sources + 1;
  m.comps.push_back(CompDef{g, specGate(static_cast<GateOp>(rng.pick(3)), rng.coin(), arity), 0});
  m.comps.back().seed = u16(genSeed(rng, m.comps.back().spec));
  m.comps.push_back(CompDef{g + 1, specActuator(), 0});
  for (unsigned j = 0; j < arity; ++j) {
    const unsigned nd = 1 + rng.pick(3);
    for (unsigned k = 0; k < nd; ++k) m.wires.push_back(makeWire(inPin(g, j), outPin(1 + rng.pick(sources))));
  }
  m.wires.push_back(makeWire(outPin(g), inPin(g + 1, 0)));
  m.sortAndNormalize();
  return m;
}

Model genChain(unsigned length) {
  Model m;
  m.comps.push_back(CompDef{1, specSensor(), 0});
  for (unsigned i = 0; i < length; ++i) m.comps.push_back(CompDef{2 + i, specNot(), 15});
  m.comps.push_back(CompDef{2 + length, specActuator(), 0});
  for (unsigned i = 0; i <= length; ++i) m.wires.push_back(makeWire(outPin(1 + i), inPin(2 + i, 0)));
  m.sortAndNormalize();
  return m;
}

Model genDag(SplitMix64& rng, const DagConfig& cfg) {
  for (;;) {
    const unsigned n = cfg.minComps + rng.pick(cfg.maxComps - cfg.minComps + 1);
    Model m;
    const unsigned sources = 1 + rng.pick(std::max(1U, n / 3));
    for (unsigned i = 0; i < n; ++i) {
      CompDef c;
      c.id = 1 + i * (1 + rng.pick(3));
      if (i > 0 && c.id <= m.comps.back().id) c.id = m.comps.back().id + 1;
      if (i < sources) {
        c.spec = (i == 0 || rng.coin()) ? specSensor() : specConst(rng.pick(16));
      } else {
        const unsigned r = rng.pick(100);
        Kind k = Kind::Gate;
        if (r < 60) k = Kind::Gate;
        else if (r < 72) k = cfg.withCompare ? Kind::Compare : Kind::Gate;
        else if (r < 85) k = cfg.withDelay ? Kind::Delay : Kind::Gate;
        else k = Kind::Actuator;
        c.spec = genSpecOfKind(rng, k, rng.chance(6, 100));
      }
      c.seed = u16(genSeed(rng, c.spec));
      m.comps.push_back(c);
    }
    struct DagNet { std::vector<Pin> pins; };
    std::vector<DagNet> nets;
    std::vector<bool> driverUsed(n, false);
    for (unsigned i = sources; i < n; ++i) {
      const CompDef& c = m.comps[i];
      for (unsigned j = 0; j < inputCount(c.spec); ++j) {
        const unsigned r = rng.pick(100);
        if (r < 12) continue;
        if (r < 45 && !nets.empty()) {
          nets[rng.pick(static_cast<unsigned>(nets.size()))].pins.push_back(inPin(c.id, j));
          continue;
        }
        std::vector<unsigned> free;
        for (unsigned d = 0; d < i; ++d) {
          if (hasOutput(m.comps[d].spec) && !driverUsed[d]) free.push_back(d);
        }
        if (free.empty()) {
          if (!nets.empty()) nets[rng.pick(static_cast<unsigned>(nets.size()))].pins.push_back(inPin(c.id, j));
          continue;
        }
        rng.shuffle(free);
        const unsigned count = std::min<unsigned>(1 + rng.pick(3), static_cast<unsigned>(free.size()));
        DagNet net;
        for (unsigned k = 0; k < count; ++k) {
          driverUsed[free[k]] = true;
          net.pins.push_back(outPin(m.comps[free[k]].id));
        }
        net.pins.push_back(inPin(c.id, j));
        nets.push_back(std::move(net));
      }
    }
    for (DagNet& net : nets) {
      std::vector<Pin> pins = net.pins;
      rng.shuffle(pins);
      for (std::size_t k = 1; k < pins.size(); ++k) {
        m.wires.push_back(makeWire(pins[k], pins[static_cast<std::size_t>(rng.below(k))]));
      }
      if (pins.size() >= 3 && rng.chance(30, 100)) {   // a redundant wire closing a cycle in the wire graph
        m.wires.push_back(makeWire(pins[rng.pick(static_cast<unsigned>(pins.size()))], pins[rng.pick(static_cast<unsigned>(pins.size()))]));
        if (m.wires.back().a == m.wires.back().b) m.wires.pop_back();
      }
    }
    m.sortAndNormalize();
    if (inducedGraphIsAcyclic(analyzeNets(m), m)) return m;
  }
}

Model genAnyModel(SplitMix64& rng) {
  const unsigned r = rng.pick(100);
  if (r < 52) {
    GenConfig cfg;
    cfg.sparseIds = rng.chance(30, 100);
    cfg.maxComps = 4 + rng.pick(14);
    return genModel(rng, cfg);
  }
  if (r < 60) return genRing(rng, 2 + rng.pick(8), true);
  if (r < 68) return genWide(rng, 5 + rng.pick(4));
  if (r < 80) {
    DagConfig cfg;
    return genDag(rng, cfg);
  }
  if (r < 86) {   // a lone self-loop, optionally watched by an actuator
    Model m;
    m.comps.push_back(CompDef{1, specGate(static_cast<GateOp>(rng.pick(3)), rng.coin(), 1 + rng.pick(2)), 0});
    m.comps.back().seed = u16(genSeed(rng, m.comps.back().spec));
    m.wires.push_back(makeWire(outPin(1), inPin(1, 0)));
    if (rng.coin()) {
      m.comps.push_back(CompDef{2, specActuator(), 0});
      m.wires.push_back(makeWire(outPin(1), inPin(2, 0)));
    }
    m.sortAndNormalize();
    return m;
  }
  GenConfig cfg;
  cfg.minComps = 0;
  cfg.maxComps = 4;
  return genModel(rng, cfg);
}

Scenario scenarioFromFrames(const Model& m, const std::vector<std::vector<Signal>>& frames) {
  Scenario sc;
  sc.net = m;
  sc.ticks = frames.empty() ? 0U : u32(frames.size() - 1);
  const std::vector<ComponentId> sensors = [&m] {
    std::vector<ComponentId> r;
    for (const CompDef& c : m.comps) {
      if (c.spec.kind == Kind::Sensor) r.push_back(c.id);
    }
    return r;
  }();
  for (std::size_t s = 0; s < sensors.size(); ++s) {
    Signal last = 0;
    for (std::size_t t = 0; t < frames.size(); ++t) {
      if (frames[t][s] != last) {
        sc.inputs.push_back(SensorOrder{t, sensors[s], frames[t][s]});
        last = frames[t][s];
      }
    }
  }
  std::stable_sort(sc.inputs.begin(), sc.inputs.end(), [](const SensorOrder& a, const SensorOrder& b) {
    if (a.tau != b.tau) return a.tau < b.tau;
    return a.id < b.id;
  });
  return sc;
}

std::vector<std::vector<Signal>> framesOf(const Scenario& sc) {
  std::vector<ComponentId> sensors;
  for (const CompDef& c : sc.net.comps) {
    if (c.spec.kind == Kind::Sensor) sensors.push_back(c.id);
  }
  std::vector<std::vector<Signal>> frames;
  std::map<ComponentId, Signal> held;
  std::size_t pos = 0;
  for (unsigned t = 0; t <= sc.ticks; ++t) {
    while (pos < sc.inputs.size() && sc.inputs[pos].tau <= t) {
      held[sc.inputs[pos].id] = sc.inputs[pos].value;
      ++pos;
    }
    std::vector<Signal> row;
    for (ComponentId id : sensors) {
      const auto it = held.find(id);
      row.push_back(it == held.end() ? Signal{0} : it->second);
    }
    frames.push_back(std::move(row));
  }
  return frames;
}

BlobData randomLegalState(SplitMix64& rng, const Model& m, Tick tick) {
  BlobData d = blobFromModel(m, 0, tick);
  for (BlobComp& c : d.comps) {
    switch (c.spec.kind) {
      case Kind::Const: case Kind::Sensor: break;
      case Kind::Actuator: case Kind::Compare: c.state[0] = u8(rng.pick(16)); break;
      case Kind::Gate: case Kind::Latch: c.state[0] = rng.coin() ? 15 : 0; break;
      case Kind::Toggle: c.state[0] = rng.coin() ? 15 : 0; c.state[1] = rng.coin() ? 1 : 0; break;
      case Kind::Clock: {
        const unsigned phase = rng.pick(c.spec.p0);
        c.state[0] = u8(phase & 0xFFU);
        c.state[1] = u8(phase >> 8);
        break;
      }
      case Kind::Delay:
        for (std::uint8_t& v : c.state) v = u8(rng.pick(16));
        break;
    }
  }
  return d;
}

// ---- samples ----------------------------------------------------------------------------------------------

SampleStyle genStyle(SplitMix64& rng) { return static_cast<SampleStyle>(rng.pick(4)); }

std::vector<std::vector<Signal>> genFrames(SplitMix64& rng, std::size_t sensors, unsigned ticks, SampleStyle style) {
  std::vector<std::vector<Signal>> frames(ticks, std::vector<Signal>(sensors, 0));
  auto value = [&rng]() -> Signal {
    const unsigned r = rng.pick(100);
    if (r < 30) return 0;
    if (r < 60) return 15;
    return u8(1 + rng.pick(14));
  };
  for (std::size_t s = 0; s < sensors; ++s) {
    Signal level = value();
    for (unsigned t = 0; t < ticks; ++t) {
      switch (style) {
        case SampleStyle::Random: level = value(); break;
        case SampleStyle::Constant: break;
        case SampleStyle::Sparse: level = rng.chance(12, 100) ? value() : Signal{0}; break;
        case SampleStyle::Steps:
          if (rng.chance(18, 100)) level = value();
          break;
      }
      frames[t][s] = level;
    }
  }
  return frames;
}

// ---- edits --------------------------------------------------------------------------------------------------

GenState GenState::fromModel(const Model& m) {
  GenState g;
  for (const CompDef& c : m.comps) g.comps[c.id] = c.spec;
  g.wires.insert(m.wires.begin(), m.wires.end());
  g.nextId = m.nextIdHint();
  return g;
}

void GenState::apply(const EditOp& op) {
  switch (op.kind) {
    case EditKind::Add:
      comps[op.id] = op.spec;
      nextId = op.id + 1;
      break;
    case EditKind::Remove:
      comps.erase(op.id);
      for (auto it = wires.begin(); it != wires.end();) {
        if (it->a.component == op.id || it->b.component == op.id) it = wires.erase(it);
        else ++it;
      }
      retired.insert(op.id);
      break;
    case EditKind::Replace: comps[op.id] = op.spec; break;
    case EditKind::Connect: wires.insert(makeWire(op.a, op.b)); break;
    case EditKind::Disconnect: wires.erase(makeWire(op.a, op.b)); break;
  }
}

std::vector<Pin> GenState::allPins() const {
  std::vector<Pin> pins;
  for (const auto& [id, s] : comps) {
    for (unsigned j = 0; j < inputCount(s); ++j) pins.push_back(inPin(id, j));
    if (hasOutput(s)) pins.push_back(outPin(id));
  }
  return pins;
}

std::vector<EditOp> genBatch(SplitMix64& rng, GenState& st, unsigned maxOps, unsigned maxComps) {
  std::vector<EditOp> ops;
  const unsigned count = 1 + rng.pick(maxOps);
  const bool viaAdd = rng.coin();   // never mixed in one batch: "base + adds so far" is only defined for add()
  auto push = [&](const EditOp& op) {
    st.apply(op);
    ops.push_back(op);
  };
  for (unsigned k = 0; k < count; ++k) {
    unsigned wAdd = st.comps.size() < maxComps ? 22 : 0;
    unsigned wRemove = st.comps.empty() ? 0 : 12;
    unsigned wReplace = st.comps.empty() ? 0 : 14;
    const std::vector<Pin> pins = st.allPins();
    unsigned wConnect = pins.size() >= 2 ? 34 : 0;
    unsigned wDisconnect = st.wires.empty() ? 0 : 18;
    const unsigned total = wAdd + wRemove + wReplace + wConnect + wDisconnect;
    if (total == 0) break;
    unsigned r = rng.pick(total);
    if (r < wAdd) {
      const Spec s = genSpec(rng, false);
      const ComponentId id = viaAdd ? st.nextId : st.nextId + rng.pick(4);
      push(opAdd(id, s, genSeed(rng, s), viaAdd));
      continue;
    }
    r -= wAdd;
    if (r < wRemove) {
      auto it = st.comps.begin();
      std::advance(it, static_cast<std::ptrdiff_t>(rng.pick(static_cast<unsigned>(st.comps.size()))));
      push(opRemove(it->first));
      continue;
    }
    r -= wRemove;
    if (r < wReplace) {
      auto it = st.comps.begin();
      std::advance(it, static_cast<std::ptrdiff_t>(rng.pick(static_cast<unsigned>(st.comps.size()))));
      const ComponentId id = it->first;
      const Spec s = genSpec(rng, false);
      std::vector<Wire> clash;
      for (const Wire& w : st.wires) {
        if ((w.a.component == id && !pinValid(s, w.a.index)) || (w.b.component == id && !pinValid(s, w.b.index))) {
          clash.push_back(w);
        }
      }
      for (const Wire& w : clash) push(opDisconnect(w.a, w.b));
      push(opReplace(id, s, genSeed(rng, s)));
      continue;
    }
    r -= wReplace;
    if (r < wConnect) {
      for (unsigned attempt = 0; attempt < 8; ++attempt) {
        const Pin a = pins[rng.pick(static_cast<unsigned>(pins.size()))];
        const Pin b = pins[rng.pick(static_cast<unsigned>(pins.size()))];
        if (a == b || st.wires.count(makeWire(a, b)) != 0) continue;
        push(opConnect(a, b));
        break;
      }
      continue;
    }
    auto it = st.wires.begin();
    std::advance(it, static_cast<std::ptrdiff_t>(rng.pick(static_cast<unsigned>(st.wires.size()))));
    const Wire w = *it;
    push(opDisconnect(w.a, w.b));
  }
  return ops;
}

const char* faultName(Fault f) noexcept {
  static const char* const names[] = {"BadSpecDomain", "BadSpecSeed", "BadSpecUnused", "BadPinConnect", "BadPinReplace",
                                      "UnknownRemove", "UnknownConnect", "DuplicateId", "RetiredId", "UnknownWire",
                                      "DuplicateWire", "StaleBase", "NextIdExhausted", "OverCap", "SpecOrder",
                                      "DisconnectUnknownComponent", "DisconnectBadPin", "WireOrder", "AutoIdCollision"};
  const auto i = static_cast<unsigned>(f);
  return i < sizeof(names) / sizeof(names[0]) ? names[i] : "?";
}

namespace {

// A pin of a component that does not exist: above every id the netlist ever handed out.
Pin missingPin(const GenState& st, SplitMix64& rng) {
  const ComponentId id = st.nextId + 3 + rng.pick(5);
  return rng.coin() ? outPin(id) : inPin(id, rng.pick(3));
}

// An index that no pin of any kind has (inputs end at 8 gates wide, and the output is kOut).
Pin badPinOf(ComponentId c, SplitMix64& rng) { return inPin(c, 100 + rng.pick(100)); }

// An op that creates `spec` with the id the batch would give it: add() when nothing before it fixed an id by hand, so
// the id add() hands out is the one the op names.
EditOp faultAdd(SplitMix64& rng, const GenState& st, const std::vector<EditOp>& prefix, Spec spec, unsigned seed) {
  bool explicitBefore = false;
  for (const EditOp& op : prefix) explicitBefore = explicitBefore || (op.kind == EditKind::Add && !op.viaAdd);
  return opAdd(st.nextId, spec, seed, !explicitBefore && rng.coin());
}

// A spec whose arity, Delay length or Clock period is above the default cap (01 section 7, B2); everything else about it
// is as legal as it can be, or wrong in a way that the cap must win over (a high width outside 1..P-1).
Spec overCapSpec(SplitMix64& rng) {
  static const unsigned delays[] = {256, 257, 300, 1000, 65535};
  static const unsigned arities[] = {9, 10, 16, 255, 256, 65535};
  static const unsigned periods[] = {4097, 5000, 40000, 65535};
  switch (rng.pick(3)) {
    case 0: return specDelay(delays[rng.pick(5)]);
    case 1: return specGate(static_cast<GateOp>(rng.pick(3)), rng.coin(), arities[rng.pick(6)]);
    default: {
      const unsigned p = periods[rng.pick(4)];
      const unsigned h = rng.coin() ? (rng.coin() ? 0U : rng.coin() ? p : 65535U) : 1 + rng.pick(p - 1);
      return specClock(p, h);
    }
  }
}

}  // namespace

bool genFaultyBatch(SplitMix64& rng, GenState& st, Fault f, std::vector<EditOp>& ops, Status& want) {
  ops.clear();
  want = Status::Ok;
  if (f != Fault::AutoIdCollision && rng.coin()) ops = genBatch(rng, st, 3);
  auto pickComp = [&]() -> ComponentId {
    auto it = st.comps.begin();
    std::advance(it, static_cast<std::ptrdiff_t>(rng.pick(static_cast<unsigned>(st.comps.size()))));
    return it->first;
  };
  auto pickWire = [&]() -> Wire {
    auto it = st.wires.begin();
    std::advance(it, static_cast<std::ptrdiff_t>(rng.pick(static_cast<unsigned>(st.wires.size()))));
    return *it;
  };
  auto realPin = [&]() -> Pin {
    const std::vector<Pin> pins = st.allPins();
    return pins[rng.pick(static_cast<unsigned>(pins.size()))];
  };
  switch (f) {
    case Fault::BadSpecDomain: {
      Spec s;
      switch (rng.pick(5)) {
        case 0: s = specGate(GateOp::And, false, 2); s.p0 = 3; break;
        case 1: s = specConst(16); break;
        case 2: s = specClock(2, 1); s.p0 = 1; break;
        case 3: s = specDelay(1); s.p0 = 0; break;
        default: s = specCompare(CompareMode::Compare); s.p0 = 2; break;
      }
      want = Status::BadSpec;
      ops.push_back(faultAdd(rng, st, ops, s, 0));
      return true;
    }
    case Fault::BadSpecSeed: {
      want = Status::BadSpec;
      switch (rng.pick(4)) {
        case 0: ops.push_back(faultAdd(rng, st, ops, specGate(GateOp::Or, false, 2), 7)); break;
        case 1: ops.push_back(faultAdd(rng, st, ops, specClock(4, 2), 4 + rng.pick(3))); break;
        case 2: ops.push_back(faultAdd(rng, st, ops, specLatch(), 3)); break;
        default: ops.push_back(faultAdd(rng, st, ops, specDelay(2), 16 + rng.pick(3))); break;
      }
      return true;
    }
    case Fault::BadSpecUnused: {
      Spec s = rng.coin() ? specSensor() : specLatch();
      (rng.coin() ? s.p1 : s.p2) = 1;
      want = Status::BadSpec;
      ops.push_back(faultAdd(rng, st, ops, s, 0));
      return true;
    }
    case Fault::OverCap: {
      // LimitExceeded whatever else is wrong with the seed or the high width: the cap is judged first (B2). Through add()
      // or replace() alike.
      const Spec s = overCapSpec(rng);
      const unsigned seed = rng.chance(1, 3) ? 200U + rng.pick(60000) : rng.pick(16);
      want = Status::LimitExceeded;
      if (!st.comps.empty() && rng.coin()) ops.push_back(opReplace(pickComp(), s, seed));
      else ops.push_back(faultAdd(rng, st, ops, s, seed));
      return true;
    }
    case Fault::SpecOrder: {
      // A malformed parameter is BadSpec even when the arity, the length or the period is also above its cap.
      Spec s;
      switch (rng.pick(7)) {
        case 0: s = specGate(GateOp::Or, false, 300); s.p0 = 3; break;
        case 1: s = specGate(GateOp::Xor, true, 9); s.p1 = 2; break;
        case 2: s = specDelay(300); s.p1 = 1; break;
        case 3: s = specDelay(65535); s.p2 = 1; break;
        case 4: s = specClock(5000, 2); s.p2 = 1; break;
        case 5: s = specClock(1, 1); break;
        default: s = specClock(0, 0); break;
      }
      want = Status::BadSpec;
      if (!st.comps.empty() && rng.coin()) ops.push_back(opReplace(pickComp(), s, 0));
      else ops.push_back(faultAdd(rng, st, ops, s, 0));
      return true;
    }
    case Fault::BadPinConnect: {
      if (st.comps.empty()) return false;
      const ComponentId c = pickComp();
      Pin bad = inPin(c, 200);
      if (st.comps.find(c)->second.kind == Kind::Actuator && rng.coin()) bad = outPin(c);
      Pin other = realPin();
      if (other == bad) other = inPin(c, 201);
      want = Status::BadPin;
      ops.push_back(rng.coin() ? opConnect(bad, other) : opConnect(other, bad));
      return true;
    }
    case Fault::BadPinReplace: {
      if (st.wires.empty()) return false;
      const Wire w = pickWire();
      const Pin p = rng.coin() ? w.a : w.b;
      // A spec that lacks the pin p: an actuator has no output, a const has no inputs.
      const Spec s = p.index == kOut ? specActuator() : specConst(0);
      want = Status::BadPin;
      ops.push_back(opReplace(p.component, s, 0));
      return true;
    }
    case Fault::UnknownRemove:
      want = Status::UnknownComponent;
      ops.push_back(opRemove(st.nextId + 5));
      return true;
    case Fault::UnknownConnect: {
      if (st.comps.empty()) return false;
      const ComponentId c = pickComp();
      const Spec& s = st.comps.find(c)->second;
      const Pin real = hasOutput(s) ? outPin(c) : inPin(c, 0);
      want = Status::UnknownComponent;
      ops.push_back(opConnect(real, inPin(st.nextId + 7, 0)));
      return true;
    }
    case Fault::DuplicateId: {
      if (st.comps.empty()) return false;
      const ComponentId c = pickComp();
      want = Status::DuplicateId;
      ops.push_back(opAdd(c, specSensor(), 0));
      return true;
    }
    case Fault::RetiredId: {
      if (st.comps.empty()) return false;
      const ComponentId c = pickComp();
      const Spec s = st.comps.find(c)->second;
      ops.push_back(opRemove(c));
      st.apply(ops.back());
      ops.push_back(opAdd(c, s, restSeed(s)));
      want = Status::RetiredId;
      return true;
    }
    case Fault::UnknownWire: {
      const std::vector<Pin> pins = st.allPins();
      if (pins.size() < 2) return false;
      for (unsigned attempt = 0; attempt < 16; ++attempt) {
        const Pin a = pins[rng.pick(static_cast<unsigned>(pins.size()))];
        const Pin b = pins[rng.pick(static_cast<unsigned>(pins.size()))];
        if (a == b || st.wires.count(makeWire(a, b)) != 0) continue;
        want = Status::UnknownWire;
        ops.push_back(opDisconnect(a, b));
        return true;
      }
      return false;
    }
    case Fault::DuplicateWire: {
      if (st.wires.empty()) return false;
      const Wire w = pickWire();
      want = Status::DuplicateWire;
      ops.push_back(rng.coin() ? opConnect(w.a, w.b) : opConnect(w.b, w.a));
      return true;
    }
    case Fault::DisconnectUnknownComponent: {
      // B4: disconnect judges a pin like connect does, so a wire that names a missing component is UnknownComponent and
      // not UnknownWire, also when the other pin is bad as well (pin a is judged first, component before pin).
      if (st.comps.empty()) return false;
      want = Status::UnknownComponent;
      switch (rng.pick(6)) {
        case 0: ops.push_back(opDisconnect(missingPin(st, rng), realPin())); return true;
        case 1: ops.push_back(opDisconnect(realPin(), missingPin(st, rng))); return true;
        case 2: ops.push_back(opDisconnect(missingPin(st, rng), missingPin(st, rng))); return true;
        case 3: ops.push_back(opDisconnect(missingPin(st, rng), badPinOf(pickComp(), rng))); return true;
        default: {
          // the realistic one: a component is removed and, in the same batch, one of its wires is disconnected
          if (st.wires.empty()) return false;
          const Wire w = pickWire();
          ops.push_back(opRemove(rng.coin() ? w.a.component : w.b.component));
          st.apply(ops.back());
          ops.push_back(rng.coin() ? opDisconnect(w.a, w.b) : opDisconnect(w.b, w.a));
          return true;
        }
      }
      return false;
    }
    case Fault::DisconnectBadPin: {
      if (st.comps.empty()) return false;
      const ComponentId c = pickComp();
      want = Status::BadPin;
      switch (rng.pick(5)) {
        case 0: ops.push_back(opDisconnect(badPinOf(c, rng), realPin())); return true;
        case 1: ops.push_back(opDisconnect(realPin(), badPinOf(c, rng))); return true;
        case 2: {
          const Pin p = rng.coin() ? realPin() : badPinOf(c, rng);
          ops.push_back(opDisconnect(p, p));   // a pin to itself, valid or not
          return true;
        }
        case 3: ops.push_back(opDisconnect(badPinOf(c, rng), missingPin(st, rng))); return true;   // a is judged first
        default: {
          for (const auto& [id, spec] : st.comps) {   // an actuator has no output pin
            if (spec.kind != Kind::Actuator) continue;
            ops.push_back(rng.coin() ? opDisconnect(outPin(id), realPin()) : opDisconnect(realPin(), outPin(id)));
            return true;
          }
          ops.push_back(opDisconnect(badPinOf(c, rng), realPin()));
          return true;
        }
      }
      return false;
    }
    case Fault::WireOrder: {
      // Two defects in one wire: the first pin is judged completely before the second, for connect and disconnect alike.
      if (st.comps.empty()) return false;
      const ComponentId c = pickComp();
      const bool badFirst = rng.coin();
      const Pin bad = badPinOf(c, rng);
      const Pin gone = missingPin(st, rng);
      const Pin a = badFirst ? bad : gone;
      const Pin b = badFirst ? gone : bad;
      want = badFirst ? Status::BadPin : Status::UnknownComponent;
      ops.push_back(rng.coin() ? opConnect(a, b) : opDisconnect(a, b));
      return true;
    }
    case Fault::AutoIdCollision: {
      // add() gives base + the add() calls so far; an explicit id in that place collides when the add() reaches it, and the
      // collision is what apply reports: DuplicateId, or RetiredId when the explicit one was removed again.
      const ComponentId base = st.nextId;
      const unsigned before = rng.pick(3);
      for (unsigned i = 0; i < before; ++i) {
        const Spec first = genSpec(rng, false);
        ops.push_back(opAdd(base + i, first, genSeed(rng, first), true));
      }
      const Spec taken = genSpec(rng, false);
      ops.push_back(opAdd(base + before, taken, genSeed(rng, taken), false));
      const bool removed = rng.coin();
      if (removed) ops.push_back(opRemove(base + before));
      const Spec late = genSpec(rng, false);
      ops.push_back(opAdd(base + before, late, genSeed(rng, late), true));
      want = removed ? Status::RetiredId : Status::DuplicateId;
      return true;
    }
    case Fault::StaleBase: case Fault::NextIdExhausted: case Fault::kCount: return false;
  }
  return false;
}

// ---- scenarios ----------------------------------------------------------------------------------------------

Scenario genScenario(SplitMix64& rng, const ScenarioConfig& cfg) {
  Scenario sc;
  sc.net = genModel(rng, cfg.net);
  sc.ticks = cfg.minTicks + rng.pick(cfg.maxTicks - cfg.minTicks + 1);
  GenState st = GenState::fromModel(sc.net);
  for (unsigned t = 0; t < sc.ticks && cfg.editPct != 0; ++t) {
    if (!rng.chance(cfg.editPct, 100)) continue;
    const std::vector<EditOp> batch = genBatch(rng, st, 4, 32);
    for (const EditOp& op : batch) sc.edits.push_back(TimedEdit{t, op});
  }
  // Which ids are sensors at each row (after the batch of that row): a sensor order for anything else is an input
  // error for the oracle, so orders are only written where they apply.
  std::set<ComponentId> now;
  for (const CompDef& c : sc.net.comps) {
    if (c.spec.kind == Kind::Sensor) now.insert(c.id);
  }
  std::vector<std::set<ComponentId>> aliveAt(sc.ticks + 1);
  std::set<ComponentId> everSensor = now;
  std::size_t pos = 0;
  for (unsigned t = 0; t <= sc.ticks; ++t) {
    while (pos < sc.edits.size() && sc.edits[pos].tau <= t) {
      const EditOp& op = sc.edits[pos].op;
      if (op.kind == EditKind::Add || op.kind == EditKind::Replace) {
        if (op.spec.kind == Kind::Sensor) {
          now.insert(op.id);
          everSensor.insert(op.id);
        } else {
          now.erase(op.id);
        }
      } else if (op.kind == EditKind::Remove) {
        now.erase(op.id);
      }
      ++pos;
    }
    aliveAt[t] = now;
  }
  for (ComponentId id : everSensor) {
    const std::vector<std::vector<Signal>> series = genFrames(rng, 1, sc.ticks, genStyle(rng));
    Signal last = 0;
    for (unsigned t = 0; t < sc.ticks; ++t) {
      if (aliveAt[t].count(id) != 0 && series[t][0] != last) {
        sc.inputs.push_back(SensorOrder{t, id, series[t][0]});
        last = series[t][0];
      }
    }
  }
  std::stable_sort(sc.inputs.begin(), sc.inputs.end(), [](const SensorOrder& a, const SensorOrder& b) {
    if (a.tau != b.tau) return a.tau < b.tau;
    return a.id < b.id;
  });
  return sc;
}

Scenario reduceScenario(const Scenario& sc, const std::function<bool(const Scenario&)>& fails) {
  Scenario cur = sc;
  bool progress = true;
  unsigned rounds = 0;
  while (progress && rounds++ < 12) {
    progress = false;
    for (unsigned t = 1; t < cur.ticks; ++t) {   // shortest prefix that still fails
      Scenario cand = cur;
      cand.ticks = t;
      if (fails(cand)) {
        cur = std::move(cand);
        progress = true;
        break;
      }
    }
    for (std::size_t i = cur.edits.size(); i-- > 0;) {   // drop whole batches
      const Tick tau = cur.edits[i].tau;
      Scenario cand = cur;
      cand.edits.erase(std::remove_if(cand.edits.begin(), cand.edits.end(),
                                      [tau](const TimedEdit& e) { return e.tau == tau; }),
                       cand.edits.end());
      if (cand.edits.size() != cur.edits.size() && fails(cand)) {
        cur = std::move(cand);
        progress = true;
        i = std::min(i, cur.edits.size());
      }
    }
    for (std::size_t i = cur.inputs.size(); i-- > 0;) {
      Scenario cand = cur;
      cand.inputs.erase(cand.inputs.begin() + static_cast<std::ptrdiff_t>(i));
      if (fails(cand)) {
        cur = std::move(cand);
        progress = true;
      }
    }
    for (std::size_t i = cur.net.wires.size(); i-- > 0;) {
      Scenario cand = cur;
      cand.net.wires.erase(cand.net.wires.begin() + static_cast<std::ptrdiff_t>(i));
      if (fails(cand)) {
        cur = std::move(cand);
        progress = true;
      }
    }
    for (std::size_t i = cur.net.comps.size(); i-- > 0;) {
      Scenario cand = cur;
      const ComponentId id = cand.net.comps[i].id;
      cand.net.comps.erase(cand.net.comps.begin() + static_cast<std::ptrdiff_t>(i));
      cand.net.wires.erase(std::remove_if(cand.net.wires.begin(), cand.net.wires.end(),
                                          [id](const Wire& w) { return w.a.component == id || w.b.component == id; }),
                           cand.net.wires.end());
      if (fails(cand)) {
        cur = std::move(cand);
        progress = true;
      }
    }
  }
  return cur;
}

// ---- blob corruption ------------------------------------------------------------------------------------------

const char* corruptionName(Corruption c) noexcept {
  static const char* const names[] = {
      "GateState7", "ClockPhaseAtPeriod", "DelaySlot16", "NextIdAtMaxId", "WireUnnormalized", "WireUnsorted",
      "WireDuplicate", "WireMissingComponent", "WirePinOutOfRange", "IdsNotAscending", "UnusedParamNonzero", "KindZero",
      "KindTen", "GateOpThree", "GateArityZero", "LatchState7", "TogglePhaseTwo", "ToggleLevel7", "CompareState16",
      "ActuatorState16", "ClockPeriodOne", "ClockHighZero", "ClockHighEqualsPeriod", "DelayLengthZero",
      "ExtraByteWithTail"};
  const auto i = static_cast<unsigned>(c);
  return i < sizeof(names) / sizeof(names[0]) ? names[i] : "?";
}

static BlobComp* firstOfKind(BlobData& d, Kind k) {
  for (BlobComp& c : d.comps) {
    if (c.spec.kind == k) return &c;
  }
  return nullptr;
}

bool corruptBlob(const BlobData& base, Corruption c, std::vector<std::uint8_t>& out) {
  BlobData d = base;
  switch (c) {
    case Corruption::GateState7: {
      BlobComp* g = firstOfKind(d, Kind::Gate);
      if (g == nullptr) return false;
      g->state[0] = 7;
      break;
    }
    case Corruption::ClockPhaseAtPeriod: {
      BlobComp* k = firstOfKind(d, Kind::Clock);
      if (k == nullptr) return false;
      k->state = {u8(k->spec.p0 & 0xFFU), u8(k->spec.p0 >> 8)};
      break;
    }
    case Corruption::DelaySlot16: {
      BlobComp* k = firstOfKind(d, Kind::Delay);
      if (k == nullptr) return false;
      k->state[0] = 16;
      break;
    }
    case Corruption::NextIdAtMaxId:
      if (d.comps.empty()) return false;
      d.nextId = d.comps.back().id;
      break;
    case Corruption::WireUnnormalized:
      if (d.wires.empty()) return false;
      std::swap(d.wires[0].a, d.wires[0].b);
      break;
    case Corruption::WireUnsorted:
      if (d.wires.size() < 2) return false;
      std::swap(d.wires[0], d.wires[1]);
      break;
    case Corruption::WireDuplicate:
      if (d.wires.empty()) return false;
      d.wires.insert(d.wires.begin(), d.wires[0]);
      break;
    case Corruption::WireMissingComponent:
      if (d.wires.empty()) return false;
      d.wires.back().b = Pin{0xFFFFFFF0U, 0};
      break;
    case Corruption::WirePinOutOfRange:
      if (d.wires.empty()) return false;
      d.wires.back().b.index = 100;
      break;
    case Corruption::IdsNotAscending:
      if (d.comps.size() < 2) return false;
      std::swap(d.comps[0], d.comps[1]);
      break;
    case Corruption::UnusedParamNonzero: {
      BlobComp* k = firstOfKind(d, Kind::Sensor);
      if (k == nullptr) k = firstOfKind(d, Kind::Latch);
      if (k == nullptr) k = firstOfKind(d, Kind::Actuator);
      if (k == nullptr) return false;
      k->spec.p2 = 1;
      break;
    }
    case Corruption::KindZero:
    case Corruption::KindTen: {
      if (d.comps.empty()) return false;
      d.comps[0].spec.kind = static_cast<Kind>(c == Corruption::KindZero ? 0 : 10);
      d.comps[0].state.clear();
      break;
    }
    case Corruption::GateOpThree: {
      BlobComp* k = firstOfKind(d, Kind::Gate);
      if (k == nullptr) return false;
      k->spec.p0 = 3;
      break;
    }
    case Corruption::GateArityZero: {
      BlobComp* k = firstOfKind(d, Kind::Gate);
      if (k == nullptr) return false;
      k->spec.p2 = 0;
      break;
    }
    case Corruption::LatchState7: {
      BlobComp* k = firstOfKind(d, Kind::Latch);
      if (k == nullptr) return false;
      k->state[0] = 7;
      break;
    }
    case Corruption::TogglePhaseTwo: {
      BlobComp* k = firstOfKind(d, Kind::Toggle);
      if (k == nullptr) return false;
      k->state[1] = 2;
      break;
    }
    case Corruption::ToggleLevel7: {
      BlobComp* k = firstOfKind(d, Kind::Toggle);
      if (k == nullptr) return false;
      k->state[0] = 7;
      break;
    }
    case Corruption::CompareState16: {
      BlobComp* k = firstOfKind(d, Kind::Compare);
      if (k == nullptr) return false;
      k->state[0] = 16;
      break;
    }
    case Corruption::ActuatorState16: {
      BlobComp* k = firstOfKind(d, Kind::Actuator);
      if (k == nullptr) return false;
      k->state[0] = 16;
      break;
    }
    case Corruption::ClockPeriodOne: {
      BlobComp* k = firstOfKind(d, Kind::Clock);
      if (k == nullptr) return false;
      k->spec.p0 = 1;
      k->spec.p1 = 1;
      k->state = {0, 0};
      break;
    }
    case Corruption::ClockHighZero: {
      BlobComp* k = firstOfKind(d, Kind::Clock);
      if (k == nullptr) return false;
      k->spec.p1 = 0;
      break;
    }
    case Corruption::ClockHighEqualsPeriod: {
      BlobComp* k = firstOfKind(d, Kind::Clock);
      if (k == nullptr) return false;
      k->spec.p1 = k->spec.p0;
      break;
    }
    case Corruption::DelayLengthZero: {
      BlobComp* k = firstOfKind(d, Kind::Delay);
      if (k == nullptr) return false;
      k->spec.p0 = 0;
      k->state.clear();
      break;
    }
    case Corruption::ExtraByteWithTail: {
      const std::vector<std::uint8_t> blob = encodeBlob(d);
      out.clear();
      for (std::size_t i = 0; i + kBlobTailBytes < blob.size(); ++i) out.push_back(blob[i]);
      out.push_back(0x5A);
      for (std::size_t i = 0; i < kBlobTailBytes; ++i) out.push_back(0);
      refreshTail(out);
      return true;
    }
    case Corruption::kCount: return false;
  }
  out = encodeBlob(d);
  return true;
}

}  // namespace slgt
