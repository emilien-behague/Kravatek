#include "dsp/Playback.h"
#include "dsp/WavWriter.h"
#include "genetic/Features.h"
#include "genetic/LayerRenderer.h"
#include "genetic/Profile.h"
#include "genetic/UniversalDsp.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <functional>
#include <iostream>
#include <map>
#include <numbers>
#include <set>
#include <stdexcept>
using namespace kravatek::genetic;
namespace {
void check(bool v, const char *m) {
  if (!v)
    throw std::runtime_error(m);
}
void stable(const Render &r) {
  check(r.mix.size() >= 384, "Empty universal render");
  double mean = 0, peak = 0;
  for (float x : r.mix) {
    check(std::isfinite(x), "Nonfinite universal sample");
    peak = std::max(peak, std::abs(double(x)));
    mean += x;
  }
  check(peak > .00001 && peak < .892, "Invalid universal peak");
  check(std::abs(mean / r.mix.size()) < 2e-7, "Universal DC offset");
  check(r.mix.front() == 0 && r.mix.back() == 0, "Nonzero boundaries");
}
void architectures() {
  std::set<std::string> hashes;
  for (int k = 0; k < 6; ++k) {
    auto g = universalFoundation(static_cast<Architecture>(k));
    for (auto rate : {44100., 48000.}) {
      auto r = render(g, rate);
      stable(r);
      check(r.mix == render(g, rate).mix,
            "Nondeterministic universal architecture");
      if (rate == 48000)
        hashes.insert(
            sha256(std::string(reinterpret_cast<const char *>(r.mix.data()),
                               r.mix.size() * sizeof(float))));
    }
  }
  check(hashes.size() == 6, "Architecture audio duplicated");
  auto g = universalFoundation(Architecture::acoustic);
  g.sub.level = g.attack.level = g.tail.level = 0;
  const auto modal = render(g).mix;
  g.body.inharmonicity = .9;
  check(render(g).mix != modal, "Modal spread inactive");
  g.body.damping = .1;
  check(render(g).mix != modal, "Modal damping inactive");
}
void distortion() {
  std::set<std::string> hashes;
  for (int k = 0; k < 8; ++k) {
    auto g = universalFoundation(Architecture::distortedHard);
    g.body.fx.kind = static_cast<DistortionKind>(k);
    g.body.fx.driveMotion = 1;
    g.body.fx.bits = 5;
    g.body.fx.holdHz = 1500;
    for (auto rate : {44100., 48000.}) {
      auto r = render(g, rate);
      stable(r);
      if (rate == 48000)
        hashes.insert(
            sha256(std::string(reinterpret_cast<const char *>(r.mix.data()),
                               r.mix.size() * sizeof(float))));
    }
  }
  check(hashes.size() == 8, "Distortion models are aliases");
  auto tail = universalFoundation(Architecture::tonal);
  tail.body.level = tail.sub.level = tail.attack.level = 0;
  tail.tail.fx.kind = DistortionKind::tanh;
  const auto plain = render(tail).layers[3];
  tail.tail.fx.kind = DistortionKind::rectify;
  tail.tail.fx.bias = .4;
  const auto rectified = render(tail).layers[3];
  double difference = 0, energy = 0;
  for (std::size_t i = 0; i < plain.size(); ++i) {
    difference += std::pow(plain[i] - rectified[i], 2);
    energy += plain[i] * plain[i];
  }
  check(difference / energy > .01, "Tail rectification is only a renamed tanh");
  tail.tail.fx.bias = .1;
  check(render(tail).layers[3] != rectified, "Tail rectification mix inactive");
  auto g = universalFoundation(Architecture::distortedHard);
  const auto before = render(g).layers[0];
  g.body.fx.multiband = 1;
  g.body.fx.lowDrive = 20;
  g.body.fx.highDrive = 30;
  g.tail.fx.kind = DistortionKind::asymmetric;
  auto r = render(g);
  check(r.layers[0] == before, "Body multiband affects isolated sub");
  stable(r);
  for (auto &f : {std::ref(g.body.fx), std::ref(g.tail.fx)}) {
    f.get().preCut = 80;
    f.get().postCut = 80;
    f.get().stageDrive = 24;
    f.get().bias = .8;
    f.get().crossover = 800;
    f.get().bits = 4;
    f.get().holdHz = 500;
  }
  stable(render(g));
}
void spectrum() {
  auto g = universalFoundation(Architecture::analog);
  g.sub.level = g.attack.level = g.tail.level = 0;
  g.body.kind = BodyKind::sine;
  g.body.startHz = g.body.pivotHz = 1800;
  g.body.endHz = 150;
  g.body.pivotTime = .12;
  g.body.settleTime = .3;
  g.body.duration = 1;
  g.body.drive = 30;
  g.body.fold = 0;
  g.body.cutoff = 16000;
  g.body.resonance = 0;
  g.body.sweep = 0;
  g.body.fx.preCut = g.body.fx.postCut = 0;
  g.body.fx.kind = DistortionKind::tanh;
  auto r = render(g);
  const auto mag = [&](double hz) {
    double re = 0, im = 0;
    for (int i = 0; i < 3456; ++i) {
      const auto w = .5 - .5 * std::cos(2 * std::numbers::pi * i / 3455),
                 a = 2 * std::numbers::pi * hz * i / 48000;
      const auto v = r.mix[static_cast<std::size_t>(1536 + i)] * w;
      re += v * std::cos(a);
      im -= v * std::sin(a);
    }
    return std::hypot(re, im);
  };
  const auto ratio = mag(600) / mag(1800);
  std::cout << "UKE saturation alias 600/1800=" << 20 * std::log10(ratio)
            << " dB\n";
  check(ratio < .0056, "Universal oversampling regression");
  // Every distortion control has an audible-domain effect in an active context.
  for (const auto &[name, value] :
       std::vector<std::pair<std::string, double>>{{"stageDrive", 20},
                                                   {"preCut", 400},
                                                   {"postCut", 900},
                                                   {"bias", -.7},
                                                   {"multiband", 1},
                                                   {"lowDrive", 18},
                                                   {"highDrive", 28},
                                                   {"driveDecay", .7},
                                                   {"driveMotion", 1},
                                                   {"bits", 4},
                                                   {"holdHz", 1800},
                                                   {"crossover", 600}}) {
    auto base = universalFoundation(Architecture::distortedHard);
    base.body.fx.driveMotion = .6;
    base.body.fx.kind = (name == "bits" || name == "holdHz")
                            ? DistortionKind::digital
                            : DistortionKind::asymmetric;
    auto before = render(base).layers[2];
    parameterSpec("body.fx." + name).value(base) = value;
    check(render(base).layers[2] != before, "Universal FX parameter inactive");
  }
}
void longBuffer() {
  auto g = universalFoundation(Architecture::tonal);
  g.sub.duration = 4;
  g.sub.decay = 2;
  g.body.level = g.attack.level = g.tail.level = 0;
  auto r = render(g, 192000, false);
  check(r.mix.size() == 768000, "Long render truncated");
  stable(r);
  kravatek::Playback p;
  check(p.queue(r.mix), "Long playback buffer rejected");
  kravatek::writePcm24("long-universal.wav", r.mix, 192000, "test");
  check(std::filesystem::file_size("long-universal.wav") > r.mix.size() * 3,
        "Long WAV truncated");
  std::filesystem::remove("long-universal.wav");
}
void tuning() {
  auto g = universalFoundation(Architecture::tonal);
  g.body.level = g.attack.level = g.tail.level = 0;
  g.sub.duration = 3;
  g.sub.decay = 1.5;
  g.sub.attack = .004;
  g.sub.release = .04;
  tune(g, 33);
  check(std::abs(g.sub.endHz - 55) < 1e-9, "MIDI tuning incorrect");
  for (auto rate : {44100., 48000.}) {
    auto r = render(g, rate);
    stable(r);
    auto f = measure(r.mix, rate);
    check(std::abs(f.fundamentalHz - 55) < 1.5, "Tuned fundamental incorrect");
  }
  for (int note : {24, 60}) {
    auto high = universalFoundation(Architecture::tonal);
    high.body.level = high.attack.level = high.tail.level = 0;
    high.sub.duration = .8;
    high.sub.decay = .3;
    tune(high, note);
    for (auto rate : {44100., 48000.}) {
      auto signal = render(high, rate);
      auto f = measure(signal.mix, rate, true);
      check(std::abs(f.fundamentalHz - midiFrequency(note)) < 2,
            "Universal note estimator outside low bass range");
    }
  }
  const auto oldSub = g.sub;
  tune(g, 45, {true, false, false, false});
  check(g.sub == oldSub, "Tuning ignores sub lock");
  g.sub.endHz = 35;
  g.sub.glideTime = .5;
  stable(render(g));
  auto before = g.id;
  g.sub.phase = 1.2;
  identify(g);
  check(g.id != before, "Phase absent from identity");
}
void versioned() {
  auto old = foundation(Mode::schranz);
  const auto before = render(old);
  auto g = old;
  upgrade(g);
  g.body = universalFoundation(Architecture::acoustic).body;
  identify(g);
  const auto after = render(g);
  check(before.layers[0] == after.layers[0] &&
            before.layers[1] == after.layers[1] &&
            before.layers[3] == after.layers[3],
        "Mixed-version layers changed");
  check(before.layers[2] != after.layers[2], "New layer renderer inactive");
}
} // namespace
int main(int argc, char **argv) {
  try {
    std::map<std::string, std::function<void()>> t{
        {"universal-spectrum", spectrum},
        {"universal-long-buffer", longBuffer},
        {"universal-architectures", architectures},
        {"universal-distortion", distortion},
        {"universal-tuning", tuning},
        {"universal-versioned-layers", versioned}};
    if (argc != 2)
      throw std::invalid_argument("Choose test");
    t.at(argv[1])();
    std::cout << "PASS " << argv[1] << '\n';
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
