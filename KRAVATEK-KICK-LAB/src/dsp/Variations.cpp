#include "KickSynth.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace kravatek {
const char* styleName(KickStyle style) noexcept {
    switch (style) {
        case KickStyle::reference: return "01 - Pure foundation";
        case KickStyle::hardTechno: return "02 - Warehouse punch";
        case KickStyle::psy: return "03 - HARD PSY / Pressure";
        case KickStyle::schranz: return "04 - SCHRANZ / Steel";
        case KickStyle::hardcore: return "05 - Concrete impact";
        case KickStyle::deep: return "06 - DEEP / Machinery";
        case KickStyle::experimental: return "07 - EXPERIMENTAL / Liquid relay";
        case KickStyle::mutant: return "08 - MUTANT / Reactor";
        default: return "Custom";
    }
}

KickParameters factoryPreset(KickStyle style) {
    KickParameters p;
    switch (style) {
        case KickStyle::reference: break;
        case KickStyle::hardTechno:
            p.startHz = 380; p.endHz = 48; p.pitchDropSeconds = .045; p.durationSeconds = .410;
            p.attackSeconds = .0007; p.releaseSeconds = .014; p.pitchCurve = .75; p.ampCurve = 1.45;
            p.harmonics = .28; p.fmAmount = .65; p.driveDb = 14; p.subMix = .28; p.clickLevel = .22;
            break;
        case KickStyle::psy:
            p.startHz = 950; p.endHz = 54; p.pitchDropSeconds = .024; p.durationSeconds = .190;
            p.attackSeconds = .00035; p.releaseSeconds = .007; p.pitchCurve = .68; p.ampCurve = 1.12;
            p.fmAmount = .28; p.driveDb = 5; p.subMix = .45; p.clickLevel = .16; p.clickSeconds = .0015;
            break;
        case KickStyle::schranz:
            p.startHz = 510; p.endHz = 47; p.pitchDropSeconds = .065; p.durationSeconds = .460;
            p.attackSeconds = .0006; p.releaseSeconds = .018; p.pitchCurve = .85; p.ampCurve = 1.6;
            p.harmonics = .7; p.fmAmount = 1.7; p.driveDb = 22; p.subMix = .12;
            p.clickLevel = .34; p.clickSeconds = .007; p.shape = .2; p.filterHz = 7800; p.tailLevel = .25; p.tailSeconds = .17; p.tailMotion = .25;
            break;
        case KickStyle::hardcore:
            p.startHz = 1250; p.endHz = 58; p.pitchDropSeconds = .035; p.durationSeconds = .560;
            p.attackSeconds = .0003; p.releaseSeconds = .020; p.pitchCurve = .7; p.ampCurve = 1.7;
            p.harmonics = .48; p.fmAmount = 2.3; p.driveDb = 26; p.subMix = .08;
            p.clickLevel = .32; p.clickSeconds = .003;
            break;
        case KickStyle::deep:
            p.startHz = 155; p.endHz = 38; p.pitchDropSeconds = .095; p.durationSeconds = .720;
            p.attackSeconds = .0018; p.releaseSeconds = .025; p.pitchCurve = 1.05; p.ampCurve = 1.55;
            p.harmonics = .06; p.driveDb = 6; p.subMix = .65; p.clickLevel = .04;
            break;
        case KickStyle::experimental:
            p.startHz = 620; p.endHz = 46; p.pitchDropSeconds = .090; p.durationSeconds = .850;
            p.attackSeconds = .001; p.ampCurve = 1.4; p.fmAmount = 1.8; p.fmSeconds = .180;
            p.driveDb = 10; p.shape = .25; p.tailLevel = .65; p.tailSeconds = .260;
            p.tailRatio = 1.7; p.tailMotion = .7; p.filterHz = 950; p.filterSweep = 3; p.resonance = .68;
            p.subMix = .32; p.subSeconds = .210; p.clickLevel = .10;
            break;
        case KickStyle::mutant:
            p.startHz = 1350; p.endHz = 42; p.pitchDropSeconds = .075; p.durationSeconds = .930;
            p.attackSeconds = .0004; p.ampCurve = 1.7; p.harmonics = .7;
            p.fmAmount = 3; p.fmSeconds = .120; p.driveDb = 24; p.shape = .65;
            p.tailLevel = .85; p.tailSeconds = .350; p.tailRatio = 2.3; p.tailMotion = .93;
            p.filterHz = 2400; p.filterSweep = -1.8; p.resonance = .78;
            p.subMix = .18; p.subSeconds = .180; p.clickLevel = .3;
            break;
        default: throw std::invalid_argument("Unknown factory preset");
    }
    return p;
}

