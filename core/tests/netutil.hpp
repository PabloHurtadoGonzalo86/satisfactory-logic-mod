// Nets of a model computed with a plain union-find, shared by the generators and by T3, T4 and T7.
#pragma once

#include <cstddef>
#include <map>
#include <utility>
#include <vector>

#include "tmodel.hpp"

namespace slgt {

struct NetInfo {
  std::vector<Pin> pins;       // ascending
  std::vector<Pin> drivers;    // output pins of components that have one, ascending
  std::vector<Pin> readers;    // input pins, ascending
};

struct NetAnalysis {
  std::vector<NetInfo> nets;               // ordered by the smallest pin of each net
  std::map<Pin, std::size_t> netOf;        // every valid pin of every component, wired or not
  // edge c -> c' when (c, out) lies in the same net as an input of c' (01 section 5); unique, ascending
  std::vector<std::pair<ComponentId, ComponentId>> edges;
};

NetAnalysis analyzeNets(const Model& m);

// Is the net-induced graph free of cycles (autoedges included)? Iterative.
bool inducedGraphIsAcyclic(const NetAnalysis& na, const Model& m);

}  // namespace slgt
