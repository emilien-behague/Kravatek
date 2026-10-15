#include "UniversalDsp.h"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>
namespace kravatek::genetic {
namespace {
double saturation(double x, double db) {
  const auto g = std::pow(10., db / 20);
  return std::tanh(x * g) / std::tanh(g);
}
} // namespace
DistortionChain::DistortionChain(const FxGenes &g, double rate)
    : genes_(g), rate_(rate),
      preAlpha_(1 - std::exp(-2 * std::numbers::pi * g.preCut / rate)),
      postAlpha_(1 - std::exp(-2 * std::numbers::pi * g.postCut / rate)),
      splitAlpha_(1 - std::exp(-2 * std::numbers::pi * g.crossover / rate)) {}
double DistortionChain::process(double input, double t, double drive,
                                double fold) {
  const auto &p = genes_;
  if (p.preCut > 0) {
    pre_ += preAlpha_ * (input - pre_);
    input = pre_;
  }
  const auto gain = std::pow(
      10., (drive * (1 + p.driveMotion * std::exp(-t / p.driveDecay))) / 20);
  const auto x = input * gain;
  double y = 0;
  switch (p.kind) {
  case DistortionKind::soft:
    y = std::atan(x) / std::atan(gain);
    break;
  case DistortionKind::hard:
    y = std::clamp(x, -1., 1.);
    break;
  case DistortionKind::tanh:
    y = std::tanh(x) / std::tanh(gain);
    break;
  case DistortionKind::polynomial: {
    const auto v = std::clamp(x, -1., 1.);
    y = 1.5 * (v - v * v * v / 3);
    break;
  }
  case DistortionKind::foldback:
    y = std::sin(x * (1 + fold * 2));
    break;
  case DistortionKind::asymmetric:
    y = (std::tanh(x + p.bias) - std::tanh(p.bias)) /
        (1 + std::abs(p.bias) * .2);
    break;
  case DistortionKind::rectify: {
    const auto amount = std::clamp(std::abs(p.bias) / .8, 0., 1.);
    const auto mix = amount + (1 - amount) * fold;
    y = std::tanh((1 - mix) * x + mix * std::abs(x));
  } break;
  case DistortionKind::digital: {
    clock_ += p.holdHz / rate_;
    if (clock_ >= 1) {
      clock_ -= std::floor(clock_);
      const auto steps = std::pow(2., std::round(p.bits) - 1);
      held_ = std::round(std::clamp(x, -1., 1.) * steps) / steps;
    }
    y = held_;
    break;
  }
  default:
    throw std::invalid_argument("Invalid distortion");
  }
  if (p.kind != DistortionKind::foldback && p.kind != DistortionKind::rectify)
    y = (1 - fold) * y + fold * std::sin(x);
  // A second nonlinear stage and complementary split; no shared processing of
  // SUB.
  if (p.stageDrive > 0)
    y = saturation(y, p.stageDrive);
  if (p.multiband > 0) {
    split_ += splitAlpha_ * (y - split_);
    const auto shaped =
        saturation(split_, p.lowDrive) + saturation(y - split_, p.highDrive);
    y += (shaped - y) * p.multiband;
  }
  if (p.postCut > 0) {
    post_ += postAlpha_ * (y - post_);
    y = post_;
  }
  return y;
}
void upgrade(Genome &g) { g.engine = universalVersion; }
double midiFrequency(int n) {
  if (n < 24 || n > 60)
    throw std::invalid_argument("Tuning note must be 24..60");
  return 440 * std::pow(2., (n - 69) / 12.);
}
void tune(Genome &g, int note, Locks locks) {
  const auto target = midiFrequency(note), ratio = target / g.body.endHz;
  upgrade(g);
  if (!locks.sub) {
    g.sub.renderVersion = 4;
    g.sub.hz = std::clamp(g.sub.hz * ratio, 25., 400.);
    g.sub.endHz = std::clamp(g.sub.endHz * ratio, 25., 400.);
  }
  if (!locks.body) {
    g.body.renderVersion = 4;
    g.body.startHz = std::clamp(g.body.startHz * ratio, 30., 2000.);
    g.body.pivotHz = std::clamp(g.body.pivotHz * ratio, 25., 2000.);
    g.body.endHz = target;
  }
  if (!locks.tail) {
    g.tail.renderVersion = 4;
    g.tail.hz = std::clamp(g.tail.hz * ratio, 25., 800.);
    g.tail.endHz = std::clamp(g.tail.endHz * ratio, 25., 800.);
  }
  identify(g);
}
Genome universalFoundation(Architecture a) {
  if (a < Architecture::analog || a >= Architecture::count)
    throw std::invalid_argument("Invalid synthesis architecture");
  auto g = foundation(Mode::deep);
  upgrade(g);
  g.architecture = a;
  g.name = architectureName(a);
  g.sub.renderVersion = g.attack.renderVersion = g.body.renderVersion =
      g.tail.renderVersion = 4;
  g.sub.hz = g.sub.endHz = g.body.endHz = g.tail.hz = g.tail.endHz = 50;
  g.sub.duration = .28;
  g.sub.decay = .09;
  g.sub.level = .5;
  g.body.startHz = 220;
  g.body.pivotHz = 75;
  g.body.pivotTime = .008;
  g.body.settleTime = .05;
  g.body.duration = .32;
  g.body.level = .75;
  g.body.drive = 2;
  g.tail.duration = .25;
  g.tail.level = .15;
  g.tail.drive = 1;
  switch (a) {
  case Architecture::analog:
    g.body.kind = BodyKind::sine;
    g.attack.kind = AttackKind::tonal;
    g.attack.level = .15;
    break;
  case Architecture::psyPunch:
    g.body.kind = BodyKind::additive;
    g.body.startHz = 1100;
    g.body.pivotHz = 110;
    g.body.settleTime = .022;
    g.body.pivotTime = .004;
    g.body.duration = .17;
    g.body.harmonics = .18;
    g.sub.duration = .16;
    g.sub.decay = .05;
    g.tail.duration = .13;
    g.tail.level = .08;
    g.attack.kind = AttackKind::hybrid;
    break;
  case Architecture::distortedHard:
    g.body.kind = BodyKind::saw;
    g.body.drive = 18;
    g.body.fx.kind = DistortionKind::asymmetric;
    g.body.fx.stageDrive = 7;
    g.body.fx.multiband = .6;
    g.body.fx.lowDrive = 1;
    g.body.fx.highDrive = 15;
    g.tail.kind = TailKind::gated;
    g.tail.level = .4;
    g.tail.drive = 16;
    g.tail.fx.kind = DistortionKind::hard;
    g.attack.kind = AttackKind::harmonic;
    break;
  case Architecture::tonal:
    g.body.kind = BodyKind::additive;
    g.body.duration = .4;
    g.body.harmonics = .55;
    g.tail.duration = 1.2;
    g.tail.level = .7;
    g.tail.decay = .6;
    g.tail.harmonics = .6;
    g.tail.drive = 12;
    g.sub.duration = .65;
    g.sub.decay = .25;
    break;
  case Architecture::acoustic:
    g.body.kind = BodyKind::membrane;
    g.body.startHz = 100;
    g.body.pivotHz = 66;
    g.body.settleTime = .025;
    g.body.drive = 0;
    g.body.duration = .4;
    g.body.harmonics = .65;
    g.body.noise = .12;
    g.body.damping = .65;
    g.sub.level = .22;
    g.attack.kind = AttackKind::noise;
    g.attack.colour = .25;
    g.tail.level = .03;
    break;
  case Architecture::experimental:
    g.body.kind = BodyKind::ring;
    g.body.fm = 2;
    g.body.harmonics = .7;
    g.body.duration = .7;
    g.body.fx.kind = DistortionKind::digital;
    g.body.fx.bits = 9;
    g.body.fx.holdHz = 12000;
    g.body.drive = 8;
    g.tail.kind = TailKind::phaseWarp;
    g.tail.motion = .7;
    g.tail.duration = .8;
    g.tail.level = .5;
    break;
  default:
    break;
  }
  identify(g);
  return g;
}
const char *architectureName(Architecture a) noexcept {
  static constexpr const char *n[] = {
      "ANALOG",          "PSY PUNCH",         "DISTORTED HARD",
      "TONAL / PITCHED", "HYBRID / ACOUSTIC", "EXPERIMENTAL"};
  return a >= Architecture::analog && a < Architecture::count
             ? n[static_cast<int>(a)]
             : "AUTO";
}
const char *characterName(Character c) noexcept {
  static constexpr const char *n[] = {
      "PROFILE",  "DEEP",    "PUNCHY", "WARM",  "CLEAN", "DISTORTED",
      "METALLIC", "ORGANIC", "BOUNCY", "TONAL", "RAW",   "EXPERIMENTAL"};
  return c >= Character::neutral && c < Character::count
             ? n[static_cast<int>(c)]
             : "INVALID";
}
const char *distortionName(DistortionKind k) noexcept {
  static constexpr const char *n[] = {"SOFT / ATAN", "HARD CLIP", "TANH",
                                      "POLYNOMIAL",  "FOLDBACK",  "ASYMMETRIC",
                                      "RECTIFY",     "DIGITAL"};
  return k >= DistortionKind::soft && k < DistortionKind::count
             ? n[static_cast<int>(k)]
             : "INVALID";
}
} // namespace kravatek::genetic
