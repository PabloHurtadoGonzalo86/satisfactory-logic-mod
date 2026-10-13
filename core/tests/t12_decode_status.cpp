// T12, exact statuses of decode: which of FormatCorrupt and LimitExceeded a blob that breaks more than one rule gets.
// Every expected value is the oracle's (ambiguities.md B2, B7, B8; decisions D28 to D30) and nothing accepts "either":
//  - a parameter above its cap (arity, Delay length, Clock period) is LimitExceeded, any other invalid parameter is
//    FormatCorrupt, and the Clock period is judged before its high width;
//  - the header counts are held to the limits first (LimitExceeded) and then, each on its own and not their sum, to the
//    bytes that are left (FormatCorrupt); a blob that fails later for another reason keeps that reason;
//  - the limits of the whole netlist (total Delay cells, then pins per net) are judged last, after ids, nextId, wires and
//    "no byte left over", so a blob that is corrupt anywhere is FormatCorrupt even when it also exceeds one of them;
//  - the first component whose record fails decides, in id order, and the object decode was called on is never touched.
// Both engines run them: the reference decoder checks the table, the core is checked against the table.
// Not claimed: anything with several corrupt fields in which the spec does not order the checks (the earlier field wins).
#include <string>
#include <utility>
#include <vector>

#include "blob.hpp"
#include "engine.hpp"
#include "fnv.hpp"
#include "testkit.hpp"

