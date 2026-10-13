#include "blob.hpp"

#include "fnv.hpp"

namespace slgt {

std::vector<std::uint8_t> initialStateBytes(const Spec& s, unsigned seed) {
  std::vector<std::uint8_t> st;
  switch (s.kind) {
    case Kind::Const: case Kind::Sensor: break;
    case Kind::Actuator: case Kind::Gate: case Kind::Compare: case Kind::Latch: st.push_back(u8(seed)); break;
    case Kind::Toggle: st.push_back(u8(seed)); st.push_back(1); break;
    case Kind::Clock: st.push_back(u8(seed & 0xFFU)); st.push_back(u8(seed >> 8)); break;
    case Kind::Delay: st.assign(s.p0, u8(seed)); break;
  }
  return st;
}

static void putRecord(ByteWriter& w, const BlobComp& c, bool withState) {
  w.u32(c.id);
  w.u8(static_cast<std::uint8_t>(c.spec.kind));
  w.u16(c.spec.p0);
  w.u16(c.spec.p1);
  w.u16(c.spec.p2);
  if (withState) {
    for (std::uint8_t b : c.state) w.u8(b);
  }
}

static void putPin(ByteWriter& w, const Pin& p) {
  w.u32(p.component);
  w.u8(p.index);
}

std::vector<std::uint8_t> encodeBlob(const BlobData& d) {
  ByteWriter w;
  w.u8(0x53); w.u8(0x4C); w.u8(0x47); w.u8(0x43);
  w.u16(d.formatVersion);
  w.u16(d.semanticsVersion);
  w.u32(d.hostStepMicros);
  w.u64(d.tick);
  w.u32(d.nextId);
  w.u32(u32(d.comps.size()));
  w.u32(u32(d.wires.size()));
  for (const BlobComp& c : d.comps) putRecord(w, c, true);
  for (const Wire& x : d.wires) {
    putPin(w, x.a);
    putPin(w, x.b);
  }
  w.u64(fnv1a64(w.bytes().data(), w.bytes().size()));
  return std::move(w.bytes());
}

bool parseBlob(const std::uint8_t* data, std::size_t n, BlobData& out) {
  if (n < kBlobHeaderBytes + kBlobTailBytes) return false;
  if (data[0] != 0x53 || data[1] != 0x4C || data[2] != 0x47 || data[3] != 0x43) return false;
  if (readU64(data + n - kBlobTailBytes) != fnv1a64(data, n - kBlobTailBytes)) return false;
  BlobData d;
  d.formatVersion = readU16(data + 4);
  d.semanticsVersion = readU16(data + 6);
  d.hostStepMicros = readU32(data + 8);
  d.tick = readU64(data + 12);
  d.nextId = readU32(data + 20);
  const std::uint32_t compCount = readU32(data + 24);
  const std::uint32_t wireCount = readU32(data + 28);
  const std::size_t end = n - kBlobTailBytes;
  std::size_t pos = kBlobHeaderBytes;
  for (std::uint32_t i = 0; i < compCount; ++i) {
    if (end - pos < kBlobRecordBytes) return false;
    BlobComp c;
    c.id = readU32(data + pos);
    c.spec.kind = static_cast<Kind>(data[pos + 4]);
    c.spec.p0 = readU16(data + pos + 5);
    c.spec.p1 = readU16(data + pos + 7);
    c.spec.p2 = readU16(data + pos + 9);
    if (!kindInRange(c.spec.kind)) return false;
    pos += kBlobRecordBytes;
    const std::size_t ss = stateSize(c.spec);
    if (end - pos < ss) return false;
    c.state.assign(data + pos, data + pos + ss);
    pos += ss;
    d.comps.push_back(std::move(c));
  }
  for (std::uint32_t i = 0; i < wireCount; ++i) {
    if (end - pos < kBlobWireBytes) return false;
    Wire w;
    w.a = Pin{readU32(data + pos), data[pos + 4]};
    w.b = Pin{readU32(data + pos + 5), data[pos + 9]};
    d.wires.push_back(w);
    pos += kBlobWireBytes;
  }
  if (pos != end) return false;
  out = std::move(d);
  return true;
}

std::vector<std::uint8_t> stateStream(const BlobData& d) {
  ByteWriter w;
  for (const BlobComp& c : d.comps) {
    w.u32(c.id);
    w.u8(static_cast<std::uint8_t>(c.spec.kind));
    for (std::uint8_t b : c.state) w.u8(b);
  }
  return std::move(w.bytes());
}

std::uint64_t stateHashOf(const BlobData& d) {
  const std::vector<std::uint8_t> s = stateStream(d);
  return fnv1a64(s.data(), s.size());
}

std::uint64_t structureHashOf(const BlobData& d) {
  ByteWriter w;
  w.u32(u32(d.comps.size()));
  w.u32(u32(d.wires.size()));
  for (const BlobComp& c : d.comps) putRecord(w, c, false);
  for (const Wire& x : d.wires) {
    putPin(w, x.a);
    putPin(w, x.b);
  }
  return fnv1a64(w.bytes().data(), w.bytes().size());
}

void refreshTail(std::vector<std::uint8_t>& blob) {
  if (blob.size() < kBlobTailBytes) return;
  const std::size_t body = blob.size() - kBlobTailBytes;
  const std::uint64_t h = fnv1a64(blob.data(), body);
  for (unsigned i = 0; i < 8; ++i) blob[body + i] = u8((h >> (8 * i)) & 0xFFU);
}

bool layoutOf(const std::vector<std::uint8_t>& blob, BlobLayout& out) {
  BlobData d;
  if (!parseBlob(blob, d)) return false;
  BlobLayout l;
  std::size_t pos = kBlobHeaderBytes;
  for (const BlobComp& c : d.comps) {
    l.compOffset.push_back(pos);
    const std::size_t len = kBlobRecordBytes + c.state.size();
    l.compBytes.push_back(len);
    pos += len;
  }
  l.wiresOffset = pos;
  l.tailOffset = blob.size() - kBlobTailBytes;
  out = std::move(l);
  return true;
}

BlobData blobFromModel(const Model& m, std::uint32_t hostStepMicros, Tick tick) {
  BlobData d;
  d.hostStepMicros = hostStepMicros;
  d.tick = tick;
  d.nextId = m.nextIdHint();
  for (const CompDef& c : m.comps) d.comps.push_back(BlobComp{c.id, c.spec, initialStateBytes(c.spec, c.seed)});
  d.wires = m.wires;
  return d;
}

}  // namespace slgt
