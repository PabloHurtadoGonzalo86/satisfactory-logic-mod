#pragma once

#include <cstdint>

namespace slg::internal {

inline constexpr std::uint64_t kFnvBasis = 0xcbf29ce484222325;
inline constexpr std::uint64_t kFnvPrime = 0x100000001b3;

// FNV-1a, 64 bits, fed byte by byte; multi-byte values go in little-endian order.
class Fnv64 {
 public:
  void byte(std::uint8_t b) noexcept {
    hash_ ^= static_cast<std::uint64_t>(b);
    hash_ *= kFnvPrime;
  }
  void u16(std::uint16_t v) noexcept {
    byte(static_cast<std::uint8_t>(v & 0xFFu));
    byte(static_cast<std::uint8_t>((v >> 8) & 0xFFu));
  }
  void u32(std::uint32_t v) noexcept {
    for (unsigned shift = 0; shift < 32; shift += 8) byte(static_cast<std::uint8_t>((v >> shift) & 0xFFu));
  }
  void u64(std::uint64_t v) noexcept {
    for (unsigned shift = 0; shift < 64; shift += 8) byte(static_cast<std::uint8_t>((v >> shift) & 0xFFu));
  }
  std::uint64_t value() const noexcept { return hash_; }

 private:
  std::uint64_t hash_ = kFnvBasis;
};

}  // namespace slg::internal
