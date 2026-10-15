#pragma once
#include "Genome.h"
#include <array>
#include <vector>
namespace kravatek::genetic {
struct Render {
  double sampleRate = 48000;
  std::array<std::vector<float>, 4>
      layers; // SUB, ATTACK, BODY, TAIL; native independent lengths.
  std::vector<float> mix;
  double safetyGain = 1;
};
Render render(const Genome &, double sampleRate = 48000,
              bool retainLayers = true);
} // namespace kravatek::genetic
