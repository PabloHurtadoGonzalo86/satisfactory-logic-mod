#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

#include "slg/types.hpp"

namespace slg::internal {

inline constexpr ComponentId kIdSentinel = 0xFFFFFFFF;  // never a valid id; nextId may reach it

bool validKindByte(std::uint8_t raw) noexcept;
std::uint8_t inputCount(const Spec&) noexcept;
bool hasOutput(Kind) noexcept;
bool pinExists(const Spec&, std::uint8_t index) noexcept;

// Parameters only (no seed): BadSpec when malformed, LimitExceeded when over `Limits`.
Status validateParams(const Spec&, const Limits&) noexcept;
bool seedInDomain(const Spec&, std::uint16_t seed) noexcept;

// Canonical state bytes (02 section 6.1).
std::uint32_t stateSize(const Spec&) noexcept;
void writeInitialState(const Spec&, std::uint16_t seed, std::span<std::uint8_t> dst) noexcept;
bool stateInDomain(const Spec&, std::span<const std::uint8_t> state) noexcept;

}  // namespace slg::internal
