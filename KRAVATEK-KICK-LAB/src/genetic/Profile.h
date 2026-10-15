#pragma once
#include "UniversalDsp.h"
#include <span>
namespace kravatek::genetic {
enum class Distribution { uniform, logarithmic, triangular };
struct ParameterSpec {
  const char *name;
  double low, high;
  double &(*value)(Genome &);
};
struct ProfileRange {
  std::string parameter;
  double low = 0, high = 1;
  Distribution distribution = Distribution::uniform;
  bool operator==(const ProfileRange &) const = default;
};
struct GenreProfile {
  std::string id, name, family, digest;
  std::uint32_t revision = 1;
  std::vector<std::string> parents;
  double bpmLow = 100, bpmHigh = 150, fundamentalLow = 40, fundamentalHigh = 65;
  double subGlide = 0, tailInterval = 0;
  std::array<double, 6> architectures{1, 0, 0, 0, 0, 0},
      bodyWeights{1, 0, 0, 0, 0, 0}, tailWeights{1, 0, 0, 0, 0, 0};
  std::array<double, 4> attackWeights{1, 0, 0, 0};
  std::array<double, 8> distortions{0, 0, 1, 0, 0, 0, 0, 0};
  std::vector<ProfileRange> ranges;
  bool operator==(const GenreProfile &) const = default;
};
struct UniversalRequest {
  GenreProfile profile;
  Architecture overrideArchitecture = Architecture::count;
  Character character = Character::neutral;
  int note = 0; // 0: profile tuning, otherwise MIDI note number 24..60.
  bool operator==(const UniversalRequest &) const = default;
};
std::span<const ParameterSpec> parameterSpecs();
const ParameterSpec &parameterSpec(const std::string &);
void validateProfile(const GenreProfile &);
std::string profileCanonical(const GenreProfile &);
void identifyProfile(GenreProfile &);
Genome generateUniversal(const UniversalRequest &, std::uint32_t seed,
                         double novelty);
Genome evolveUniversal(const Genome &, const UniversalRequest &,
                       std::uint32_t seed, double novelty, Locks,
                       double strength = 1);
} // namespace kravatek::genetic
