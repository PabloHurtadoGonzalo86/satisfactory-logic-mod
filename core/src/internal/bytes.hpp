#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace slg::internal {

class ByteWriter {
 public:
  explicit ByteWriter(std::vector<std::uint8_t>& out) noexcept : out_(out) {}

  void u8(std::uint8_t v) { out_.push_back(v); }
  void u16(std::uint16_t v) {
    u8(static_cast<std::uint8_t>(v & 0xFFu));
    u8(static_cast<std::uint8_t>((v >> 8) & 0xFFu));
  }
  void u32(std::uint32_t v) {
    for (unsigned shift = 0; shift < 32; shift += 8) u8(static_cast<std::uint8_t>((v >> shift) & 0xFFu));
  }
  void u64(std::uint64_t v) {
    for (unsigned shift = 0; shift < 64; shift += 8) u8(static_cast<std::uint8_t>((v >> shift) & 0xFFu));
  }

 private:
  std::vector<std::uint8_t>& out_;
};

// Every read checks the bytes left and reports failure instead of advancing past the end.
class ByteReader {
 public:
  explicit ByteReader(std::span<const std::uint8_t> data) noexcept : data_(data) {}

  std::size_t remaining() const noexcept { return data_.size() - pos_; }

  bool u8(std::uint8_t& v) noexcept {
    if (remaining() < 1) return false;
    v = data_[pos_++];
    return true;
  }
  bool u16(std::uint16_t& v) noexcept {
    if (remaining() < 2) return false;
    v = static_cast<std::uint16_t>(static_cast<unsigned>(data_[pos_]) | (static_cast<unsigned>(data_[pos_ + 1]) << 8));
    pos_ += 2;
    return true;
  }
  bool u32(std::uint32_t& v) noexcept {
    if (remaining() < 4) return false;
    v = 0;
    for (unsigned i = 0; i < 4; ++i) v |= static_cast<std::uint32_t>(data_[pos_ + i]) << (8 * i);
    pos_ += 4;
    return true;
  }
  bool u64(std::uint64_t& v) noexcept {
    if (remaining() < 8) return false;
    v = 0;
    for (unsigned i = 0; i < 8; ++i) v |= static_cast<std::uint64_t>(data_[pos_ + i]) << (8 * i);
    pos_ += 8;
    return true;
  }
  bool skip(std::size_t n) noexcept {
    if (remaining() < n) return false;
    pos_ += n;
    return true;
  }
  // View of the next n bytes without copying.
  bool take(std::size_t n, std::span<const std::uint8_t>& view) noexcept {
    if (remaining() < n) return false;
    view = data_.subspan(pos_, n);
    pos_ += n;
    return true;
  }

 private:
  std::span<const std::uint8_t> data_;
  std::size_t pos_ = 0;
};

}  // namespace slg::internal
