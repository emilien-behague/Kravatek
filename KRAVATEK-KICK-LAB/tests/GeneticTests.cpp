#include "genetic/Features.h"
#include "genetic/Generator.h"
#include "genetic/Genome.h"
#include "genetic/LayerRenderer.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <numbers>
#include <numeric>
#include <set>
#include <stdexcept>
using namespace kravatek::genetic;
namespace {
void check(bool v, const char *text) {
  if (!v)
    throw std::runtime_error(text);
}
template <class F> void rejects(F f) {
  bool bad = false;
  try {
    f();
  } catch (const std::invalid_argument &) {
    bad = true;
  }
  check(bad, "Invalid genetic data accepted");
}
void safe(const Render &r) {
  check(!r.mix.empty(), "Empty genetic render");
  double p = 0, sum = 0;
  for (auto v : r.mix) {
    check(std::isfinite(v), "Nonfinite genetic sample");
    p = std::max(p, std::abs(static_cast<double>(v)));
    sum += v;
  }
  check(p <= .891251, "Uncontrolled peak");
  check(p > .001, "Silent genetic render");
  check(std::abs(sum / static_cast<double>(r.mix.size())) < 1e-7,
        "Genetic DC offset");
  check(r.mix.front() == 0 && r.mix.back() == 0, "Genetic endpoints click");
}
void identity() {
  check(sha256("") ==
            "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
        "SHA256 empty vector");
  check(sha256("abc") ==
            "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
        "SHA256 abc vector");
  auto a = foundation(Mode::deep);
  auto b = a;
  identify(b);
  check(a.id == b.id, "Unstable ID");
  b.body.drive += 1;
  identify(b);
  check(a.id != b.id, "Genes do not affect ID");
  b = a;
  b.engine = "future";
  rejects([&] { render(b); });
  b = a;
  b.body.resonance = 1;
  rejects([&] { render(b); });
  b = a;
  b.novelty = std::numeric_limits<double>::quiet_NaN();
  rejects([&] { render(b); });
}
void prngTest() {
  Random a(42), b(42), other(43);
  bool different = false;
  for (int i = 0; i < 100; ++i) {
    auto x = a.next();
    check(x == b.next(), "PRNG not reproducible");
    different |= x != other.next();
  }
  check(different, "PRNG seeds identical");
}
void layers() {
  for (int m = 0; m < 5; ++m)
    for (auto rate : {44100., 48000.}) {
      auto g = foundation(static_cast<Mode>(m));
      auto r = render(g, rate);
      safe(r);
      check(r.mix == render(g, rate).mix, "Layer rendering not deterministic");
      for (std::size_t i = 0; i < 4; ++i) {
        check(r.layers[i].front() == 0 && r.layers[i].back() == 0,
              "Layer edge");
        auto isolated = g;
        isolated.body.drive = 30;
        isolated.tail.motion = 1;
        if (i < 2)
          check(r.layers[i] == render(isolated, rate).layers[i],
                "Layer depends on other genes");
      }
    }
}

void modes() {
  for (int m = 0; m < 5; ++m)
    for (auto novelty : {0., .3, .31, .7, .71, 1.})
      for (std::uint32_t seed = 0; seed < 20; ++seed) {
        const auto g = generateGenome(static_cast<Mode>(m), seed, novelty);
        validate(g);
        check(g == generateGenome(static_cast<Mode>(m), seed, novelty),
              "Generation parameters not repeatable");
      }
  auto a = generateGenome(Mode::experimental, 42, .75),
       b = generateGenome(Mode::experimental, 43, .75);
  check(a.id != b.id && render(a).mix != render(b).mix,
        "Distinct seeds fail to change signal");
  const auto deep = generateGenome(Mode::deep, 71, 1),
             psy = generateGenome(Mode::hardPsy, 71, 1);
  check(deep.sub.duration > psy.sub.duration && deep.sub.decay > psy.sub.decay,
        "Mode duration logic collapsed");
  const auto quiet = generateGenome(Mode::experimental, 91, 0),
             wild = generateGenome(Mode::experimental, 91, 1);
  check(featureDistance(measure(render(quiet).mix, 48000),
                        measure(render(wild).mix, 48000)) > .02,
        "Novelty has no measurable effect");
  rejects([] { generateGenome(Mode::deep, 1, 1.01); });
}
void measures() {
  for (auto rate : {44100., 48000.}) {
    std::vector<float> tone(static_cast<std::size_t>(rate));
    for (std::size_t i = 0; i < tone.size(); ++i)
      tone[i] =
          static_cast<float>(.5 * std::sin(2 * std::numbers::pi * 50 *
                                           static_cast<double>(i) / rate));
    const auto f = measure(tone, rate);
    check(std::abs(f.basic.peakDb + 6.0206) < .001, "Feature peak calibration");
    check(std::abs(f.basic.rmsDb + 9.0309) < .001, "Feature RMS calibration");
    check(std::abs(f.fundamentalHz - 50) < .5 && f.fundamentalConfidence > .95,
          "F0 estimate calibration");
    check(f.bandEnergy[0] > .99 && f.bandEnergy[1] < .01,
          "Low-band energy calibration");
    check(std::abs(f.centroidHz - 50) < 3, "Spectral centroid calibration");
    check(std::abs(f.effectiveSeconds - 1) < .001,
          "Effective duration calibration");
    check(std::abs(similarity(tone, f, tone, f) - 1) < 1e-9,
          "Self similarity is not one");
    check(featureDistance(f, f) == 0, "Self feature distance is not zero");
    for (std::size_t i = 0; i < tone.size(); ++i)
      tone[i] =
          static_cast<float>(.5 * std::sin(2 * std::numbers::pi * 4000 *
                                           static_cast<double>(i) / rate));
    auto high = measure(tone, rate);
    check(high.bandEnergy[2] > .99 && high.centroidHz > 3900,
          "High band calibration");
    check(featureDistance(f, high) > .2,
          "Feature metric cannot distinguish low/high tones");
    std::fill(tone.begin(), tone.end(), 0);
    auto silent = measure(tone, rate);
    check(silent.fundamentalHz == 0 && silent.basic.rmsDb == -120 &&
              silent.centroidHz == 0,
          "Silence features invalid");
  }
}
void hundred() {
  const auto start = std::chrono::steady_clock::now();
  std::set<std::string> ids, audio;
  std::set<int> bodies, tails;
  std::vector<Features> features;
  double minRms = 0, maxRms = -120, maxPeak = -120, maxStep = 0;
  int safetyReductions = 0;
  for (std::uint32_t i = 0; i < 100; ++i) {
    const auto g = generateGenome(static_cast<Mode>(i % 5), 0xfeed0000U + i, 1);
    ids.insert(g.id);
    bodies.insert(static_cast<int>(g.body.kind));
    tails.insert(static_cast<int>(g.tail.kind));
    for (auto rate : {44100., 48000.}) {
      auto r = render(g, rate, false);
      safe(r);
      safetyReductions += r.safetyGain < 1;
      if (rate == 48000) {
        const auto f = measure(r.mix, rate);
        features.push_back(f);
        minRms = std::min(minRms, f.basic.rmsDb);
        maxRms = std::max(maxRms, f.basic.rmsDb);
        maxPeak = std::max(maxPeak, f.basic.peakDb);
        audio.insert(
            sha256(std::string(reinterpret_cast<const char *>(r.mix.data()),
                               r.mix.size() * sizeof(float))));
        for (std::size_t j = 1; j < r.mix.size(); ++j)
          maxStep = std::max(
              maxStep, std::abs(static_cast<double>(r.mix[j] - r.mix[j - 1])));
      }
    }
  }
  double total = 0, maximum = 0;
  std::size_t pairs = 0;
  for (std::size_t i = 0; i < features.size(); ++i)
    for (std::size_t j = 0; j < i; ++j) {
      const auto d = featureDistance(features[i], features[j]);
      total += d;
      maximum = std::max(maximum, d);
      ++pairs;
    }
  const auto mean = total / static_cast<double>(pairs);
  check(ids.size() == 100 && audio.size() == 100,
        "Fixed-seed population contains exact duplicates");
  check(bodies.size() >= 3 && tails.size() >= 3,
        "Population does not vary architectures");
  check(mean > .035 && maximum > .2,
        "Population has collapsed acoustic diversity");
  check(maxRms - minRms > 3, "Automatic normalisation hides level differences");
  const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                      std::chrono::steady_clock::now() - start)
                      .count();
  std::ofstream report("genetic-100-report.json");
  report << "{\n  \"candidates\":100, \"sampleRates\":[44100,48000], "
            "\"renders\":200,\n  \"uniqueIds\":"
         << ids.size() << ", \"uniqueAudio48000\":" << audio.size()
         << ",\n  \"bodyArchitectures\":" << bodies.size()
         << ", \"tailArchitectures\":" << tails.size()
         << ",\n  \"meanFeatureDistance\":" << mean
         << ", \"maxFeatureDistance\":" << maximum
         << ",\n  \"minRmsDb\":" << minRms << ", \"maxRmsDb\":" << maxRms
         << ", \"maxPeakDb\":" << maxPeak
         << ",\n  \"maxAdjacentStep\":" << maxStep
         << ", \"safetyReductions\":" << safetyReductions
         << ", \"elapsedMs\":" << ms
         << ",\n  \"musicalQualityVerified\":false\n}\n";
  check(report.good(), "Cannot save population report");
  std::cout << "100 candidates / 200 renders: mean distance=" << mean
            << " RMS range=" << minRms << ".." << maxRms
            << " dBFS, elapsed=" << ms << " ms\n";
}

