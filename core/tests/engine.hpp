// The surface the property tests drive: a tiny engine interface with two implementations, the
// reference interpreter (always available) and the core's Simulator (behind the single bridge file).
// Properties written against Engine run twice, as <name>[ref] and <name>[sim]: the [ref] run checks
// the test logic itself against the reference, the [sim] run checks the core.
#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "harness.hpp"
#include "reference_sim.hpp"
#include "tmodel.hpp"

namespace slgt {

class Engine {
 public:
  virtual ~Engine() = default;
  virtual const char* label() const = 0;
  virtual bool isSim() const = 0;

  // EditBatch semantics. prepare() captures the base nextId at the moment it is called, like
  // Simulator::newBatch(); applyPrepared() applies it later (StaleBatch when nextId moved on).
  virtual Status apply(const std::vector<EditOp>& ops) = 0;
  virtual std::size_t prepare(const std::vector<EditOp>& ops) = 0;
  virtual Status applyPrepared(std::size_t handle) = 0;
  // false when an EditBatch::add handed out an id other than op.id (the core broke "base + adds so far").
  virtual bool lastBatchIdsMatched() const = 0;

  virtual Status step(const std::vector<Signal>& samples) = 0;
  virtual Tick tick() const = 0;
  virtual std::uint32_t nextId() const = 0;
  virtual std::uint32_t hostStepMicros() const = 0;
  virtual std::uint32_t clampedSamples() const = 0;
  virtual std::vector<ComponentId> sensorIds() const = 0;
  virtual std::vector<ComponentId> actuatorIds() const = 0;
  virtual std::vector<Signal> commands() const = 0;
  virtual std::uint64_t stateHash() const = 0;
  virtual std::uint64_t structureHash() const = 0;
  virtual std::vector<std::uint8_t> encode() const = 0;

  // decode into a fresh engine object and adopt it only on Ok (the adapter's way, 02 section 6.3)
  virtual Status loadBlob(const std::vector<std::uint8_t>& bytes) = 0;
  // the static decode straight onto this object: on a non-Ok status nothing may have changed
  virtual Status decodeDirect(const std::vector<std::uint8_t>& bytes, const Limits& lim) = 0;

  const Limits& limits() const noexcept { return limits_; }

 protected:
  explicit Engine(const Limits& l) : limits_(l) {}
  Limits limits_;
};

struct EngineFactory {
  const char* label;
  bool isSim;
  std::function<std::unique_ptr<Engine>(const Limits&, std::uint32_t hostStepMicros)> make;
};

const EngineFactory& refFactory();
EngineFactory quirkedRefFactory(Quirk q);   // the reference with a deliberate misreading (s01_sensitivity.cpp)
const EngineFactory* simFactory();   // nullptr when the core is not linked into this binary

// ---- registration of properties ---------------------------------------------------------------------

using PropertyFn = void (*)(Ctx&, const EngineFactory&);
enum class RunOn : std::uint8_t { Both, SimOnly, RefOnly };
struct PropertyInfo {
  PropertyFn fn;
  RunOn on;
  int number;
};
const std::map<std::string, PropertyInfo>& propertyTable();   // every SLG_PROPERTY by function name

struct PropertyRegistrar {
  PropertyRegistrar(const char* name, int property, const char* what, PropertyFn fn, RunOn on);
};

#define SLG_PROPERTY(fn, number, what, on)                                                         \
  static void fn(::slgt::Ctx&, const ::slgt::EngineFactory&);                                      \
  static ::slgt::PropertyRegistrar fn##_registrar(#fn, number, what, &fn, on);                      \
  static void fn([[maybe_unused]] ::slgt::Ctx& ctx, [[maybe_unused]] const ::slgt::EngineFactory& factory)

#define SLG_TEST(fn, number, what)                                                                  \
  static void fn(::slgt::Ctx&);                                                                    \
  static ::slgt::Registrar fn##_registrar(#fn, number, false, what, &fn);                          \
  static void fn([[maybe_unused]] ::slgt::Ctx& ctx)

}  // namespace slgt
