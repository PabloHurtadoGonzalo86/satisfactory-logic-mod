#include "testkit.hpp"

#include <algorithm>
#include <cstdlib>
#include <map>

#include "gen.hpp"

namespace slgt {

Obs observe(const Engine& e) {
  Obs o;
  o.bytes = e.encode();
  o.parsed = parseBlob(o.bytes, o.data);
  o.sensors = e.sensorIds();
  o.actuators = e.actuatorIds();
  o.commands = e.commands();
  o.stateHash = e.stateHash();
  o.structureHash = e.structureHash();
  o.tick = e.tick();
  o.nextId = e.nextId();
  o.hostStep = e.hostStepMicros();
  o.clamped = e.clampedSamples();
  return o;
}

std::string hexBytes(const std::vector<std::uint8_t>& v, std::size_t maxBytes) {
  std::string s;
  for (std::size_t i = 0; i < v.size() && i < maxBytes; ++i) {
    if (i != 0) s += " ";
    s += hex(v[i], 2);
  }
  if (v.size() > maxBytes) s += " ...";
  return s;
}

std::string describeComp(const BlobComp& c) {
  std::string s = cat(c.id, ":", kindToken(c.spec.kind), "(", c.spec.p0, ",", c.spec.p1, ",", c.spec.p2, ")=");
  s += c.state.empty() ? std::string("-") : hexBytes(c.state, 20);
  return s;
}

std::string describeState(const BlobData& d) {
  std::string s;
  for (const BlobComp& c : d.comps) s += describeComp(c) + "  ";
  return s;
}

std::string selfCheck(const Obs& o) {
  if (!o.parsed) return cat("encode() is not a well-formed blob: ", hexBytes(o.bytes));
  if (encodeBlob(o.data) != o.bytes) return "encode() is not canonical (re-encoding the parsed blob differs)";
  if (stateHashOf(o.data) != o.stateHash) {
    return cat("stateHash() = ", hex(o.stateHash), " but the FNV of the state stream is ", hex(stateHashOf(o.data)));
  }
  if (structureHashOf(o.data) != o.structureHash) {
    return cat("structureHash() = ", hex(o.structureHash), " but the FNV of the structure is ", hex(structureHashOf(o.data)));
  }
  if (o.data.tick != o.tick) return cat("tick() = ", o.tick, " but the blob says ", o.data.tick);
  if (o.data.nextId != o.nextId) return cat("nextId() = ", o.nextId, " but the blob says ", o.data.nextId);
  if (o.data.hostStepMicros != o.hostStep) return "hostStepMicros() differs from the blob header";
  std::vector<ComponentId> sensors, actuators;
  std::vector<Signal> cmds;
  for (const BlobComp& c : o.data.comps) {
    if (c.spec.kind == Kind::Sensor) sensors.push_back(c.id);
    if (c.spec.kind == Kind::Actuator) {
      actuators.push_back(c.id);
      cmds.push_back(c.state[0]);
    }
  }
  if (sensors != o.sensors) return "sensorIds() disagrees with the sensors in the blob";
  if (actuators != o.actuators) return "actuatorIds() disagrees with the actuators in the blob";
  if (cmds != o.commands) return "commands() disagrees with the actuator states in the blob";
  return "";
}

std::string diffObs(const Obs& a, const Obs& b, bool meta) {
  if (!a.parsed || !b.parsed) return "an observation is not a parsable blob";
  if (a.data.comps.size() != b.data.comps.size()) {
    return cat("component count ", a.data.comps.size(), " vs ", b.data.comps.size());
  }
  for (std::size_t i = 0; i < a.data.comps.size(); ++i) {
    if (!(a.data.comps[i] == b.data.comps[i])) {
      return cat("component differs: ", describeComp(a.data.comps[i]), " vs ", describeComp(b.data.comps[i]));
    }
  }
  if (a.data.wires != b.data.wires) return "wires differ";
  if (a.commands != b.commands) return "commands differ";
  if (a.stateHash != b.stateHash) return cat("stateHash ", hex(a.stateHash), " vs ", hex(b.stateHash));
  if (meta) {
    if (a.tick != b.tick) return cat("tick ", a.tick, " vs ", b.tick);
    if (a.nextId != b.nextId) return cat("nextId ", a.nextId, " vs ", b.nextId);
    if (a.hostStep != b.hostStep) return "hostStepMicros differ";
    if (a.clamped != b.clamped) return cat("clampedSamples ", a.clamped, " vs ", b.clamped);
    if (a.structureHash != b.structureHash) return "structureHash differ";
    if (a.bytes != b.bytes) return "encode() bytes differ";
  }
  return "";
}

std::vector<ComponentId> sensorIdsOf(const Model& m) {
  std::vector<ComponentId> r;
  for (const CompDef& c : m.comps) {
    if (c.spec.kind == Kind::Sensor) r.push_back(c.id);
  }
  return r;
}

std::unique_ptr<Engine> makeEngine(const EngineFactory& f, const Model& m, std::string& err, const Limits& lim,
                                   std::uint32_t hostStepMicros) {
  std::unique_ptr<Engine> e = f.make(lim, hostStepMicros);
  const Status s = e->apply(opsForModel(m));
  if (s != Status::Ok) {
    err = cat("building the netlist was rejected: ", statusName(s));
    return nullptr;
  }
  return e;
}

Status injectState(Engine& e, const BlobData& d) {
  BlobData x = d;
  x.nextId = e.nextId();
  x.hostStepMicros = e.hostStepMicros();
  x.tick = e.tick();
  return e.loadBlob(encodeBlob(x));
}

bool runScenario(Engine& e, const Scenario& sc, unsigned rows, std::vector<TraceRow>& out, std::string& err) {
  out.clear();
  {
    const Status s = e.apply(opsForModel(sc.net));
    if (s != Status::Ok) {
      err = cat("initial netlist rejected: ", statusName(s));
      return false;
    }
  }
  std::map<ComponentId, Signal> held;
  std::size_t editPos = 0;
  std::size_t inPos = 0;
  for (unsigned tau = 0; tau < rows; ++tau) {
    std::vector<EditOp> batch;
    while (editPos < sc.edits.size() && sc.edits[editPos].tau <= tau) {
      if (sc.edits[editPos].tau == tau) batch.push_back(sc.edits[editPos].op);
      ++editPos;
    }
    if (!batch.empty()) {
      const Status s = e.apply(batch);
      if (s != Status::Ok) {
        err = cat("edit batch at tau ", tau, " rejected: ", statusName(s));
        return false;
      }
    }
    while (inPos < sc.inputs.size() && sc.inputs[inPos].tau <= tau) {
      held[sc.inputs[inPos].id] = sc.inputs[inPos].value;
      ++inPos;
    }
    const std::vector<ComponentId> sensors = e.sensorIds();
    std::vector<Signal> samples;
    for (ComponentId id : sensors) {
      const auto it = held.find(id);
      samples.push_back(it == held.end() ? Signal{0} : it->second);
    }
    const Obs o = observe(e);
    if (!o.parsed) {
      err = cat("encode() is not a well-formed blob at tau ", tau);
      return false;
    }
    TraceRow row;
    row.tau = tau;
    for (std::size_t i = 0; i < sensors.size(); ++i) row.in.emplace_back(sensors[i], samples[i]);
    for (const BlobComp& c : o.data.comps) {
      if (!c.state.empty()) row.st.push_back(c);
    }
    for (std::size_t i = 0; i < o.actuators.size(); ++i) row.cmd.emplace_back(o.actuators[i], o.commands[i]);
    row.hash = o.stateHash;
    out.push_back(std::move(row));
    if (tau + 1 < rows) {
      const Status s = e.step(samples);
      if (s != Status::Ok) {
        err = cat("step at tau ", tau, " failed: ", statusName(s));
        return false;
      }
    }
  }
  return true;
}

std::string diffRows(const TraceRow& a, const TraceRow& b) {
  if (a.in != b.in) return "sensor samples differ";
  if (a.st.size() != b.st.size()) return "state lists differ in size";
  for (std::size_t i = 0; i < a.st.size(); ++i) {
    if (!(a.st[i] == b.st[i])) return cat("state differs: ", describeComp(a.st[i]), " vs ", describeComp(b.st[i]));
  }
  if (a.cmd != b.cmd) return "actuator commands differ";
  if (a.hash != b.hash) return cat("hash ", hex(a.hash), " vs ", hex(b.hash));
  return "";
}

Signal netValueFrom(const Obs& o, Pin p, const std::vector<Signal>& samples, const Limits& lim) {
  ReferenceSim ref(lim);
  BlobData d = o.data;
  ref.load(d);
  return ref.netValue(p, samples);
}

}  // namespace slgt

