#pragma once

#include <compare>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

#include "slg/types.hpp"

namespace slg {

struct Pin {
  ComponentId component = 0;
  std::uint8_t index = 0;  // input number, or kOut
  friend auto operator<=>(const Pin&, const Pin&) = default;
};

struct Wire {
  Pin a, b;  // stored normalized (a < b), sorted, unique
};

// Operations are recorded here and judged in order by Netlist::apply.
class EditBatch {
 public:
  // The base is the nextId of the netlist the batch is meant for (a fresh netlist starts at 1).
  explicit EditBatch(ComponentId baseNextId = 1) noexcept : base_(baseNextId), next_(baseNextId) {}

  ComponentId add(Spec spec, std::uint16_t seed);  // id = base nextId + adds so far
  void addWithId(ComponentId id, Spec spec, std::uint16_t seed);
  void remove(ComponentId id);
  void replace(ComponentId id, Spec spec, std::uint16_t seed);
  void connect(Pin a, Pin b);
  void disconnect(Pin a, Pin b);

  ComponentId baseNextId() const noexcept { return base_; }
  std::size_t size() const noexcept { return ops_.size(); }
  bool empty() const noexcept { return ops_.empty(); }

 private:
  friend class Netlist;

  enum class OpKind : std::uint8_t { Add, AddWithId, Remove, Replace, Connect, Disconnect };
  struct Op {
    OpKind kind = OpKind::Add;
    ComponentId id = 0;
    Spec spec;
    std::uint16_t seed = 0;
    Pin a, b;
  };

  ComponentId base_;
  std::uint64_t next_;  // mirrors the working nextId of apply, so add() can hand out the final id
  std::vector<Op> ops_;
};

// Structure only: no state, no seeds.
class Netlist {
 public:
  explicit Netlist(Limits limits = {});

  Status apply(const EditBatch&);  // all-or-nothing (seeds are not stored)
  std::span<const std::pair<ComponentId, Spec>> components() const noexcept;  // ascending id
  std::span<const Wire> wires() const noexcept;
  ComponentId nextId() const noexcept { return nextId_; }
  std::uint64_t structureHash() const noexcept { return structureHash_; }
  const Limits& limits() const noexcept { return limits_; }
  EditBatch newBatch() const { return EditBatch(nextId_); }

 private:
  friend class Simulator;

  struct Fresh {  // a component created or replaced by a batch: its state restarts at `seed`
    ComponentId id;
    std::uint16_t seed;
  };

  Status applyInto(const EditBatch& batch, bool checkSeeds, Netlist& out, std::vector<Fresh>* fresh) const;
  void finalize();

  Limits limits_;
  std::vector<std::pair<ComponentId, Spec>> components_;
  std::vector<Wire> wires_;
  ComponentId nextId_ = 1;
  std::uint64_t structureHash_ = 0;
};

}  // namespace slg
