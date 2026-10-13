// FNV-1a 64 and the little-endian, byte-by-byte writers of 02 section 6.2. Test-side copy, written
// from the spec text, so the blob codec below is independent of the core's.
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace slgt {

inline constexpr std::uint64_t kFnvBasis = 0xcbf29ce484222325ULL;
inline constexpr std::uint64_t kFnvPrime = 0x100000001b3ULL;

class Fnv1a64 {
 public:
  void byte(std::uint8_t b) noexcept {
    h_ ^= b;
    h_ *= kFnvPrime;
  }
  void u16(std::uint16_t v) noexcept { byte(static_cast<std::uint8_t>(v & 0xFFU)); byte(static_cast<std::uint8_t>(v >> 8)); }
  void u32(std::uint32_t v) noexcept {
    for (unsigned i = 0; i < 4; ++i) byte(static_cast<std::uint8_t>((v >> (8 * i)) & 0xFFU));
  }
  void bytes(const std::uint8_t* p, std::size_t n) noexcept {
    for (std::size_t i = 0; i < n; ++i) byte(p[i]);
  }
  std::uint64_t value() const noexcept { return h_; }

 private:
  std::uint64_t h_ = kFnvBasis;
};

inline std::uint64_t fnv1a64(const std::uint8_t* p, std::size_t n) noexcept {
  Fnv1a64 f;
  f.bytes(p, n);
  return f.value();
}

class ByteWriter {
 public:
  void u8(std::uint8_t v) { out_.push_back(v); }
  void u16(std::uint16_t v) { u8(static_cast<std::uint8_t>(v & 0xFFU)); u8(static_cast<std::uint8_t>(v >> 8)); }
  void u32(std::uint32_t v) {
    for (unsigned i = 0; i < 4; ++i) u8(static_cast<std::uint8_t>((v >> (8 * i)) & 0xFFU));
  }
  void u64(std::uint64_t v) {
    for (unsigned i = 0; i < 8; ++i) u8(static_cast<std::uint8_t>((v >> (8 * i)) & 0xFFU));
  }
  std::vector<std::uint8_t>& bytes() { return out_; }

 private:
  std::vector<std::uint8_t> out_;
};

inline std::uint32_t readU32(const std::uint8_t* p) noexcept {
  return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) |
         (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
}
inline std::uint16_t readU16(const std::uint8_t* p) noexcept {
  return static_cast<std::uint16_t>(static_cast<unsigned>(p[0]) | (static_cast<unsigned>(p[1]) << 8));
}
inline std::uint64_t readU64(const std::uint8_t* p) noexcept {
  std::uint64_t v = 0;
  for (unsigned i = 0; i < 8; ++i) v |= static_cast<std::uint64_t>(p[i]) << (8 * i);
  return v;
}

}  // namespace slgt
