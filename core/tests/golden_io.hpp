// Reader and writer of the golden text formats of 03 section 4: lnet (netlist + seeds), lscn (scenario),
// ltrace (output). UTF-8, '\n', numbers formatted by hand (never the locale).
//
// 03 gives the grammar as one-line sketches. The layout written here is the one the oracle fixes in
// docs/spec/ambiguities.md (A1 to A9): decimal values, a literal " | " between the four groups of a row
// (empty groups are written), upper-case 16-digit hash, st only for components with state, n+1 rows for
// "ticks n". The reader is lenient (comments after '#', a number as one hex digit or as decimal, op and
// m by name or number) so that either reading of the sketches can be parsed.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "blob.hpp"
#include "tmodel.hpp"

namespace slgt {

struct TraceRow {
  Tick tau = 0;
  std::vector<std::pair<ComponentId, Signal>> in;   // the sample fed to each sensor at this tick
  std::vector<BlobComp> st;                         // components that have state, ascending id
  std::vector<std::pair<ComponentId, Signal>> cmd;  // o of each actuator
  std::uint64_t hash = 0;                           // stateHash of the row
};

// The content of one ltrace row as text, independent of the layout (state values kept as written).
struct ParsedRow {
  Tick tau = 0;
  std::vector<std::pair<ComponentId, Signal>> in;
  std::vector<std::pair<ComponentId, std::string>> st;
  std::vector<std::pair<ComponentId, Signal>> cmd;
  std::uint64_t hash = 0;
  friend bool operator==(const ParsedRow&, const ParsedRow&) = default;
};
ParsedRow toParsed(const TraceRow& r);

std::string formatLnet(const Model& m);
std::string formatLscn(const Scenario& sc);
std::string formatLtrace(const std::vector<TraceRow>& rows);

bool parseLtrace(const std::string& text, std::vector<ParsedRow>& rows, std::string& err);
bool parseLnet(const std::string& text, Model& out, std::string& err);
// Fills ticks, inputs and edits of sc (sc.net is left alone).
bool parseLscn(const std::string& text, Scenario& sc, std::string& err);
std::size_t countRows(const std::string& ltrace);

bool readFile(const std::string& path, std::string& out);
bool writeFile(const std::string& path, const std::string& data);

}  // namespace slgt
