#pragma once

#include <cstdint>

#include "slg/types.hpp"

namespace slg::host {

// 0 if num <= 0 or den <= 0, else min(15, 1 + 14 * num / den) in exact integer arithmetic.
Signal fullness(std::int64_t num, std::int64_t den) noexcept;

}  // namespace slg::host
