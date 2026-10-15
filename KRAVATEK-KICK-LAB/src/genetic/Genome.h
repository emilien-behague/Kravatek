#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace kravatek::genetic {
inline constexpr const char *engineVersion = "KRAVA-GSE-003.1";
inline constexpr const char *universalVersion = "KRAVA-UKE-004.1";
enum class Architecture {
  analog,
  psyPunch,
  distortedHard,
  tonal,
  acoustic,
  experimental,
  count
};
enum class Character {
  neutral,
  deep,
  punchy,
  warm,
  clean,
  distorted,
  metallic,
  organic,
  bouncy,
  tonal,
  raw,
  experimental,
  count
};
enum class DistortionKind {
  soft,
  hard,
  tanh,
  polynomial,
  foldback,
  asymmetric,
  rectify,
  digital,
  count
};
enum class Mode { deep, hardPsy, schranz, experimental, mutant, count };
enum class BodyKind { sine, additive, fm, ring, membrane, saw, count };
enum class AttackKind { noise, tonal, harmonic, hybrid, count };
enum class TailKind {
  tonal,
  elastic,
  resonant,
  phaseWarp,
  gated,
  reverse,
  count
};

struct FxGenes {
  DistortionKind kind = DistortionKind::tanh;
  double stageDrive = 0, preCut = 16000, postCut = 16000, bias = .2;
  double multiband = 0, crossover = 180, lowDrive = 0, highDrive = 12;
  double driveDecay = .1, driveMotion = 0, bits = 12, holdHz = 48000;
  bool operator==(const FxGenes &) const = default;
};

struct SubGenes {
  int renderVersion = 3;
  double hz = 45, duration = .48, decay = .15, attack = .004, release = .016,
         level = .65, phase = 0;
  double endHz = 45, glideTime = .08;
  bool operator==(const SubGenes &) const = default;
};
struct AttackGenes {
  int renderVersion = 3;
  AttackKind kind = AttackKind::noise;
  std::uint32_t seed = 1;
  double hz = 1800, duration = .025, decay = .003, attack = .0003,
         release = .002, level = .2, colour = .6, phase = 0;
  bool operator==(const AttackGenes &) const = default;
};
struct BodyGenes {
  int renderVersion = 3;
  BodyKind kind = BodyKind::sine;
  std::uint32_t seed = 2;
  double startHz = 280, pivotHz = 90, endHz = 48, pivotTime = .012,
         settleTime = .060, curve = 1;
  double duration = .35, attack = .0007, release = .012, ampCurve = 1.3,
         level = .75;
  double harmonics = .2, fm = .4, fmTime = .03, drive = 8, fold = 0,
         cutoff = 16000, resonance = 0, sweep = 0, phase = 0;
  double damping = .5, inharmonicity = .2, noise = .05;
  FxGenes fx;
  bool operator==(const BodyGenes &) const = default;
};
struct TailGenes {
  int renderVersion = 3;
  TailKind kind = TailKind::tonal;
  std::uint32_t seed = 3;
  double hz = 48, endHz = 48, duration = .46, decay = .13, attack = .012,
         release = .018, level = .25;
  double motion = .1, speed = 7, cutoff = 7000, resonance = .1, sweep = 0,
         drive = 4, phase = 0;
  double glideTime = .09, harmonics = .2, gate = .65;
  FxGenes fx;
  bool operator==(const TailGenes &) const = default;
};
struct Locks {
  bool sub = false, attack = false, body = false, tail = false;
  bool operator==(const Locks &) const = default;
};
struct Genome {
  std::string engine = engineVersion;
  std::string id;
  std::string name = "Kick";
  std::vector<std::string> parents;
  std::uint32_t seed = 1;
  std::uint32_t generation = 0;
  Mode mode = Mode::deep;
  double novelty = .25;
  Locks locks;
  Architecture architecture = Architecture::analog;
  Character character = Character::neutral;
  std::string profileId, profileDigest;
  std::uint32_t profileRevision = 0;
  SubGenes sub;
  AttackGenes attack;
  BodyGenes body;
  TailGenes tail;
  bool operator==(const Genome &) const = default;
};

// SplitMix32, explicit uint32 arithmetic; independent streams per
// layer/operation.
class Random {
public:
  explicit Random(std::uint32_t seed) : state_(seed) {}
  std::uint32_t next() noexcept;
  double unit() noexcept;
  double between(double lo, double hi) noexcept;
  double logarithmic(double lo, double hi) noexcept;

private:
  std::uint32_t state_;
};
const char *modeName(Mode) noexcept;
const char *bodyName(BodyKind) noexcept;
const char *tailName(TailKind) noexcept;
const char *architectureName(Architecture) noexcept;
const char *characterName(Character) noexcept;
const char *distortionName(DistortionKind) noexcept;
void validate(const Genome &, double sampleRate = 48000);
Genome foundation(Mode);
// Canonical versioned representation, includes parents and seeds, excludes UI
// locks/name/id.
std::string canonical(const Genome &);
std::string sha256(const std::string &);
void identify(Genome &);
double bodyPitchAt(const BodyGenes &, double seconds) noexcept;
double bodyAmplitudeAt(const BodyGenes &, double seconds) noexcept;
} // namespace kravatek::genetic
