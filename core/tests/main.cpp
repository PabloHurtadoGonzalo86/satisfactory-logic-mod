// Runs every registered test (properties T1..T14 on both engines where applicable, the worked examples
// G1..G14, the golden runner and the checks of the tests themselves) and prints one summary line:
// "TESTS: n passed, m failed". A property runs twice, as NAME[ref] (the reference interpreter, which checks the
// test logic) and NAME[sim] (the core); [sim] is skipped, with a message, when the core is not linked.
// usage: slg_tests [--list] [name-substring ...]
// env:   SLG_CASES (default 200)  SLG_SEED (XORed into every case seed)  SLG_CASE (replay one case)  SLG_ONLY (names)
//        SLG_GOLDEN_DIR (default core/golden)  SLG_T7_CHAIN (size of the long chain of T7, default 100000)
//        SLG_ORACLE_CMD / SLG_EMIT_DIR / SLG_TMP_DIR (T9, see t09_differential.cpp)  SLG_REGRESSION_DIR (reduced failures)
//        SLG_SKIP_SENSITIVITY=1 (skip S01)
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "engine.hpp"
#include "harness.hpp"

namespace slgt {
void registerGoldenTests();   // golden_runner.cpp: one test per lnet/lscn(/ltrace) found in core/golden
}

int main(int argc, char** argv) {
  using namespace slgt;
  bool list = false;
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--list") == 0) list = true;
    else env().only.emplace_back(argv[i]);
  }
  registerGoldenTests();
  std::vector<TestEntry>& tests = registry();
  std::stable_sort(tests.begin(), tests.end(), [](const TestEntry& a, const TestEntry& b) { return a.name < b.name; });

  if (list) {
    for (const TestEntry& t : tests) std::printf("%s\t%s\n", t.name.c_str(), t.what.c_str());
    return 0;
  }
  std::printf("%s\n", cat("slg_tests: SLG_CASES=", env().cases, " SLG_SEED=", env().seed,
                         env().haveCase ? " (single case)" : "", ", core ",
                         simFactory() != nullptr ? "linked" : "NOT linked (spec-only run)").c_str());

  unsigned passed = 0;
  unsigned failed = 0;
  std::vector<std::string> skipped;
  std::vector<std::string> failedNames;
  for (const TestEntry& t : tests) {
    if (!nameSelected(t.name)) continue;
    Ctx ctx(t.name, t.property);
    if (t.needsSim && simFactory() == nullptr) {
      skipped.push_back(t.name + ": core not linked");
      std::printf("SKIP %s: core not linked\n", t.name.c_str());
      continue;
    }
    t.run(ctx);
    if (ctx.skipped()) {
      skipped.push_back(t.name + ": " + ctx.skipReason());
      std::printf("SKIP %s: %s\n", t.name.c_str(), ctx.skipReason().c_str());
    } else if (ctx.failed()) {
      ++failed;
      failedNames.push_back(t.name);
      std::printf("%s\n", cat("FAIL ", t.name, " (", ctx.failures(), " failures in ", ctx.checks(), " checks)").c_str());
    } else {
      ++passed;
      std::printf("%s\n", cat("PASS ", t.name, " (", ctx.checks(), " checks)").c_str());
    }
    std::fflush(stdout);
  }
  if (!skipped.empty()) std::printf("%s\n", cat("SKIPPED: ", skipped.size(), " test(s) did not run").c_str());
  for (const std::string& n : failedNames) std::printf("FAILED: %s\n", n.c_str());
  std::printf("%s\n", cat("TESTS: ", passed, " passed, ", failed, " failed").c_str());
  return failed == 0 ? 0 : 1;
}
