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
         * std::exp(-6.0 * std::pow(t / p.durationSeconds, p.ampCurve));
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

std::vector<double> makeFilter(int factor) {
    const auto count = 32 * factor + 1;
    const auto centre = (count - 1) / 2;
    const auto cutoff = .40 / factor;
    std::vector<double> coefficients(static_cast<std::size_t>(count));
    double sum = 0;
    for (int i = 0; i < count; ++i) {
        const auto x = i - centre;
        const auto sinc = x == 0 ? 2 * cutoff : std::sin(twoPi * cutoff * x) / (std::numbers::pi * x);
        const auto window = .42 - .5 * std::cos(twoPi * i / (count - 1))
                                 + .08 * std::cos(2 * twoPi * i / (count - 1));
        coefficients[static_cast<std::size_t>(i)] = sinc * window;
        sum += sinc * window;
    }
    for (auto& c : coefficients) c /= sum;
    return coefficients;
}

void colourRender(RenderedKick& result) {
    const auto& p = result.parameters;
    const int factor = result.sampleRate < 24000 ? 8 : 4;
    const auto highRate = result.sampleRate * factor;
    static const auto filter4 = makeFilter(4), filter8 = makeFilter(8);
    const auto& filter = factor == 4 ? filter4 : filter8;
    const auto half = filter.size() / 2;
    const auto highCount = result.samples.size() * static_cast<std::size_t>(factor);
    std::vector<float> high(highCount + 2 * half, 0);
    SineOscillator body(highRate), modulator(highRate), tail(highRate), tailModulator(highRate), sub(highRate);
    // Topology-preserving state-variable low-pass: damping stays positive at all settings.
    double filterState1 = 0, filterState2 = 0;
    Random random(p.transientSeed);
    const auto drive = std::pow(10.0, p.driveDb / 20.0);
    const auto driveScale = std::tanh(drive);
    const auto lowAlpha = 1.0 - std::exp(-twoPi * 1600 / highRate);
    const auto highAlpha = 1.0 - std::exp(-twoPi * std::min(9000.0, result.sampleRate * .4) / highRate);
    double noiseLow = 0, noiseBand = 0;
    const auto lastTime = static_cast<double>(highCount - 1) / highRate;
    for (std::size_t i = 0; i < highCount; ++i) {
        const auto t = static_cast<double>(i) / highRate;
        const auto frequency = pitchAt(p, t);
        const auto phase = body.phaseRadians();
        const auto modulation = modulator.next(frequency * 1.73);
        const auto depth = p.fmAmount * std::exp(-t / p.fmSeconds);
        body.next(frequency * (1 + .18 * depth * modulation));
        const auto fm = depth * modulation;
        auto rich = std::sin(phase + fm);
        rich += p.harmonics * (.34 * std::sin(3 * phase) + .16 * std::sin(5 * phase) + .08 * std::sin(7 * phase));
        const auto noise = random.between(-1, 1);
        noiseLow += lowAlpha * (noise - noiseLow);
        noiseBand += highAlpha * (noise - noiseLow - noiseBand);
        const auto envelope = amplitudeAt(p, t, lastTime);
        const auto transient = p.clickLevel * noiseBand * std::exp(-t / p.clickSeconds) * 1.8;
        const auto input = (rich + transient) * envelope;
        const auto saturated = p.driveDb > 0 ? std::tanh(input * drive) / driveScale : input;
        const auto folded = std::sin(input * (1 + drive * .28));
        const auto bodySignal = saturated * (1 - p.shape) + folded * p.shape;
        const auto movingRatio = p.tailRatio * std::pow(2.0, .7 * p.tailMotion
            * std::sin(twoPi * (3 + p.tailMotion * 13) * t) * (1 - std::exp(-t / .040)));
        const auto tailFrequency = std::min(p.endHz * movingRatio, highRate * .20);
        const auto tailPhase = tail.phaseRadians(); tail.next(tailFrequency);
        // A non-integer ratio needs its own phase accumulator; multiplying the
        // wrapped body phase would jump at every fundamental cycle.
        const auto tailModulation = tailModulator.next(frequency * 1.5 + 7);
        const auto tailSignal = std::sin(tailPhase + p.tailMotion * 3 * tailModulation);
        const auto tailOnset = .5 * (1 - std::cos(std::numbers::pi * std::clamp(t / .012, 0.0, 1.0)));
        const auto releaseProgress = std::clamp((lastTime - t) / p.releaseSeconds, 0.0, 1.0);
        const auto finalRelease = .5 * (1 - std::cos(std::numbers::pi * releaseProgress));
        const auto tailEnvelope = tailOnset * std::exp(-t / p.tailSeconds) * finalRelease;
        const auto tailDrive = 1 + p.driveDb * .12 * p.tailMotion * (1 - std::exp(-t / .060));
        const auto layer = bodySignal + p.tailLevel * std::tanh(tailSignal * tailDrive) * tailEnvelope;
        const auto cutoff = std::clamp(p.filterHz * std::pow(2.0, p.filterSweep * std::exp(-t / .080)),
                                       40.0, std::min(18000.0, result.sampleRate * .40));
        const auto g = std::tan(std::numbers::pi * cutoff / highRate);
        const auto damping = 2.0 - 1.9 * p.resonance;
        const auto a1 = 1 / (1 + g * (g + damping));
        const auto v1 = a1 * (filterState1 + g * (layer - filterState2));
        const auto v2 = filterState2 + g * v1;
        filterState1 = 2 * v1 - filterState1; filterState2 = 2 * v2 - filterState2;
        const bool filtering = p.filterHz < 15999 || p.resonance > 0 || p.filterSweep != 0;
        const auto coloured = filtering ? v2 : layer;
        const auto subEnvelope = tailOnset * std::exp(-t / p.subSeconds) * finalRelease;
        const auto cleanSub = sub.next(p.endHz) * subEnvelope;
        high[half + i] = static_cast<float>((1 - p.subMix) * coloured + p.subMix * cleanSub);
    }
    for (std::size_t i = 0; i < result.samples.size(); ++i) {
        const auto at = i * static_cast<std::size_t>(factor);
        double sample = 0;
        for (std::size_t k = 0; k < filter.size(); ++k)
            sample += filter[k] * high[at + k];
        // Short endpoint tapers also remove the pre-ringing of the centred FIR.
        const auto start = std::clamp(static_cast<double>(i) / (result.sampleRate * .00025), 0.0, 1.0);
        const auto stop = std::clamp(static_cast<double>(result.samples.size() - 1 - i) / (result.sampleRate * .001), 0.0, 1.0);
        result.samples[i] = static_cast<float>(sample * .5 * (1 - std::cos(std::numbers::pi * start))
                                                    * .5 * (1 - std::cos(std::numbers::pi * stop)));
    }
}
} // namespace