void exactLocks() {
  const auto parent = generateGenome(Mode::experimental, 4444, .75),
             original = parent;
  for (auto rate : {44100., 48000.}) {
    const auto before = render(parent, rate);
    for (std::uint32_t i = 0; i < 20; ++i) {
      const auto child = evolveGenome(parent, Mode::mutant, 500 + i, 1,
                                      {true, true, false, false});
      check(child.sub == parent.sub && child.attack == parent.attack,
            "Locked layer parameters changed");
      const auto after = render(child, rate);
      check(before.layers[0] == after.layers[0], "Locked SUB audio changed");
      check(before.layers[1] == after.layers[1], "Locked ATTACK audio changed");
      check(parent == original, "Evolution overwrote its parent");
    }
    const auto all =
        evolveGenome(parent, Mode::mutant, 555, 1, {true, true, true, true});
    check(render(all, rate).mix == before.mix,
          "All locked layers changed the mix");
    const auto held =
        evolveGenome(parent, Mode::deep, 558, 1, {false, false, true, true});
    const auto heldAudio = render(held, rate);
    check(heldAudio.layers[2] == before.layers[2] &&
              heldAudio.layers[3] == before.layers[3],
          "BODY/TAIL locks changed their audio");
    const auto zero = evolveGenome(parent, Mode::mutant, 559, 1, {}, 0);
    check(render(zero, rate).mix == before.mix, "Zero evolution changes audio");
  }
  const auto subtle = evolveGenome(parent, Mode::experimental, 57, .75, {}, .1);
  check(subtle.body.kind == parent.body.kind &&
            subtle.body.seed == parent.body.seed,
        "Refinement changes architecture");
  check(subtle.parents.size() == 1 && subtle.parents[0] == parent.id &&
            subtle.generation == parent.generation + 1,
        "Parent provenance missing");
}

