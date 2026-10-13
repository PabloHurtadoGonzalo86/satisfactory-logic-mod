// The worked examples of 01 and 02 (goldens G1 to G14) as models and constants, shared by the tests
// that fix them (G tests here, T3, T7, T8, T10, T12). Every number comes from the spec text.
#pragma once

#include <cstdint>
#include <vector>

#include "tmodel.hpp"

namespace slgt {

Model g1Model();                 // sensor 1 -> NOT 2 (seed 15) -> actuator 3
extern const std::uint64_t kG1StateHash[5];   // rows tau = 0..4 (02 section 6.4)
extern const unsigned kG1Nand[5];             // N in each row
extern const unsigned kG1Act[5];              // A in each row
const std::vector<std::uint8_t>& g7Blob();    // the 95 bytes with hostStepMicros = 100000, tick 0
inline constexpr std::uint64_t kG7Tail = 0x3D144EE313AE954FULL;
inline constexpr std::uint64_t kG7StructureHash = 0x84C3FB7A3ACB833CULL;
inline constexpr std::uint32_t kG7HostStepMicros = 100000;

Model g3Model();                 // two sensors -> AND -> Delay(2) -> actuator
Model g4Model();                 // sensors S=1, R=2; NOR cross pair Q=3, Qn=4; native latch 5
Model g5Model();                 // two NOT in a ring, seeds (0,0)
Model g6Model();                 // two Const(9) on one actuator input
Model g10Model();                // Clock(6,3) -> actuator
Model g13Model();                // sensor -> three NOT in a chain, seeds 0
Model g14Model();                // the merge loop: sensor A=1, NOT B=2 (seed 15), actuator C=3

class Ctx;
struct EngineFactory;
void checkG1Rows(Ctx& ctx, const EngineFactory& factory);    // rows and stateHash of 02 section 6.4
void checkG7Bytes(Ctx& ctx, const EngineFactory& factory);   // the 95-byte blob of G1

// '_' for 0, '#' for 15, a hex digit otherwise (the wave notation of the spec).
char waveChar(unsigned v);

}  // namespace slgt
