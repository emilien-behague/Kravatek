#include "KickSynth.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace kravatek {
namespace {
constexpr auto twoPi = 2.0 * std::numbers::pi;

double amplitudeAt(const KickParameters& p, double t, double lastTime) noexcept {
    const auto attack = std::clamp(t / p.attackSeconds, 0.0, 1.0);
    const auto release = std::clamp((lastTime - t) / p.releaseSeconds, 0.0, 1.0);
    return 0.5 * (1.0 - std::cos(std::numbers::pi * attack))
         * 0.5 * (1.0 - std::cos(std::numbers::pi * release))
         * std::exp(-6.0 * t / p.durationSeconds);
}

void requireRange(double v, double lo, double hi, const char* name) {
    if (!std::isfinite(v) || v < lo || v > hi)
        throw std::invalid_argument(name);
}

class Random {
public:
    explicit Random(std::uint32_t seed) : state_(seed) {}
    double between(double low, double high) noexcept {
        // Fixed integer algorithm, independent of std::uniform_real_distribution.
        state_ += 0x9e3779b9U;
        auto z = state_;
        z = (z ^ (z >> 16U)) * 0x21f0aaadU;
        z = (z ^ (z >> 15U)) * 0x735a2d97U;
        z ^= z >> 15U;
        return low + (high - low) * (static_cast<double>(z) / 4294967296.0);
    }
private:
    std::uint32_t state_;
};
} // namespace

SineOscillator::SineOscillator(double sampleRate) : sampleRate_(sampleRate) {
    requireRange(sampleRate, 8000.0, 192000.0, "Sample rate must be 8000..192000 Hz");
}

double SineOscillator::next(double frequency) noexcept {
    const auto value = std::sin(phase_);
    phase_ += twoPi * frequency / sampleRate_;
    if (phase_ >= twoPi)
        phase_ -= twoPi;
    return value;
}

void validate(const KickParameters& p, double sampleRate) {
    requireRange(sampleRate, 8000.0, 192000.0, "Sample rate must be 8000..192000 Hz");
    requireRange(p.startHz, 20.0, 2000.0, "Start pitch must be 20..2000 Hz");
    requireRange(p.endHz, 20.0, p.startHz, "End pitch must be 20 Hz..start pitch");
    requireRange(p.durationSeconds, 0.040, 2.0, "Duration must be 40..2000 ms");
    requireRange(p.pitchDropSeconds, 0.005, 0.300, "Pitch drop must be 5..300 ms");
    requireRange(p.attackSeconds, 0.0005, p.durationSeconds / 4.0, "Invalid attack time");
    requireRange(p.releaseSeconds, 0.001, p.durationSeconds / 4.0, "Invalid release time");
    requireRange(p.peakDb, -60.0, -3.0, "Peak must leave at least 3 dB of headroom");
}

double pitchAt(const KickParameters& p, double seconds) noexcept {
    return p.endHz + (p.startHz - p.endHz)
         * std::exp(-std::log(100.0) * std::max(0.0, seconds) / p.pitchDropSeconds);
}

KickParameters variationFromSeed(std::uint32_t seed) noexcept {
    Random random(seed);
    KickParameters p;
    p.seed = seed;
    if (seed == 1) return p; // Seed 1 is the documented 170 -> 52 Hz reference.
    p.startHz = random.between(145.0, 200.0);
    p.endHz = std::round(random.between(46.0, 62.0) * 10.0) / 10.0;
    p.pitchDropSeconds = random.between(0.045, 0.080);
    p.durationSeconds = std::round(random.between(0.260, 0.400) * 1000.0) / 1000.0;
    p.attackSeconds = random.between(0.001, 0.002);
    return p;
}

RenderedKick renderKick(const KickParameters& p, double sampleRate) {
    validate(p, sampleRate);
    RenderedKick result {p, sampleRate, {}};
    const auto count = static_cast<std::size_t>(std::llround(p.durationSeconds * sampleRate));
    result.samples.resize(count);
    const auto lastTime = static_cast<double>(count - 1) / sampleRate;
    SineOscillator oscillator(sampleRate);
    double sum = 0.0;
    double weights = 0.0;
    for (std::size_t i = 0; i < count; ++i) {
        const auto t = static_cast<double>(i) / sampleRate;
        const auto envelope = amplitudeAt(p, t, lastTime);
        const auto sample = oscillator.next(pitchAt(p, t)) * envelope;
        result.samples[i] = static_cast<float>(sample);
        sum += static_cast<double>(result.samples[i]);
        weights += envelope;
    }

    // Offline, envelope-weighted DC correction preserves silent endpoints.
    const auto correction = sum / weights;
    double peak = 0.0;
    for (std::size_t i = 0; i < count; ++i) {
        auto& sample = result.samples[i];
        sample = static_cast<float>(sample - correction
            * amplitudeAt(p, static_cast<double>(i) / sampleRate, lastTime));
        peak = std::max(peak, std::abs(static_cast<double>(sample)));
    }
    const auto target = std::pow(10.0, p.peakDb / 20.0);
    const auto gain = peak > 0.0 ? target / peak : 0.0;
    for (auto& sample : result.samples)
        sample = static_cast<float>(sample * gain);
    result.samples.front() = result.samples.back() = 0.0f;
    return result;
}

} // namespace kravatek