void geneticSpectrum() {
  auto g = foundation(Mode::deep);
  g.sub.level = g.attack.level = g.tail.level = 0;
  g.body.kind = BodyKind::sine;
  g.body.startHz = g.body.pivotHz = 1800;
  g.body.endHz = 150;
  g.body.pivotTime = .12;
  g.body.settleTime = .3;
  g.body.duration = 1;
  g.body.ampCurve = 2.3;
  g.body.drive = 30;
  g.body.fold = 0;
  g.body.cutoff = 16000;
  g.body.resonance = 0;
  g.body.sweep = 0;
  const auto r = render(g);
  const auto magnitude = [&](double hz) {
    double re = 0, im = 0;
    constexpr int first = 1536, count = 3456;
    for (int i = 0; i < count; ++i) {
      const auto w = .5 - .5 * std::cos(2 * std::numbers::pi * i / (count - 1));
      const auto a = 2 * std::numbers::pi * hz * i / 48000;
      re += r.mix[static_cast<std::size_t>(first + i)] * w * std::cos(a);
      im -= r.mix[static_cast<std::size_t>(first + i)] * w * std::sin(a);
    }
    return std::hypot(re, im);
  };
  const auto ratio = magnitude(600) / magnitude(1800);
  std::cout << "Genetic saturation alias 600/1800="
            << 20 * std::log10(std::max(1e-15, ratio)) << " dB\n";
  check(ratio < .0056, "Genetic oversampling alias regression");
}
} // namespace
int main(int argc, char **argv) {
  try {
    const std::map<std::string, std::function<void()>> tests{
        {"genetic-spectrum", geneticSpectrum},
        {"genetic-locks", exactLocks},
        {"genetic-modes", modes},
        {"genetic-measures", measures},
        {"genetic-100", hundred},
        {"genetic-identity", identity},
        {"genetic-prng", prngTest},
        {"genetic-layers", layers}};
    if (argc != 2 || !tests.contains(argv[1]))
      throw std::runtime_error("Choose genetic test");
    tests.at(argv[1])();
    std::cout << "PASS " << argv[1] << '\n';
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "FAIL " << e.what() << '\n';
    return 1;
  }
}
