#include "harness.hpp"

#include <algorithm>
#include <cstdlib>

#if defined(__unix__) || defined(__APPLE__)
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>
#define SLG_HAVE_FORK 1
#endif

namespace slgt {

static bool parseU64(const char* s, std::uint64_t& out) {
  if (s == nullptr || *s == '\0') return false;
  char* end = nullptr;
  const std::uint64_t v = std::strtoull(s, &end, 0);
  if (end == s || *end != '\0') return false;
  out = v;
  return true;
}

static void splitComma(const std::string& s, std::vector<std::string>& out) {
  std::size_t pos = 0;
  while (pos <= s.size()) {
    std::size_t comma = s.find(',', pos);
    if (comma == std::string::npos) comma = s.size();
    if (comma > pos) out.push_back(s.substr(pos, comma - pos));
    pos = comma + 1;
  }
}

Env& env() {
  static Env e = [] {
    Env r;
    std::uint64_t v = 0;
    if (parseU64(std::getenv("SLG_CASES"), v) && v > 0) r.cases = v;
    if (parseU64(std::getenv("SLG_SEED"), v)) r.seed = v;
    if (parseU64(std::getenv("SLG_CASE"), v)) {
      r.haveCase = true;
      r.onlyCase = v;
    }
    if (const char* only = std::getenv("SLG_ONLY")) splitComma(only, r.only);
    if (const char* verbose = std::getenv("SLG_VERBOSE")) r.verbose = verbose[0] != '\0' && verbose[0] != '0';
    return r;
  }();
  return e;
}

bool nameSelected(const std::string& name) {
  const Env& e = env();
  if (e.only.empty()) return true;
  for (const std::string& f : e.only) {
    if (name.find(f) != std::string::npos) return true;
  }
  return false;
}

bool Ctx::check(bool cond, const char* expr, const char* file, int line) {
  ++checks_;
  if (!cond) fail(cat("check failed: ", expr), file, line);
  return cond;
}

bool Ctx::checkMsg(bool cond, const std::string& msg, const char* file, int line) {
  ++checks_;
  if (!cond) fail(msg, file, line);
  return cond;
}

void Ctx::fail(const std::string& msg, const char* file, int line) {
  ++failures_;
  if (failures_ > 6 || quiet_) return;
  std::string where = cat("  FAIL ", name_, ": SLG_SEED=", env().seed);
  if (haveCase_) where += cat(" SLG_CASE=", case_, " (replay: SLG_ONLY=", name_, " SLG_CASE=", case_, ")");
  if (!stage_.empty()) where += cat(" [", stage_, "]");
  if (file != nullptr) {
    const char* base = file;
    for (const char* p = file; *p != '\0'; ++p) {
      if (*p == '/') base = p + 1;
    }
    where += cat(" ", base, ":", line);
  }
  std::printf("%s\n    %s\n", where.c_str(), msg.c_str());
  std::fflush(stdout);
}

void Ctx::note(const std::string& msg) {
  if (quiet_ || std::find(notes_.begin(), notes_.end(), msg) != notes_.end()) return;
  notes_.push_back(msg);
  std::printf("  NOTE %s: %s\n", name_.c_str(), msg.c_str());
  std::fflush(stdout);
}

void Ctx::skip(const std::string& reason) {
  skipped_ = true;
  skipReason_ = reason;
}

std::vector<TestEntry>& registry() {
  static std::vector<TestEntry> r;
  return r;
}

void registerTest(TestEntry e) { registry().push_back(std::move(e)); }

bool canIsolate() noexcept {
#ifdef SLG_HAVE_FORK
  return true;
#else
  return false;
#endif
}

bool runIsolated(const std::function<bool()>& fn, std::size_t stackBytes, std::string& why) {
#ifdef SLG_HAVE_FORK
  std::fflush(stdout);
  std::fflush(stderr);
  const pid_t pid = fork();
  if (pid < 0) {
    why = "fork failed";
    return false;
  }
  if (pid == 0) {
    struct rlimit rl;
    rl.rlim_cur = static_cast<rlim_t>(stackBytes);
    rl.rlim_max = static_cast<rlim_t>(stackBytes);
    if (stackBytes != 0 && setrlimit(RLIMIT_STACK, &rl) != 0) _exit(3);
    const bool ok = fn();
    std::fflush(stdout);
    _exit(ok ? 0 : 1);
  }
  int status = 0;
  if (waitpid(pid, &status, 0) < 0) {
    why = "waitpid failed";
    return false;
  }
  if (WIFSIGNALED(status)) {
    why = cat("child killed by signal ", WTERMSIG(status), " (stack limit ", stackBytes, " bytes)");
    return false;
  }
  if (WIFEXITED(status) && WEXITSTATUS(status) == 0) return true;
  why = cat("child failed, exit code ", WIFEXITED(status) ? WEXITSTATUS(status) : -1);
  return false;
#else
  (void)stackBytes;
  const bool ok = fn();
  if (!ok) why = "check failed";
  return ok;
#endif
}

}  // namespace slgt
