// Tiny dependency-free test harness: named tests registered from any translation unit, non-fatal
// checks, per-test counters, and the environment of 03 section 6:
//   SLG_CASES  cases per property (default 200)        SLG_SEED  extra base XORed into every case seed
//   SLG_CASE   run only this case index (replay)       SLG_ONLY  comma list of name substrings to run
// A failure prints SLG_SEED, SLG_CASE and the test name, which is everything needed to replay it.
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "prng.hpp"

namespace slgt {

// ---- string building without iostreams or the locale -------------------------------------------

template <class T>
  requires(std::is_integral_v<T> && !std::is_same_v<T, bool> && !std::is_same_v<T, char>)
void appendOne(std::string& s, T v) {
  using U = std::make_unsigned_t<T>;
  U mag = static_cast<U>(v);
  if constexpr (std::is_signed_v<T>) {
    if (v < 0) {
      s.push_back('-');
      mag = static_cast<U>(U{0} - mag);
    }
  }
  char buf[24];
  std::size_t n = 0;
  do {
    buf[n++] = static_cast<char>('0' + static_cast<int>(mag % 10));
    mag = static_cast<U>(mag / 10);
  } while (mag != 0);
  while (n > 0) s.push_back(buf[--n]);
}
inline void appendOne(std::string& s, const char* v) { s += v; }
inline void appendOne(std::string& s, const std::string& v) { s += v; }
inline void appendOne(std::string& s, char v) { s.push_back(v); }
inline void appendOne(std::string& s, bool v) { s += v ? "true" : "false"; }

template <class... A>
std::string cat(const A&... a) {
  std::string r;
  (appendOne(r, a), ...);
  return r;
}

inline std::string hex(std::uint64_t v, unsigned digits = 16) {
  std::string s(digits, '0');
  for (unsigned i = 0; i < digits; ++i) {
    s[digits - 1 - i] = "0123456789ABCDEF"[(v >> (4 * i)) & 0xFU];
  }
  return s;
}

// ---- environment ---------------------------------------------------------------------------------

struct Env {
  std::uint64_t cases = 200;
  std::uint64_t seed = 0;
  bool haveCase = false;
  std::uint64_t onlyCase = 0;
  std::vector<std::string> only;
  bool verbose = false;
};
Env& env();                         // parsed once from the environment; main() adds argv filters
bool nameSelected(const std::string& name);

// ---- per-test context ----------------------------------------------------------------------------

class Ctx {
 public:
  Ctx(std::string name, int property) : name_(std::move(name)), property_(property) {}

  const std::string& name() const noexcept { return name_; }
  int property() const noexcept { return property_; }

  // Cases for a randomized property: SLG_CASES, optionally scaled (never below 1 unless scale is 0).
  std::uint64_t cases(std::uint64_t num = 1, std::uint64_t den = 1) const noexcept {
    const std::uint64_t c = env().cases * num / den;
    return c == 0 ? 1 : c;
  }
  // 03 section 6: case i of Tn uses SplitMix64(base_Tn XOR i), base_Tn = 2026101300 + n.
  std::uint64_t caseSeed(std::uint64_t i) const noexcept {
    const std::uint64_t base = std::uint64_t{2026101300} + static_cast<std::uint64_t>(property_ < 0 ? 0 : property_);
    return (base ^ env().seed) ^ i;
  }
  bool caseSelected(std::uint64_t i) const noexcept { return !env().haveCase || env().onlyCase == i; }

  void beginCase(std::uint64_t i) noexcept { case_ = i; haveCase_ = true; }
  void endCases() noexcept { haveCase_ = false; }
  void setStage(std::string s) { stage_ = std::move(s); }

  bool check(bool cond, const char* expr, const char* file, int line);
  bool checkMsg(bool cond, const std::string& msg, const char* file, int line);
  void fail(const std::string& msg, const char* file = nullptr, int line = 0);
  void note(const std::string& msg);           // information the integrator must see (once per text)
  void skip(const std::string& reason);
  void setQuiet(bool q) noexcept { quiet_ = q; }   // a context that only records (the sensitivity test)

  bool failed() const noexcept { return failures_ != 0; }
  bool skipped() const noexcept { return skipped_; }
  const std::string& skipReason() const noexcept { return skipReason_; }
  std::uint64_t checks() const noexcept { return checks_; }
  std::uint64_t failures() const noexcept { return failures_; }
  bool tooManyFailures() const noexcept { return failures_ >= 6; }
  const std::vector<std::string>& notes() const noexcept { return notes_; }

 private:
  std::string name_;
  int property_;
  std::string stage_;
  std::uint64_t case_ = 0;
  bool haveCase_ = false;
  std::uint64_t checks_ = 0;
  std::uint64_t failures_ = 0;
  bool skipped_ = false;
  std::string skipReason_;
  std::vector<std::string> notes_;
  bool quiet_ = false;
};

#define SLG_CHECK(ctx, cond) (ctx).check(static_cast<bool>(cond), #cond, __FILE__, __LINE__)
#define SLG_CHECK_MSG(ctx, cond, msg) (ctx).checkMsg(static_cast<bool>(cond), (msg), __FILE__, __LINE__)
#define SLG_FAIL(ctx, msg) (ctx).fail((msg), __FILE__, __LINE__)

// Runs f(i, rng) for each selected case with rng seeded per 03 section 6; stops after a few failures.
template <class F>
void forEachCase(Ctx& ctx, std::uint64_t count, F&& f) {
  for (std::uint64_t i = 0; i < count; ++i) {
    if (!ctx.caseSelected(i)) continue;
    ctx.beginCase(i);
    SplitMix64 rng(ctx.caseSeed(i));
    f(i, rng);
    if (ctx.tooManyFailures()) break;
  }
  ctx.endCases();
}

// ---- registry ------------------------------------------------------------------------------------

struct TestEntry {
  std::string name;
  int property = 0;
  bool needsSim = false;
  std::string what;
  std::function<void(Ctx&)> run;
};

std::vector<TestEntry>& registry();
void registerTest(TestEntry e);

struct Registrar {
  Registrar(const char* name, int property, bool needsSim, const char* what, std::function<void(Ctx&)> run) {
    registerTest(TestEntry{name, property, needsSim, what, std::move(run)});
  }
};

// Runs fn in a forked child with a reduced stack (POSIX); false if the child crashed or fn failed.
// Falls back to an in-process call where fork is unavailable. Used for the 10^5 chain of T7.
bool runIsolated(const std::function<bool()>& fn, std::size_t stackBytes, std::string& why);
bool canIsolate() noexcept;   // false where fork is unavailable: runIsolated then runs fn in this process

}  // namespace slgt