namespace slgt {

Obs observeRef(const ReferenceSim& r) {
  Obs o;
  o.data = r.toBlobData();
  o.bytes = encodeBlob(o.data);
  o.parsed = true;
  o.sensors = r.sensorIds();
  o.actuators = r.actuatorIds();
  o.commands = r.commands();
  o.stateHash = r.stateHash();
  o.structureHash = r.structureHash();
  o.tick = r.tick();
  o.nextId = r.nextId();
  o.hostStep = r.hostStepMicros();
  o.clamped = r.clampedSamples();
  return o;
}

std::string runLockstep(Engine& a, Engine& b, const Scenario& sc, unsigned rows) {
  {
    const Status sa = a.apply(opsForModel(sc.net));
    const Status sb = b.apply(opsForModel(sc.net));
    if (sa != Status::Ok || sb != Status::Ok) {
      return cat("initial netlist: ", a.label(), " ", statusName(sa), ", ", b.label(), " ", statusName(sb));
    }
  }
  std::map<ComponentId, Signal> held;
  std::size_t editPos = 0;
  std::size_t inPos = 0;
  for (unsigned tau = 0; tau < rows; ++tau) {
    std::vector<EditOp> batch;
    while (editPos < sc.edits.size() && sc.edits[editPos].tau <= tau) {
      if (sc.edits[editPos].tau == tau) batch.push_back(sc.edits[editPos].op);
      ++editPos;
    }
    if (!batch.empty()) {
      const Status sa = a.apply(batch);
      const Status sb = b.apply(batch);
      if (sa != sb) return cat("tau ", tau, ": apply gave ", a.label(), "=", statusName(sa), " but ", b.label(), "=", statusName(sb));
      if (!a.lastBatchIdsMatched() || !b.lastBatchIdsMatched()) return cat("tau ", tau, ": EditBatch::add handed out an unexpected id");
    }
    while (inPos < sc.inputs.size() && sc.inputs[inPos].tau <= tau) {
      held[sc.inputs[inPos].id] = sc.inputs[inPos].value;
      ++inPos;
    }
    const Obs oa = observe(a);
    const Obs ob = observe(b);
    const std::string bad = selfCheck(oa);
    if (!bad.empty()) return cat("tau ", tau, ": ", a.label(), " is not self-consistent: ", bad);
    const std::string diff = diffObs(oa, ob, true);
    if (!diff.empty()) return cat("tau ", tau, ": ", a.label(), " vs ", b.label(), ": ", diff);
    if (oa.sensors != ob.sensors) return cat("tau ", tau, ": sensorIds differ");
    std::vector<Signal> samples;
    for (ComponentId id : oa.sensors) {
      const auto it = held.find(id);
      samples.push_back(it == held.end() ? Signal{0} : it->second);
    }
    if (tau + 1 < rows) {
      const Status sa = a.step(samples);
      const Status sb = b.step(samples);
      if (sa != Status::Ok || sb != Status::Ok) return cat("tau ", tau, ": step failed: ", statusName(sa), ", ", statusName(sb));
    }
  }
  return "";
}

void reportScenario(Ctx& ctx, const std::string& what, const Scenario& sc,
                    const std::function<bool(const Scenario&)>& fails) {
  const Scenario small = reduceScenario(sc, fails);
  const std::string lnet = formatLnet(small.net);
  const std::string lscn = formatLscn(small);
  std::string text = cat("reduced scenario for ", what, " (", small.net.comps.size(), " components, ", small.ticks, " ticks):\n");
  text += "--- NAME.lnet ---\n" + lnet + "--- NAME.lscn ---\n" + lscn;
  ctx.note(text);
  if (const char* dir = std::getenv("SLG_REGRESSION_DIR")) {
    const std::string base = cat(dir, "/", ctx.name(), "_seed", env().seed);
    writeFile(base + ".lnet", lnet);
    writeFile(base + ".lscn", lscn);
  }
}

}  // namespace slgt

