#include "genetic/Features.h"
#include "genetic/LayerRenderer.h"
#include "genetic/UniversalDsp.h"
#include <algorithm>
#include <cmath>
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
