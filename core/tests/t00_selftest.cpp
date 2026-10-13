// Checks of the test infrastructure itself: SplitMix64 vectors (03 section 6), FNV-1a vectors, the
// blob codec against the byte-level description of 02 section 6.4, and the golden text round trips.
#include "blob.hpp"
#include "engine.hpp"
#include "fnv.hpp"
#include "gen.hpp"
#include "golden_io.hpp"
#include "harness.hpp"
#include "prng.hpp"
#include "testkit.hpp"

namespace slgt {

// Vectors computed with an independent Python implementation of the public SplitMix64 algorithm
// (seed 0 also matches the published first outputs E220A8397B1DCDAF, 6E789E6AA1B965F4, 06C45D188009454F).
SLG_TEST(S00_prng_vectors, 0, "SplitMix64 and Fisher-Yates reproduce fixed vectors; case seeds follow 03 section 6") {
  struct V { std::uint64_t seed; std::uint64_t out[4]; std::uint64_t below10[8]; std::uint64_t shuffled[10]; };
  static const V vectors[] = {
      {0, {0xE220A8397B1DCDAFULL, 0x6E789E6AA1B965F4ULL, 0x06C45D188009454FULL, 0xF88BB8A8724C81ECULL},
       {5, 0, 9, 4, 7, 0, 3, 0}, {6, 3, 2, 9, 8, 1, 4, 7, 0, 5}},
      {1, {0x910A2DEC89025CC1ULL, 0xBEEB8DA1658EEC67ULL, 0xF893A2EEFB32555EULL, 0x71C18690EE42C90BULL},
       {5, 9, 0, 5, 1, 8, 5, 3}, {4, 2, 8, 1, 9, 3, 0, 6, 7, 5}},
      {2026101300, {0xB38CDCA5CF60EF1EULL, 0xD8E22B6D353D64B9ULL, 0xC265E5E22D1C60F2ULL, 0x750877A7EA35E192ULL},
       {6, 3, 6, 4, 8, 3, 7, 1}, {5, 7, 0, 1, 9, 4, 8, 2, 3, 6}},
  };
  for (const V& v : vectors) {
    SplitMix64 r(v.seed);
    for (std::uint64_t want : v.out) {
      const std::uint64_t got = r.next();
      SLG_CHECK_MSG(ctx, got == want, cat("seed ", v.seed, ": got ", hex(got), " want ", hex(want)));
    }
    SplitMix64 b(v.seed);
    for (std::uint64_t want : v.below10) {
      const std::uint64_t got = b.below(10);
      SLG_CHECK_MSG(ctx, got == want, cat("below(10) seed ", v.seed, ": got ", got, " want ", want));
    }
    SplitMix64 s(v.seed);
    std::vector<unsigned> arr{0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    s.shuffle(arr);
    for (std::size_t i = 0; i < arr.size(); ++i) {
      SLG_CHECK_MSG(ctx, arr[i] == v.shuffled[i], cat("shuffle seed ", v.seed, " position ", i));
    }
  }
  SLG_CHECK(ctx, SplitMix64(0).below(0) == 0);
  // Case i of Tn: SplitMix64(base_Tn XOR i) with base_Tn = 2026101300 + n (SLG_SEED is XORed into the base).
  const Ctx t1("probe", 1);
  SLG_CHECK(ctx, t1.caseSeed(5) == ((2026101301ULL ^ env().seed) ^ 5ULL));
  if (env().seed == 0) {
    SplitMix64 c(t1.caseSeed(5));
    SLG_CHECK_MSG(ctx, c.next() == 0x259518084100314FULL, "first output of the T1 case 5 stream");
  }
}

SLG_TEST(S00_codec_vectors, 0, "FNV-1a vectors and the blob codec against the bytes spelled out in 02 section 6.4") {
  SLG_CHECK(ctx, fnv1a64(nullptr, 0) == 0xcbf29ce484222325ULL);
  const std::uint8_t a = 'a';
  SLG_CHECK(ctx, fnv1a64(&a, 1) == 0xaf63dc4c8601ec8cULL);

  // G1 as a model: sensor 1, NOT 2 (seed 15), actuator 3.
  Model m;
  m.comps = {CompDef{1, specSensor(), 0}, CompDef{2, specNot(), 15}, CompDef{3, specActuator(), 0}};
  m.wires = {makeWire(outPin(1), inPin(2, 0)), makeWire(outPin(2), inPin(3, 0))};
  m.sortAndNormalize();
  BlobData d = blobFromModel(m, 100000, 0);
  const std::vector<std::uint8_t> blob = encodeBlob(d);
  // The 95 bytes, written out by hand from 02 section 6.2 and 6.4 (little-endian).
  const std::vector<std::uint8_t> expected = {
      0x53, 0x4C, 0x47, 0x43, 0x01, 0x00, 0x01, 0x00, 0xA0, 0x86, 0x01, 0x00,             // magic, versions, hostStepMicros
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,                                        // tick
      0x04, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,               // nextId, 3 components, 2 wires
      0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,                     // sensor
      0x02, 0x00, 0x00, 0x00, 0x04, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x0F,               // NOT, s = 15
      0x03, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,               // actuator, o = 0
      0x01, 0x00, 0x00, 0x00, 0xFF, 0x02, 0x00, 0x00, 0x00, 0x00,                           // out(1)-in(2,0)
      0x02, 0x00, 0x00, 0x00, 0xFF, 0x03, 0x00, 0x00, 0x00, 0x00,                           // out(2)-in(3,0)
      0x4F, 0x95, 0xAE, 0x13, 0xE3, 0x4E, 0x14, 0x3D};                                       // FNV tail 0x3D144EE313AE954F
  SLG_CHECK_MSG(ctx, blob.size() == 95, cat("blob size ", blob.size()));
  SLG_CHECK_MSG(ctx, blob == expected, cat("blob bytes: ", hexBytes(blob, 100)));
  SLG_CHECK(ctx, structureHashOf(d) == 0x84C3FB7A3ACB833CULL);
  SLG_CHECK(ctx, stateHashOf(d) == 0x9D16793581913B31ULL);
  BlobData back;
  SLG_CHECK(ctx, parseBlob(blob, back) && encodeBlob(back) == blob);
  BlobLayout lay;
  SLG_CHECK(ctx, layoutOf(blob, lay) && lay.compOffset.size() == 3 && lay.wiresOffset == 32 + 11 + 12 + 12 &&
                     lay.tailOffset == 87);
}

SLG_TEST(S00_golden_text_round_trip, 0, "lnet/lscn writers and readers agree; the reader accepts hex digits and decimal") {
  forEachCase(ctx, ctx.cases(1, 2), [&](std::uint64_t, SplitMix64& rng) {
    ScenarioConfig cfg;
    cfg.editPct = 25;
    cfg.net.sparseIds = rng.coin();
    const Scenario sc = genScenario(rng, cfg);
    Model m2;
    std::string err;
    SLG_CHECK_MSG(ctx, parseLnet(formatLnet(sc.net), m2, err) && m2 == sc.net, cat("lnet round trip: ", err));
    Scenario s2;
    s2.net = sc.net;
    std::vector<TimedEdit> explicitIds = sc.edits;   // a file always spells the id out, so viaAdd does not survive
    for (TimedEdit& e : explicitIds) e.op.viaAdd = false;
    const bool parsed = parseLscn(formatLscn(sc), s2, err);
    SLG_CHECK_MSG(ctx, parsed && s2.ticks == sc.ticks && s2.edits == explicitIds && s2.inputs.size() == sc.inputs.size(),
                  cat("lscn round trip: ", parsed ? std::string("the parsed scenario differs") : err));
    if (s2.inputs.size() == sc.inputs.size()) {
      for (std::size_t i = 0; i < sc.inputs.size(); ++i) {
        SLG_CHECK(ctx, s2.inputs[i].tau == sc.inputs[i].tau && s2.inputs[i].id == sc.inputs[i].id &&
                           s2.inputs[i].value == sc.inputs[i].value);
      }
    }
  });
  {   // ltrace: format, parse and compare with the rows that were formatted
    SplitMix64 rng(ctx.caseSeed(5));
    ScenarioConfig cfg;
    cfg.editPct = 20;
    const Scenario sc = genScenario(rng, cfg);
    auto e = refFactory().make(Limits{}, 0);
    std::vector<TraceRow> rows;
    std::string e1;
    SLG_CHECK_MSG(ctx, runScenario(*e, sc, sc.ticks + 1, rows, e1), e1);
    std::vector<ParsedRow> parsed;
    std::string e2;
    SLG_CHECK_MSG(ctx, parseLtrace(formatLtrace(rows), parsed, e2) && parsed.size() == rows.size(), cat("ltrace round trip: ", e2));
    for (std::size_t i = 0; i < parsed.size() && i < rows.size(); ++i) SLG_CHECK(ctx, parsed[i] == toParsed(rows[i]));
  }
  Model m;
  std::string err;
  const bool parsed = parseLnet("lnet 1  # netlist\nc 1 sensor\nc 2 const v=F\nc 3 gate op=XOR inv=1 k=3 seed=F\n"
                                "c 4 clock p=10 h=3 seed=7\nc 5 cmp m=1\nw 2.o 3.0\nw 3.o 4.0\n",
                                m, err);
  SLG_CHECK_MSG(ctx, parsed, cat("reader leniency probe: ", err));
  SLG_CHECK_MSG(ctx, m.comps.size() == 5 && m.comps[1].spec.p0 == 15 && m.comps[2].spec.p0 == 2 && m.comps[2].seed == 15 &&
                         m.comps[3].seed == 7 && m.comps[4].spec.p0 == 1,
                cat("lenient parse: ", err));
}

}  // namespace slgt
