#include "engine.hpp"

namespace slgt {

namespace {

class RefEngine final : public Engine {
 public:
  RefEngine(const Limits& l, std::uint32_t hostStep, Quirk q) : Engine(l), sim_(l, hostStep) { sim_.setQuirk(q); }

  const char* label() const override { return "ref"; }
  bool isSim() const override { return false; }

  Status apply(const std::vector<EditOp>& ops) override {
    matched_ = true;
    return sim_.apply(ops).status;
  }
  std::size_t prepare(const std::vector<EditOp>& ops) override {
    prepared_.push_back({ops, sim_.nextId()});
    return prepared_.size() - 1;
  }
  Status applyPrepared(std::size_t handle) override {
    matched_ = true;
    return sim_.applyWithBase(prepared_[handle].ops, prepared_[handle].base).status;
  }
  bool lastBatchIdsMatched() const override { return matched_; }

  Status step(const std::vector<Signal>& samples) override { return sim_.step(samples); }
  Tick tick() const override { return sim_.tick(); }
  std::uint32_t nextId() const override { return sim_.nextId(); }
  std::uint32_t hostStepMicros() const override { return sim_.hostStepMicros(); }
  std::uint32_t clampedSamples() const override { return sim_.clampedSamples(); }
  std::vector<ComponentId> sensorIds() const override { return sim_.sensorIds(); }
  std::vector<ComponentId> actuatorIds() const override { return sim_.actuatorIds(); }
  std::vector<Signal> commands() const override { return sim_.commands(); }
  std::uint64_t stateHash() const override { return sim_.stateHash(); }
  std::uint64_t structureHash() const override { return sim_.structureHash(); }
  std::vector<std::uint8_t> encode() const override { return sim_.encode(); }

  Status loadBlob(const std::vector<std::uint8_t>& bytes) override { return decodeDirect(bytes, limits_); }
  Status decodeDirect(const std::vector<std::uint8_t>& bytes, const Limits& lim) override {
    BlobData d;
    const DecodeResult r = refDecode(bytes, lim, d);
    if (r.status != Status::Ok) return r.status;
    if (!sim_.load(d)) return Status::FormatCorrupt;
    return Status::Ok;
  }

 private:
  struct Prepared {
    std::vector<EditOp> ops;
    std::uint32_t base;
  };
  ReferenceSim sim_;
  std::vector<Prepared> prepared_;
  bool matched_ = true;
};

}  // namespace

const EngineFactory& refFactory() {
  static const EngineFactory f{"ref", false, [](const Limits& l, std::uint32_t hostStep) -> std::unique_ptr<Engine> {
                                 return std::make_unique<RefEngine>(l, hostStep, Quirk::None);
                               }};
  return f;
}

EngineFactory quirkedRefFactory(Quirk q) {
  return EngineFactory{"ref (misread)", false, [q](const Limits& l, std::uint32_t hostStep) -> std::unique_ptr<Engine> {
                         return std::make_unique<RefEngine>(l, hostStep, q);
                       }};
}

namespace {
std::map<std::string, PropertyInfo>& mutableTable() {
  static std::map<std::string, PropertyInfo> t;
  return t;
}
}  // namespace

const std::map<std::string, PropertyInfo>& propertyTable() { return mutableTable(); }

PropertyRegistrar::PropertyRegistrar(const char* name, int property, const char* what, PropertyFn fn, RunOn on) {
  mutableTable()[name] = PropertyInfo{fn, on, property};
  if (on == RunOn::Both || on == RunOn::RefOnly) {
    registerTest(TestEntry{std::string(name) + "[ref]", property, false, what, [fn](Ctx& c) { fn(c, refFactory()); }});
  }
  if (on == RunOn::Both || on == RunOn::SimOnly) {
    registerTest(TestEntry{std::string(name) + "[sim]", property, true, what, [fn](Ctx& c) {
                             const EngineFactory* f = simFactory();
                             if (f == nullptr) {
                               c.skip("core not linked into this binary");
                               return;
                             }
                             fn(c, *f);
                           }});
  }
}

}  // namespace slgt
