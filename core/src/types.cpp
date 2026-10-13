#include "slg/types.hpp"

#include "internal/spec_rules.hpp"

namespace slg {

std::uint16_t restSeed(const Spec& spec) noexcept {
  // Output with every input at 0: an inverting gate rests high.
  if (spec.kind == Kind::Gate && spec.p1 != 0) return 15;
  return 0;
}

Status validate(const Spec& spec, std::uint16_t seed, const Limits& limits) noexcept {
  const Status params = internal::validateParams(spec, limits);
  if (params != Status::Ok) return params;
  return internal::seedInDomain(spec, seed) ? Status::Ok : Status::BadSpec;
}

}  // namespace slg
