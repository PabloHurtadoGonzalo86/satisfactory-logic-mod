#include "reference_sim.hpp"

#include <algorithm>

#include "fnv.hpp"

namespace slgt {

bool compatible(const ApplyResult& expected, Status got) noexcept { return expected.status == got; }

const char* describe(const ApplyResult& r) noexcept { return statusName(r.status); }

ReferenceSim::ReferenceSim(const Limits& lim, std::uint32_t hostStepMicros)
    : limits_(lim), hostStep_(hostStepMicros) {}

std::vector<std::uint16_t> ReferenceSim::seedState(const Spec& s, unsigned seed) const {
  switch (s.kind) {
    case Kind::Const: case Kind::Sensor: return {};
    case Kind::Actuator: case Kind::Gate: case Kind::Compare: case Kind::Latch: return {u16(seed)};
    case Kind::Toggle: return {u16(seed), static_cast<std::uint16_t>(quirk_ == Quirk::ToggleStartsLow ? 0 : 1)};
    case Kind::Clock: return {u16(seed)};
    case Kind::Delay: return std::vector<std::uint16_t>(s.p0 + (quirk_ == Quirk::DelayLong ? 1U : 0U), u16(seed));
  }
  return {};
}

const char* quirkName(Quirk q) noexcept {
  static const char* const names[] = {"none", "in-place update", "saturated-sum fusion", "last driver wins", "truthiness x > 1",
                                      "Delay one tick short", "Delay one tick long", "Compare with <", "Latch: S dominates",
                                      "Toggle by level", "Toggle starts with p = 0", "Toggle rearms on connect",
                                      "sensor one tick late", "actuator command one tick late", "nextId not saved",
                                      "Delay FIFO reversed in the blob"};
  const auto i = static_cast<unsigned>(q);
  return i < sizeof(names) / sizeof(names[0]) ? names[i] : "?";
}

std::vector<ComponentId> ReferenceSim::sensorIds() const {
  std::vector<ComponentId> r;
  for (const auto& [id, c] : comps_) {
    if (c.spec.kind == Kind::Sensor) r.push_back(id);
  }
  return r;
}

std::vector<ComponentId> ReferenceSim::actuatorIds() const {
  std::vector<ComponentId> r;
  for (const auto& [id, c] : comps_) {
    if (c.spec.kind == Kind::Actuator) r.push_back(id);
  }
  return r;
}

std::vector<Signal> ReferenceSim::rawCommands() const {
  std::vector<Signal> r;
  for (const auto& [id, c] : comps_) {
    if (c.spec.kind == Kind::Actuator) r.push_back(u8(c.st[0]));
  }
  return r;
}

std::vector<Signal> ReferenceSim::commands() const {
  const std::vector<Signal> now = rawCommands();
  if (quirk_ == Quirk::ActuatorLate && prevCommands_.size() == now.size()) return prevCommands_;
  return now;
}

// ---- nets and evaluation -------------------------------------------------------------------------

ReferenceSim::Adjacency ReferenceSim::buildAdjacency(const StepOrder* order) const {
  std::vector<Wire> list(wires_.begin(), wires_.end());
  Adjacency adj;
  if (order != nullptr && order->wires.size() == list.size()) {
    for (std::size_t k : order->wires) {
      adj[list[k].a].push_back(list[k].b);
      adj[list[k].b].push_back(list[k].a);
    }
  } else {
    for (const Wire& w : list) {
      adj[w.a].push_back(w.b);
      adj[w.b].push_back(w.a);
    }
  }
  return adj;
}

ReferenceSim::SampleMap ReferenceSim::sampleMap(const std::vector<Signal>& samples) const {
  SampleMap sm;
  std::size_t i = 0;
  for (const auto& [id, c] : comps_) {
    if (c.spec.kind != Kind::Sensor) continue;
    sm[id] = i < samples.size() ? std::min<Signal>(samples[i], kMaxSignal) : Signal{0};
    ++i;
  }
  return sm;
}

unsigned ReferenceSim::outputOf(ComponentId id, const Comp& c, const SampleMap& sm) const {
  switch (c.spec.kind) {
    case Kind::Const: return c.spec.p0;
    case Kind::Sensor: {
      const auto it = sm.find(id);
      return it == sm.end() ? 0U : it->second;
    }
    case Kind::Delay: return quirk_ == Quirk::DelayShort && c.st.size() > 1 ? c.st[1] : c.st[0];
    case Kind::Gate: case Kind::Compare: case Kind::Latch: case Kind::Toggle: return c.st[0];
    case Kind::Clock: return c.st[0] >= c.spec.p0 - c.spec.p1 ? 15U : 0U;
    case Kind::Actuator: return 0;
  }
  return 0;
}

// Walks the wire graph from p; the value is the max of the outputs found (0 when there are none).
// Reads the live state of every component, which is what makes stepInPlace in-place.
unsigned ReferenceSim::netValueOf(const Adjacency& adj, Pin p, const SampleMap& sm) const {
  std::set<Pin> seen;
  std::vector<Pin> stack{p};
  seen.insert(p);
  unsigned value = 0;
  while (!stack.empty()) {
    const Pin cur = stack.back();
    stack.pop_back();
    if (cur.index == kOut) {
      const auto it = comps_.find(cur.component);
      if (it != comps_.end() && hasOutput(it->second.spec)) {
        const unsigned v = outputOf(cur.component, it->second, sm);
        if (quirk_ == Quirk::SumFusion) value = std::min(15U, value + v);
        else if (quirk_ == Quirk::LastDriverWins) value = v;
        else value = std::max(value, v);
      }
    }
    const auto a = adj.find(cur);
    if (a == adj.end()) continue;
    for (const Pin& n : a->second) {
      if (seen.insert(n).second) stack.push_back(n);
    }
  }
  return value;
}

std::vector<std::uint16_t> ReferenceSim::nextState(ComponentId id, const Comp& c, const Adjacency& adj,
                                                   const SampleMap& sm) const {
  const unsigned n = inputCount(c.spec);
  std::vector<unsigned> u(n, 0);
  for (unsigned j = 0; j < n; ++j) u[j] = netValueOf(adj, inPin(id, j), sm);
  switch (c.spec.kind) {
    case Kind::Const: case Kind::Sensor: return {};
    case Kind::Actuator: return {u16(u[0])};
    case Kind::Gate: {
      unsigned ones = 0;
      for (unsigned j = 0; j < n; ++j) ones += truthy(u[j]) ? 1U : 0U;
      bool g = false;
      if (c.spec.p0 == 0) g = ones == n;
      else if (c.spec.p0 == 1) g = ones > 0;
      else g = (ones % 2) == 1;
      const bool out = g != (c.spec.p1 != 0);
      return {static_cast<std::uint16_t>(out ? 15 : 0)};
    }
    case Kind::Delay: {
      std::vector<std::uint16_t> r(c.st.begin() + 1, c.st.end());
      r.push_back(u16(u[0]));
      return r;
    }
    case Kind::Compare: {
      const unsigned a = u[0];
      const unsigned b = u[1];
      if (c.spec.p0 == 0) return {u16((quirk_ == Quirk::CompareLess ? b < a : b <= a) ? a : 0U)};
      return {u16(a > b ? a - b : 0U)};
    }
    case Kind::Latch: {
      if (quirk_ == Quirk::LatchSetDominates) {
        if (truthy(u[0])) return {15};
        if (truthy(u[1])) return {0};
        return {c.st[0]};
      }
      if (truthy(u[1])) return {0};
      if (truthy(u[0])) return {15};
      return {c.st[0]};
    }
    case Kind::Toggle: {
      const bool t = truthy(u[0]);
      const bool r = quirk_ == Quirk::ToggleByLevel ? t : (t && c.st[1] == 0);
      return {static_cast<std::uint16_t>(r ? 15 - c.st[0] : c.st[0]), static_cast<std::uint16_t>(t ? 1 : 0)};
    }
    case Kind::Clock: return {u16((c.st[0] + 1U) % c.spec.p0)};
  }
  return {};
}

Status ReferenceSim::step(const std::vector<Signal>& samples, const StepOrder* order) {
  if (samples.size() != sensorIds().size()) return Status::BadFrame;
  if (quirk_ == Quirk::InPlaceStep && order == nullptr) {
    StepOrder canonical;
    for (const auto& [id, c] : comps_) canonical.comps.push_back(id);
    for (std::size_t i = 0; i < wires_.size(); ++i) canonical.wires.push_back(i);
    return stepInPlace(samples, canonical);
  }
  for (Signal s : samples) {
    if (s > kMaxSignal) ++clamped_;
  }
  SampleMap sm = sampleMap(samples);
  if (quirk_ == Quirk::SensorLate) {
    SampleMap now = sm;
    sm = prevSamples_;
    prevSamples_ = std::move(now);
  }
  if (quirk_ == Quirk::ActuatorLate) prevCommands_ = rawCommands();
  const Adjacency adj = buildAdjacency(order);
  std::vector<ComponentId> ids;
  if (order != nullptr && order->comps.size() == comps_.size()) ids = order->comps;
  else for (const auto& [id, c] : comps_) ids.push_back(id);
  std::map<ComponentId, std::vector<std::uint16_t>> next;
  for (ComponentId id : ids) next[id] = nextState(id, comps_.find(id)->second, adj, sm);
  for (auto& [id, st] : next) comps_.find(id)->second.st = std::move(st);
  ++tick_;
  return Status::Ok;
}

Status ReferenceSim::stepInPlace(const std::vector<Signal>& samples, const StepOrder& order) {
  if (samples.size() != sensorIds().size()) return Status::BadFrame;
  for (Signal s : samples) {
    if (s > kMaxSignal) ++clamped_;
  }
  const SampleMap sm = sampleMap(samples);
  const Adjacency adj = buildAdjacency(&order);
  for (ComponentId id : order.comps) {
    Comp& c = comps_.find(id)->second;
    c.st = nextState(id, c, adj, sm);
  }
  ++tick_;
  return Status::Ok;
}

Signal ReferenceSim::netValue(Pin p, const std::vector<Signal>& samples) const {
  return u8(netValueOf(buildAdjacency(nullptr), p, sampleMap(samples)));
}

std::vector<Pin> ReferenceSim::netPins(Pin p) const {
  const Adjacency adj = buildAdjacency(nullptr);
  std::set<Pin> seen{p};
  std::vector<Pin> stack{p};
  while (!stack.empty()) {
    const Pin cur = stack.back();
    stack.pop_back();
    const auto a = adj.find(cur);
    if (a == adj.end()) continue;
    for (const Pin& n : a->second) {
      if (seen.insert(n).second) stack.push_back(n);
    }
  }
  return std::vector<Pin>(seen.begin(), seen.end());
}

// ---- blob and hashes -----------------------------------------------------------------------------

BlobData ReferenceSim::toBlobData() const {
  BlobData d;
  d.hostStepMicros = hostStep_;
  d.tick = tick_;
  d.nextId = quirk_ == Quirk::NextIdNotSaved ? (comps_.empty() ? 1U : comps_.rbegin()->first + 1U) : nextId_;
  for (const auto& [id, c] : comps_) {
    BlobComp b;
    b.id = id;
    b.spec = c.spec;
    if (c.spec.kind == Kind::Clock) {
      b.state = {u8(c.st[0] & 0xFFU), u8(c.st[0] >> 8)};
    } else {
      for (std::uint16_t v : c.st) b.state.push_back(u8(v));
    }
    if (c.spec.kind == Kind::Delay) {
      b.state.resize(c.spec.p0);
      if (quirk_ == Quirk::DelayBlobReversed) std::reverse(b.state.begin(), b.state.end());
    }
    d.comps.push_back(std::move(b));
  }
  d.wires.assign(wires_.begin(), wires_.end());
  return d;
}

std::vector<std::uint8_t> ReferenceSim::encode() const { return encodeBlob(toBlobData()); }
std::uint64_t ReferenceSim::stateHash() const { return stateHashOf(toBlobData()); }
std::uint64_t ReferenceSim::structureHash() const { return structureHashOf(toBlobData()); }

bool ReferenceSim::load(const BlobData& d) {
  std::map<ComponentId, Comp> comps;
  for (const BlobComp& b : d.comps) {
    if (!kindInRange(b.spec.kind) || b.state.size() != stateSize(b.spec)) return false;
    Comp c;
    c.spec = b.spec;
    if (b.spec.kind == Kind::Clock) c.st = {u16(b.state[0] | (static_cast<unsigned>(b.state[1]) << 8))};
    else for (std::uint8_t v : b.state) c.st.push_back(v);
    comps[b.id] = std::move(c);
  }
  comps_ = std::move(comps);
  wires_ = std::set<Wire>(d.wires.begin(), d.wires.end());
  nextId_ = d.nextId;
  tick_ = d.tick;
  hostStep_ = d.hostStepMicros;
  return true;
}

// ---- edits ---------------------------------------------------------------------------------------

namespace {

struct Work {
  std::map<ComponentId, ReferenceSim::Comp> comps;
  std::set<Wire> wires;
  std::set<ComponentId> retired;   // removed within this batch
  std::uint32_t nextId = 1;
};

ApplyResult fail(Status s) {
  ApplyResult r;
  r.status = s;
  return r;
}

// The largest set of pins joined by wires, counting the pins that appear in a wire (a lone pin is a net of one).
std::size_t largestNet(const std::set<Wire>& wires) {
  std::map<Pin, std::vector<Pin>> adj;
  for (const Wire& w : wires) {
    adj[w.a].push_back(w.b);
    adj[w.b].push_back(w.a);
  }
  std::set<Pin> done;
  std::size_t best = 0;
  for (const auto& [start, ignored] : adj) {
    (void)ignored;
    if (done.count(start) != 0) continue;
    std::set<Pin> net{start};
    std::vector<Pin> stack{start};
    while (!stack.empty()) {
      const Pin cur = stack.back();
      stack.pop_back();
      for (const Pin& next : adj[cur]) {
        if (net.insert(next).second) stack.push_back(next);
      }
    }
    best = std::max(best, net.size());
    done.insert(net.begin(), net.end());
  }
  return best;
}

std::uint64_t delayCells(const Work& w) {
  std::uint64_t n = 0;
  for (const auto& [id, c] : w.comps) {
    if (c.spec.kind == Kind::Delay) n += c.spec.p0;
  }
  return n;
}

// A pin is judged by its component first (UnknownComponent), then by the pin itself (BadPin).
Status pinStatus(const Work& w, const Pin& p) {
  const auto it = w.comps.find(p.component);
  if (it == w.comps.end()) return Status::UnknownComponent;
  return pinValid(it->second.spec, p.index) ? Status::Ok : Status::BadPin;
}

// connect and disconnect: pin a, then pin b, then a wire from a pin to itself.
Status wireStatus(const Work& w, const Pin& a, const Pin& b) {
  Status s = pinStatus(w, a);
  if (s != Status::Ok) return s;
  s = pinStatus(w, b);
  if (s != Status::Ok) return s;
  return a == b ? Status::BadPin : Status::Ok;
}

}  // namespace

ApplyResult ReferenceSim::apply(const std::vector<EditOp>& ops) { return applyWithBase(ops, nextId_); }

ApplyResult ReferenceSim::applyWithBase(const std::vector<EditOp>& ops, std::uint32_t baseNextId) {
  if (baseNextId != nextId_) return fail(Status::StaleBatch);
  Work w;
  w.comps = comps_;
  w.wires = wires_;
  w.nextId = nextId_;
  // 02 sections 2 and 4 (ambiguities.md B3): explicit ids are compared with the base nextId of the batch, in any order;
  // add() hands out base + the number of add() calls so far (explicit ids do not move it), and an id that collides is
  // DuplicateId or RetiredId when the operation is judged; afterwards nextId = max(base, highest id added + 1).
  const std::uint32_t base = nextId_;
  std::uint64_t viaCount = 0;

  for (const EditOp& op : ops) {
    switch (op.kind) {
      case EditKind::Add: {
        ComponentId id = op.id;
        if (op.viaAdd) {
          const std::uint64_t candidate = std::uint64_t{base} + viaCount;
          ++viaCount;
          if (candidate >= 0xFFFFFFFFULL) return fail(Status::LimitExceeded);
          id = u32(candidate);
        }
        if (w.comps.count(id) != 0) return fail(Status::DuplicateId);
        if (id < base || w.retired.count(id) != 0) return fail(Status::RetiredId);
        if (id >= 0xFFFFFFFFU) return fail(Status::LimitExceeded);
        const Status sc = checkSpec(op.spec, op.seed, limits_);
        if (sc != Status::Ok) return fail(sc);
        Comp c;
        c.spec = op.spec;
        c.st = seedState(op.spec, op.seed);
        w.comps[id] = std::move(c);
        w.nextId = std::max(w.nextId, id + 1U);
        break;
      }
      case EditKind::Remove: {
        if (w.comps.count(op.id) == 0) return fail(Status::UnknownComponent);
        w.comps.erase(op.id);
        for (auto it = w.wires.begin(); it != w.wires.end();) {
          if (it->a.component == op.id || it->b.component == op.id) it = w.wires.erase(it);
          else ++it;
        }
        w.retired.insert(op.id);
        break;
      }
      case EditKind::Replace: {
        const auto it = w.comps.find(op.id);
        if (it == w.comps.end()) return fail(Status::UnknownComponent);
        const Status sc = checkSpec(op.spec, op.seed, limits_);
        if (sc != Status::Ok) return fail(sc);
        for (const Wire& x : w.wires) {
          if (x.a.component == op.id && !pinValid(op.spec, x.a.index)) return fail(Status::BadPin);
          if (x.b.component == op.id && !pinValid(op.spec, x.b.index)) return fail(Status::BadPin);
        }
        it->second.spec = op.spec;
        it->second.st = seedState(op.spec, op.seed);
        break;
      }
      case EditKind::Connect: {
        const Status ws = wireStatus(w, op.a, op.b);
        if (ws != Status::Ok) return fail(ws);
        const Wire x = makeWire(op.a, op.b);
        if (w.wires.count(x) != 0) return fail(Status::DuplicateWire);
        w.wires.insert(x);
        if (quirk_ == Quirk::ToggleRearmsOnConnect) {
          for (const Pin& end : {op.a, op.b}) {
            const auto it = w.comps.find(end.component);
            if (it != w.comps.end() && it->second.spec.kind == Kind::Toggle && end.index == 0) it->second.st[1] = 1;
          }
        }
        break;
      }
      case EditKind::Disconnect: {
        const Status ws = wireStatus(w, op.a, op.b);
        if (ws != Status::Ok) return fail(ws);
        const Wire x = makeWire(op.a, op.b);
        if (w.wires.count(x) == 0) return fail(Status::UnknownWire);
        w.wires.erase(x);
        break;
      }
    }
  }
  // 01 section 7 and 02 section 5: the limits judge the netlist the batch leaves behind, not its intermediate steps.
  if (w.comps.size() > limits_.maxComponents || w.wires.size() > limits_.maxWires) return fail(Status::LimitExceeded);
  if (delayCells(w) > limits_.maxDelayCells) return fail(Status::LimitExceeded);
  if (largestNet(w.wires) > limits_.maxNetPins) return fail(Status::LimitExceeded);
  comps_ = std::move(w.comps);
  wires_ = std::move(w.wires);
  nextId_ = w.nextId;
  return ApplyResult{};
}

}  // namespace slgt

