// T12: the decoder is strict and total.
//  - encode(decode(b)) == b for every accepted b (the G7 fixture, the oracle's all_kinds blob, random blobs);
//  - every single-byte substitution and every truncation of those fixtures is rejected, and the object that
//    decode was called on is left untouched;
//  - invariants broken with the tail recomputed (Gate state 7, Clock phase >= P, Delay slot 16, nextId <= a
//    present id, an unnormalized wire, ...) give FormatCorrupt;
//  - a version above the supported one gives FormatTooNew without looking further (a bad tail included);
//  - every limit exceeded gives LimitExceeded; random mutations with a recomputed tail are classified like the
//    reference decoder (the strict reading of 02 section 6.3) and never crash (run it under the sanitizers).
// Not claimed: changes of several bytes that happen to preserve the tail.
#include <algorithm>
#include <cstdlib>

#include "engine.hpp"
#include "fnv.hpp"
#include "gen.hpp"
#include "netutil.hpp"
#include "spec_examples.hpp"
#include "testkit.hpp"

namespace slgt {

namespace {

Model richModel() {
  Model m;
  m.comps = {CompDef{1, specSensor(), 0}, CompDef{2, specSensor(), 0}, CompDef{3, specConst(5), 0},
             CompDef{4, specGate(GateOp::Xor, false, 3), 0}, CompDef{5, specGate(GateOp::And, true, 2), 15},
             CompDef{6, specDelay(3), 0}, CompDef{7, specCompare(CompareMode::Compare), 0},
             CompDef{8, specCompare(CompareMode::Subtract), 0}, CompDef{9, specLatch(), 0}, CompDef{10, specToggle(), 0},
             CompDef{11, specClock(7, 3), 2}, CompDef{12, specActuator(), 0}, CompDef{13, specActuator(), 0}};
  m.wires = {makeWire(outPin(1), inPin(4, 0)), makeWire(outPin(2), inPin(4, 1)), makeWire(outPin(3), inPin(4, 2)),
             makeWire(outPin(1), inPin(5, 0)), makeWire(outPin(2), inPin(5, 1)), makeWire(outPin(4), inPin(6, 0)),
             makeWire(outPin(1), inPin(7, 0)), makeWire(outPin(3), inPin(7, 1)), makeWire(outPin(1), inPin(8, 0)),
             makeWire(outPin(2), inPin(8, 1)), makeWire(outPin(7), inPin(9, 0)), makeWire(outPin(2), inPin(9, 1)),
             makeWire(outPin(2), inPin(10, 0)), makeWire(outPin(6), inPin(12, 0)), makeWire(outPin(8), inPin(13, 0)),
             makeWire(outPin(11), inPin(13, 0))};
  m.sortAndNormalize();
  return m;
}

std::vector<std::uint8_t> bytesOf(const std::string& s) { return std::vector<std::uint8_t>(s.begin(), s.end()); }

std::string loadGolden(const char* name) {
  const char* dir = std::getenv("SLG_GOLDEN_DIR");
  const char* candidates[] = {dir != nullptr ? dir : "", "core/golden", "../golden", "golden"};
  for (const char* c : candidates) {
    if (c[0] == '\0') continue;
    std::string text;
    if (readFile(cat(c, "/", name), text)) return text;
  }
  return "";
}

// A victim engine holding a non-trivial state, to be decoded onto.
struct Victim {
  std::unique_ptr<Engine> engine;
  std::vector<std::uint8_t> bytes;
  Limits limits;
};

bool makeVictim(const EngineFactory& f, const Limits& lim, Victim& v, std::string& err) {
  v.limits = lim;
  v.engine = f.make(lim, 77);
  if (v.engine->apply(opsForModel(richModel())) != Status::Ok) {
    err = "building the victim netlist failed";
    return false;
  }
  for (unsigned t = 0; t < 5; ++t) v.engine->step({u8(3 + t), u8(9 * t)});
  v.bytes = v.engine->encode();
  return true;
}

// "" when the core classifies bytes like the strict reference decoder and respects the contract on failure.
std::string checkDecode(const EngineFactory& f, Victim& v, const std::vector<std::uint8_t>& bytes, const Limits& lim,
                        const char* what, Status* gotOut = nullptr) {
  BlobData d;
  const DecodeResult want = refDecode(bytes, lim, d);
  const Status got = v.engine->decodeDirect(bytes, lim);
  if (gotOut != nullptr) *gotOut = got;
  if (!compatible(want, got)) {
    return cat(what, ": decode answered ", statusName(got), ", the strict reading of 02 section 6.3 says ", describe(want), " (", bytes.size(),
               " bytes: ", hexBytes(bytes, 40), ")");
  }
  if (got != Status::Ok) {
    if (v.engine->encode() != v.bytes) return cat(what, ": decode answered ", statusName(got), " but changed the object it was called on");
    return "";
  }
  if (v.engine->encode() != bytes) return cat(what, ": decode accepted the bytes but encode(decode(b)) != b (", bytes.size(), " bytes)");
  if (v.engine->loadBlob(v.bytes) != Status::Ok) return cat(what, ": cannot restore the victim");
  (void)f;
  return "";
}

void exhaustive(Ctx& ctx, const EngineFactory& f, const std::vector<std::uint8_t>& fixture, const char* name) {
  Victim v;
  std::string err;
  if (!makeVictim(f, Limits{}, v, err)) {
    SLG_FAIL(ctx, err);
    return;
  }
  // the fixture itself decodes and re-encodes to itself
  {
    const std::string why = checkDecode(f, v, fixture, Limits{}, name);
    if (!why.empty()) {
      SLG_FAIL(ctx, why);
      return;
    }
    auto e = f.make(Limits{}, 0);
    SLG_CHECK_MSG(ctx, e->loadBlob(fixture) == Status::Ok && e->encode() == fixture, cat(name, ": decode then encode does not give the fixture back"));
  }
  // every one-byte substitution
  for (std::size_t i = 0; i < fixture.size() && !ctx.tooManyFailures(); ++i) {
    for (unsigned val = 0; val < 256; ++val) {
      if (val == fixture[i]) continue;
      std::vector<std::uint8_t> b = fixture;
      b[i] = u8(val);
      Status got = Status::Ok;
      const std::string why = checkDecode(f, v, b, Limits{}, name, &got);
      if (!why.empty() || got == Status::Ok) {
        SLG_FAIL(ctx, why.empty() ? cat(name, ": substituting byte ", i, " by ", val, " was accepted (FNV-1a steps are bijective, the tail must change)") : why);
        return;
      }
    }
  }
  // every truncation, and one extra byte with and without a recomputed tail
  for (std::size_t len = 0; len < fixture.size(); ++len) {
    const std::vector<std::uint8_t> b(fixture.begin(), fixture.begin() + static_cast<std::ptrdiff_t>(len));
    Status got = Status::Ok;
    const std::string why = checkDecode(f, v, b, Limits{}, name, &got);
    if (!why.empty() || got == Status::Ok) {
      SLG_FAIL(ctx, why.empty() ? cat(name, ": a truncation to ", len, " bytes was accepted") : why);
      return;
    }
  }
  {
    std::vector<std::uint8_t> longer(fixture.size() + 1, 0x5A);
    std::copy(fixture.begin(), fixture.end(), longer.begin());
    Status got = Status::Ok;
    std::string why = checkDecode(f, v, longer, Limits{}, name, &got);
    SLG_CHECK_MSG(ctx, why.empty() && got != Status::Ok, cat(name, ": one trailing byte: ", why));
    BlobData d;
    if (parseBlob(fixture, d) && fixture.size() >= kBlobTailBytes) {
      // fixture without its tail, one extra zero byte, then a zeroed tail that refreshTail recomputes
      std::vector<std::uint8_t> padded(fixture.size() + 1, 0);
      std::copy(fixture.begin(), fixture.end() - static_cast<std::ptrdiff_t>(kBlobTailBytes), padded.begin());
      refreshTail(padded);
      why = checkDecode(f, v, padded, Limits{}, name, &got);
      SLG_CHECK_MSG(ctx, why.empty() && got == Status::FormatCorrupt, cat(name, ": a byte too many with a valid tail must be FormatCorrupt, got ", statusName(got), " ", why));
    }
  }
}

}  // namespace

SLG_PROPERTY(T12_blob_robustness, 12, "decode is strict and total: round trip, one-byte corruption, truncation, broken invariants, versions, limits", RunOn::Both) {
  std::string err;
  // fixtures: the 95 bytes of G7, our own blob of every kind, the oracle's all_kinds blob when present
  exhaustive(ctx, factory, g7Blob(), "G7");
  Victim v;
  if (!makeVictim(factory, Limits{}, v, err)) {
    SLG_FAIL(ctx, err);
    return;
  }
  exhaustive(ctx, factory, v.bytes, "all kinds");
  const std::string golden = loadGolden("all_kinds.slgc");
  if (!golden.empty()) exhaustive(ctx, factory, bytesOf(golden), "all_kinds.slgc");
  if (ctx.tooManyFailures()) return;

  // invariants broken with a recomputed tail
  BlobData base;
  if (!parseBlob(v.bytes, base)) {
    SLG_FAIL(ctx, "the engine's own blob does not parse");
    return;
  }
  for (unsigned k = 0; k < static_cast<unsigned>(Corruption::kCount); ++k) {
    std::vector<std::uint8_t> bad;
    if (!corruptBlob(base, static_cast<Corruption>(k), bad)) continue;
    Status got = Status::Ok;
    const std::string why = checkDecode(factory, v, bad, Limits{}, corruptionName(static_cast<Corruption>(k)), &got);
    SLG_CHECK_MSG(ctx, why.empty(), why);
    SLG_CHECK_MSG(ctx, got == Status::FormatCorrupt, cat(corruptionName(static_cast<Corruption>(k)), " must give FormatCorrupt, got ", statusName(got)));
  }
  // versions: higher is FormatTooNew without looking further (a wrong tail included); lower has no migration
  for (std::size_t at : {std::size_t{4}, std::size_t{6}}) {
    for (unsigned newVersion : {2U, 3U, 255U, 256U, 65535U}) {
      for (bool fixTail : {true, false}) {
        std::vector<std::uint8_t> b = v.bytes;
        b[at] = u8(newVersion & 0xFFU);
        b[at + 1] = u8(newVersion >> 8);
        if (fixTail) refreshTail(b);
        const Obs before = observe(*v.engine);
        const Status got = v.engine->decodeDirect(b, Limits{});
        SLG_CHECK_MSG(ctx, got == Status::FormatTooNew, cat("version ", newVersion, " at byte ", at, fixTail ? "" : " with a stale tail", ": ", statusName(got)));
        SLG_CHECK_MSG(ctx, diffObs(before, observe(*v.engine), true).empty(), "FormatTooNew must not mutate");
      }
    }
    std::vector<std::uint8_t> zero = v.bytes;
    zero[at] = 0;
    zero[at + 1] = 0;
    refreshTail(zero);
    SLG_CHECK_MSG(ctx, v.engine->decodeDirect(zero, Limits{}) != Status::Ok, "version 0 has no migration and must be rejected");
    SLG_CHECK(ctx, v.engine->encode() == v.bytes);
  }
  // limits: each one exceeded gives LimitExceeded
  {
    struct Probe { const char* name; Limits lim; };
    Probe probes[6];
    probes[0] = {"maxComponents", Limits{}};
    probes[0].lim.maxComponents = 12;
    probes[1] = {"maxWires", Limits{}};
    probes[1].lim.maxWires = 15;
    probes[2] = {"maxDelayCells", Limits{}};
    probes[2].lim.maxDelayCells = 2;
    probes[3] = {"maxNetPins", Limits{}};
    probes[3].lim.maxNetPins = 4;
    probes[4] = {"maxDelay", Limits{}};
    probes[4].lim.maxDelay = 2;
    probes[5] = {"maxClockPeriod", Limits{}};
    probes[5].lim.maxClockPeriod = 5;
    for (const Probe& p : probes) {
      Victim small;
      Limits looser = Limits{};
      std::string e2;
      if (!makeVictim(factory, looser, small, e2)) {
        SLG_FAIL(ctx, e2);
        return;
      }
      Status got = Status::Ok;
      const std::string why = checkDecode(factory, small, v.bytes, p.lim, p.name, &got);
      SLG_CHECK_MSG(ctx, why.empty(), why);
      const bool paramLimit = std::string(p.name) == "maxDelay" || std::string(p.name) == "maxClockPeriod";
      SLG_CHECK_MSG(ctx, got == Status::LimitExceeded || (paramLimit && got == Status::FormatCorrupt),
                    cat(p.name, " exceeded: decode answered ", statusName(got)));
    }
    // exactly at each limit is accepted, one below is not (the count limits answer LimitExceeded)
    {
      BlobData rich;
      parseBlob(v.bytes, rich);
      Model asModel;
      for (const BlobComp& c : rich.comps) asModel.comps.push_back(CompDef{c.id, c.spec, 0});
      asModel.wires = rich.wires;
      std::size_t maxNet = 1;
      for (const NetInfo& net : analyzeNets(asModel).nets) maxNet = std::max(maxNet, net.pins.size());
      unsigned cells = 0;
      unsigned longest = 0;
      unsigned widest = 0;
      unsigned period = 0;
      for (const BlobComp& c : rich.comps) {
        if (c.spec.kind == Kind::Delay) {
          cells += c.spec.p0;
          longest = std::max<unsigned>(longest, c.spec.p0);
        }
        if (c.spec.kind == Kind::Gate) widest = std::max<unsigned>(widest, c.spec.p2);
        if (c.spec.kind == Kind::Clock) period = std::max<unsigned>(period, c.spec.p0);
      }
      struct Edge { const char* name; std::uint32_t count; void (*set)(Limits&, std::uint32_t); bool paramLimit; };
      const Edge edges[] = {
          {"maxComponents", u32(rich.comps.size()), [](Limits& l, std::uint32_t n) { l.maxComponents = n; }, false},
          {"maxWires", u32(rich.wires.size()), [](Limits& l, std::uint32_t n) { l.maxWires = n; }, false},
          {"maxDelayCells", cells, [](Limits& l, std::uint32_t n) { l.maxDelayCells = n; }, false},
          {"maxNetPins", u32(maxNet), [](Limits& l, std::uint32_t n) { l.maxNetPins = n; }, false},
          {"maxDelay", longest, [](Limits& l, std::uint32_t n) { l.maxDelay = u16(n); }, true},
          {"maxArity", widest, [](Limits& l, std::uint32_t n) { l.maxArity = u8(n); }, true},
          {"maxClockPeriod", period, [](Limits& l, std::uint32_t n) { l.maxClockPeriod = u16(n); }, true}};
      for (const Edge& e : edges) {
        Limits at;
        e.set(at, e.count);
        Victim fresh;
        std::string e4;
        if (!makeVictim(factory, Limits{}, fresh, e4)) {
          SLG_FAIL(ctx, e4);
          return;
        }
        Status got = Status::Ok;
        std::string why = checkDecode(factory, fresh, v.bytes, at, e.name, &got);
        SLG_CHECK_MSG(ctx, why.empty() && got == Status::Ok, cat(e.name, " = ", e.count, " (exactly what the blob holds) must be accepted, got ", statusName(got), " ", why));
        Limits below;
        e.set(below, e.count - 1);
        why = checkDecode(factory, fresh, v.bytes, below, e.name, &got);
        SLG_CHECK_MSG(ctx, why.empty() && (got == Status::LimitExceeded || (e.paramLimit && got == Status::FormatCorrupt)),
                      cat(e.name, " = ", e.count - 1, " (one below what the blob holds) must be refused as LimitExceeded, got ", statusName(got), " ", why));
      }
    }
    // a count in the header above any limit, with a tail that matches
    std::vector<std::uint8_t> huge = v.bytes;
    for (std::size_t i = 24; i < 28; ++i) huge[i] = 0xFF;
    refreshTail(huge);
    Status got = Status::Ok;
    const std::string why = checkDecode(factory, v, huge, Limits{}, "huge component count", &got);
    SLG_CHECK_MSG(ctx, why.empty() && (got == Status::LimitExceeded || got == Status::FormatCorrupt), cat("huge component count: ", why, statusName(got)));
  }
  // random mutations, tail mostly recomputed: same classification as the strict reference, no crash, no half-state
  forEachCase(ctx, ctx.cases(1, 1) * 8, [&](std::uint64_t, SplitMix64& rng) {
    std::vector<std::uint8_t> b;
    if (rng.coin()) {
      b = v.bytes;
    } else {
      ScenarioConfig cfg;
      const Scenario sc = genScenario(rng, cfg);
      std::string e3;
      auto e = makeEngine(factory, sc.net, e3);
      if (e == nullptr) return;
      const auto frames = framesOf(sc);
      for (unsigned t = 0; t < 4 && t < frames.size(); ++t) e->step(frames[t]);
      b = e->encode();
    }
    const unsigned edits = 1 + rng.pick(3);
    for (unsigned k = 0; k < edits; ++k) {
      switch (rng.pick(4)) {
        case 0: b[static_cast<std::size_t>(rng.below(b.size()))] = u8(rng.pick(256)); break;
        case 1: b[static_cast<std::size_t>(rng.below(b.size()))] ^= u8(1U << rng.pick(8)); break;
        case 2:
          if (b.size() > 41) b.erase(b.begin() + static_cast<std::ptrdiff_t>(32 + rng.below(b.size() - 40)));
          break;
        default: b.insert(b.begin() + static_cast<std::ptrdiff_t>(rng.below(b.size())), u8(rng.pick(256))); break;
      }
    }
    if (rng.chance(9, 10)) refreshTail(b);
    const std::string why = checkDecode(factory, v, b, Limits{}, "random mutation");
    SLG_CHECK_MSG(ctx, why.empty(), why);
  });
}

}  // namespace slgt
