#include "Profile.h"
#include "Generator.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <locale>
#include <set>
#include <sstream>
#include <stdexcept>
namespace kravatek::genetic {
std::span<const ParameterSpec> parameterSpecs() {
#define P(group, field, lo, hi)                                                \
  {                                                                            \
    #group "." #field, lo, hi,                                                 \
        [](Genome &g) -> double & { return g.group.field; }                    \
  }
  static const ParameterSpec specs[] = {P(sub, duration, .008, 4),
                                        P(sub, decay, .015, 3),
                                        P(sub, attack, .0001, .2),
                                        P(sub, release, .0003, .2),
                                        P(sub, level, 0, 1),
                                        P(sub, glideTime, .001, 2),
                                        P(attack, hz, 150, 6000),
                                        P(attack, duration, .008, .3),
                                        P(attack, decay, .0005, .08),
                                        P(attack, level, 0, 1),
                                        P(attack, colour, 0, 1),
                                        P(body, startHz, 30, 2000),
                                        P(body, pivotHz, 25, 2000),
                                        P(body, pivotTime, .001, .12),
                                        P(body, settleTime, .002, .3),
                                        P(body, curve, .4, 2.5),
                                        P(body, duration, .008, 4),
                                        P(body, attack, .0001, .2),
                                        P(body, release, .0003, .2),
                                        P(body, ampCurve, .45, 2.3),
                                        P(body, level, 0, 1),
                                        P(body, harmonics, 0, 1),
                                        P(body, fm, 0, 4),
                                        P(body, fmTime, .005, .5),
                                        P(body, drive, 0, 30),
                                        P(body, fold, 0, 1),
                                        P(body, cutoff, 80, 16000),
                                        P(body, resonance, 0, .95),
                                        P(body, sweep, -4, 4),
                                        P(body, damping, 0, 1),
                                        P(body, inharmonicity, 0, 1),
                                        P(body, noise, 0, 1),
                                        P(tail, duration, .008, 4),
                                        P(tail, decay, .015, 3),
                                        P(tail, attack, .0001, .2),
                                        P(tail, release, .0003, .2),
                                        P(tail, level, 0, 1),
                                        P(tail, motion, 0, 1),
                                        P(tail, speed, 1, 40),
                                        P(tail, cutoff, 80, 16000),
                                        P(tail, resonance, 0, .95),
                                        P(tail, sweep, -4, 4),
                                        P(tail, drive, 0, 30),
                                        P(tail, glideTime, .001, 2),
                                        P(tail, harmonics, 0, 1),
                                        P(tail, gate, .05, .95),
#define FX(group)                                                              \
  P(group, fx.stageDrive, 0, 24), P(group, fx.preCut, 0, 18000),               \
      P(group, fx.postCut, 0, 18000), P(group, fx.bias, -.8, .8),              \
      P(group, fx.multiband, 0, 1), P(group, fx.crossover, 60, 800),           \
      P(group, fx.lowDrive, 0, 24), P(group, fx.highDrive, 0, 30),             \
      P(group, fx.driveDecay, .005, 2), P(group, fx.driveMotion, 0, 1),        \
      P(group, fx.bits, 4, 16), P(group, fx.holdHz, 500, 48000)
                                        FX(body),
                                        FX(tail)};
#undef FX
#undef P
  return specs;
}
const ParameterSpec &parameterSpec(const std::string &name) {
  for (const auto &s : parameterSpecs())
    if (name == s.name)
      return s;
  throw std::invalid_argument("Unknown profile parameter: " + name);
}
void validateProfile(const GenreProfile &p) {
  if (p.id.empty() || p.id.size() > 80 ||
      p.id.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789.-") !=
          std::string::npos ||
      p.name.empty() || p.name.size() > 80 || p.family.empty() ||
      p.family.size() > 40 || p.revision < 1 || p.parents.size() > 4)
    throw std::invalid_argument("Invalid profile identity");
  const auto bounded = [](double x, double lo, double hi) {
    return std::isfinite(x) && x >= lo && x <= hi;
  };
  if (!bounded(p.bpmLow, 20, 500) || !bounded(p.bpmHigh, p.bpmLow, 500) ||
      !bounded(p.fundamentalLow, 25, 150) ||
      !bounded(p.fundamentalHigh, p.fundamentalLow, 150) ||
      !bounded(p.subGlide, -12, 12) || !bounded(p.tailInterval, -12, 24))
    throw std::invalid_argument("Invalid acoustic profile guidance");
  const auto weights = [&](const auto &v) {
    double sum = 0;
    for (double x : v) {
      if (!bounded(x, 0, 1))
        throw std::invalid_argument("Invalid module probability");
      sum += x;
    }
    if (std::abs(sum - 1) > 1e-8)
      throw std::invalid_argument("Module weights must sum to one");
  };
  weights(p.architectures);
  weights(p.bodyWeights);
  weights(p.tailWeights);
  weights(p.attackWeights);
  weights(p.distortions);
  std::set<std::string> fields;
  for (const auto &r : p.ranges) {
    const auto &s = parameterSpec(r.parameter);
    if (!fields.insert(r.parameter).second || !bounded(r.low, s.low, s.high) ||
        !bounded(r.high, r.low, s.high) ||
        r.distribution < Distribution::uniform ||
        r.distribution > Distribution::triangular ||
        (r.distribution == Distribution::logarithmic && r.low <= 0))
      throw std::invalid_argument("Invalid profile range: " + r.parameter);
  }
  if (p.ranges.size() < 8)
    throw std::invalid_argument("Profile must define meaningful ranges");
}
std::string profileCanonical(const GenreProfile &p) {
  std::ostringstream s;
  s.imbue(std::locale::classic());
  s << std::setprecision(17) << "KRAVA_PROFILE_004|" << p.id << '|'
    << p.revision << '|' << p.name.size() << ':' << p.name << '|'
    << p.family.size() << ':' << p.family;
  s << "|parents=" << p.parents.size();
  for (const auto &x : p.parents)
    s << '|' << x;
  s << '|' << p.bpmLow << '|' << p.bpmHigh << '|' << p.fundamentalLow << '|'
    << p.fundamentalHigh << '|' << p.subGlide << '|' << p.tailInterval;
  const auto put = [&](const auto &a) {
    for (auto x : a)
      s << '|' << x;
  };
  put(p.architectures);
  put(p.bodyWeights);
  put(p.tailWeights);
  put(p.attackWeights);
  put(p.distortions);
  for (const auto &r : p.ranges)
    s << '|' << r.parameter << '|' << r.low << '|' << r.high << '|'
      << static_cast<int>(r.distribution);
  return s.str();
}
void identifyProfile(GenreProfile &p) {
  validateProfile(p);
  p.digest = sha256(profileCanonical(p));
}
namespace {
template <class A> int choose(Random &rng, const A &weights, double novelty) {
  const auto best = static_cast<int>(
      std::max_element(weights.begin(), weights.end()) - weights.begin());
  if (rng.unit() > novelty * novelty)
    return best;
  auto x = rng.unit();
  for (std::size_t i = 0; i < weights.size(); ++i) {
    x -= weights[i];
    if (x <= 0)
      return static_cast<int>(i);
  }
  return best;
}
void constrain(Genome &g) {
  for (const auto &s : parameterSpecs())
    s.value(g) = std::clamp(s.value(g), s.low, s.high);
  const auto env = [](auto &e) {
    e.attack = std::min(e.attack, e.duration * .35);
    e.release = std::min(e.release, e.duration * .35);
  };
  env(g.sub);
  env(g.attack);
  env(g.body);
  env(g.tail);
  g.body.pivotTime = std::min(g.body.pivotTime, g.body.settleTime - .001);
}
void character(Genome &g, Character c) {
  switch (c) {
  case Character::deep:
    g.sub.level = std::min(1., g.sub.level * 1.3);
    g.sub.duration *= 1.2;
    g.sub.decay *= 1.2;
    g.body.startHz *= .7;
    g.tail.level *= .65;
    break;
  case Character::punchy:
    g.body.duration *= .7;
    g.sub.duration *= .8;
    g.body.attack = .0003;
    g.attack.level = std::min(1., g.attack.level + .2);
    g.body.startHz *= 1.3;
    break;
  case Character::warm:
    g.body.fx.kind = DistortionKind::soft;
    g.body.drive = std::min(12., g.body.drive + 3);
    g.body.cutoff = std::min(6000., g.body.cutoff);
    g.tail.harmonics *= .5;
    break;
  case Character::clean:
    g.body.fx.kind = g.tail.fx.kind = DistortionKind::tanh;
    g.body.drive = g.tail.drive = 0;
    g.body.fold = 0;
    g.body.fx.multiband = g.tail.fx.multiband = 0;
    g.body.fx.stageDrive = g.tail.fx.stageDrive = 0;
    g.body.harmonics *= .3;
    break;
  case Character::distorted:
    g.body.fx.kind = DistortionKind::asymmetric;
    g.body.drive = 25;
    g.body.fx.stageDrive = 12;
    g.tail.drive = 22;
    g.tail.harmonics = .8;
    break;
  case Character::metallic:
    g.body.kind = BodyKind::fm;
    g.body.fm = 2.8;
    g.body.fmTime = .18;
    g.body.harmonics = .7;
    g.tail.kind = TailKind::resonant;
    g.tail.resonance = .8;
    break;
  case Character::organic:
    g.body.kind = BodyKind::membrane;
    g.body.damping = .65;
    g.body.noise = .2;
    g.body.drive = 2;
    g.attack.kind = AttackKind::noise;
    g.attack.colour = .3;
    break;
  case Character::bouncy:
    g.tail.kind = TailKind::elastic;
    g.tail.motion = .8;
    g.tail.speed = 11;
    g.tail.level = std::max(.5, g.tail.level);
    g.tail.duration = std::max(.5, g.tail.duration);
    break;
  case Character::tonal:
    g.body.kind = BodyKind::additive;
    g.tail.kind = TailKind::tonal;
    g.tail.harmonics = .6;
    g.tail.level = std::max(.4, g.tail.level);
    g.tail.duration = std::max(.8, g.tail.duration);
    break;
  case Character::raw:
    g.body.kind = BodyKind::saw;
    g.body.harmonics = .9;
    g.body.fx.kind = DistortionKind::hard;
    g.body.drive = 24;
    g.tail.kind = TailKind::gated;
    g.tail.drive = 25;
    break;
  case Character::experimental:
    g.body.kind = BodyKind::ring;
    g.body.fm = 3;
    g.tail.kind = TailKind::phaseWarp;
    g.tail.motion = .9;
    g.body.fx.kind = DistortionKind::digital;
    g.body.fx.bits = 7;
    g.body.fx.holdHz = 7000;
    break;
  default:
    break;
  }
}
} // namespace
Genome generateUniversal(const UniversalRequest &r, std::uint32_t seed,
                         double n) {
  validateProfile(r.profile);
  if (!std::isfinite(n) || n < 0 || n > 1 || r.character < Character::neutral ||
      r.character >= Character::count ||
      r.overrideArchitecture < Architecture::analog ||
      r.overrideArchitecture > Architecture::count ||
      (r.note != 0 && (r.note < 24 || r.note > 60)))
    throw std::invalid_argument("Invalid universal request");
  auto checked = r.profile;
  identifyProfile(checked);
  if (checked.digest != r.profile.digest)
    throw std::invalid_argument("Profile fingerprint mismatch");
  Random rng(seed ^ 0x554b4534U);
  const auto a =
      r.overrideArchitecture == Architecture::count
          ? static_cast<Architecture>(choose(rng, r.profile.architectures, n))
          : r.overrideArchitecture;
  auto g = universalFoundation(a);
  g.seed = seed;
  g.novelty = n;
  g.profileId = r.profile.id;
  g.profileRevision = r.profile.revision;
  g.profileDigest = r.profile.digest;
  g.character = r.character;
  g.name = r.profile.name + " / " + std::to_string(seed);
  const auto logLo = std::log(r.profile.fundamentalLow),
             logHi = std::log(r.profile.fundamentalHigh);
  const auto frequency =
      std::exp((logLo + logHi) * .5 +
               (rng.unit() - .5) * (logHi - logLo) * (.04 + .96 * n));
  g.sub.hz =
      std::clamp(frequency * std::pow(2., r.profile.subGlide / 12), 25., 400.);
  g.sub.endHz = g.body.endHz = frequency;
  g.tail.hz = g.tail.endHz = std::clamp(
      frequency * std::pow(2., r.profile.tailInterval / 12), 25., 800.);
  g.sub.phase = 0;
  g.attack.seed = rng.next();
  g.body.seed = rng.next();
  g.tail.seed = rng.next();
  if (r.overrideArchitecture == Architecture::count) {
    g.body.kind = static_cast<BodyKind>(choose(rng, r.profile.bodyWeights, n));
    g.tail.kind = static_cast<TailKind>(choose(rng, r.profile.tailWeights, n));
  }
  g.attack.kind =
      static_cast<AttackKind>(choose(rng, r.profile.attackWeights, n));
  g.body.fx.kind =
      static_cast<DistortionKind>(choose(rng, r.profile.distortions, n));
  for (const auto &field : r.profile.ranges) {
    const auto &s = parameterSpec(field.parameter);
    double x = field.distribution == Distribution::triangular
                   ? (rng.unit() + rng.unit()) * .5
                   : rng.unit();
    x = .5 + (x - .5) * (.04 + .96 * n);
    s.value(g) =
        field.distribution == Distribution::logarithmic
            ? std::exp(std::log(field.low) +
                       (std::log(field.high) - std::log(field.low)) * x)
            : field.low + (field.high - field.low) * x;
  }
  character(g, r.character);
  constrain(g);
  if (r.note)
    tune(g, r.note);
  identify(g);
  return g;
}
Genome evolveUniversal(const Genome &parent, const UniversalRequest &r,
                       std::uint32_t seed, double n, Locks locks,
                       double strength) {
  auto target = generateUniversal(r, seed, n);
  if (locks.sub) {
    const auto hz =
        parent.sub.renderVersion == 4 ? parent.sub.endHz : parent.sub.hz;
    const auto ratio = hz / target.body.endHz;
    target.body.startHz = std::clamp(target.body.startHz * ratio, 30., 2000.);
    target.body.pivotHz = std::clamp(target.body.pivotHz * ratio, 25., 2000.);
    target.body.endHz = hz;
    target.tail.hz = std::clamp(target.tail.hz * ratio, 25., 800.);
    target.tail.endHz = std::clamp(target.tail.endHz * ratio, 25., 800.);
    identify(target);
  }
  auto child = evolveToward(parent, target, locks, strength);
  child.name = r.profile.name + " G" + std::to_string(child.generation) +
               " / " + std::to_string(seed);
  return child;
}
} // namespace kravatek::genetic
