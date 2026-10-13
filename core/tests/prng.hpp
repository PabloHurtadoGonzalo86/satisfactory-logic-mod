// SplitMix64 (public-domain algorithm) and Fisher-Yates as fixed in 03 section 6. No <random>, no
// std::shuffle. The test vectors live in the PRNG test (main file of the suite, see t00_prng.cpp).
//
// Choices 03 does not pin down (listed as an ambiguity in the report):
//  - below(n) is unbiased by rejection: values below (2^64 mod n) are discarded, the rest taken mod n;
//  - shuffle runs i from n-1 down to 1 with j = below(i+1) and swaps v[i], v[j].
#pragma once

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace slgt {

class SplitMix64 {
 public:
  explicit SplitMix64(std::uint64_t seed = 0) noexcept : s_(seed) {}

  std::uint64_t next() noexcept {
    s_ += 0x9E3779B97F4A7C15ULL;
    std::uint64_t z = s_;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
  }

  // Uniform in [0, n); 0 when n == 0.
  std::uint64_t below(std::uint64_t n) noexcept {
    if (n == 0) return 0;
    const std::uint64_t threshold = (0 - n) % n;
    for (;;) {
      const std::uint64_t r = next();
      if (r >= threshold) return r % n;
    }
  }

  // Inclusive range.
  std::uint64_t range(std::uint64_t lo, std::uint64_t hi) noexcept { return lo + below(hi - lo + 1); }
  unsigned pick(unsigned n) noexcept { return static_cast<unsigned>(below(n)); }
  bool chance(unsigned num, unsigned den) noexcept { return below(den) < num; }
  bool coin() noexcept { return (next() & 1U) != 0; }

  template <class T>
  void shuffle(std::vector<T>& v) noexcept {
    for (std::size_t i = v.size(); i > 1; --i) {
      const std::size_t j = static_cast<std::size_t>(below(i));
      std::swap(v[i - 1], v[j]);
    }
  }

  template <class T>
  const T& choose(const std::vector<T>& v) noexcept { return v[static_cast<std::size_t>(below(v.size()))]; }

  std::uint64_t state() const noexcept { return s_; }

 private:
  std::uint64_t s_;
};

}  // namespace slgt
