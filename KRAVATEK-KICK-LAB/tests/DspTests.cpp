#include "dsp/KickSynth.h"
#include "dsp/Analysis.h"
#include "dsp/Playback.h"
#include "dsp/WavWriter.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <chrono>
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

void soundFamilies() {
    std::vector<float> previous;
    const auto started = std::chrono::steady_clock::now();
    for (int style = 0; style < static_cast<int>(kravatek::KickStyle::count); ++style) {
        for (auto rate : {8000., 44100., 48000., 96000., 192000.}) {
            const auto p = kravatek::factoryPreset(static_cast<kravatek::KickStyle>(style));
            auto k = kravatek::renderKick(p, rate);
            safe(k);
            if (rate == 48000) {
                check(k.samples != previous, "Factory sounds are identical");
                check(k.samples == kravatek::renderKick(p, rate).samples, "Coloured rendering is not repeatable");
                previous = k.samples;
            }
        }
    }
    for (std::uint32_t seed = 0; seed < 32; ++seed) {
        auto p = kravatek::seededPreset(static_cast<kravatek::KickStyle>(seed % static_cast<int>(kravatek::KickStyle::count)), seed);
        safe(kravatek::renderKick(p));
    }
    std::cout << "Factory and mutation rendering ms=" << std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - started).count() << '\n';
}

void dnaLocks() {
    const auto base = kravatek::factoryPreset(kravatek::KickStyle::hardTechno);
    const auto audio = kravatek::renderKick(base).samples;
    const auto locked = kravatek::mutate(base, 929, .8, {true, true, true, true, true, true, true});
    check(kravatek::renderKick(locked).samples == audio, "DNA locks changed the sound");
    check(kravatek::renderKick(kravatek::mutate(base, 929, 0)).samples == audio, "Zero mutation changed the sound");
    const auto p = kravatek::mutate(base, 929, .8, {true, false, false, true});
    check(p.startHz == base.startHz && p.endHz == base.endHz && p.pitchDropSeconds == base.pitchDropSeconds
        && p.pitchCurve == base.pitchCurve, "Pitch DNA changed");
    check(p.clickLevel == base.clickLevel && p.clickSeconds == base.clickSeconds && p.transientSeed == base.transientSeed,
        "Transient DNA changed");
    check(p.driveDb != base.driveDb || p.ampCurve != base.ampCurve, "Unlocked DNA did not mutate");
    check(kravatek::renderKick(p).samples == kravatek::renderKick(kravatek::mutate(base, 929, .8, {true,false,false,true})).samples,
        "Mutation is not reproducible");
    rejects([&] { kravatek::mutate(base, 5, std::numeric_limits<double>::quiet_NaN()); });
    auto invalid = base; invalid.driveDb = 31;
    rejects([&] { kravatek::renderKick(invalid); });
    invalid = base; invalid.fmAmount = std::numeric_limits<double>::infinity();
    rejects([&] { kravatek::renderKick(invalid); });
}

double spectralMagnitude(const std::vector<float>& samples, double frequency, int first, int count) {
    double real = 0, imaginary = 0;
    for (int i = 0; i < count; ++i) {
        const auto window = .5 - .5 * std::cos(2 * std::numbers::pi * i / (count - 1));
        const auto phase = 2 * std::numbers::pi * frequency * i / 48000;
        real += samples[static_cast<std::size_t>(first + i)] * window * std::cos(phase);
        imaginary -= samples[static_cast<std::size_t>(first + i)] * window * std::sin(phase);
    }
    return std::hypot(real, imaginary);
}

void saturation() {
    kravatek::KickParameters p;
    p.startHz = p.endHz = 50; p.durationSeconds = .6; p.ampCurve = 1.7;
    auto clean = kravatek::renderKick(p);
    p.driveDb = 24;
    auto driven = kravatek::renderKick(p);
    safe(driven);
    const auto cleanHarmonic = spectralMagnitude(clean.samples, 150, 4800, 9600);
    const auto drivenHarmonic = spectralMagnitude(driven.samples, 150, 4800, 9600);
    check(drivenHarmonic > cleanHarmonic * 20 + 1, "Saturation has no audible harmonic effect");
    p.startHz = p.endHz = 1800; p.durationSeconds = 1; p.ampCurve = 3; p.driveDb = 30;
    auto high = kravatek::renderKick(p);
    const auto alias = spectralMagnitude(high.samples, 600, 1440, 19200);
    const auto fundamental = spectralMagnitude(high.samples, 1800, 1440, 19200);
    const auto ratio = alias / fundamental;
    std::cout << "Alias 600 Hz relative to 1800 Hz=" << 20 * std::log10(std::max(ratio, 1e-15)) << " dB\n";
    check(ratio < .002, "Saturation alias rejection regressed");
}

