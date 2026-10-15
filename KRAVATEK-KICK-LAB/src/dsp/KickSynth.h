#pragma once

#include <cstdint>
#include <vector>

namespace kravatek {

struct KickParameters {
    double startHz = 170.0;
    double endHz = 52.0;
    double pitchDropSeconds = 0.060; // 99% of the pitch drop by this time.
    double durationSeconds = 0.320;
    double attackSeconds = 0.0015;
    double releaseSeconds = 0.008;
    double peakDb = -3.0;
    std::uint32_t seed = 1;
};

struct RenderedKick {
    KickParameters parameters;
    double sampleRate = 48000.0;
    std::vector<float> samples;
};

class SineOscillator {
public:
    explicit SineOscillator(double sampleRate);
    // Call with a finite frequency in [0, sampleRate / 2).
    double next(double frequency) noexcept;
private:
    double sampleRate_;
    double phase_ = 0.0;
};

void validate(const KickParameters& parameters, double sampleRate);
double pitchAt(const KickParameters& parameters, double seconds) noexcept;
KickParameters variationFromSeed(std::uint32_t seed) noexcept;
RenderedKick renderKick(const KickParameters& parameters, double sampleRate = 48000.0);

} // namespace kravatek
