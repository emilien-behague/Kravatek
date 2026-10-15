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
} // namespace
int main(int argc, char **argv) {
  try {
    const std::map<std::string, std::function<void()>> tests{
        {"genetic-modes", modes},   {"genetic-measures", measures},
        {"genetic-100", hundred},   {"genetic-identity", identity},
        {"genetic-prng", prngTest}, {"genetic-layers", layers}};
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
