// Helpers shared by the property tests: observing an engine through its blob, comparing observations,
// building an engine from a model, and running an lscn scenario row by row.
#pragma once

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "blob.hpp"
#include "engine.hpp"
#include "golden_io.hpp"

namespace slgt {

struct Obs {
  bool parsed = false;
  BlobData data;                           // full state (from encode())
  std::vector<std::uint8_t> bytes;         // encode() itself
  std::vector<ComponentId> sensors, actuators;
  std::vector<Signal> commands;
  std::uint64_t stateHash = 0;
  std::uint64_t structureHash = 0;
  Tick tick = 0;
  std::uint32_t nextId = 0;
  std::uint32_t hostStep = 0;
  std::uint32_t clamped = 0;
};

Obs observe(const Engine& e);
// The engine's own accessors against the blob it produces and against hashes recomputed here; "" if consistent.
std::string selfCheck(const Obs& o);
// Differences in components, wires, commands and stateHash; with meta also tick, nextId, structureHash,
// hostStepMicros, clampedSamples and the raw bytes. "" when identical.
std::string diffObs(const Obs& a, const Obs& b, bool meta);

std::string describeComp(const BlobComp& c);
std::string describeState(const BlobData& d);
std::string hexBytes(const std::vector<std::uint8_t>& v, std::size_t maxBytes = 48);

// The same observation of a ReferenceSim (hash and blob computed by the independent codec).
Obs observeRef(const ReferenceSim& r);

// Fresh engine holding the model (one batch: adds then connects). err is set and null returned on failure.
std::unique_ptr<Engine> makeEngine(const EngineFactory& f, const Model& m, std::string& err, const Limits& lim = {},
                                   std::uint32_t hostStepMicros = 0);
// Replace the state of every component by the one in d (same netlist). Status of the decode.
Status injectState(Engine& e, const BlobData& d);

// The ids of the sensors of a model, ascending.
std::vector<ComponentId> sensorIdsOf(const Model& m);

// Runs an lscn scenario: builds sc.net, then for tau in [0, rows): applies the batch of tau, takes the
// held sensor values, records the row, and steps (except after the last row). false + err on a rejected
// batch or a failed step.
bool runScenario(Engine& e, const Scenario& sc, unsigned rows, std::vector<TraceRow>& out, std::string& err);
// Runs the scenario on both engines in lockstep and compares the complete observation after every
// apply and every step (blob bytes included). "" if they agree for rows rows, else a description.
std::string runLockstep(Engine& a, Engine& b, const Scenario& sc, unsigned rows);

// Walks an lscn scenario row by row for an engine that may start part-way (after a decode).
class ScenarioCursor {
 public:
  explicit ScenarioCursor(const Scenario& sc) : sc_(sc) {}
  // Row tau: applies the edit batch of tau to e (status in applyStatus), takes the input orders of tau, and returns
  // the samples for e's sensors in ascending id.
  std::vector<Signal> begin(Engine& e, unsigned tau, Status* applyStatus);
  // For an engine restored from a blob taken at row tau after its batch: consume the batches and orders up to tau
  // without applying them, then return the samples of row tau.
  std::vector<Signal> resume(const Engine& e, unsigned tau);

 private:
  std::vector<Signal> samplesFor(const Engine& e) const;
  const Scenario& sc_;
  std::size_t editPos_ = 0;
  std::size_t inPos_ = 0;
  std::map<ComponentId, Signal> held_;
};

// Reduces sc with fails() and prints the result as lnet/lscn text ready for core/golden/regressions/;
// when SLG_REGRESSION_DIR is set the two files are written there as NAME.lnet and NAME.lscn.
void reportScenario(Ctx& ctx, const std::string& what, const Scenario& sc,
                    const std::function<bool(const Scenario&)>& fails);

// "" when the rows agree on in, st, cmd and hash.
std::string diffRows(const TraceRow& a, const TraceRow& b);

// Value of the net at an input pin or output pin at the current tick, computed with the reference
// from the observed state of an engine and the samples that were fed at that tick.
Signal netValueFrom(const Obs& o, Pin p, const std::vector<Signal>& samples, const Limits& lim = {});

}  // namespace slgt
