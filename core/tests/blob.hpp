// Independent codec of the v1 blob (02 section 6.2) and of both hashes. It is the test's only window
// into the state of a Simulator (encode() is the one place the state is visible) and the only way to
// inject an arbitrary legal state (decode of a blob built here).
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "tmodel.hpp"

namespace slgt {

struct BlobComp {
  ComponentId id = 0;
  Spec spec;
  std::vector<std::uint8_t> state;   // bytes of 02 section 6.1
  friend bool operator==(const BlobComp&, const BlobComp&) = default;
};

struct BlobData {
  std::uint16_t formatVersion = 1;
  std::uint16_t semanticsVersion = 1;
  std::uint32_t hostStepMicros = 0;
  Tick tick = 0;
  std::uint32_t nextId = 1;
  std::vector<BlobComp> comps;   // ascending id
  std::vector<Wire> wires;       // normalized, ascending
};

inline constexpr std::size_t kBlobHeaderBytes = 32;
inline constexpr std::size_t kBlobTailBytes = 8;
inline constexpr std::size_t kBlobRecordBytes = 11;
inline constexpr std::size_t kBlobWireBytes = 10;

std::vector<std::uint8_t> initialStateBytes(const Spec& s, unsigned seed);

std::vector<std::uint8_t> encodeBlob(const BlobData& d);
// Structure only (magic, lengths, tail); false when the bytes are not a well-formed blob. Semantic
// validity (domains, ordering) is not judged here: that is the job of the core's decode.
bool parseBlob(const std::uint8_t* data, std::size_t n, BlobData& out);
inline bool parseBlob(const std::vector<std::uint8_t>& v, BlobData& out) { return parseBlob(v.data(), v.size(), out); }

std::uint64_t stateHashOf(const BlobData& d);       // FNV-1a of (u32 id, u8 kind, state) per component
std::uint64_t structureHashOf(const BlobData& d);   // FNV-1a of counts, spec records without state, wires
std::vector<std::uint8_t> stateStream(const BlobData& d);

void refreshTail(std::vector<std::uint8_t>& blob);   // recompute the 8 trailing bytes over the rest

struct BlobLayout {
  std::vector<std::size_t> compOffset;   // start of each component record
  std::vector<std::size_t> compBytes;    // length of each record (11 + state)
  std::size_t wiresOffset = 0;
  std::size_t tailOffset = 0;
};
bool layoutOf(const std::vector<std::uint8_t>& blob, BlobLayout& out);

// A blob for a model with every component at its seed.
BlobData blobFromModel(const Model& m, std::uint32_t hostStepMicros = 0, Tick tick = 0);

}  // namespace slgt
