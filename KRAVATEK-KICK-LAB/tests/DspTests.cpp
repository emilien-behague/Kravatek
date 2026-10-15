#include "dsp/KickSynth.h"
#include "dsp/Playback.h"
#include "dsp/WavWriter.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <limits>
#include <map>
#include <numbers>
#include <numeric>
#include <stdexcept>
#include <string>
#include <thread>

namespace {
void check(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}
template <class Function> void rejects(Function function) {
    bool rejected = false;
    try { function(); } catch (const std::invalid_argument&) { rejected = true; }
    check(rejected, "Invalid input was accepted");
}
double peak(const std::vector<float>& data) {
    double result = 0;
    for (auto value : data) result = std::max(result, std::abs(static_cast<double>(value)));
    return result;
}
void safe(const kravatek::RenderedKick& k) {
    const auto& data = k.samples;
    check(!data.empty(), "Empty kick");
    for (auto value : data) check(std::isfinite(value), "Non-finite sample");
    check(peak(data) <= std::pow(10.0, -3.0 / 20.0) + 1e-7, "Less than 3 dB headroom");
    check(peak(data) > 0.1, "Kick is silent or too quiet");
    const auto dc = std::accumulate(data.begin(), data.end(), 0.0) / static_cast<double>(data.size());
    check(std::abs(dc) < 1e-7, "Significant DC offset");
    check(data.front() == 0 && data.back() == 0, "Nonsilent endpoints");
}
std::uint32_t le(const std::vector<unsigned char>& bytes, std::size_t at, int count) {
    check(at + static_cast<std::size_t>(count) <= bytes.size(), "Truncated WAV");
    std::uint32_t value = 0;
    for (int i = 0; i < count; ++i) value |= static_cast<std::uint32_t>(bytes[at + static_cast<std::size_t>(i)]) << (8 * i);
    return value;
}

void oscillator() {
    kravatek::SineOscillator osc(48000);
    double phase = 0;
    for (int i = 0; i < 100000; ++i) {
        const auto frequency = 52.0 + 118.0 * std::exp(-static_cast<double>(i) / 2000.0);
        check(std::abs(osc.next(frequency) - std::sin(phase)) < 1e-8, "Phase discontinuity");
        phase += 2.0 * std::numbers::pi * frequency / 48000.0;
    }
}
void pitch() {
    const kravatek::KickParameters p;
    check(kravatek::pitchAt(p, 0) == 170, "Wrong initial pitch");
    check(std::abs(kravatek::pitchAt(p, .060) - 53.18) < 1e-10, "Wrong exponential drop");
    check(std::abs(kravatek::pitchAt(p, .320) - 52) < 1e-7, "Wrong final pitch");
    double previous = p.startHz;
    for (int i = 0; i < 15360; ++i) {
        auto f = kravatek::pitchAt(p, static_cast<double>(i) / 48000);
        check(f <= previous && f >= p.endHz, "Non-monotonic pitch envelope");
        previous = f;
    }
    const auto audio = kravatek::renderKick(p).samples;
    std::vector<double> crossings;
    for (std::size_t i = 5761; i < audio.size(); ++i) {
        if (audio[i - 1] < 0 && audio[i] >= 0) {
            const auto fraction = -static_cast<double>(audio[i - 1]) / (audio[i] - audio[i - 1]);
            crossings.push_back((static_cast<double>(i - 1) + fraction) / 48000.0);
        }
    }
    check(crossings.size() >= 5, "Rendered kick lacks an audible periodic tail");
    for (std::size_t i = 1; i < crossings.size(); ++i)
        check(std::abs(1.0 / (crossings[i] - crossings[i - 1]) - 52.0) < .1,
              "Actual rendered tail is not 52 Hz");
}
void reference() {
    const auto k = kravatek::renderKick({});
    check(k.samples.size() == 15360, "Default kick must be 320 ms at 48 kHz");
    safe(k);
    check(std::abs(20 * std::log10(peak(k.samples)) + 3) < 0.00001, "Wrong peak level");
    std::cout << "frames=" << k.samples.size() << " peakDb=" << 20 * std::log10(peak(k.samples))
              << " DC=" << std::accumulate(k.samples.begin(), k.samples.end(), 0.0) / 15360.0 << '\n';
}
void reproducibility() {
    for (auto seed : {0U, 1U, 123456789U, 0xffffffffU}) {
        auto first = kravatek::renderKick(kravatek::variationFromSeed(seed));
        auto second = kravatek::renderKick(kravatek::variationFromSeed(seed));
        check(first.samples == second.samples, "Seed is not deterministic");
        check(kravatek::describe(first) == kravatek::describe(second), "Metadata not deterministic");
        auto p = first.parameters;
        p.endHz = 40;
        check(first.samples != kravatek::renderKick(p).samples, "PITCH END has no effect");
        p.durationSeconds = .8;
        check(kravatek::renderKick(p).samples.size() == 38400, "DECAY has no effect");
    }
}
void variations() {
    auto previous = kravatek::renderKick(kravatek::variationFromSeed(0)).samples;
    for (std::uint32_t seed = 1; seed <= 512; ++seed) {
        auto k = kravatek::renderKick(kravatek::variationFromSeed(seed));
        safe(k);
        check(k.samples != previous, "GENERATE repeated the same kick");
        previous = std::move(k.samples);
    }
}
void sampleRates() {
    for (auto rate : {8000., 44100., 48000., 96000., 192000.}) {
        auto k = kravatek::renderKick({}, rate);
        safe(k);
        check(k.samples.size() == static_cast<std::size_t>(std::llround(.320 * rate)), "Wrong resampled duration");
        kravatek::KickParameters p;
        p.durationSeconds = 2.0;
        auto maximum = kravatek::renderKick(p, rate);
        safe(maximum);
        check(maximum.samples.size() <= kravatek::Playback::maxSamples, "Playback buffer too short");
    }
}
void invalidInput() {
    for (auto value : {0., -48000., 7999., 192001., std::numeric_limits<double>::infinity(),
                       std::numeric_limits<double>::quiet_NaN()})
        rejects([&] { kravatek::renderKick({}, value); });
    kravatek::KickParameters p;
    p.endHz = std::numeric_limits<double>::quiet_NaN();
    rejects([&] { kravatek::renderKick(p); });
    p = {}; p.endHz = 200;
    rejects([&] { kravatek::renderKick(p); });
    p = {}; p.peakDb = 0;
    rejects([&] { kravatek::renderKick(p); });
    p = {}; p.durationSeconds = 0;
    rejects([&] { kravatek::renderKick(p); });
    p = {}; p.attackSeconds = 0;
    rejects([&] { kravatek::renderKick(p); });
    p = {}; p.releaseSeconds = -1;
    rejects([&] { kravatek::renderKick(p); });
}
void envelope() {
    const auto k = kravatek::renderKick({});
    const auto& a = k.samples;
    check(std::abs(a[1]) < .0001f && std::abs(a[a.size() - 2]) < .00001f, "Endpoint click");
    for (std::size_t i = 1; i < a.size(); ++i)
        check(std::abs(a[i] - a[i - 1]) < .03f, "Discontinuous waveform");
    double early = 0, late = 0;
    for (std::size_t i = 1000; i < 4000; ++i) early += a[i] * a[i];
    for (std::size_t i = a.size() - 3000; i < a.size(); ++i) late += a[i] * a[i];
    check(late < early * .01, "Amplitude does not decay");
}
void wav() {
    for (auto duration : {.320, 15361.0 / 48000.0}) {
        auto p = kravatek::variationFromSeed(123456789);
        p.durationSeconds = duration;
        auto k = kravatek::renderKick(p);
        const auto path = std::filesystem::path("test-kick.wav");
        kravatek::writeWav24(path, k);
        std::ifstream in(path, std::ios::binary);
        std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(in)), {});
        in.close(); // Windows cannot remove a file while this stream owns it.
        check(std::string(bytes.begin(), bytes.begin() + 4) == "RIFF", "Not RIFF");
        check(le(bytes, 4, 4) + 8 == bytes.size(), "Incorrect RIFF size/padding");
        check(le(bytes, 20, 2) == 1 && le(bytes, 22, 2) == 1, "Not mono PCM");
        check(le(bytes, 24, 4) == 48000 && le(bytes, 34, 2) == 24, "Not 48 kHz / 24 bit");
        check(le(bytes, 40, 4) == k.samples.size() * 3, "Wrong data chunk length");
        for (std::size_t i = 0; i < k.samples.size(); ++i) {
            const auto raw = le(bytes, 44 + 3 * i, 3);
            const auto signedPcm = static_cast<std::int32_t>(raw) - ((raw & 0x800000U) ? 0x1000000 : 0);
            check(std::abs(static_cast<double>(signedPcm) / 8388607 - k.samples[i]) < 0.000000061,
                  "PCM does not match rendered waveform");
        }
        const std::string contents(bytes.begin(), bytes.end());
        check(contents.find("LIST") != std::string::npos && contents.find("ICMT") != std::string::npos,
              "Missing WAV metadata");
        check(contents.find(kravatek::describe(k)) != std::string::npos, "Seed/parameters not saved");
        std::filesystem::remove(path);
    }
    auto k = kravatek::renderKick({});
    k.samples[10] = std::numeric_limits<float>::quiet_NaN();
    rejects([&] { kravatek::writeWav24("invalid.wav", k); });
    bool failed = false;
    try { kravatek::writeWav24("missing-parent/test.wav", kravatek::renderKick({})); }
    catch (const std::exception&) { failed = true; }
    check(failed, "Export failure not reported");
}
void playback() {
    kravatek::Playback player;
    player.setSampleRate(std::numeric_limits<double>::max());
    player.setSampleRate(std::numeric_limits<double>::quiet_NaN());
    player.setSampleRate(48000);
    auto k = kravatek::renderKick({});
    check(player.queue(k.samples), "Unable to queue kick");
    std::array<float, 256> left {}, right {};
    float* channels[] {left.data(), right.data(), nullptr};
    std::size_t at = 0;
    while (at < k.samples.size() + 512) {
        player.process(channels, 3, 256);
        for (std::size_t i = 0; i < left.size(); ++i) {
            const auto expected = at + i < k.samples.size() ? k.samples[at + i] : 0.0f;
            check(left[i] == expected && right[i] == expected, "Playback differs from export buffer");
        }
        at += 256;
    }
    player.reset();
    check(!player.queue({}), "Empty kick accepted");
}
void retrigger() {
    kravatek::Playback player;
    player.setSampleRate(48000);
    auto k = kravatek::renderKick({});
    std::array<float, 64> audio {};
    float* channels[] {audio.data()};
    check(player.queue(k.samples), "Queue failed");
    float previous = 0;
    for (int block = 0; block < 500; ++block) {
        check(player.queue(k.samples), "Repeated PLAY exhausted slots");
        player.process(channels, 1, 64);
        for (auto sample : audio) {
            check(std::isfinite(sample) && std::abs(sample) < .708f, "Unsafe retrigger");
            check(std::abs(sample - previous) < .035f, "Retrigger click");
            previous = sample;
        }
    }
    player.reset();
    player.process(channels, 1, 64);
    check(std::all_of(audio.begin(), audio.end(), [](auto s) { return s == 0; }), "Reset did not stop audio");
}
void concurrency() {
    kravatek::Playback player;
    const auto a = kravatek::renderKick({});
    const auto b = kravatek::renderKick(kravatek::variationFromSeed(77));
    std::atomic<bool> done {false};
    std::atomic<int> accepted {0};
    std::thread producer([&] {
        for (int i = 0; i < 4000; ++i)
            if (player.queue(i % 2 == 0 ? a.samples : b.samples)) ++accepted;
        done.store(true);
    });
    std::array<float, 127> audio {};
    float* channels[] {audio.data()};
    bool valid = true;
    int tail = 0;
    while (!done.load() || tail++ < 400) {
        player.process(channels, 1, 127);
        for (auto sample : audio) valid = valid && std::isfinite(sample) && std::abs(sample) < .708f;
    }
    producer.join();
    check(valid, "Concurrent PLAY corrupted audio");
    check(accepted > 0, "No concurrent command executed");
}
}

int main(int argc, char** argv) {
    const std::map<std::string, std::function<void()>> tests {
        {"oscillator", oscillator}, {"pitch", pitch}, {"reference", reference},
        {"reproducibility", reproducibility}, {"variations", variations},
        {"sample-rates", sampleRates}, {"invalid-input", invalidInput}, {"envelope", envelope},
        {"wav", wav}, {"playback", playback}, {"retrigger", retrigger}, {"concurrency", concurrency}
    };
    try {
        if (argc != 2 || !tests.contains(argv[1])) throw std::runtime_error("Choose a named test");
        tests.at(argv[1])();
        std::cout << "PASS " << argv[1] << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
