#include "slg/netlist.hpp"

#include <map>
#include <set>

#include "internal/fnv.hpp"
#include "internal/nets.hpp"
#include "internal/spec_rules.hpp"

namespace slg {

ComponentId EditBatch::add(Spec spec, std::uint16_t seed) {
  ComponentId id = 0;  // 0 marks an exhausted id space; apply reports LimitExceeded
  if (next_ < internal::kIdSentinel) {
    id = static_cast<ComponentId>(next_);
    ++next_;
  }
  Op op;
  op.kind = OpKind::Add;
  op.id = id;
  op.spec = spec;
  op.seed = seed;
  ops_.push_back(op);
  return id;
}

void EditBatch::addWithId(ComponentId id, Spec spec, std::uint16_t seed) {
  if (static_cast<std::uint64_t>(id) >= next_) next_ = static_cast<std::uint64_t>(id) + 1;
  Op op;
  op.kind = OpKind::AddWithId;
  op.id = id;
  op.spec = spec;
  op.seed = seed;
  ops_.push_back(op);
}

void EditBatch::remove(ComponentId id) {
  Op op;
  op.kind = OpKind::Remove;
  op.id = id;
  ops_.push_back(op);
}

void EditBatch::replace(ComponentId id, Spec spec, std::uint16_t seed) {
  Op op;
  op.kind = OpKind::Replace;
  op.id = id;
  op.spec = spec;
  op.seed = seed;
  ops_.push_back(op);
}

void EditBatch::connect(Pin a, Pin b) {
  Op op;
  op.kind = OpKind::Connect;
  op.a = a;
  op.b = b;
  ops_.push_back(op);
}

void EditBatch::disconnect(Pin a, Pin b) {
  Op op;
  op.kind = OpKind::Disconnect;
  op.a = a;
  op.b = b;
  ops_.push_back(op);
}

Netlist::Netlist(Limits limits) : limits_(limits) { finalize(); }

std::span<const std::pair<ComponentId, Spec>> Netlist::components() const noexcept { return components_; }

std::span<const Wire> Netlist::wires() const noexcept { return wires_; }

void Netlist::finalize() {
  internal::Fnv64 hash;
  hash.u32(static_cast<std::uint32_t>(components_.size()));
  hash.u32(static_cast<std::uint32_t>(wires_.size()));
  for (const auto& [id, spec] : components_) {
    hash.u32(id);
    hash.byte(static_cast<std::uint8_t>(spec.kind));
    hash.u16(spec.p0);
    hash.u16(spec.p1);
    hash.u16(spec.p2);
  }
  for (const Wire& wire : wires_) {
    hash.u32(wire.a.component);
    hash.byte(wire.a.index);
    hash.u32(wire.b.component);
    hash.byte(wire.b.index);
  }
  structureHash_ = hash.value();
}

Status Netlist::apply(const EditBatch& batch) {
  Netlist result(limits_);
  const Status status = applyInto(batch, false, result, nullptr);
  if (status != Status::Ok) return status;
  *this = std::move(result);
  return Status::Ok;
}

Status Netlist::applyInto(const EditBatch& batch, bool checkSeeds, Netlist& out, std::vector<Fresh>* fresh) const {
  using EditOp = EditBatch::OpKind;
  using WirePair = std::pair<Pin, Pin>;

  if (batch.base_ != nextId_) return Status::StaleBatch;

  // Ordered containers keep every walk by ascending id/pin and make a failed batch free to discard.
  std::map<ComponentId, Spec> comps;
  for (const auto& c : components_) comps.emplace_hint(comps.end(), c.first, c.second);
  std::set<WirePair> adjacency;  // both directions of every wire
  for (const Wire& w : wires_) {
    adjacency.emplace(w.a, w.b);
    adjacency.emplace(w.b, w.a);
  }
  std::map<ComponentId, std::uint16_t> freshSeeds;
  // Explicit ids need only be >= the base nextId and distinct, in any order (decode, permuted rebuilds);
  // `high` is where the next auto id goes and what nextId becomes, so it never moves down.
  const ComponentId base = nextId_;
  std::uint64_t high = base;
  std::set<ComponentId> retiredHere;  // created and removed within this batch
  bool anyConnect = false;

  const auto checkSpec = [&](const Spec& spec, std::uint16_t seed) noexcept {
    return checkSeeds ? validate(spec, seed, limits_) : internal::validateParams(spec, limits_);
  };

  const auto resolvePin = [&comps](const Pin& pin) noexcept {
    const auto it = comps.find(pin.component);
    if (it == comps.end()) return Status::UnknownComponent;
    return internal::pinExists(it->second, pin.index) ? Status::Ok : Status::BadPin;
  };

  for (const EditBatch::Op& op : batch.ops_) {
    switch (op.kind) {
      case EditOp::Add:
      case EditOp::AddWithId: {
        ComponentId id = 0;
        if (op.kind == EditOp::Add) {
          if (high >= internal::kIdSentinel) return Status::LimitExceeded;
          id = static_cast<ComponentId>(high);
        } else {
          if (op.id < base) return comps.contains(op.id) ? Status::DuplicateId : Status::RetiredId;
          if (comps.contains(op.id)) return Status::DuplicateId;
          if (retiredHere.contains(op.id)) return Status::RetiredId;
          if (op.id >= internal::kIdSentinel) return Status::LimitExceeded;
          id = op.id;
        }
        const Status status = checkSpec(op.spec, op.seed);
        if (status != Status::Ok) return status;
        comps.emplace(id, op.spec);
        freshSeeds[id] = op.seed;
        if (static_cast<std::uint64_t>(id) + 1 > high) high = static_cast<std::uint64_t>(id) + 1;
        break;
      }
      case EditOp::Remove: {
        const auto it = comps.find(op.id);
        if (it == comps.end()) return Status::UnknownComponent;
        std::vector<WirePair> doomed;
        for (auto w = adjacency.lower_bound(WirePair{Pin{op.id, 0}, Pin{}});
             w != adjacency.end() && w->first.component == op.id; ++w) {
          doomed.push_back(*w);
        }
        for (const WirePair& w : doomed) {
          adjacency.erase(w);
          adjacency.erase(WirePair{w.second, w.first});
        }
        freshSeeds.erase(op.id);
        comps.erase(it);
        if (op.id >= base) retiredHere.insert(op.id);
        break;
      }
      case EditOp::Replace: {
        const auto it = comps.find(op.id);
        if (it == comps.end()) return Status::UnknownComponent;
        const Status status = checkSpec(op.spec, op.seed);
        if (status != Status::Ok) return status;
        for (auto w = adjacency.lower_bound(WirePair{Pin{op.id, 0}, Pin{}});
             w != adjacency.end() && w->first.component == op.id; ++w) {
          if (!internal::pinExists(op.spec, w->first.index)) return Status::BadPin;
        }
        it->second = op.spec;
        freshSeeds[op.id] = op.seed;
        break;
      }
      case EditOp::Connect: {
        Status status = resolvePin(op.a);
        if (status != Status::Ok) return status;
        status = resolvePin(op.b);
        if (status != Status::Ok) return status;
        if (op.a == op.b) return Status::BadPin;
        if (adjacency.contains(WirePair{op.a, op.b})) return Status::DuplicateWire;
        adjacency.emplace(op.a, op.b);
        adjacency.emplace(op.b, op.a);
        anyConnect = true;
        break;
      }
      case EditOp::Disconnect: {
        if (!adjacency.contains(WirePair{op.a, op.b})) return Status::UnknownWire;
        adjacency.erase(WirePair{op.a, op.b});
        adjacency.erase(WirePair{op.b, op.a});
        break;
      }
    }
  }

  // Limits judge the netlist the batch leaves behind, not its intermediate steps.
  if (comps.size() > limits_.maxComponents) return Status::LimitExceeded;
  if (adjacency.size() / 2 > limits_.maxWires) return Status::LimitExceeded;
  std::uint64_t delayCells = 0;
  for (const auto& c : comps) {
    if (c.second.kind == Kind::Delay) delayCells += c.second.p0;
  }
  if (delayCells > limits_.maxDelayCells) return Status::LimitExceeded;

  out.limits_ = limits_;
  out.components_.clear();
  out.components_.reserve(comps.size());
  for (const auto& c : comps) out.components_.emplace_back(c.first, c.second);
  out.wires_.clear();
  out.wires_.reserve(adjacency.size() / 2);
  for (const WirePair& w : adjacency) {
    if (w.first < w.second) out.wires_.push_back(Wire{w.first, w.second});
  }

  if (anyConnect) {
    const internal::PinTable table = internal::buildPinTable(out.components_);
    const internal::Nets nets = internal::buildNets(table, out.components_, out.wires_);
    for (const std::uint32_t pins : nets.pinsInNet) {
      if (pins > limits_.maxNetPins) return Status::LimitExceeded;
    }
  }

  out.nextId_ = static_cast<ComponentId>(high);
  out.finalize();
  if (fresh != nullptr) {
    fresh->clear();
    fresh->reserve(freshSeeds.size());
    for (const auto& f : freshSeeds) fresh->push_back(Fresh{f.first, f.second});
  }
  return Status::Ok;
}

}  // namespace slg
