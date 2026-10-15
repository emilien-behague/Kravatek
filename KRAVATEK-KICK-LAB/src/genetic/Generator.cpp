#include "Generator.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
namespace kravatek::genetic {
namespace {
struct Region {
  double startLow, startHigh, pivotLow, pivotHigh, dropLow, dropHigh,
      durationLow, durationHigh;
  double harmonicsLow, harmonicsHigh, fmLow, fmHigh, driveLow, driveHigh,
      tailLow, tailHigh;
  std::array<double, 4> bodyWeights, attackWeights, tailWeights;
};
const std::array<Region, 5> regions{{{90,
                                      420,
                                      40,
                                      100,
                                      .04,
                                      .16,
                                      .25,
                                      .65,
                                      0,
                                      .25,
                                      0,
                                      .5,
                                      0,
                                      10,
                                      .35,
                                      1.1,
                                      {.65, .3, .04, .01},
                                      {.65, .3, .03, .02},
                                      {.8, .15, .04, .01}},
                                     {400,
                                      1500,
                                      70,
                                      220,
                                      .014,
                                      .05,
                                      .09,
                                      .24,
                                      .1,
                                      .65,
                                      .1,
                                      1.8,
                                      2,
                                      14,
                                      .06,
                                      .20,
                                      {.1, .55, .3, .05},
                                      {.2, .2, .2, .4},
                                      {.8, .12, .04, .04}},
                                     {180,
                                      1400,
                                      80,
                                      600,
                                      .03,
                                      .15,
                                      .18,
                                      .7,
                                      .35,
                                      1,
                                      .4,
                                      3.4,
                                      12,
                                      30,
                                      .18,
                                      .8,
                                      {.02, .28, .45, .25},
                                      {.4, .05, .2, .35},
                                      {.1, .15, .5, .25}},
                                     {80,
                                      1800,
                                      35,
                                      1800,
                                      .025,
                                      .25,
                                      .15,
                                      1.1,
                                      .05,
                                      .9,
                                      .1,
                                      3.5,
                                      0,
                                      26,
                                      .15,
                                      1.2,
                                      {.1, .25, .25, .4},
                                      {.2, .3, .2, .3},
                                      {.1, .35, .3, .25}},
                                     {80,
                                      2000,
                                      25,
                                      2000,
                                      .012,
                                      .28,
                                      .10,
                                      1.3,
                                      0,
                                      1,
                                      0,
                                      4,
                                      0,
                                      30,
                                      .12,
                                      1.4,
                                      {.05, .15, .45, .35},
                                      {.15, .2, .3, .35},
                                      {.05, .2, .35, .4}}}};
int choose(Random &r, const std::array<double, 4> &weights) {
  auto x = r.unit();
  for (int i = 0; i < 4; ++i) {
    x -= weights[static_cast<std::size_t>(i)];
    if (x <= 0)
      return i;
  }
  return 3;
}
} // namespace
Genome generateGenome(Mode mode, std::uint32_t seed, double n) {
  if (!std::isfinite(n) || n < 0 || n > 1)
    throw std::invalid_argument("Novelty must be 0..1");
  auto g = foundation(mode);
  g.seed = seed;
  g.novelty = n;
  g.name = std::string(modeName(mode)) + " / " + std::to_string(seed);
  const auto &z = regions.at(static_cast<std::size_t>(mode));
  const auto amount = .035 + .965 * n;
  Random sub(seed ^ 0x53554231U), attack(seed ^ 0x41545431U),
      body(seed ^ 0x424f4431U), tail(seed ^ 0x54414931U);
  const auto blend = [&](double base, double target) {
    return base + (target - base) * amount;
  };
  const auto logBlend = [&](double base, double target) {
    return std::exp(std::log(base) +
                    (std::log(target) - std::log(base)) * amount);
  };
  auto &s = g.sub;
  const auto low = mode == Mode::deep ? 30. : mode == Mode::hardPsy ? 44. : 32.;
  const auto high = mode == Mode::deep      ? 55.
                    : mode == Mode::hardPsy ? 66.
                                            : 80.;
  s.hz = logBlend(s.hz, sub.logarithmic(low, high));
  s.level = blend(s.level, sub.between(mode == Mode::deep ? .65 : .25,
                                       mode == Mode::deep ? 1. : .75));
  s.duration =
      blend(s.duration, sub.between(mode == Mode::hardPsy ? .10 : .30,
                                    mode == Mode::hardPsy ? .25 : 1.1));
  s.decay = blend(s.decay, sub.between(mode == Mode::hardPsy ? .03 : .09,
                                       mode == Mode::hardPsy ? .10 : .40));
  s.attack = blend(s.attack, sub.between(.002, .012));
  s.release = blend(s.release, sub.between(.005, .035));
  s.phase = n * n * sub.between(0, .3);
  auto &a = g.attack;
  a.seed = attack.next();
  if (attack.unit() < n * n)
    a.kind = static_cast<AttackKind>(choose(attack, z.attackWeights));
  a.hz = logBlend(
      a.hz, attack.logarithmic(300, mode == Mode::mutant ? 6000. : 4200.));
  a.level = blend(a.level, attack.between(mode == Mode::deep ? .025 : .12,
                                          mode == Mode::deep ? .24 : .65));
  a.duration = blend(a.duration,
                     attack.between(.010, mode == Mode::mutant ? .070 : .035));
  a.decay = blend(
      a.decay, attack.logarithmic(.0008, mode == Mode::mutant ? .020 : .008));
  a.attack = blend(a.attack, attack.between(.00015, .0012));
  a.release =
      std::min(a.duration * .3, blend(a.release, attack.between(.001, .005)));
  a.colour = blend(a.colour, attack.unit());
  auto &b = g.body;
  b.seed = body.next();
  if (body.unit() < n * n)
    b.kind = static_cast<BodyKind>(choose(body, z.bodyWeights));
  b.startHz = logBlend(b.startHz, body.logarithmic(z.startLow, z.startHigh));
  b.endHz =
      std::clamp(s.hz * std::pow(2.0, body.between(-1, 1) * n * n *
                                          (mode == Mode::mutant ? .25 : .06)),
                 25.0, 150.0);
  b.pivotHz = logBlend(b.pivotHz, body.logarithmic(z.pivotLow, z.pivotHigh));
  const auto settle =
      logBlend(b.settleTime, body.between(z.dropLow, z.dropHigh));
  b.pivotTime = std::clamp(settle * body.between(.12, .55), .001, .12);
  b.settleTime = std::clamp(settle, b.pivotTime + .001, .3);
  b.curve = blend(b.curve, body.between(.4, 2.5));
  b.duration = blend(b.duration, body.between(z.durationLow, z.durationHigh));
  b.ampCurve = blend(b.ampCurve, body.between(.5, 2.2));
  b.attack =
      blend(b.attack, body.between(.00015, mode == Mode::deep ? .006 : .003));
  b.release =
      std::min(b.duration * .3, blend(b.release, body.between(.003, .035)));
  b.level = blend(b.level, body.between(.35, .95));
  b.harmonics =
      blend(b.harmonics, body.between(z.harmonicsLow, z.harmonicsHigh));
  b.fm = blend(b.fm, body.between(z.fmLow, z.fmHigh));
  b.fmTime = logBlend(b.fmTime,
                      body.logarithmic(.005, mode == Mode::hardPsy ? .06 : .4));
  b.drive = blend(b.drive, body.between(z.driveLow, z.driveHigh));
  b.fold = blend(b.fold, body.unit() * (mode == Mode::deep ? .1 : n));
  b.cutoff = logBlend(
      b.cutoff, body.logarithmic(mode == Mode::hardPsy ? 2000. : 80., 16000));
  b.resonance = blend(b.resonance,
                      body.unit() * (mode == Mode::deep ? .3 : .45 + .5 * n));
  b.sweep = blend(b.sweep, body.between(-4, 4) * n);
  auto &t = g.tail;
  t.seed = tail.next();
  if (tail.unit() < n * n)
    t.kind = static_cast<TailKind>(choose(tail, z.tailWeights));
  constexpr std::array<double, 7> ratios{.5, .75, 1, 1.5, 2, 3, 4};
  const auto ratio = ratios[tail.next() % ratios.size()];
  t.hz =
      std::clamp(logBlend(t.hz, s.hz * (1 + (ratio - 1) * n) *
                                    std::pow(2.0, tail.between(-1, 1) * n * n)),
                 25.0, 800.0);
  t.endHz = std::clamp(
      logBlend(t.endHz, s.hz * std::pow(2.0, tail.between(-.5, .5) * n)), 25.0,
      800.0);
  t.duration = blend(t.duration, tail.between(z.tailLow, z.tailHigh));
  t.decay =
      blend(t.decay, tail.between(.035, mode == Mode::hardPsy ? .09 : .5));
  t.attack = blend(t.attack, tail.between(.003, .030));
  t.release =
      std::min(t.duration * .3, blend(t.release, tail.between(.006, .045)));
  t.attack = std::min(t.attack, t.duration * .3);
  t.level = blend(t.level, tail.between(mode == Mode::deep ? .05 : .12,
                                        mode == Mode::deep ? .3 : .9));
  t.motion = blend(t.motion, tail.unit() * n * (mode == Mode::deep ? .35 : 1));
  t.speed = logBlend(t.speed, tail.logarithmic(1, 40));
  t.cutoff = logBlend(t.cutoff, tail.logarithmic(80, 16000));
  t.resonance = blend(t.resonance, tail.unit() * (.35 + .6 * n));
  t.sweep = blend(t.sweep, tail.between(-4, 4) * n);
  t.drive = blend(t.drive, tail.between(0, mode == Mode::deep ? 8. : 30.));
  identify(g);
  return g;
}
} // namespace kravatek::genetic
namespace kravatek::genetic {
Genome evolveGenome(const Genome &parent, Mode mode, std::uint32_t seed,
                    double novelty, Locks locks, double strength) {
  validate(parent);
  if (!std::isfinite(strength) || strength < 0 || strength > 1)
    throw std::invalid_argument("Evolution strength must be 0..1");
  auto checked = parent;
  identify(checked);
  if (checked.id != parent.id)
    throw std::invalid_argument("Parent identity does not match its genes");
  if (parent.generation >= 1000000)
    throw std::invalid_argument("Generation limit reached");
  const auto target = generateGenome(mode, seed, novelty);
  auto child = parent;
  child.mode = mode;
  child.seed = seed;
  child.novelty = novelty;
  child.locks = locks;
  child.parents = {parent.id};
  child.generation = parent.generation + 1;
  child.name = std::string(modeName(mode)) + " G" +
               std::to_string(child.generation) + " / " + std::to_string(seed);
  const auto amount = strength * (.06 + .94 * novelty);
  const auto mix = [&](double a, double b) { return a + (b - a) * amount; };
  Random structure(seed ^ 0x45564f31U);
  const bool structural =
      strength >= .2 && structure.unit() < novelty * novelty * strength;
#define MIX(group, field)                                                      \
  child.group.field = mix(parent.group.field, target.group.field)
  if (!locks.sub) {
    MIX(sub, hz);
    MIX(sub, duration);
    MIX(sub, decay);
    MIX(sub, attack);
    MIX(sub, release);
    MIX(sub, level);
    MIX(sub, phase);
  }
  if (!locks.attack) {
    MIX(attack, hz);
    MIX(attack, duration);
    MIX(attack, decay);
    MIX(attack, attack);
    MIX(attack, release);
    MIX(attack, level);
    MIX(attack, colour);
    MIX(attack, phase);
    if (structural) {
      child.attack.kind = target.attack.kind;
      child.attack.seed = target.attack.seed;
    }
  }
  if (!locks.body) {
    MIX(body, startHz);
    MIX(body, pivotHz);
    MIX(body, endHz);
    MIX(body, pivotTime);
    MIX(body, settleTime);
    MIX(body, curve);
    MIX(body, duration);
    MIX(body, attack);
    MIX(body, release);
    MIX(body, ampCurve);
    MIX(body, level);
    MIX(body, harmonics);
    MIX(body, fm);
    MIX(body, fmTime);
    MIX(body, drive);
    MIX(body, fold);
    MIX(body, cutoff);
    MIX(body, resonance);
    MIX(body, sweep);
    MIX(body, phase);
    if (structural) {
      child.body.kind = target.body.kind;
      child.body.seed = target.body.seed;
    }
  }
  if (!locks.tail) {
    MIX(tail, hz);
    MIX(tail, endHz);
    MIX(tail, duration);
    MIX(tail, decay);
    MIX(tail, attack);
    MIX(tail, release);
    MIX(tail, level);
    MIX(tail, motion);
    MIX(tail, speed);
    MIX(tail, cutoff);
    MIX(tail, resonance);
    MIX(tail, sweep);
    MIX(tail, drive);
    MIX(tail, phase);
    if (structural) {
      child.tail.kind = target.tail.kind;
      child.tail.seed = target.tail.seed;
    }
  }
#undef MIX
  identify(child);
  return child;
}
} // namespace kravatek::genetic
