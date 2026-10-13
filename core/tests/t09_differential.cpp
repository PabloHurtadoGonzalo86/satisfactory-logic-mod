// T9: the Simulator, the reference and (when SLG_ORACLE_CMD is set) the Python oracle give the same complete
// state and commands at every tick, with random edit batches. Cases that exceed 15 on an input are compared
// between engines only (the saturation counter included), since the oracle shows raw values.
//   SLG_ORACLE_CMD  command template with {lnet} and {lscn}, e.g. "python3 -I -B tools/oracle/oracle.py run {lnet} {lscn}"
//   SLG_EMIT_DIR    directory that receives every case as NAME.lnet / NAME.lscn / NAME.ltrace (our trace) for an external cmp
//   SLG_TMP_DIR     scratch directory for the oracle run (default build/slg_tmp)
// Not claimed: performance.
#include <cstdio>
#include <cstdlib>
#include <filesystem>

#include "engine.hpp"
#include "gen.hpp"
#include "testkit.hpp"

namespace slgt {

namespace {

std::string replaceAll(std::string s, const std::string& from, const std::string& to) {
  std::size_t pos = 0;
  while ((pos = s.find(from, pos)) != std::string::npos) {
    s.replace(pos, from.size(), to);
    pos += to.size();
  }
  return s;
}

std::string tmpDir() {
  const char* d = std::getenv("SLG_TMP_DIR");
  const std::string dir = d != nullptr ? d : "build/slg_tmp";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  return dir;
}

// "" if the oracle agrees with our trace byte for byte
std::string askOracle(Ctx& ctx, const char* cmdTemplate, const Scenario& sc, const std::string& ours, std::uint64_t index) {
  const std::string dir = tmpDir();
  std::string tag = ctx.name();
  for (char& ch : tag) {
    if (ch == '[' || ch == ']') ch = '_';
  }
  const std::string base = cat(dir, "/", tag, "_", env().seed, "_", index);
  if (!writeFile(base + ".lnet", formatLnet(sc.net)) || !writeFile(base + ".lscn", formatLscn(sc))) return "cannot write the scratch files";
  std::string cmd = replaceAll(replaceAll(cmdTemplate, "{lnet}", "'" + base + ".lnet'"), "{lscn}", "'" + base + ".lscn'");
  cmd += " > '" + base + ".oracle.ltrace' 2> '" + base + ".oracle.err'";
  const int rc = std::system(cmd.c_str());
  std::string theirs;
  if (rc != 0 || !readFile(base + ".oracle.ltrace", theirs)) {
    std::string err;
    readFile(base + ".oracle.err", err);
    return cat("the oracle failed (exit ", rc, "): ", err);
  }
  if (theirs != ours) {
    std::size_t i = 0;
    while (i < theirs.size() && i < ours.size() && theirs[i] == ours[i]) ++i;
    std::size_t lineStart = ours.rfind('\n', i == 0 ? 0 : i - 1);
    lineStart = lineStart == std::string::npos ? 0 : lineStart + 1;
    std::size_t lineEnd = ours.find('\n', i);
    std::size_t tStart = theirs.rfind('\n', i == 0 ? 0 : i - 1);
    tStart = tStart == std::string::npos ? 0 : tStart + 1;
    std::size_t tEnd = theirs.find('\n', i);
    return cat("the oracle's ltrace differs from ours at byte ", i, " (files kept as ", base, ".*):\n      ours:   ",
               ours.substr(lineStart, lineEnd == std::string::npos ? std::string::npos : lineEnd - lineStart), "\n      oracle: ",
               theirs.substr(tStart, tEnd == std::string::npos ? std::string::npos : tEnd - tStart));
  }
  for (const char* ext : {".lnet", ".lscn", ".oracle.ltrace", ".oracle.err"}) std::remove((base + ext).c_str());
  return "";
}

}  // namespace

SLG_PROPERTY(T09_differential, 9, "Simulator = reference (= oracle) on states and commands, with random edit batches and frames", RunOn::Both) {
  const char* oracle = std::getenv("SLG_ORACLE_CMD");
  const char* emit = std::getenv("SLG_EMIT_DIR");
  forEachCase(ctx, ctx.cases(), [&](std::uint64_t index, SplitMix64& rng) {
    ScenarioConfig cfg;
    cfg.editPct = rng.pick(4) == 0 ? 0U : 12 + rng.pick(30);
    cfg.net.maxComps = 4 + rng.pick(12);
    cfg.net.sparseIds = rng.coin();
    cfg.minTicks = 8;
    cfg.maxTicks = 30;
    Scenario sc = genScenario(rng, cfg);
    bool over15 = false;
    if (rng.chance(1, 8)) {   // the core saturates (I8); the oracle does not model the counter
      for (SensorOrder& o : sc.inputs) {
        if (rng.chance(1, 3)) {
          o.value = u8(16 + rng.pick(240));
          over15 = true;
        }
      }
    }
    const unsigned rows = sc.ticks + 1;
    auto a = factory.make(Limits{}, 0);
    auto b = refFactory().make(Limits{}, 0);
    const std::string why = runLockstep(*a, *b, sc, rows);
    if (!why.empty()) {
      SLG_FAIL(ctx, why);
      reportScenario(ctx, "T9", sc, [&](const Scenario& s) {
        auto x = factory.make(Limits{}, 0);
        auto y = refFactory().make(Limits{}, 0);
        return !runLockstep(*x, *y, s, s.ticks + 1).empty();
      });
      return;
    }
    SLG_CHECK(ctx, a->tick() + 1 == rows);
    if (over15 || (oracle == nullptr && emit == nullptr)) return;
    std::vector<TraceRow> trace;
    std::string err;
    auto c = factory.make(Limits{}, 0);
    if (!runScenario(*c, sc, rows, trace, err)) {
      SLG_FAIL(ctx, err);
      return;
    }
    const std::string ours = formatLtrace(trace);
    if (emit != nullptr) {
      std::error_code ec;
      std::filesystem::create_directories(emit, ec);
      const std::string base = cat(emit, "/", ctx.name(), "_", env().seed, "_", index);
      writeFile(base + ".lnet", formatLnet(sc.net));
      writeFile(base + ".lscn", formatLscn(sc));
      writeFile(base + ".ltrace", ours);
    }
    if (oracle != nullptr && factory.isSim) {
      const std::string o = askOracle(ctx, oracle, sc, ours, index);
      SLG_CHECK_MSG(ctx, o.empty(), o);
    }
  });
}

}  // namespace slgt
