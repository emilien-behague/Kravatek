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
    double pitchCurve = 1.0;
    double ampCurve = 1.0;
    double harmonics = 0.0;
    double fmAmount = 0.0;
    double driveDb = 0.0;
    double subMix = 0.0;
    double clickLevel = 0.0;
    double clickSeconds = 0.004;
    std::uint32_t transientSeed = 1;
    double fmSeconds = .028;
    double shape = 0.0; // Crossfade tanh -> bounded sine folding, at the oversampled rate.
    double tailLevel = 0.0;
    double tailSeconds = .180;
    double tailRatio = 1.0;
    double tailMotion = 0.0;
    double filterHz = 16000.0;
    double resonance = 0.0;
    double filterSweep = 0.0; // Octaves at attack, exponentially settles to filterHz.
    double subSeconds = .240;
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
    double phaseRadians() const noexcept { return phase_; }
private:
    double sampleRate_;
    double phase_ = 0.0;
};

void validate(const KickParameters& parameters, double sampleRate);
double pitchAt(const KickParameters& parameters, double seconds) noexcept;
double amplitudeAt(const KickParameters& parameters, double seconds) noexcept;
KickParameters variationFromSeed(std::uint32_t seed) noexcept;
RenderedKick renderKick(const KickParameters& parameters, double sampleRate = 48000.0);

enum class KickStyle { reference, hardTechno, psy, schranz, hardcore, deep, experimental, mutant, count };
struct DnaLocks {
    bool pitch = false, amplitude = false, tone = false, transient = false;
    bool tail = false, filter = false, sub = false;
};
const char* styleName(KickStyle style) noexcept;
KickParameters factoryPreset(KickStyle style);
KickParameters mutate(const KickParameters& source, std::uint32_t seed, double amount, DnaLocks locks = {});
KickParameters generate(KickStyle style, std::uint32_t seed, double novelty);
KickParameters evolve(const KickParameters& source, std::uint32_t seed, double amount,
                      double novelty, DnaLocks locks = {});
KickParameters seededPreset(KickStyle style, std::uint32_t seed);

} // namespace kravatek