SineOscillator::SineOscillator(double sampleRate) : sampleRate_(sampleRate) {
    requireRange(sampleRate, 8000.0, 768000.0, "Oscillator sample rate must be 8000..768000 Hz");
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
    requireRange(p.attackSeconds, 0.0001, p.durationSeconds / 4.0, "Invalid attack time");
    requireRange(p.releaseSeconds, 0.001, p.durationSeconds / 4.0, "Invalid release time");
    requireRange(p.peakDb, -60.0, -3.0, "Peak must leave at least 3 dB of headroom");
    requireRange(p.pitchCurve, .35, 3.0, "Pitch curve must be .35..3");
    requireRange(p.ampCurve, .35, 3.0, "Amplitude curve must be .35..3");
    requireRange(p.harmonics, 0, 1, "Harmonics must be 0..1");
    requireRange(p.fmAmount, 0, 4, "FM amount must be 0..4");
    requireRange(p.driveDb, 0, 30, "Drive must be 0..30 dB");
    requireRange(p.subMix, 0, 1, "Clean sub mix must be 0..1");
    requireRange(p.clickLevel, 0, 1, "Transient level must be 0..1");
    requireRange(p.fmSeconds, .005, .500, "FM decay must be 5..500 ms");
    requireRange(p.shape, 0, 1, "Shape morph must be 0..1");
    requireRange(p.tailLevel, 0, 1, "Tail level must be 0..1");
    requireRange(p.tailSeconds, .020, 1, "Tail decay must be 20..1000 ms");
    requireRange(p.tailRatio, .5, 4, "Tail ratio must be .5..4");
    requireRange(p.tailMotion, 0, 1, "Tail motion must be 0..1");
    requireRange(p.filterHz, 80, 16000, "Filter cutoff must be 80..16000 Hz");
    requireRange(p.resonance, 0, .95, "Resonance must be 0...95");
    requireRange(p.filterSweep, -4, 4, "Filter sweep must be -4..4 octaves");
    requireRange(p.subSeconds, .020, 1, "Sub decay must be 20..1000 ms");
    requireRange(p.clickSeconds, .0005, .030, "Transient decay must be .5..30 ms");
}

double pitchAt(const KickParameters& p, double seconds) noexcept {
    return p.endHz + (p.startHz - p.endHz)
         * std::exp(-std::log(100.0) * std::pow(std::max(0.0, seconds) / p.pitchDropSeconds, p.pitchCurve));
}

double amplitudeAt(const KickParameters& p, double seconds) noexcept {
    return amplitudeAt(p, std::clamp(seconds, 0.0, p.durationSeconds), p.durationSeconds);
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

    if (p.harmonics > 0 || p.fmAmount > 0 || p.driveDb > 0 || p.clickLevel > 0
        || p.tailLevel > 0 || p.subMix > 0 || p.shape > 0 || p.filterHz < 15999
        || p.resonance > 0 || p.filterSweep != 0) {
        colourRender(result);
        sum = 0;
        for (const auto sample : result.samples) sum += sample;
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