namespace slgt {

std::vector<Signal> ScenarioCursor::samplesFor(const Engine& e) const {
  std::vector<Signal> samples;
  for (ComponentId id : e.sensorIds()) {
    const auto it = held_.find(id);
    samples.push_back(it == held_.end() ? Signal{0} : it->second);
  }
  return samples;
}

std::vector<Signal> ScenarioCursor::begin(Engine& e, unsigned tau, Status* applyStatus) {
  std::vector<EditOp> batch;
  while (editPos_ < sc_.edits.size() && sc_.edits[editPos_].tau <= tau) {
    if (sc_.edits[editPos_].tau == tau) batch.push_back(sc_.edits[editPos_].op);
    ++editPos_;
  }
  Status s = Status::Ok;
  if (!batch.empty()) s = e.apply(batch);
  if (applyStatus != nullptr) *applyStatus = s;
  while (inPos_ < sc_.inputs.size() && sc_.inputs[inPos_].tau <= tau) {
    held_[sc_.inputs[inPos_].id] = sc_.inputs[inPos_].value;
    ++inPos_;
  }
  return samplesFor(e);
}

std::vector<Signal> ScenarioCursor::resume(const Engine& e, unsigned tau) {
  while (editPos_ < sc_.edits.size() && sc_.edits[editPos_].tau <= tau) ++editPos_;
  while (inPos_ < sc_.inputs.size() && sc_.inputs[inPos_].tau <= tau) {
    held_[sc_.inputs[inPos_].id] = sc_.inputs[inPos_].value;
    ++inPos_;
  }
  return samplesFor(e);
}

}  // namespace slgt
