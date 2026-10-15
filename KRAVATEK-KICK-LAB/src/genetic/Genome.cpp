#include "Genome.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <iomanip>
#include <locale>
#include <numbers>
#include <sstream>
#include <stdexcept>

namespace kravatek::genetic {
std::uint32_t Random::next() noexcept {
  state_ += 0x9e3779b9U;
  auto z = state_;
  z = (z ^ (z >> 16U)) * 0x21f0aaadU;
  z = (z ^ (z >> 15U)) * 0x735a2d97U;
  return z ^ (z >> 15U);
}
double Random::unit() noexcept {
  return static_cast<double>(next()) / 4294967296.0;
}
double Random::between(double lo, double hi) noexcept {
  return lo + (hi - lo) * unit();
}
double Random::logarithmic(double lo, double hi) noexcept {
  return std::exp(between(std::log(lo), std::log(hi)));
}
const char *modeName(Mode m) noexcept {
  switch (m) {
  case Mode::deep:
    return "DEEP";
  case Mode::hardPsy:
    return "HARD PSY";
  case Mode::schranz:
    return "SCHRANZ";
  case Mode::experimental:
    return "EXPERIMENTAL";
  case Mode::mutant:
    return "MUTANT";
  default:
    return "INVALID";
  }
}
const char *bodyName(BodyKind k) noexcept {
  switch (k) {
  case BodyKind::sine:
    return "SINE";
  case BodyKind::additive:
    return "ADDITIVE";
  case BodyKind::fm:
    return "FM / PM";
  case BodyKind::ring:
    return "RING";
  case BodyKind::membrane:
    return "MEMBRANE";
  case BodyKind::saw:
    return "BLEP SAW";
  default:
    return "INVALID";
  }
}
const char *tailName(TailKind k) noexcept {
  switch (k) {
  case TailKind::tonal:
    return "TONAL";
  case TailKind::elastic:
    return "ELASTIC";
  case TailKind::resonant:
    return "RESONANT";
  case TailKind::phaseWarp:
    return "PHASE WARP";
  case TailKind::gated:
    return "GATED";
  case TailKind::reverse:
    return "REVERSE";
  default:
    return "INVALID";
  }
}
namespace {
void range(double v, double a, double b, const char *name) {
  if (!std::isfinite(v) || v < a || v > b)
    throw std::invalid_argument(name);
}
void envelope(double duration, double attack, double release, double level,
              int version) {
  range(duration, .008, version == 4 ? 4.0 : 1.5,
        "Layer duration outside .008..1.5 seconds");
  range(attack, .0001, duration * .4, "Invalid layer attack");
  range(release, .0003, duration * .4, "Invalid layer release");
  range(level, 0, 1, "Invalid layer level");
}
void phase(double v) {
  range(v, 0, 2 * std::numbers::pi, "Invalid initial phase");
}
} // namespace
void validate(const Genome &g, double rate) {
  if (g.engine != engineVersion && g.engine != universalVersion)
    throw std::invalid_argument("Unsupported genetic engine version");
  for (auto v : {g.sub.renderVersion, g.attack.renderVersion,
                 g.body.renderVersion, g.tail.renderVersion})
    if ((v != 3 && v != 4) || (g.engine == engineVersion && v != 3))
      throw std::invalid_argument("Invalid layer renderer version");
  if (g.architecture < Architecture::analog ||
      g.architecture >= Architecture::count ||
      g.character < Character::neutral || g.character >= Character::count ||
      g.profileId.size() > 80 ||
      (!g.profileDigest.empty() &&
       (g.profileDigest.size() != 64 ||
        g.profileDigest.find_first_not_of("0123456789abcdef") !=
            std::string::npos)))
    throw std::invalid_argument("Invalid universal provenance");
  range(rate, 8000, 192000, "Invalid genetic sample rate");
  if (g.mode < Mode::deep || g.mode >= Mode::count)
    throw std::invalid_argument("Invalid genetic mode");
  range(g.novelty, 0, 1, "Novelty must be 0..1");
  if (g.parents.size() > 2 || g.generation > 1000000 || g.name.size() > 160)
    throw std::invalid_argument("Invalid provenance");
  for (const auto &parent : g.parents)
    if (parent.size() != 67 ||
        (parent.rfind("K3-", 0) != 0 && parent.rfind("K4-", 0) != 0) ||
        parent.find_first_not_of("0123456789abcdef", 3) != std::string::npos)
      throw std::invalid_argument("Invalid parent identifier");
  const auto &s = g.sub;
  envelope(s.duration, s.attack, s.release, s.level, s.renderVersion);
  range(s.hz, 25, s.renderVersion == 4 ? 400. : 100., "Invalid sub frequency");
  range(s.decay, .015, s.renderVersion == 4 ? 3. : 1., "Invalid sub decay");
  phase(s.phase);
  const auto &a = g.attack;
  envelope(a.duration, a.attack, a.release, a.level, a.renderVersion);
  if (a.kind < AttackKind::noise || a.kind >= AttackKind::count)
    throw std::invalid_argument("Invalid transient architecture");
  range(a.hz, 150, 6000, "Invalid transient frequency");
  range(a.decay, .0005, .08, "Invalid transient decay");
  range(a.colour, 0, 1, "Invalid transient colour");
  phase(a.phase);
  const auto &b = g.body;
  envelope(b.duration, b.attack, b.release, b.level, b.renderVersion);
  if (b.kind < BodyKind::sine ||
      b.kind >= (b.renderVersion == 4 ? BodyKind::count : BodyKind::membrane))
    throw std::invalid_argument("Invalid body architecture");
  range(b.startHz, 30, 2000, "Invalid start pitch");
  range(b.pivotHz, 25, 2000, "Invalid intermediate pitch");
  range(b.endHz, 25, b.renderVersion == 4 ? 400. : 150., "Invalid end pitch");
  range(b.pivotTime, .001, .12, "Invalid pitch pivot time");
  range(b.settleTime, b.pivotTime + .001, .3, "Invalid pitch settle time");
  range(b.curve, .4, 2.5, "Invalid pitch curve");
  range(b.ampCurve, .45, 2.3, "Invalid amplitude curve");
  range(b.harmonics, 0, 1, "Invalid harmonics");
  range(b.fm, 0, 4, "Invalid modulation depth");
  range(b.fmTime, .005, .5, "Invalid modulation decay");
  range(b.drive, 0, 30, "Invalid body drive");
  range(b.fold, 0, 1, "Invalid body fold");
  range(b.cutoff, 80, 16000, "Invalid body filter");
  range(b.resonance, 0, .95, "Invalid body resonance");
  range(b.sweep, -4, 4, "Invalid body sweep");
  phase(b.phase);
  const auto &t = g.tail;
  envelope(t.duration, t.attack, t.release, t.level, t.renderVersion);
  if (t.kind < TailKind::tonal ||
      t.kind >= (t.renderVersion == 4 ? TailKind::count : TailKind::gated))
    throw std::invalid_argument("Invalid tail architecture");
  range(t.hz, 25, 800, "Invalid tail frequency");
  range(t.endHz, 25, 800, "Invalid tail final frequency");
  range(t.decay, .015, t.renderVersion == 4 ? 3. : 1., "Invalid tail decay");
  range(t.motion, 0, 1, "Invalid tail motion");
  range(t.speed, 1, 40, "Invalid tail speed");
  range(t.cutoff, 80, 16000, "Invalid tail filter");
  range(t.resonance, 0, .95, "Invalid tail resonance");
  range(t.sweep, -4, 4, "Invalid tail sweep");
  range(t.drive, 0, 30, "Invalid tail drive");
  phase(t.phase);
  if (g.engine == universalVersion) {
    range(s.endHz, 25, 400, "Invalid sub glide target");
    range(s.glideTime, .001, 2, "Invalid sub glide time");
    range(b.damping, 0, 1, "Invalid membrane damping");
    range(b.inharmonicity, 0, 1, "Invalid modal spread");
    range(b.noise, 0, 1, "Invalid body noise");
    range(t.glideTime, .001, 2, "Invalid tail glide time");
    range(t.harmonics, 0, 1, "Invalid tail harmonics");
    range(t.gate, .05, .95, "Invalid tail gate");
    for (const auto *f : {&b.fx, &t.fx}) {
      if (f->kind < DistortionKind::soft || f->kind >= DistortionKind::count)
        throw std::invalid_argument("Invalid distortion model");
      range(f->stageDrive, 0, 24, "Invalid second stage drive");
      range(f->preCut, 0, 18000, "Invalid pre filter");
      range(f->postCut, 0, 18000, "Invalid post filter");
      range(f->bias, -.8, .8, "Invalid asymmetry");
      range(f->multiband, 0, 1, "Invalid multiband mix");
      range(f->crossover, 60, 800, "Invalid crossover");
      range(f->lowDrive, 0, 24, "Invalid low band drive");
      range(f->highDrive, 0, 30, "Invalid high band drive");
      range(f->driveDecay, .005, 2, "Invalid drive envelope");
      range(f->driveMotion, 0, 1, "Invalid drive motion");
      range(f->bits, 4, 16, "Invalid bit depth");
      range(f->holdHz, 500, 48000, "Invalid sample hold rate");
    }
  }
}
Genome foundation(Mode mode) {
  Genome g;
  g.mode = mode;
  g.name = modeName(mode);
  switch (mode) {
  case Mode::deep:
    g.sub.hz = 38;
    g.sub.level = .85;
    g.sub.duration = .72;
    g.sub.decay = .24;
    g.body.startHz = 160;
    g.body.endHz = 38;
    g.body.pivotHz = 55;
    g.body.level = .48;
    g.tail.level = .12;
    g.tail.hz = g.tail.endHz = 38;
    break;
  case Mode::hardPsy:
    g.sub.hz = 53;
    g.sub.duration = .19;
    g.sub.decay = .07;
    g.sub.level = .45;
    g.body.kind = BodyKind::additive;
    g.body.startHz = 950;
    g.body.pivotHz = 95;
    g.body.endHz = 53;
    g.body.pivotTime = .005;
    g.body.settleTime = .025;
    g.body.duration = .19;
    g.body.harmonics = .25;
    g.tail.duration = .16;
    g.tail.hz = g.tail.endHz = 53;
    g.attack.kind = AttackKind::hybrid;
    break;
  case Mode::schranz:
    g.body.kind = BodyKind::fm;
    g.body.fm = 1.6;
    g.body.drive = 22;
    g.body.fold = .25;
    g.body.harmonics = .7;
    g.body.level = .8;
    g.body.cutoff = 7000;
    g.tail.kind = TailKind::resonant;
    g.tail.level = .4;
    g.tail.resonance = .65;
    g.attack.level = .4;
    g.attack.kind = AttackKind::hybrid;
    break;
  case Mode::experimental:
    g.body.kind = BodyKind::ring;
    g.body.duration = .6;
    g.body.fm = 1.3;
    g.body.pivotHz = 280;
    g.body.startHz = 160;
    g.body.sweep = 2;
    g.body.resonance = .55;
    g.body.cutoff = 1600;
    g.tail.kind = TailKind::elastic;
    g.tail.duration = .8;
    g.tail.level = .6;
    g.tail.motion = .65;
    g.tail.hz = 75;
    g.tail.endHz = 40;
    g.tail.decay = .25;
    g.attack.kind = AttackKind::tonal;
    break;
  case Mode::mutant:
    g.body.kind = BodyKind::fm;
    g.body.fm = 3;
    g.body.drive = 24;
    g.body.fold = .65;
    g.body.duration = .7;
    g.body.resonance = .7;
    g.body.cutoff = 2600;
    g.tail.kind = TailKind::phaseWarp;
    g.tail.duration = .95;
    g.tail.level = .7;
    g.tail.motion = .9;
    g.tail.hz = 110;
    g.tail.endHz = 39;
    g.tail.decay = .3;
    g.tail.drive = 20;
    g.sub.level = .4;
    g.attack.kind = AttackKind::harmonic;
    break;
  default:
    throw std::invalid_argument("Invalid genetic mode");
  }
  identify(g);
  return g;
}
double bodyPitchAt(const BodyGenes &b, double t) noexcept {
  const bool first = t < b.pivotTime;
  const auto start = first ? b.startHz : b.pivotHz,
             end = first ? b.pivotHz : b.endHz;
  const auto x = std::pow(
      std::clamp(first ? t / b.pivotTime
                       : (t - b.pivotTime) / (b.settleTime - b.pivotTime),
                 0.0, 1.0),
      b.curve);
  const auto smooth = x * x * (3 - 2 * x);
  return std::exp(std::log(start) + (std::log(end) - std::log(start)) * smooth);
}
double bodyAmplitudeAt(const BodyGenes &b, double t) noexcept {
  const auto a = std::clamp(t / b.attack, 0.0, 1.0),
             r = std::clamp((b.duration - t) / b.release, 0.0, 1.0);
  return .25 * (1 - std::cos(std::numbers::pi * a)) *
         (1 - std::cos(std::numbers::pi * r)) *
         std::exp(-6 * std::pow(std::max(0.0, t) / b.duration, b.ampCurve));
}
std::string canonical(const Genome &g) {
  std::ostringstream o;
  o.imbue(std::locale::classic());
  o << std::setprecision(17) << g.engine << '|' << g.seed << '|'
    << static_cast<int>(g.mode) << '|' << g.novelty << '|' << g.generation
    << '|' << g.parents.size();
  for (const auto &id : g.parents)
    o << '|' << id;
  const auto put = [&](auto... values) { ((o << '|' << values), ...); };
  const auto &s = g.sub;
  put(s.hz, s.duration, s.decay, s.attack, s.release, s.level, s.phase);
  const auto &a = g.attack;
  put(static_cast<int>(a.kind), a.seed, a.hz, a.duration, a.decay, a.attack,
      a.release, a.level, a.colour, a.phase);
  const auto &b = g.body;
  put(static_cast<int>(b.kind), b.seed, b.startHz, b.pivotHz, b.endHz,
      b.pivotTime, b.settleTime, b.curve, b.duration, b.attack, b.release,
      b.ampCurve, b.level, b.harmonics, b.fm, b.fmTime, b.drive, b.fold,
      b.cutoff, b.resonance, b.sweep, b.phase);
  const auto &t = g.tail;
  put(static_cast<int>(t.kind), t.seed, t.hz, t.endHz, t.duration, t.decay,
      t.attack, t.release, t.level, t.motion, t.speed, t.cutoff, t.resonance,
      t.sweep, t.drive, t.phase);
  if (g.engine == universalVersion) {
    put(static_cast<int>(g.architecture), static_cast<int>(g.character),
        g.profileId.size(), g.profileId, g.profileRevision, g.profileDigest);
    put(s.renderVersion, s.endHz, s.glideTime, a.renderVersion, b.renderVersion,
        b.damping, b.inharmonicity, b.noise, t.renderVersion, t.glideTime,
        t.harmonics, t.gate);
    for (const auto *f : {&b.fx, &t.fx})
      put(static_cast<int>(f->kind), f->stageDrive, f->preCut, f->postCut,
          f->bias, f->multiband, f->crossover, f->lowDrive, f->highDrive,
          f->driveDecay, f->driveMotion, f->bits, f->holdHz);
  }
  return o.str();
}
std::string sha256(const std::string &input) {
  constexpr std::array<std::uint32_t, 64> k{
      0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1,
      0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
      0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786,
      0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
      0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
      0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
      0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
      0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
      0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a,
      0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
      0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
  std::array<std::uint32_t, 8> h{0x6a09e667, 0xbb67ae85, 0x3c6ef372,
                                 0xa54ff53a, 0x510e527f, 0x9b05688c,
                                 0x1f83d9ab, 0x5be0cd19};
  std::vector<unsigned char> bytes(input.begin(), input.end());
  const auto bits = static_cast<std::uint64_t>(bytes.size()) * 8;
  bytes.push_back(0x80);
  while (bytes.size() % 64 != 56)
    bytes.push_back(0);
  for (int i = 7; i >= 0; --i)
    bytes.push_back(static_cast<unsigned char>(bits >> (i * 8)));
  for (std::size_t offset = 0; offset < bytes.size(); offset += 64) {
    std::array<std::uint32_t, 64> w{};
    for (std::size_t i = 0; i < 16; ++i)
      for (int j = 0; j < 4; ++j)
        w[i] =
            (w[i] << 8) | bytes[offset + i * 4 + static_cast<std::size_t>(j)];
    for (std::size_t i = 16; i < 64; ++i) {
      const auto a = w[i - 15], b = w[i - 2];
      w[i] = w[i - 16] + (std::rotr(a, 7) ^ std::rotr(a, 18) ^ (a >> 3)) +
             w[i - 7] + (std::rotr(b, 17) ^ std::rotr(b, 19) ^ (b >> 10));
    }
    auto v = h;
    for (std::size_t i = 0; i < 64; ++i) {
      const auto t1 =
          v[7] +
          (std::rotr(v[4], 6) ^ std::rotr(v[4], 11) ^ std::rotr(v[4], 25)) +
          ((v[4] & v[5]) ^ (~v[4] & v[6])) + k[i] + w[i];
      const auto t2 =
          (std::rotr(v[0], 2) ^ std::rotr(v[0], 13) ^ std::rotr(v[0], 22)) +
          ((v[0] & v[1]) ^ (v[0] & v[2]) ^ (v[1] & v[2]));
      v = {t1 + t2, v[0], v[1], v[2], v[3] + t1, v[4], v[5], v[6]};
    }
    for (std::size_t i = 0; i < 8; ++i)
      h[i] += v[i];
  }
  std::ostringstream o;
  o << std::hex << std::setfill('0');
  for (auto v : h)
    o << std::setw(8) << v;
  return o.str();
}
void identify(Genome &g) {
  validate(g);
  g.id = std::string(g.engine == universalVersion ? "K4-" : "K3-") +
         sha256(canonical(g));
}
} // namespace kravatek::genetic
