#pragma once
#include "Features.h"
#include "Generator.h"
#include "LayerRenderer.h"
#include "Profile.h"
#include <functional>
#include <optional>
#include <stop_token>
namespace kravatek::genetic {
struct BatchRequest {
  Mode mode = Mode::experimental;
  std::uint32_t seed = 1;
  double novelty = .75, sampleRate = 48000;
  int count = 20;
  bool refine = false;
  Locks locks;
  std::optional<Genome> parent;
  std::optional<UniversalRequest> universal;
};
struct Candidate {
  Genome genome;
  Render audio;
  Features features;
  int attempts = 1;
  double nearestSimilarity = 0;
  bool nearDuplicate = false;
};
std::vector<Candidate> population(const BatchRequest &, std::stop_token = {},
                                  std::function<void(int, int)> progress = {});
class Library {
public:
  static constexpr std::size_t maximum = 512;
  const std::vector<Genome> &entries() const noexcept { return entries_; }
  const Genome &at(std::size_t index) const { return entries_.at(index); }
  std::size_t append(const Genome &);

private:
  std::vector<Genome> entries_;
};
struct Session {
  Library library;
  std::vector<int> visible;
  int selected = -1, compareA = -1, compareB = -1;
  BatchRequest settings;
  double bpm = 150;
  bool loop = true, matchLevel = true, sidecar = true;
};
void validateSession(const Session &);
} // namespace kravatek::genetic
