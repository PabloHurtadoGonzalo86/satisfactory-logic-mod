// T10: determinism and the hash/serialization contract.
//  - the same (N0, seeds, batches per tick, frames) gives the same stateHash and blob at every tick, twice;
//  - encode at tick k, decode, continue == the uninterrupted run, and encode(decode(b)) == b;
//  - changing any field of the state of any kind changes stateHash; changing tick, nextId or hostStepMicros does not;
//  - G1 and G7 (hashes and the 95 bytes) and the oracle's all_kinds blob (Delay, Toggle, Clock bytes, a hole in the ids).
// Not claimed: float to nibble conversion, the non-determinism of the game.
#include <cstdlib>
#include <filesystem>

#include "engine.hpp"
#include "gen.hpp"
#include "spec_examples.hpp"
#include "testkit.hpp"

namespace slgt {

namespace {

std::string goldenFile(const char* name) {
  const char* dir = std::getenv("SLG_GOLDEN_DIR");
  const char* candidates[] = {dir != nullptr ? dir : "", "core/golden", "../golden", "golden"};
  for (const char* c : candidates) {
    if (c[0] == '\0') continue;
    std::string text;
    if (readFile(cat(c, "/", name), text)) return text;
  }
  return "";
}

std::vector<std::uint8_t> bytesOf(const std::string& s) { return std::vector<std::uint8_t>(s.begin(), s.end()); }

// "" or why not
std::string continuation(const EngineFactory& f, const Scenario& sc, unsigned rows, unsigned k) {
  auto a = f.make(Limits{}, 0);
  if (a->apply(opsForModel(sc.net)) != Status::Ok) return "initial netlist rejected";
  ScenarioCursor cur(sc);
  std::vector<std::vector<std::uint8_t>> blobs(rows);
  std::vector<std::uint64_t> hashes(rows);
  std::vector<Signal> samplesAtK;
  std::vector<std::uint8_t> blobAtK;
  for (unsigned tau = 0; tau < rows; ++tau) {
    Status st = Status::Ok;
    const std::vector<Signal> samples = cur.begin(*a, tau, &st);
    if (st != Status::Ok) return cat("batch at ", tau, " rejected: ", statusName(st));
    blobs[tau] = a->encode();
    hashes[tau] = a->stateHash();
    if (tau == k) blobAtK = blobs[tau];
    if (tau + 1 < rows) a->step(samples);
  }
  auto b = f.make(Limits{}, 0);
  const Status ds = b->loadBlob(blobAtK);
  if (ds != Status::Ok) return cat("decode of a blob the core itself produced at tick ", k, ": ", statusName(ds));
  if (b->encode() != blobAtK) return cat("encode(decode(b)) != b at tick ", k);
  ScenarioCursor cur2(sc);
  for (unsigned tau = k; tau < rows; ++tau) {
    Status st = Status::Ok;
    const std::vector<Signal> samples = tau == k ? cur2.resume(*b, tau) : cur2.begin(*b, tau, &st);
    if (st != Status::Ok) return cat("batch at ", tau, " rejected after the restore: ", statusName(st));
    if (b->encode() != blobs[tau] || b->stateHash() != hashes[tau]) {
      return cat("the run restored at tick ", k, " differs from the uninterrupted run at tick ", tau);
    }
    if (tau + 1 < rows) b->step(samples);
  }
  return "";
}

}  // namespace

SLG_PROPERTY(T10_determinism_and_hashes, 10, "same inputs same hashes; save/restore/continue; every state field is in stateHash, tick is not; G1, G7",
             RunOn::Both) {
  forEachCase(ctx, ctx.cases(), [&](std::uint64_t, SplitMix64& rng) {
    ScenarioConfig cfg;
    cfg.editPct = rng.coin() ? 0U : 20;
    cfg.net.maxComps = 4 + rng.pick(10);
    cfg.minTicks = 8;
    cfg.maxTicks = 24;
    const Scenario sc = genScenario(rng, cfg);
    const unsigned rows = sc.ticks + 1;
    // (1) twice: identical blobs and hashes at every tick
    {
      auto x = factory.make(Limits{}, 123);
      auto y = factory.make(Limits{}, 123);
      const std::string why = runLockstep(*x, *y, sc, rows);
      if (!why.empty()) {
        SLG_FAIL(ctx, cat("two runs of the same scenario differ: ", why));
        reportScenario(ctx, "T10 determinism", sc, [&](const Scenario& s) {
          auto p = factory.make(Limits{}, 123);
          auto q = factory.make(Limits{}, 123);
          return !runLockstep(*p, *q, s, s.ticks + 1).empty();
        });
        return;
      }
    }
    // (2) save at a random tick, restore into a new engine, continue
    {
      const unsigned k = rng.pick(rows);
      const std::string why = continuation(factory, sc, rows, k);
      if (!why.empty()) {
        SLG_FAIL(ctx, why);
        return;
      }
    }
    // (3) the hash covers every state field and nothing else
    {
      auto e = factory.make(Limits{}, 0);
      if (e->apply(opsForModel(sc.net)) != Status::Ok) {
        SLG_FAIL(ctx, "initial netlist rejected");
        return;
      }
      const auto frames = genFrames(rng, sensorIdsOf(sc.net).size(), 6, SampleStyle::Random);
      for (const auto& fr : frames) e->step(fr);
      const Obs base = observe(*e);
      std::vector<std::pair<std::size_t, std::size_t>> fields;   // (component, byte)
      for (std::size_t i = 0; i < base.data.comps.size(); ++i) {
        for (std::size_t j = 0; j < base.data.comps[i].state.size(); ++j) fields.emplace_back(i, j);
      }
      rng.shuffle(fields);
      if (fields.size() > 14) fields.resize(14);
      for (const auto& [ci, bj] : fields) {
        BlobData d = base.data;
        BlobComp& c = d.comps[ci];
        std::uint8_t& v = c.state[bj];
        switch (c.spec.kind) {
          case Kind::Gate: case Kind::Latch: v = v == 0 ? 15 : 0; break;
          case Kind::Toggle: v = bj == 0 ? (v == 0 ? 15 : 0) : (v == 0 ? 1 : 0); break;
          case Kind::Clock: {
            const unsigned phase = c.state[0] + 256U * c.state[1];
            const unsigned next = (phase + 1) % c.spec.p0;
            c.state[0] = u8(next & 0xFFU);
            c.state[1] = u8(next >> 8);
            break;
          }
          default: v = u8((v + 1 + rng.pick(15)) % 16); break;
        }
        auto g = factory.make(Limits{}, 0);
        const Status s = g->loadBlob(encodeBlob(d));
        if (s != Status::Ok) {
          SLG_FAIL(ctx, cat("a legal edited state was rejected: ", statusName(s), " (", describeComp(c), ")"));
          return;
        }
        if (g->stateHash() == base.stateHash || g->stateHash() != stateHashOf(d)) {
          SLG_FAIL(ctx, cat("changing byte ", bj, " of ", describeComp(c), " must change stateHash (", hex(base.stateHash), " -> ",
                            hex(g->stateHash()), ", independent FNV says ", hex(stateHashOf(d)), ")"));
          return;
        }
      }
      BlobData d = base.data;
      d.tick += 1000;
      d.nextId += 7;
      d.hostStepMicros += 5;
      auto g = factory.make(Limits{}, 0);
      if (g->loadBlob(encodeBlob(d)) != Status::Ok) {
        SLG_FAIL(ctx, "a blob with another tick, nextId and hostStepMicros was rejected");
        return;
      }
      SLG_CHECK_MSG(ctx, g->stateHash() == base.stateHash, "tick, nextId and hostStepMicros must not enter stateHash");
      SLG_CHECK_MSG(ctx, g->tick() == base.tick + 1000 && g->nextId() == base.nextId + 7 && g->hostStepMicros() == base.hostStep + 5,
                    "the header fields of the blob must come back from tick(), nextId() and hostStepMicros()");
    }
  });
  checkG1Rows(ctx, factory);
  checkG7Bytes(ctx, factory);
  // the oracle's fixtures: lat_not.slgc must be the 95 bytes of the spec; all_kinds.slgc must round-trip
  const std::string lat = goldenFile("lat_not.slgc");
  if (!lat.empty()) SLG_CHECK_MSG(ctx, bytesOf(lat) == g7Blob(), "core/golden/lat_not.slgc is not the 95 bytes written out in 02 section 6.4");
  const std::string all = goldenFile("all_kinds.slgc");
  if (!all.empty()) {
    auto e = factory.make(Limits{}, 0);
    const Status s = e->loadBlob(bytesOf(all));
    SLG_CHECK_MSG(ctx, s == Status::Ok, cat("decode of all_kinds.slgc: ", statusName(s)));
    if (s == Status::Ok) SLG_CHECK_MSG(ctx, e->encode() == bytesOf(all), "encode(decode(all_kinds.slgc)) != the file");
  }
}

}  // namespace slgt