void parameterEffects() {
    const auto source = kravatek::factoryPreset(kravatek::KickStyle::mutant);
    const auto reference = kravatek::renderKick(source).samples;
    const std::vector<std::pair<double kravatek::KickParameters::*, double>> changes {
        {&kravatek::KickParameters::startHz, 750}, {&kravatek::KickParameters::endHz, 51},
        {&kravatek::KickParameters::pitchDropSeconds, .03}, {&kravatek::KickParameters::pitchCurve, 1.3},
        {&kravatek::KickParameters::durationSeconds, .7}, {&kravatek::KickParameters::attackSeconds, .004},
        {&kravatek::KickParameters::ampCurve, .7}, {&kravatek::KickParameters::releaseSeconds, .030},
        {&kravatek::KickParameters::driveDb, 8}, {&kravatek::KickParameters::fmAmount, .3},
        {&kravatek::KickParameters::fmSeconds, .01}, {&kravatek::KickParameters::harmonics, .05},
        {&kravatek::KickParameters::tailLevel, .1}, {&kravatek::KickParameters::tailSeconds, .05},
        {&kravatek::KickParameters::tailRatio, .7}, {&kravatek::KickParameters::tailMotion, .1},
        {&kravatek::KickParameters::filterHz, 700}, {&kravatek::KickParameters::resonance, .1},
        {&kravatek::KickParameters::filterSweep, 3}, {&kravatek::KickParameters::shape, .05},
        {&kravatek::KickParameters::clickLevel, .9}, {&kravatek::KickParameters::clickSeconds, .02},
        {&kravatek::KickParameters::subMix, .8}, {&kravatek::KickParameters::subSeconds, .05}
    };
    for (const auto& [member, value] : changes) {
        auto p = source; p.*member = value;
        const auto kick = kravatek::renderKick(p); safe(kick);
        check(kick.samples != reference, "A synthesis control has no effect");
    }
    // In a sub-only render the body/tail/filter cannot contaminate the sub layer.
    auto isolated = source; isolated.subMix = 1;
    const auto sub = kravatek::renderKick(isolated).samples;
    isolated.tailLevel = 0; isolated.filterHz = 100; isolated.fmAmount = 0; isolated.driveDb = 0;
    check(sub == kravatek::renderKick(isolated).samples, "Sub layer is affected by body or filter");
    // Non-integer ratios must use an independent phase accumulator. Multiplying
    // an already wrapped body phase creates discontinuities on each body cycle.
    kravatek::KickParameters movingTail;
    movingTail.startHz = movingTail.endHz = 50; movingTail.durationSeconds = 1;
    movingTail.tailLevel = 1; movingTail.tailMotion = 1; movingTail.tailRatio = 2; movingTail.tailSeconds = .5;
    const auto moving = kravatek::renderKick(movingTail); double maximumStep = 0;
    for (std::size_t i = 2401; i < moving.samples.size() - 100; ++i)
        maximumStep = std::max(maximumStep, std::abs(static_cast<double>(moving.samples[i] - moving.samples[i - 1])));
    std::cout << "Moving tail maximum step=" << maximumStep << '\n';
    check(maximumStep < .04, "Non-integer tail modulation introduces phase discontinuities");
}

void filterStability() {
    for (auto rate : {8000., 44100., 48000., 192000.})
        for (auto cutoff : {80., 500., 16000.})
            for (auto sweep : {-4., 4.}) {
                auto p = kravatek::factoryPreset(kravatek::KickStyle::mutant);
                p.resonance = .95; p.filterHz = cutoff; p.filterSweep = sweep;
                p.driveDb = 30; p.fmAmount = 4; p.shape = 1; p.tailMotion = 1; p.subMix = 0;
                p.durationSeconds = .150;
                safe(kravatek::renderKick(p, rate));
            }
    auto p = kravatek::factoryPreset(kravatek::KickStyle::mutant);
    p.resonance = 1; rejects([&] {kravatek::renderKick(p);});
    p.resonance = .5; p.filterHz = std::numeric_limits<double>::quiet_NaN();
    rejects([&] {kravatek::renderKick(p);});
}

