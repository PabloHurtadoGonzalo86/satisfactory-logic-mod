// Executes the goldens of core/golden through the core and compares byte for byte:
//   NAME.lnet + NAME.lscn + NAME.ltrace   the Simulator trace, row by row (ltrace written here must equal the file)
//   NAME.slgc                             Simulator::encode() at the tick in the blob's header (G7, all_kinds)
//   NAME.lburst / NAME.lpacer             FrameDriver calls (G11) and StepPacer calls (G12), formats of core/golden/README.md
// One test per file group, named GOLDEN/<relative name>. The goldens are emitted by the oracle; while the
// folder has none, a single skipped GOLDEN test says so.
//
// The reference runs the same scenarios, so a failure says whether the Simulator disagrees with the
// reference (a semantics bug) or both agree and differ from the file (layout or oracle question).
// "ticks n" is n+1 rows (docs/spec/ambiguities.md A1), the oracle's reading of 03 section 4.
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <string>

#include "bridge.hpp"
#include "engine.hpp"
#include "golden_io.hpp"
#include "harness.hpp"
#include "testkit.hpp"

namespace slgt {

namespace {

namespace fs = std::filesystem;

bool dirExists(const fs::path& p) {
  std::error_code ec;
  return fs::is_directory(p, ec);
}

bool fileExists(const std::string& p) {
  std::error_code ec;
  return fs::is_regular_file(fs::path(p), ec);
}

std::string findGoldenDir() {
  if (const char* e = std::getenv("SLG_GOLDEN_DIR")) {
    if (dirExists(e)) return e;
  }
  for (const char* c : {"core/golden", "../golden", "golden", "../../core/golden", "../core/golden"}) {
    if (dirExists(c)) return c;
  }
  return "";
}

struct GoldenFile {
  std::string name;   // relative path, extension included for the host formats
  std::string base;   // path without extension
  std::string kind;   // "scn", "lburst", "lpacer"
};

std::vector<GoldenFile> discover(const std::string& dir) {
  std::vector<GoldenFile> out;
  std::error_code ec;
  fs::recursive_directory_iterator it(dir, ec);
  const fs::recursive_directory_iterator end;
  while (!ec && it != end) {
    const fs::path p = it->path();
    std::error_code ec2;
    if (fs::is_regular_file(p, ec2)) {
      fs::path base = p;
      base.replace_extension("");
      std::error_code ec3;
      const fs::path rel = fs::relative(base, fs::path(dir), ec3);
      const std::string relName = ec3 ? base.string() : rel.generic_string();
      const std::string ext = p.extension().string();
      if (ext == ".lnet" && fileExists(base.string() + ".lscn")) {
        out.push_back(GoldenFile{relName, base.string(), "scn"});
      } else if (ext == ".lburst") {
        out.push_back(GoldenFile{relName + ".lburst", p.string(), "lburst"});
      } else if (ext == ".lpacer") {
        out.push_back(GoldenFile{relName + ".lpacer", p.string(), "lpacer"});
      }
    }
    it.increment(ec);
  }
  std::stable_sort(out.begin(), out.end(), [](const GoldenFile& a, const GoldenFile& b) { return a.name < b.name; });
  return out;
}

struct FirstDiff {
  std::size_t line = 0;
  std::string expected, got;
  bool differs = false;
};

FirstDiff firstDiff(const std::string& expected, const std::string& got) {
  FirstDiff d;
  std::size_t pe = 0;
  std::size_t pg = 0;
  std::size_t line = 1;
  for (;;) {
    const bool eoE = pe >= expected.size();
    const bool eoG = pg >= got.size();
    if (eoE && eoG) return d;
    std::size_t ne = expected.find('\n', pe);
    std::size_t ng = got.find('\n', pg);
    if (ne == std::string::npos) ne = expected.size();
    if (ng == std::string::npos) ng = got.size();
    const std::string le = eoE ? std::string("<end of file>") : expected.substr(pe, ne - pe);
    const std::string lg = eoG ? std::string("<end of file>") : got.substr(pg, ng - pg);
    if (le != lg || eoE != eoG) {
      d.line = line;
      d.expected = le;
      d.got = lg;
      d.differs = true;
      return d;
    }
    pe = ne + 1;
    pg = ng + 1;
    ++line;
  }
}

void runScenarioGolden(Ctx& ctx, const GoldenFile& g) {
  const EngineFactory* simF = simFactory();
  std::string lnetText, lscnText, err;
  if (!readFile(g.base + ".lnet", lnetText) || !readFile(g.base + ".lscn", lscnText)) {
    SLG_FAIL(ctx, "cannot read the lnet/lscn pair");
    return;
  }
  Scenario sc;
  if (!parseLnet(lnetText, sc.net, err)) {
    SLG_FAIL(ctx, cat("lnet: ", err));
    return;
  }
  if (!parseLscn(lscnText, sc, err)) {
    SLG_FAIL(ctx, cat("lscn: ", err));
    return;
  }
  const unsigned rows = sc.ticks + 1;
  std::vector<TraceRow> simRows;
  std::vector<TraceRow> refRows;
  auto sim = simF->make(Limits{}, 0);
  auto ref = refFactory().make(Limits{}, 0);
  if (!runScenario(*sim, sc, rows, simRows, err)) {
    SLG_FAIL(ctx, cat("Simulator: ", err));
    return;
  }
  if (!runScenario(*ref, sc, rows, refRows, err)) {
    SLG_FAIL(ctx, cat("reference: ", err, " (the scenario is not valid under the reference semantics)"));
    return;
  }
  bool agree = true;
  for (std::size_t i = 0; i < simRows.size(); ++i) {
    const std::string d = diffRows(simRows[i], refRows[i]);
    if (!d.empty()) {
      SLG_FAIL(ctx, cat("Simulator and reference disagree at row ", i, ": ", d));
      agree = false;
      break;
    }
  }

  std::string expected;
  if (readFile(g.base + ".ltrace", expected)) {
    const std::string got = formatLtrace(simRows);
    if (got == expected) {
      SLG_CHECK(ctx, got == expected);
    } else {
      const FirstDiff fd = firstDiff(expected, got);
      std::string why = cat("ltrace differs from the golden at line ", fd.line, ":\n      golden: ", fd.expected,
                            "\n      ours:   ", fd.got);
      std::vector<ParsedRow> theirs;
      std::string perr;
      if (!parseLtrace(expected, theirs, perr)) {
        why += cat("\n      the golden does not parse as an ltrace: ", perr);
      } else {
        bool same = theirs.size() == simRows.size();
        for (std::size_t k = 0; same && k < theirs.size(); ++k) same = theirs[k] == toParsed(simRows[k]);
        if (same) why += "\n      the golden and ours hold the same rows: only the text layout differs";
        else {
          for (std::size_t k = 0; k < theirs.size() && k < simRows.size(); ++k) {
            if (!(theirs[k] == toParsed(simRows[k]))) {
              why += cat("\n      first row whose content differs: ", k);
              break;
            }
          }
        }
      }
      if (formatLtrace(refRows) == expected) why += "\n      the reference reproduces the golden: the Simulator is wrong";
      else if (agree) why += "\n      Simulator and reference agree with each other: check the layout (docs/spec/ambiguities.md A1-A5) or the oracle";
      SLG_FAIL(ctx, why);
    }
  } else {
    ctx.note("no .ltrace next to this scenario: checked the Simulator against the reference only");
  }

  std::string blob;
  if (readFile(g.base + ".slgc", blob)) {
    const std::vector<std::uint8_t> want(blob.begin(), blob.end());
    BlobData d;
    if (!parseBlob(want, d)) {
      SLG_FAIL(ctx, ".slgc is not a well-formed blob");
      return;
    }
    if (d.tick > sc.ticks) {
      SLG_FAIL(ctx, cat(".slgc tick ", d.tick, " is beyond the scenario (", sc.ticks, " ticks)"));
      return;
    }
    auto at = simF->make(Limits{}, d.hostStepMicros);
    std::vector<TraceRow> tmp;
    if (!runScenario(*at, sc, u32(d.tick) + 1, tmp, err)) {
      SLG_FAIL(ctx, cat("running to the blob tick: ", err));
      return;
    }
    const std::vector<std::uint8_t> got = at->encode();
    SLG_CHECK_MSG(ctx, got == want, cat("encode() at tick ", d.tick, " differs from the .slgc (", got.size(), " vs ", want.size(),
                                        " bytes):\n      ours:   ", hexBytes(got, 120), "\n      golden: ", hexBytes(want, 120)));
  }
}

// ---- lburst and lpacer -----------------------------------------------------------------------------------------

std::vector<std::string> splitWords(const std::string& s) {
  std::vector<std::string> w;
  std::string cur;
  for (char ch : s) {
    if (ch == ' ' || ch == '\t' || ch == '\r') {
      if (!cur.empty()) w.push_back(cur);
      cur.clear();
    } else {
      cur.push_back(ch);
    }
  }
  if (!cur.empty()) w.push_back(cur);
  return w;
}

std::vector<std::string> splitLinesPlain(const std::string& text) {
  std::vector<std::string> lines;
  std::size_t pos = 0;
  while (pos < text.size()) {
    std::size_t eol = text.find('\n', pos);
    if (eol == std::string::npos) eol = text.size();
    std::string l = text.substr(pos, eol - pos);
    const std::size_t hash = l.find('#');
    if (hash != std::string::npos) l.resize(hash);
    while (!l.empty() && (l.back() == ' ' || l.back() == '\r' || l.back() == '\t')) l.pop_back();
    if (!l.empty()) lines.push_back(l);
    pos = eol + 1;
  }
  return lines;
}

bool toU64(const std::string& s, std::uint64_t& out) {
  if (s.empty() || s.size() > 18) return false;
  std::uint64_t v = 0;
  for (char ch : s) {
    if (ch < '0' || ch > '9') return false;
    v = v * 10 + static_cast<std::uint64_t>(ch - '0');
  }
  out = v;
  return true;
}

void runBurstGolden(Ctx& ctx, const GoldenFile& g) {
  std::string text, err;
  if (!readFile(g.base, text)) {
    SLG_FAIL(ctx, "cannot read the lburst file");
    return;
  }
  const std::vector<std::string> lines = splitLinesPlain(text);
  if (lines.empty() || lines[0] != "lburst 1") {
    SLG_FAIL(ctx, "missing 'lburst 1' header");
    return;
  }
  const std::string dir = fs::path(g.base).parent_path().string();
  std::unique_ptr<bridge::Driver> drv;
  bool ended = false;
  bridge::DriverOptions opt;
  opt.settleGuard = false;   // the oracle's model has no settling guard (core/golden/README.md)
  std::size_t callNo = 0;
  for (std::size_t i = 1; i < lines.size(); ++i) {
    const std::vector<std::string> w = splitWords(lines[i]);
    if (w.empty()) continue;
    if (w[0] == "net" && w.size() == 2) {
      std::string netText;
      Model m;
      if (!readFile((fs::path(dir) / w[1]).string(), netText) || !parseLnet(netText, m, err)) {
        SLG_FAIL(ctx, cat("net ", w[1], ": ", err));
        return;
      }
      drv = bridge::makeDriver(Limits{}, 0, opt);
      const Status s = drv->apply(opsForModel(m));
      if (s != Status::Ok) {
        SLG_FAIL(ctx, cat("loading the net through FrameDriver::apply: ", statusName(s)));
        return;
      }
    } else if (w[0] == "sensor" && w.size() == 3 && drv != nullptr) {
      std::uint64_t id = 0;
      toU64(w[1], id);
      drv->setSensorMode(u32(id), w[2] == "pulse");
    } else if (w[0] == "call" && drv != nullptr) {
      ++callNo;
      std::uint64_t steps = 0;
      if (w.size() < 2 || !toU64(w[1], steps)) {
        SLG_FAIL(ctx, cat("bad call line: ", lines[i]));
        return;
      }
      std::size_t k = 2;
      if (k < w.size() && w[k] == "offer") ++k;
      std::string expectedChanges;
      bool afterBar = false;
      for (; k < w.size(); ++k) {
        if (w[k] == "|") {
          afterBar = true;
          continue;
        }
        if (afterBar) {
          if (w[k] == "changes") continue;
          expectedChanges += (expectedChanges.empty() ? "" : " ") + w[k];
        } else {
          const std::size_t eq = w[k].find('=');
          std::uint64_t id = 0;
          std::uint64_t v = 0;
          if (eq == std::string::npos || !toU64(w[k].substr(0, eq), id) || !toU64(w[k].substr(eq + 1), v)) {
            SLG_FAIL(ctx, cat("bad offer '", w[k], "'"));
            return;
          }
          drv->offer(u32(id), u8(v));
        }
      }
      const Status st = drv->runCall(u32(steps), u32(steps) * 100000U);
      SLG_CHECK_MSG(ctx, st == Status::Ok, cat("call ", callNo, ": runCall returned ", statusName(st)));
      std::string got;
      const std::vector<bridge::DriverChange> ch = drv->changes();
      for (const bridge::DriverChange& c : ch) {
        got += cat(got.empty() ? "" : " ", c.id, ":", static_cast<unsigned>(c.previous), ">", static_cast<unsigned>(c.value));
      }
      SLG_CHECK_MSG(ctx, got == expectedChanges, cat("call ", callNo, ": changes '", got, "' but the golden says '", expectedChanges, "'"));
      for (const bridge::DriverChange& c : ch) drv->acknowledge(c.id);   // the host confirms every change at the end of a call
      drv->endCall();
    } else if (w[0] == "end" && w.size() == 4 && drv != nullptr) {
      ended = true;
      std::uint64_t tick = 0;
      toU64(w[2], tick);
      SLG_CHECK_MSG(ctx, drv->simTick() == tick, cat("end tick ", drv->simTick(), " but the golden says ", tick));
      SLG_CHECK_MSG(ctx, w[3] == cat("h=", hex(drv->simStateHash())), cat("end hash ", hex(drv->simStateHash()), " but the golden says ", w[3]));
    } else if (w[0] != "net") {
      SLG_FAIL(ctx, cat("unrecognized line: ", lines[i]));
      return;
    }
  }
  SLG_CHECK_MSG(ctx, ended, "the lburst has no 'end' line");
}

void runPacerGolden(Ctx& ctx, const GoldenFile& g) {
  std::string text;
  if (!readFile(g.base, text)) {
    SLG_FAIL(ctx, "cannot read the lpacer file");
    return;
  }
  const std::vector<std::string> lines = splitLinesPlain(text);
  if (lines.size() < 2 || lines[0] != "lpacer 1") {
    SLG_FAIL(ctx, "missing 'lpacer 1' header");
    return;
  }
  const std::vector<std::string> cfg = splitWords(lines[1]);
  std::uint64_t step = 0;
  std::uint64_t max = 0;
  if (cfg.size() != 4 || cfg[0] != "step" || !toU64(cfg[1], step) || cfg[2] != "max" || !toU64(cfg[3], max)) {
    SLG_FAIL(ctx, cat("bad config line: ", lines[1]));
    return;
  }
  auto pacer = bridge::makePacer(u32(step), u32(max));
  std::uint64_t sumDt = 0;
  std::uint64_t sumSteps = 0;
  for (std::size_t i = 2; i < lines.size(); ++i) {
    const std::vector<std::string> w = splitWords(lines[i]);
    std::uint64_t dt = 0;
    std::uint64_t paused = 0;
    if (w.size() < 4 || w[0] != "call" || !toU64(w[1], dt) || !toU64(w[2], paused) || w[3] != "|") {
      SLG_FAIL(ctx, cat("bad call line: ", lines[i]));
      return;
    }
    const std::uint32_t steps = pacer->advance(u32(dt), paused != 0);
    sumDt += dt;
    sumSteps += steps;
    const std::uint64_t acc = sumDt - pacer->pausedMicros() - (sumSteps + pacer->droppedSteps()) * step;
    std::string got = cat("steps ", steps, " dropped ", pacer->droppedSteps(), " paused ", pacer->pausedMicros(), " acc ", acc);
    std::string want;
    for (std::size_t k = 4; k < w.size(); ++k) want += (want.empty() ? "" : " ") + w[k];
    SLG_CHECK_MSG(ctx, got == want, cat("call ", i - 1, ": '", got, "' but the golden says '", want, "'"));
  }
}

}  // namespace

void registerGoldenTests() {
  const std::string dir = findGoldenDir();
  std::vector<GoldenFile> files;
  if (!dir.empty()) files = discover(dir);
  if (files.empty()) {
    registerTest(TestEntry{"GOLDEN", 0, false, "golden traces executed through the Simulator",
                           [dir](Ctx& c) {
                             c.skip(dir.empty() ? "no core/golden directory found (set SLG_GOLDEN_DIR)"
                                                : "core/golden holds no goldens yet (the oracle emits them)");
                           }});
    return;
  }
  for (const GoldenFile& g : files) {
    registerTest(TestEntry{"GOLDEN/" + g.name, 0, true, "golden", [g](Ctx& c) {
                             if (g.kind == "scn") runScenarioGolden(c, g);
                             else if (g.kind == "lburst") runBurstGolden(c, g);
                             else runPacerGolden(c, g);
                           }});
  }
}

}  // namespace slgt
