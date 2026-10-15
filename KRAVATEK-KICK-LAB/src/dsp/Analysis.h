#pragma once
#include <array>
#include <span>
namespace kravatek {
struct AudioAnalysis {
    double peakDb = -120, rmsDb = -120, dc = 0, durationSeconds = 0;
    std::array<double, 64> spectrumDb {};
};
AudioAnalysis analyse(std::span<const float> samples, double sampleRate);
double comparisonRms(std::span<const float> samples, double sampleRate) noexcept;
}