void exploration() {
    auto p = kravatek::factoryPreset(kravatek::KickStyle::mutant);
    const kravatek::DnaLocks all {true,true,true,true,true,true,true};
    const auto base = kravatek::renderKick(p).samples;
    check(base == kravatek::renderKick(kravatek::evolve(p,77,1,1,all)).samples, "EVOLVE changes locked sound");
    check(base == kravatek::renderKick(kravatek::evolve(p,77,0,0)).samples, "Zero EVOLVE changes sound");
    const kravatek::DnaLocks subAttack {false,false,false,true,false,false,true};
    const auto evolved = kravatek::evolve(p,77,1,1,subAttack);
    check(evolved.endHz == p.endHz && evolved.subMix == p.subMix && evolved.subSeconds == p.subSeconds
        && evolved.durationSeconds == p.durationSeconds, "Locked sub fields changed");
    check(evolved.clickLevel == p.clickLevel && evolved.clickSeconds == p.clickSeconds
        && evolved.transientSeed == p.transientSeed, "Locked transient fields changed");
    check(evolved.tailMotion != p.tailMotion || evolved.tailRatio != p.tailRatio, "Unlocked tail did not evolve");
    const auto conventional = kravatek::generate(kravatek::KickStyle::mutant,77,0);
    const auto unusual = kravatek::generate(kravatek::KickStyle::mutant,77,1);
    check(conventional.tailMotion == 0 && conventional.shape <= .15, "Low novelty permits extreme regions");
    check(unusual.tailMotion > conventional.tailMotion, "Novelty did not open modulation regions");
    check(kravatek::renderKick(unusual).samples == kravatek::renderKick(kravatek::generate(
        kravatek::KickStyle::mutant,77,1)).samples, "GENERATE is not repeatable");
    rejects([&] {kravatek::generate(kravatek::KickStyle::deep,0,std::numeric_limits<double>::quiet_NaN());});
    for (std::uint32_t seed=0; seed<24; ++seed)
        safe(kravatek::renderKick(kravatek::generate(static_cast<kravatek::KickStyle>(seed % 8), seed, seed / 23.0)));
}

void analysis() {
    std::vector<float> tone(8192);
    for (std::size_t i=0;i<tone.size();++i) tone[i]=static_cast<float>(.5*std::sin(2*std::numbers::pi*16*static_cast<double>(i)/8192));
    const auto a=kravatek::analyse(tone,48000);
    check(std::abs(a.peakDb+6.0206)<.001 && std::abs(a.rmsDb+9.0309)<.001, "Analysis peak/RMS calibration failed");
    check(std::abs(a.dc)<1e-6 && std::abs(a.durationSeconds-8192.0/48000)<1e-9, "DC/duration measurement failed");
    const auto maximum=*std::max_element(a.spectrumDb.begin(),a.spectrumDb.end());
    check(std::abs(maximum+6.0206)<.02, "Spectrum dBFS calibration failed");
    const auto match=kravatek::comparisonRms(tone,48000);
    check(std::abs(match-std::sqrt(.125*8192/19200))<1e-6, "A/B window is not silence padded");
    const std::array<float,1> one {0}; const auto tiny=kravatek::analyse(one,48000);
    check(std::all_of(tiny.spectrumDb.begin(),tiny.spectrumDb.end(),[](auto x){return std::isfinite(x);}), "Tiny analysis input is invalid");
    tone[3]=std::numeric_limits<float>::quiet_NaN(); rejects([&]{kravatek::analyse(tone,48000);});
}

void looping() {
    kravatek::Playback player;
    const auto k = kravatek::renderKick({});
    check(player.queue(k.samples, 24000), "Unable to start tempo preview");
    std::array<float, 125> audio {};
    float* channels[] {audio.data()};
    for (std::size_t offset = 0; offset < 72000; offset += audio.size()) {
        player.process(channels, 1, static_cast<int>(audio.size()));
        for (std::size_t i = 0; i < audio.size(); ++i) {
            const auto phase = (offset + i) % 24000;
            const auto expected = phase < k.samples.size() ? k.samples[phase] : 0.0f;
            check(audio[i] == expected, "Loop has incorrect tempo or sample ordering");
        }
    }
    player.stop();
    for (int i = 0; i < 8; ++i) player.process(channels, 1, static_cast<int>(audio.size()));
    check(std::all_of(audio.begin(), audio.end(), [](auto x) {return x == 0;}), "STOP left audio running");
    check(player.queue(k.samples, 4800), "Unable to restart tempo preview");
    float previous = 0;
    for (int block = 0; block < 160; ++block) {
        player.process(channels, 1, static_cast<int>(audio.size()));
        for (auto sample : audio) {
            check(std::isfinite(sample) && std::abs(sample - previous) < .04f, "Tempo retrigger click");
            previous = sample;
        }
    }
}
}

int main(int argc, char** argv) {
    const std::map<std::string, std::function<void()>> tests {
        {"oscillator", oscillator}, {"pitch", pitch}, {"reference", reference},
        {"reproducibility", reproducibility}, {"variations", variations},
        {"sample-rates", sampleRates}, {"invalid-input", invalidInput}, {"envelope", envelope},
        {"wav", wav}, {"playback", playback}, {"retrigger", retrigger}, {"concurrency", concurrency},
        {"analysis", analysis}, {"parameter-effects", parameterEffects}, {"filter-stability", filterStability}, {"exploration", exploration},
        {"sound-families", soundFamilies}, {"dna-locks", dnaLocks}, {"saturation", saturation}, {"looping", looping}
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
