#include "slg/simulator.hpp"

#include <algorithm>

#include "internal/fnv.hpp"
#include "internal/nets.hpp"
#include "internal/spec_rules.hpp"

namespace slg {
namespace {

inline unsigned loadU16(const std::uint8_t* p) noexcept {
  return static_cast<unsigned>(p[0]) | (static_cast<unsigned>(p[1]) << 8);
}

inline void storeU16(std::uint8_t* p, unsigned v) noexcept {
  p[0] = static_cast<std::uint8_t>(v & 0xFFu);
  p[1] = static_cast<std::uint8_t>((v >> 8) & 0xFFu);
}

}  // namespace

Simulator::Simulator(Limits limits, std::uint32_t hostStepMicros) : netlist_(limits), hostStepMicros_(hostStepMicros) {
  rebuildPlan();
}

EditBatch Simulator::newBatch() const { return netlist_.newBatch(); }

void Simulator::rebuildPlan() {
  const auto comps = netlist_.components();
  const internal::PinTable table = internal::buildPinTable(comps);
  internal::Nets nets = internal::buildNets(table, comps, netlist_.wires());

  slots_.assign(comps.size(), detail::Slot{});
  sensorIds_.clear();
  actuatorIds_.clear();
  sensorSlot_.clear();
  actuatorSlot_.clear();

  std::uint32_t offset = 0;
  for (std::size_t i = 0; i < comps.size(); ++i) {
    const Spec& spec = comps[i].second;
    detail::Slot& slot = slots_[i];
    slot.kind = spec.kind;
    slot.inputs = table.inputs[i];
    slot.pinFirst = table.first[i];
    switch (spec.kind) {
      case Kind::Gate:
        slot.mode = static_cast<std::uint8_t>(spec.p0);
        slot.invert = static_cast<std::uint8_t>(spec.p1);
        break;
      case Kind::Compare:
        slot.mode = static_cast<std::uint8_t>(spec.p0);
        break;
      case Kind::Const:
      case Kind::Delay:
        slot.p0 = spec.p0;
        break;
      case Kind::Clock:
        slot.p0 = spec.p0;
        slot.p1 = spec.p1;
        break;
      case Kind::Sensor:
        sensorIds_.push_back(comps[i].first);
        sensorSlot_.push_back(static_cast<std::uint32_t>(i));
        break;
      case Kind::Actuator:
        actuatorIds_.push_back(comps[i].first);
        actuatorSlot_.push_back(static_cast<std::uint32_t>(i));
        break;
      case Kind::Latch:
      case Kind::Toggle:
        break;
    }
    slot.stateOffset = offset;
    slot.stateSize = internal::stateSize(spec);
    offset += slot.stateSize;
  }

  pinNet_ = std::move(nets.pinNet);
  netValue_.assign(nets.netCount, 0);
  sensorOut_.assign(comps.size(), 0);
  next_.assign(offset, 0);
}

void Simulator::refreshCommands() noexcept {
  commands_.resize(actuatorSlot_.size());
  for (std::size_t i = 0; i < actuatorSlot_.size(); ++i) {
    commands_[i] = state_[slots_[actuatorSlot_[i]].stateOffset];
  }
}

Status Simulator::apply(const EditBatch& batch) {
  if (batch.empty()) return batch.baseNextId() == netlist_.nextId() ? Status::Ok : Status::StaleBatch;

  Netlist candidate(netlist_.limits());
  std::vector<Netlist::Fresh> fresh;
  const Status status = netlist_.applyInto(batch, true, candidate, &fresh);
  if (status != Status::Ok) return status;

  // Survivors keep their bytes; created and replaced components restart at their seed (02 section 5).
  const auto oldComps = netlist_.components();
  std::vector<std::uint8_t> newState;
  newState.reserve(state_.size());
  std::size_t freshAt = 0;
  std::size_t oldAt = 0;
  for (const auto& [id, spec] : candidate.components()) {
    const std::uint32_t size = internal::stateSize(spec);
    const std::size_t at = newState.size();
    newState.resize(at + size);
    while (freshAt < fresh.size() && fresh[freshAt].id < id) ++freshAt;
    while (oldAt < oldComps.size() && oldComps[oldAt].first < id) ++oldAt;
    const bool isFresh = freshAt < fresh.size() && fresh[freshAt].id == id;
    const bool survives = oldAt < oldComps.size() && oldComps[oldAt].first == id;
    if (!isFresh && survives) {
      const detail::Slot& old = slots_[oldAt];
      std::copy(state_.begin() + old.stateOffset, state_.begin() + old.stateOffset + old.stateSize,
                newState.begin() + static_cast<std::ptrdiff_t>(at));
    } else {
      const std::uint16_t seed = isFresh ? fresh[freshAt].seed : restSeed(spec);
      internal::writeInitialState(spec, seed, std::span<std::uint8_t>(newState).subspan(at, size));
    }
  }

  netlist_ = std::move(candidate);
  state_ = std::move(newState);
  rebuildPlan();
  refreshCommands();
  return Status::Ok;
}

Status Simulator::step(std::span<const Signal> samples) {
  if (samples.size() != sensorSlot_.size()) return Status::BadFrame;

  for (std::size_t i = 0; i < samples.size(); ++i) {
    Signal value = samples[i];
    if (value > 15) {
      value = 15;
      if (clamped_ != 0xFFFFFFFFu) ++clamped_;
    }
    sensorOut_[sensorSlot_[i]] = value;
  }
  std::fill(netValue_.begin(), netValue_.end(), Signal{0});

  const std::uint8_t* cur = state_.data();
  std::uint8_t* nxt = next_.data();
  Signal* nets = netValue_.data();
  const std::uint32_t* pinNet = pinNet_.data();
  const std::size_t count = slots_.size();

  // Phase A and B: every output comes from X_tau alone, then each net takes the max of its conductors.
  for (std::size_t i = 0; i < count; ++i) {
    const detail::Slot& slot = slots_[i];
    Signal out = 0;
    switch (slot.kind) {
      case Kind::Const:
        out = static_cast<Signal>(slot.p0);
        break;
      case Kind::Sensor:
        out = sensorOut_[i];
        break;
      case Kind::Actuator:
        continue;
      case Kind::Gate:
      case Kind::Delay:
      case Kind::Compare:
      case Kind::Latch:
      case Kind::Toggle:
        out = cur[slot.stateOffset];
        break;
      case Kind::Clock:
        out = static_cast<int>(loadU16(cur + slot.stateOffset)) >= static_cast<int>(slot.p0) - static_cast<int>(slot.p1)
                  ? Signal{15}
                  : Signal{0};
        break;
    }
    Signal& net = nets[pinNet[slot.pinFirst + slot.inputs]];
    if (out > net) net = out;
  }

  // Phase C: X_{tau+1} is written into the other buffer; nothing reads it until the swap.
  for (std::size_t i = 0; i < count; ++i) {
    const detail::Slot& slot = slots_[i];
    const std::uint32_t at = slot.stateOffset;
    const std::uint32_t pins = slot.pinFirst;
    switch (slot.kind) {
      case Kind::Const:
      case Kind::Sensor:
        break;
      case Kind::Actuator:
        nxt[at] = nets[pinNet[pins]];
        break;
      case Kind::Gate: {
        unsigned truthy = 0;
        for (unsigned j = 0; j < slot.inputs; ++j) {
          if (nets[pinNet[pins + j]] > 0) ++truthy;
        }
        bool gate = false;
        switch (static_cast<GateOp>(slot.mode)) {
          case GateOp::And:
            gate = truthy == slot.inputs;
            break;
          case GateOp::Or:
            gate = truthy > 0;
            break;
          case GateOp::Xor:
            gate = (truthy & 1u) != 0;
            break;
        }
        nxt[at] = gate != (slot.invert != 0) ? std::uint8_t{15} : std::uint8_t{0};
        break;
      }
      case Kind::Delay: {
        const std::uint32_t length = slot.p0;
        std::copy(cur + at + 1, cur + at + length, nxt + at);
        nxt[at + length - 1] = nets[pinNet[pins]];
        break;
      }
      case Kind::Compare: {
        const unsigned a = nets[pinNet[pins]];
        const unsigned b = nets[pinNet[pins + 1]];
        unsigned result = 0;
        if (static_cast<CompareMode>(slot.mode) == CompareMode::Compare) {
          result = b <= a ? a : 0;
        } else {
          result = a > b ? a - b : 0;
        }
        nxt[at] = static_cast<std::uint8_t>(result);
        break;
      }
      case Kind::Latch: {
        const bool set = nets[pinNet[pins]] > 0;
        const bool reset = nets[pinNet[pins + 1]] > 0;
        nxt[at] = reset ? std::uint8_t{0} : (set ? std::uint8_t{15} : cur[at]);
        break;
      }
      case Kind::Toggle: {
        const bool high = nets[pinNet[pins]] > 0;
        const bool rising = high && cur[at + 1] == 0;
        nxt[at] = rising ? static_cast<std::uint8_t>(15 - cur[at]) : cur[at];
        nxt[at + 1] = high ? std::uint8_t{1} : std::uint8_t{0};
        break;
      }
      case Kind::Clock: {
        const unsigned phase = loadU16(cur + at) + 1;
        storeU16(nxt + at, phase == slot.p0 ? 0u : phase);
        break;
      }
    }
  }

  state_.swap(next_);
  ++tick_;
  refreshCommands();
  return Status::Ok;
}

std::uint64_t Simulator::stateHash() const noexcept {
  internal::Fnv64 hash;
  const auto comps = netlist_.components();
  for (std::size_t i = 0; i < comps.size(); ++i) {
    const detail::Slot& slot = slots_[i];
    hash.u32(comps[i].first);
    hash.byte(static_cast<std::uint8_t>(slot.kind));
    for (std::uint32_t k = 0; k < slot.stateSize; ++k) hash.byte(state_[slot.stateOffset + k]);
  }
  return hash.value();
}

}  // namespace slg
