#pragma once
#include "Genome.h"
namespace kravatek::genetic {
Genome generateGenome(Mode, std::uint32_t seed, double novelty);
}
namespace kravatek::genetic {
Genome evolveGenome(const Genome &parent, Mode, std::uint32_t seed,
                    double novelty, Locks, double strength = 1);
}
