// Linked instead of bridge_sim.cpp when the core is not available: the spec-only layer still builds
// and runs, and everything that needs the core reports "skipped".
#include "bridge.hpp"
#include "engine.hpp"

namespace slgt {

const EngineFactory* simFactory() { return nullptr; }

namespace bridge {

bool available() { return false; }
NetlistInfo netlistDirect(const Model&) {
  NetlistInfo n;
  n.status = Status::BadSpec;
  return n;
}
Status validateSpec(const Spec&, unsigned, const Limits&) { return Status::BadSpec; }
unsigned restSeedOf(const Spec&) { return 0; }
std::vector<FactoryCase> factoryCases() { return {}; }
Status analyze(const Model&, const Limits&, FeedbackReport&) { return Status::BadSpec; }
Status settlingBounds(const Model&, const Limits&, std::vector<std::optional<std::uint32_t>>&) { return Status::BadSpec; }
std::unique_ptr<Pacer> makePacer(std::uint32_t, std::uint32_t) { return nullptr; }
std::unique_ptr<Pacer> makePacerForHost(std::uint32_t, std::uint32_t) { return nullptr; }
std::uint32_t effectiveStepMicros(std::uint32_t) { return 0; }
std::int64_t fullness(std::int64_t, std::int64_t) { return 0; }
std::unique_ptr<Driver> makeDriver(const Limits&, std::uint32_t, const DriverOptions&) { return nullptr; }
std::unique_ptr<Driver> makeDriverFromBlob(const std::vector<std::uint8_t>&, const Limits&, const DriverOptions&,
                                           Status& decodeStatus) {
  decodeStatus = Status::BadSpec;
  return nullptr;
}

}  // namespace bridge
}  // namespace slgt