namespace slgt {

namespace {

struct Probe {
  std::string what;
  Limits lim;
  std::vector<std::uint8_t> bytes;
  Status want = Status::Ok;
};

BlobComp comp(ComponentId id, const Spec& spec, unsigned seed = 0) { return BlobComp{id, spec, initialStateBytes(spec, seed)}; }

std::vector<std::uint8_t> blobOf(std::vector<BlobComp> comps, std::vector<Wire> wires, std::uint32_t nextId) {
  BlobData d;
  d.nextId = nextId;
  d.comps = std::move(comps);
  d.wires = std::move(wires);
  return encodeBlob(d);
}

// header + body + tail with counts that need not match the body
std::vector<std::uint8_t> rawBlob(std::uint32_t nextId, std::uint32_t componentCount, std::uint32_t wireCount, const std::vector<std::uint8_t>& body) {
  ByteWriter w;
  for (const std::uint8_t m : {std::uint8_t{0x53}, std::uint8_t{0x4C}, std::uint8_t{0x47}, std::uint8_t{0x43}}) w.u8(m);
  w.u16(1);
  w.u16(1);
  w.u32(0);
  w.u64(0);
  w.u32(nextId);
  w.u32(componentCount);
  w.u32(wireCount);
  std::vector<std::uint8_t> out = w.bytes();
  out.insert(out.end(), body.begin(), body.end());
  out.resize(out.size() + kBlobTailBytes, 0);
  refreshTail(out);
  return out;
}

std::vector<std::uint8_t> recordOf(ComponentId id, const Spec& s) {
  ByteWriter w;
  w.u32(id);
  w.u8(static_cast<std::uint8_t>(s.kind));
  w.u16(s.p0);
  w.u16(s.p1);
  w.u16(s.p2);
  return w.bytes();
}

// one byte more than the blob holds, with the tail recomputed
std::vector<std::uint8_t> withTrailingByte(const std::vector<std::uint8_t>& blob) {
  std::vector<std::uint8_t> out(blob.begin(), blob.end() - static_cast<std::ptrdiff_t>(kBlobTailBytes));
  out.push_back(0x5A);
  out.resize(out.size() + kBlobTailBytes, 0);
  refreshTail(out);
  return out;
}

// the last wire record cut out and the tail recomputed, with the header still declaring it
std::vector<std::uint8_t> withoutLastWire(const std::vector<std::uint8_t>& blob) {
  std::vector<std::uint8_t> out(blob.begin(), blob.end() - static_cast<std::ptrdiff_t>(kBlobTailBytes + kBlobWireBytes));
  out.resize(out.size() + kBlobTailBytes, 0);
  refreshTail(out);
  return out;
}

Limits limitsWith(unsigned arity, unsigned delay, unsigned period) {
  Limits l;
  l.maxArity = u8(arity);
  l.maxDelay = u16(delay);
  l.maxClockPeriod = u16(period);
  return l;
}

// a blob of one component of the given spec, state at its seed 0
Probe oneComp(const std::string& what, const Limits& lim, const Spec& spec, Status want) {
  return Probe{what, lim, blobOf({comp(1, spec)}, {}, 2), want};
}

void run(Ctx& ctx, const EngineFactory& f, const std::vector<Probe>& probes) {
  BlobData victimData;
  victimData.nextId = 3;
  victimData.comps = {comp(1, specSensor()), comp(2, specNot(), 15)};
  victimData.wires = {makeWire(outPin(1), inPin(2, 0))};
  const std::vector<std::uint8_t> victimBytes = encodeBlob(victimData);
  for (const Probe& p : probes) {
    auto e = f.make(Limits{}, 0);
    if (e->loadBlob(victimBytes) != Status::Ok) {
      SLG_FAIL(ctx, "cannot load the victim blob");
      return;
    }
    const Status got = e->decodeDirect(p.bytes, p.lim);
    SLG_CHECK_MSG(ctx, got == p.want, cat(p.what, ": decode answered ", statusName(got), ", the spec says ", statusName(p.want), " (", p.bytes.size(), " bytes)"));
    if (got != Status::Ok) {
      SLG_CHECK_MSG(ctx, e->encode() == victimBytes, cat(p.what, ": a refused blob changed the object decode was called on"));
    } else {
      SLG_CHECK_MSG(ctx, e->encode() == p.bytes, cat(p.what, ": encode(decode(b)) != b"));
    }
  }
}

}  // namespace

SLG_PROPERTY(T12_decode_param_caps, 12,
             "a blob parameter above its cap is LimitExceeded, any other invalid parameter FormatCorrupt, the Clock period before its high width (ambiguities B2, B8)",
             RunOn::Both) {
  std::vector<Probe> probes;
  const Limits dflt;
  const Limits small = limitsWith(3, 4, 6);
  const Limits raised = limitsWith(12, 300, 5000);
  for (unsigned d : {256U, 257U, 300U, 65535U}) probes.push_back(oneComp(cat("Delay(", d, ") at the default cap"), dflt, specDelay(d), Status::LimitExceeded));
  for (unsigned k : {9U, 10U, 255U, 256U, 65535U}) probes.push_back(oneComp(cat("Gate k=", k, " at the default cap"), dflt, specGate(GateOp::Or, false, k), Status::LimitExceeded));
  const std::pair<unsigned, unsigned> clocks[] = {{4097, 1}, {4097, 4097}, {4097, 0}, {5000, 6000}, {65535, 1}, {65535, 65535}};
  for (const auto& [p, h] : clocks) {
    probes.push_back(oneComp(cat("Clock(", p, ",", h, ") at the default cap"), dflt, specClock(p, h), Status::LimitExceeded));
  }
  probes.push_back(oneComp("Delay(255) at the default cap", dflt, specDelay(255), Status::Ok));
  probes.push_back(oneComp("Gate k=8 at the default cap", dflt, specGate(GateOp::Xor, true, 8), Status::Ok));
  probes.push_back(oneComp("Clock(4096,2048) at the default cap", dflt, specClock(4096, 2048), Status::Ok));
  for (unsigned d : {5U, 255U, 256U, 300U}) probes.push_back(oneComp(cat("Delay(", d, ") with maxDelay 4"), small, specDelay(d), Status::LimitExceeded));
  for (unsigned k : {4U, 8U, 9U, 255U}) probes.push_back(oneComp(cat("Gate k=", k, " with maxArity 3"), small, specGate(GateOp::And, false, k), Status::LimitExceeded));
  for (unsigned h : {0U, 3U, 7U, 8U}) probes.push_back(oneComp(cat("Clock(7,", h, ") with maxClockPeriod 6"), small, specClock(7, h), Status::LimitExceeded));
  probes.push_back(oneComp("Clock(6,6) with maxClockPeriod 6: the period is allowed, the high width is not", small, specClock(6, 6), Status::FormatCorrupt));
  probes.push_back(oneComp("Clock(6,3) with maxClockPeriod 6", small, specClock(6, 3), Status::Ok));
  probes.push_back(oneComp("Delay(300) with maxDelay 300", raised, specDelay(300), Status::Ok));
  probes.push_back(oneComp("Delay(301) with maxDelay 300", raised, specDelay(301), Status::LimitExceeded));
  probes.push_back(oneComp("Gate k=12 with maxArity 12", raised, specGate(GateOp::Or, false, 12), Status::Ok));
  probes.push_back(oneComp("Gate k=13 with maxArity 12", raised, specGate(GateOp::Or, false, 13), Status::LimitExceeded));
  probes.push_back(oneComp("Clock(5000,2500) with maxClockPeriod 5000", raised, specClock(5000, 2500), Status::Ok));
  probes.push_back(oneComp("Clock(5001,6000) with maxClockPeriod 5000", raised, specClock(5001, 6000), Status::LimitExceeded));
  probes.push_back(oneComp("Clock(5000,5000) with maxClockPeriod 5000", raised, specClock(5000, 5000), Status::FormatCorrupt));
  // malformed parameters are FormatCorrupt even when a cap is exceeded too
  Spec s = specGate(GateOp::Or, false, 300);
  s.p0 = 3;
  probes.push_back(oneComp("Gate op 3 with k=300", dflt, s, Status::FormatCorrupt));
  s = specGate(GateOp::Xor, true, 9);
  s.p1 = 2;
  probes.push_back(oneComp("Gate inv 2 with k=9", dflt, s, Status::FormatCorrupt));
  s = specDelay(300);
  s.p1 = 1;
  probes.push_back(oneComp("Delay(300) with an unused parameter", dflt, s, Status::FormatCorrupt));
  s = specClock(5000, 2);
  s.p2 = 1;
  probes.push_back(oneComp("Clock(5000,2) with an unused parameter", dflt, s, Status::FormatCorrupt));
  probes.push_back(oneComp("Clock(1,1): period below 2", dflt, specClock(1, 1), Status::FormatCorrupt));
  probes.push_back(oneComp("Gate k=0", dflt, specGate(GateOp::And, false, 0), Status::FormatCorrupt));
  // the first record that fails decides, and a record is judged by its parameters before its state
  probes.push_back(Probe{"a corrupt record, then a parameter above its cap", dflt,
                         blobOf({BlobComp{1, specGate(GateOp::Or, false, 1), {7}}, comp(2, specDelay(300))}, {}, 3), Status::FormatCorrupt});
  probes.push_back(Probe{"a parameter above its cap, then a corrupt record", dflt,
                         blobOf({comp(1, specDelay(300)), BlobComp{2, specGate(GateOp::Or, false, 1), {7}}}, {}, 3), Status::LimitExceeded});
  probes.push_back(Probe{"a parameter above its cap with no state after it", dflt, rawBlob(2, 1, 0, recordOf(1, specDelay(300))), Status::LimitExceeded});
  probes.push_back(Probe{"a parameter above its cap with a corrupt state after it", dflt,
                         blobOf({BlobComp{1, specClock(5000, 2), {0xFF, 0xFF}}}, {}, 2), Status::LimitExceeded});
  run(ctx, factory, probes);
}

SLG_PROPERTY(T12_decode_check_order, 12,
             "decode judges the total Delay cells and the pins per net after every other check of the blob; each header count is held to the bytes left on its own (ambiguities B8)",
             RunOn::Both) {
  std::vector<Probe> probes;
  const Spec delay8 = specDelay(8);
  const auto cellsBlob = [&](std::vector<BlobComp> late, std::vector<Wire> wires, std::uint32_t nextId) {
    std::vector<BlobComp> comps = {comp(1, delay8), comp(2, delay8)};
    for (BlobComp& c : late) comps.push_back(std::move(c));
    return blobOf(std::move(comps), std::move(wires), nextId);
  };
  Limits cells;   // two Delay(8) exceed 10 cells
  cells.maxDelayCells = 10;
  Limits cellsExact;
  cellsExact.maxDelayCells = 16;
  const std::vector<Wire> oneWire = {makeWire(outPin(2), inPin(4, 0))};
  const std::vector<BlobComp> tail = {comp(3, specGate(GateOp::Or, false, 1)), comp(4, specActuator())};
  probes.push_back(Probe{"Delay cells over the total, nothing else wrong", cells, cellsBlob(tail, oneWire, 5), Status::LimitExceeded});
  probes.push_back(Probe{"Delay cells exactly at the total", cellsExact, cellsBlob(tail, oneWire, 5), Status::Ok});
  {
    BlobComp bad = comp(2, delay8);
    bad.state[0] = 16;
    probes.push_back(Probe{"the Delay that passes the total has a slot of 16", cells,
                           blobOf({comp(1, delay8), bad, tail[0], tail[1]}, oneWire, 5), Status::FormatCorrupt});
  }
  probes.push_back(Probe{"Delay cells over the total, then a Gate with state 7", cells,
                         cellsBlob({BlobComp{3, specGate(GateOp::Or, false, 1), {7}}, comp(4, specActuator())}, oneWire, 5), Status::FormatCorrupt});
  probes.push_back(Probe{"Delay cells over the total, then an Actuator with state 16", cells,
                         cellsBlob({comp(3, specGate(GateOp::Or, false, 1)), BlobComp{4, specActuator(), {16}}}, oneWire, 5), Status::FormatCorrupt});
  {
    BlobComp unknown = comp(4, specActuator());
    unknown.spec.kind = static_cast<Kind>(10);
    unknown.state.clear();
    probes.push_back(Probe{"Delay cells over the total, then a kind that does not exist", cells, cellsBlob({tail[0], unknown}, {}, 5), Status::FormatCorrupt});
  }
  probes.push_back(Probe{"Delay cells over the total, then a parameter above its cap", cells,
                         cellsBlob({tail[0], comp(4, specDelay(300))}, {}, 5), Status::LimitExceeded});
  probes.push_back(Probe{"Delay cells over the total, then a malformed parameter", cells,
                         cellsBlob({tail[0], comp(4, [] { Spec g = specGate(GateOp::Or, false, 1); g.p0 = 3; return g; }())}, {}, 5), Status::FormatCorrupt});
  probes.push_back(Probe{"Delay cells over the total, then nextId at the highest id", cells, cellsBlob(tail, oneWire, 4), Status::FormatCorrupt});
  probes.push_back(Probe{"Delay cells over the total, then nextId 0", cells, cellsBlob(tail, oneWire, 0), Status::FormatCorrupt});
  probes.push_back(Probe{"Delay cells over the total, then an unnormalized wire", cells,
                         cellsBlob(tail, {Wire{inPin(4, 0), outPin(2)}}, 5), Status::FormatCorrupt});
  probes.push_back(Probe{"Delay cells over the total, then a wire to a pin that does not exist", cells,
                         cellsBlob(tail, {makeWire(outPin(2), inPin(3, 9))}, 5), Status::FormatCorrupt});
  probes.push_back(Probe{"Delay cells over the total, then a wire to a component that does not exist", cells,
                         cellsBlob(tail, {Wire{inPin(4, 0), outPin(77)}}, 5), Status::FormatCorrupt});
  probes.push_back(Probe{"Delay cells over the total, then a byte too many", cells, withTrailingByte(cellsBlob(tail, oneWire, 5)), Status::FormatCorrupt});
  probes.push_back(Probe{"Delay cells over the total, a wire declared and its bytes missing", cells, withoutLastWire(cellsBlob(tail, oneWire, 5)), Status::FormatCorrupt});

  // pins per net, the other total
  Limits pins;
  pins.maxNetPins = 3;
  const std::vector<BlobComp> fanout = {comp(1, specConst(5)), comp(2, specActuator()), comp(3, specActuator()), comp(4, specActuator())};
  const std::vector<Wire> star = {makeWire(outPin(1), inPin(2, 0)), makeWire(outPin(1), inPin(3, 0)), makeWire(outPin(1), inPin(4, 0))};
  probes.push_back(Probe{"a net of four pins with maxNetPins 3", pins, blobOf(fanout, star, 5), Status::LimitExceeded});
  probes.push_back(Probe{"a net of four pins, then a byte too many", pins, withTrailingByte(blobOf(fanout, star, 5)), Status::FormatCorrupt});
  probes.push_back(Probe{"a net of four pins, then nextId at the highest id", pins, blobOf(fanout, star, 4), Status::FormatCorrupt});
  probes.push_back(Probe{"a net of four pins, then an unnormalized wire", pins,
                         blobOf(fanout, {makeWire(outPin(1), inPin(2, 0)), makeWire(outPin(1), inPin(3, 0)), Wire{inPin(4, 0), outPin(1)}}, 5), Status::FormatCorrupt});
  probes.push_back(Probe{"a net of three pins with maxNetPins 3", pins, blobOf(fanout, {makeWire(outPin(1), inPin(2, 0)), makeWire(outPin(1), inPin(3, 0))}, 5), Status::Ok});
  Limits both = pins;
  both.maxDelayCells = 1;
  probes.push_back(Probe{"both totals exceeded", both, blobOf({comp(1, specConst(5)), comp(2, specActuator()), comp(3, specActuator()), comp(4, specActuator()), comp(5, specDelay(2))}, star, 6), Status::LimitExceeded});

  // each count against the bytes left, on its own and not their sum
  Limits maxDelay100;
  maxDelay100.maxDelay = 100;
  probes.push_back(Probe{"one component and one wire declared, one record of a Delay above maxDelay and nothing else", maxDelay100,
                         rawBlob(2, 1, 1, recordOf(1, specDelay(200))), Status::LimitExceeded});
  probes.push_back(Probe{"the same with no wire declared", maxDelay100, rawBlob(2, 1, 0, recordOf(1, specDelay(200))), Status::LimitExceeded});
  probes.push_back(Probe{"one component and one wire declared, one Delay(256) record and nothing else", Limits{}, rawBlob(2, 1, 1, recordOf(1, specDelay(256))), Status::LimitExceeded});
  probes.push_back(Probe{"one component and one wire declared, one valid Delay record without its state", Limits{}, rawBlob(2, 1, 1, recordOf(1, specDelay(2))), Status::FormatCorrupt});
  probes.push_back(Probe{"two components declared, bytes for one", Limits{}, rawBlob(3, 2, 0, recordOf(1, specSensor())), Status::FormatCorrupt});
  probes.push_back(Probe{"two wires declared, bytes for one", Limits{}, rawBlob(1, 0, 2, std::vector<std::uint8_t>(10, 0)), Status::FormatCorrupt});
  Limits few;
  few.maxComponents = 3;
  few.maxWires = 3;
  probes.push_back(Probe{"more components than the limit, and than the bytes", few, rawBlob(1, 4, 0, {}), Status::LimitExceeded});
  probes.push_back(Probe{"more wires than the limit, and than the bytes", few, rawBlob(1, 0, 4, {}), Status::LimitExceeded});
  probes.push_back(Probe{"components within the limit but not within the bytes", few, rawBlob(1, 3, 0, {}), Status::FormatCorrupt});
  run(ctx, factory, probes);
}

}  // namespace slgt
