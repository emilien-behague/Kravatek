#pragma once
#include "dsp/Analysis.h"
#include <span>
namespace kravatek::genetic {
struct Features {
  AudioAnalysis basic;
  double fundamentalHz = 0, fundamentalConfidence = 0, effectiveSeconds = 0,
         centroidHz = 0;
  std::array<double, 3>
      bandEnergy{}; // Power fractions: <200, 200..2000, >=2000 Hz.
  std::array<double, 8>
      timeEnergy{}; // Fractions of total energy over equal-duration bins.
  double transientFraction = 0, transientPeakDb = -120, attackSeconds = 0;
};
Features measure(std::span<const float>, double rate,
                 bool completeEdges = false);
double featureDistance(const Features &, const Features &) noexcept;
double similarity(std::span<const float>, const Features &,
                  std::span<const float>, const Features &) noexcept;
} // namespace kravatek::genetic
