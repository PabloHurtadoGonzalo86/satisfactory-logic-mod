#include "slg/host/fullness.hpp"

namespace slg::host {

Signal fullness(std::int64_t num, std::int64_t den) noexcept {
  if (num <= 0 || den <= 0) return 0;
  if (num >= den) return 15;

  // floor(14 * num / den) without a 14 * num product that could overflow: with den = 14 * whole + rest,
  // q * den <= 14 * num  <=>  q * whole + ceil(q * rest / 14) <= num.
  const auto n = static_cast<std::uint64_t>(num);
  const auto d = static_cast<std::uint64_t>(den);
  const std::uint64_t whole = d / 14;
  const std::uint64_t rest = d % 14;
  unsigned level = 0;
  for (unsigned q = 13; q > 0; --q) {
    const std::uint64_t needed = q * whole + (q * rest + 13) / 14;
    if (needed <= n) {
      level = q;
      break;
    }
  }
  return static_cast<Signal>(1 + level);
}

}  // namespace slg::host