// ---- strict decode (02 section 6.3) ---------------------------------------------------------------

namespace slgt {

bool compatible(const DecodeResult& expected, Status got) noexcept { return expected.status == got; }

std::string describe(const DecodeResult& r) { return statusName(r.status); }

namespace {

DecodeResult dfail(Status s) {
  DecodeResult r;
  r.status = s;
  return r;
}

bool stateValid(const Spec& s, const std::vector<std::uint8_t>& st) {
  switch (s.kind) {
    case Kind::Const: case Kind::Sensor: return st.empty();
    case Kind::Actuator: case Kind::Compare: return st.size() == 1 && st[0] <= 15;
    case Kind::Gate: case Kind::Latch: return st.size() == 1 && (st[0] == 0 || st[0] == 15);
    case Kind::Toggle: return st.size() == 2 && (st[0] == 0 || st[0] == 15) && st[1] <= 1;
    case Kind::Clock: return st.size() == 2 && static_cast<unsigned>(st[0] | (st[1] << 8)) < s.p0;
    case Kind::Delay: {
      if (st.size() != s.p0) return false;
      for (std::uint8_t v : st) {
        if (v > 15) return false;
      }
      return true;
    }
  }
  return false;
}

}  // namespace

// The order of 02 section 6.3: magic, versions, tail, header counts against the limits (LimitExceeded) and then, count
// by count, against the bytes left (FormatCorrupt); then component by component the id, the kind, the parameters (a
// parameter over its cap is LimitExceeded, any other invalid one FormatCorrupt) and the state; nextId; the wires; no byte
// left over; and only then the limits of the whole netlist (Delay cells, then pins per net), so a blob that is corrupt
// anywhere answers FormatCorrupt even when it also exceeds one of those totals.
DecodeResult refDecode(const std::uint8_t* data, std::size_t n, const Limits& lim, BlobData& out) {
  if (n < 4 || data[0] != 0x53 || data[1] != 0x4C || data[2] != 0x47 || data[3] != 0x43) return dfail(Status::FormatCorrupt);
  if (n < 8) return dfail(Status::FormatCorrupt);
  const std::uint16_t fmt = readU16(data + 4);
  const std::uint16_t sem = readU16(data + 6);
  if (fmt > 1 || sem > 1) return dfail(Status::FormatTooNew);
  if (fmt == 0 || sem == 0) return dfail(Status::FormatCorrupt);
  if (n < kBlobHeaderBytes + kBlobTailBytes) return dfail(Status::FormatCorrupt);
  if (readU64(data + n - kBlobTailBytes) != fnv1a64(data, n - kBlobTailBytes)) return dfail(Status::FormatCorrupt);

  BlobData d;
  d.formatVersion = fmt;
  d.semanticsVersion = sem;
  d.hostStepMicros = readU32(data + 8);
  d.tick = readU64(data + 12);
  d.nextId = readU32(data + 20);
  const std::uint32_t compCount = readU32(data + 24);
  const std::uint32_t wireCount = readU32(data + 28);
  const std::size_t end = n - kBlobTailBytes;
  const std::size_t remaining = end - kBlobHeaderBytes;
  if (compCount > lim.maxComponents || wireCount > lim.maxWires) return dfail(Status::LimitExceeded);
  if (static_cast<std::uint64_t>(compCount) * kBlobRecordBytes > remaining ||
      static_cast<std::uint64_t>(wireCount) * kBlobWireBytes > remaining) {
    return dfail(Status::FormatCorrupt);
  }

  std::size_t pos = kBlobHeaderBytes;
  std::uint64_t cells = 0;
  ComponentId prev = 0;
  for (std::uint32_t i = 0; i < compCount; ++i) {
    if (end - pos < kBlobRecordBytes) return dfail(Status::FormatCorrupt);
    BlobComp c;
    c.id = readU32(data + pos);
    c.spec.kind = static_cast<Kind>(data[pos + 4]);
    c.spec.p0 = readU16(data + pos + 5);
    c.spec.p1 = readU16(data + pos + 7);
    c.spec.p2 = readU16(data + pos + 9);
    pos += kBlobRecordBytes;
    if (c.id <= prev || c.id == 0xFFFFFFFFU) return dfail(Status::FormatCorrupt);
    prev = c.id;
    if (!kindInRange(c.spec.kind)) return dfail(Status::FormatCorrupt);
    const Status params = checkParams(c.spec, lim);
    if (params == Status::LimitExceeded) return dfail(Status::LimitExceeded);
    if (params != Status::Ok) return dfail(Status::FormatCorrupt);
    const std::size_t ss = stateSize(c.spec);
    if (end - pos < ss) return dfail(Status::FormatCorrupt);
    c.state.assign(data + pos, data + pos + ss);
    pos += ss;
    if (!stateValid(c.spec, c.state)) return dfail(Status::FormatCorrupt);
    if (c.spec.kind == Kind::Delay) cells += c.spec.p0;
    d.comps.push_back(std::move(c));
  }
  if (d.nextId == 0 || d.nextId <= prev) return dfail(Status::FormatCorrupt);

  Pin prevA, prevB;
  for (std::uint32_t i = 0; i < wireCount; ++i) {
    if (end - pos < kBlobWireBytes) return dfail(Status::FormatCorrupt);
    Wire w;
    w.a = Pin{readU32(data + pos), data[pos + 4]};
    w.b = Pin{readU32(data + pos + 5), data[pos + 9]};
    pos += kBlobWireBytes;
    if (!(w.a < w.b)) return dfail(Status::FormatCorrupt);
    if (i > 0 && !(Wire{prevA, prevB} < w)) return dfail(Status::FormatCorrupt);
    prevA = w.a;
    prevB = w.b;
    const BlobComp* ca = nullptr;
    const BlobComp* cb = nullptr;
    for (const BlobComp& c : d.comps) {
      if (c.id == w.a.component) ca = &c;
      if (c.id == w.b.component) cb = &c;
    }
    if (ca == nullptr || cb == nullptr || !pinValid(ca->spec, w.a.index) || !pinValid(cb->spec, w.b.index)) {
      return dfail(Status::FormatCorrupt);
    }
    d.wires.push_back(w);
  }
  if (pos != end) return dfail(Status::FormatCorrupt);

  if (cells > lim.maxDelayCells) return dfail(Status::LimitExceeded);
  if (largestNet(std::set<Wire>(d.wires.begin(), d.wires.end())) > lim.maxNetPins) return dfail(Status::LimitExceeded);
  out = std::move(d);
  return DecodeResult{};
}

}  // namespace slgt