KickParameters mutate(const KickParameters& source, std::uint32_t seed, double amount, DnaLocks locks) {
    validate(source, 48000);
    if (!std::isfinite(amount) || amount < 0 || amount > 1)
        throw std::invalid_argument("Mutation amount must be 0..1");
    auto p = source;
    p.seed = seed;
    if (amount == 0) return p;
    auto state = seed;
    const auto random = [&]() {
        state += 0x9e3779b9U;
        auto z = state;
        z = (z ^ (z >> 16U)) * 0x21f0aaadU;
        z = (z ^ (z >> 15U)) * 0x735a2d97U;
        z ^= z >> 15U;
        return static_cast<double>(z) / 2147483648.0 - 1.0;
    };
    const auto offset = [&](double value, double span, double low, double high) {
        return std::clamp(value + random() * span * amount, low, high);
    };
    if (!locks.pitch) {
        if (!locks.sub) p.endHz = std::round(offset(p.endHz, 16, 30, 95) * 10) / 10;
        p.startHz = offset(p.startHz, 650, std::max(120.0, p.endHz), 1600);
        p.pitchDropSeconds = offset(p.pitchDropSeconds, .080, .005, .180);
        p.pitchCurve = offset(p.pitchCurve, .7, .4, 2.4);
    }
    if (!locks.amplitude) {
        if (!locks.sub) p.durationSeconds = std::round(offset(p.durationSeconds, .350, .080, 1.0) * 1000) / 1000;
        p.attackSeconds = offset(p.attackSeconds, .003, .0001, .006);
        p.releaseSeconds = offset(p.releaseSeconds, .016, .002, std::min(.035, p.durationSeconds / 4));
        p.ampCurve = offset(p.ampCurve, .65, .45, 2.3);
    }
    if (!locks.tone) {
        p.harmonics = offset(p.harmonics, .7, 0, 1);
        p.fmAmount = offset(p.fmAmount, 2.5, 0, 4);
        p.driveDb = offset(p.driveDb, 16, 0, 30);
        p.fmSeconds = offset(p.fmSeconds, .12, .005, .5);
    }
    if (!locks.sub) {
        p.subMix = offset(p.subMix, .6, 0, 1);
        p.subSeconds = offset(p.subSeconds, .25, .02, 1);
    }
    if (!locks.tail) {
        p.tailLevel = offset(p.tailLevel, .45, 0, 1);
        p.tailSeconds = offset(p.tailSeconds, .3, .02, 1);
        p.tailRatio = offset(p.tailRatio, 1.8, .5, 4);
        p.tailMotion = offset(p.tailMotion, .6, 0, 1);
    }
    if (!locks.filter) {
        p.filterHz = std::clamp(p.filterHz * std::pow(2.0, random() * 3 * amount), 80.0, 16000.0);
        p.resonance = offset(p.resonance, .6, 0, .95);
        p.filterSweep = offset(p.filterSweep, 3, -4, 4);
        p.shape = offset(p.shape, .6, 0, 1);
    }
    if (!locks.transient) {
        p.clickLevel = offset(p.clickLevel, .45, 0, 1);
        p.clickSeconds = offset(p.clickSeconds, .009, .0005, .020);
        p.transientSeed = seed;
    }
    validate(p, 48000);
    return p;
}

KickParameters evolve(const KickParameters& source, std::uint32_t seed, double amount,
                      double novelty, DnaLocks locks) {
    if (!std::isfinite(novelty) || novelty < 0 || novelty > 1)
        throw std::invalid_argument("Novelty must be 0..1");
    auto p = mutate(source, seed, amount, locks);
    if (amount == 0) return p;
    // At low novelty, evolve stays near conventional parameter regions. This is a
    // search bias, not an objective score of musical originality.
    if (!locks.tail) {
        p.tailLevel = std::min(p.tailLevel, .2 + .8 * novelty);
        p.tailMotion = std::min(p.tailMotion, novelty);
        p.tailRatio = 1 + (p.tailRatio - 1) * (.2 + .8 * novelty);
    }
    if (!locks.filter) {
        p.shape = std::min(p.shape, .15 + .85 * novelty);
        p.resonance = std::min(p.resonance, .45 + .5 * novelty);
    }
    if (!locks.tone) p.fmSeconds = std::min(p.fmSeconds, .045 + .455 * novelty);
    return p;
}

KickParameters generate(KickStyle style, std::uint32_t seed, double novelty) {
    return evolve(factoryPreset(style), seed, .30 + .65 * novelty, novelty);
}

KickParameters seededPreset(KickStyle style, std::uint32_t seed) {
    auto p = factoryPreset(style);
    if (seed == 1) return p;
    return mutate(p, seed, .55);
}
} // namespace kravatek
