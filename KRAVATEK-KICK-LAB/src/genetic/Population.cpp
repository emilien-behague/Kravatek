#include "Population.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace kravatek::genetic {
std::vector<Candidate> population(const BatchRequest &request,
                                  std::stop_token stop,
                                  std::function<void(int, int)> progress) {
  if (request.count != 1 && request.count != 4 && request.count != 8 &&
      request.count != 12 && request.count != 20)
    throw std::invalid_argument("Count must be 1, 4, 8, 12 or 20");
  if (request.sampleRate != 44100 && request.sampleRate != 48000)
    throw std::invalid_argument("Population rate must be 44100 or 48000");
  // Validate even a cancelled/empty request before reporting completion.
  if (request.universal)
    generateUniversal(*request.universal, request.seed, request.novelty);
  else
    generateGenome(request.mode, request.seed, request.novelty);
  if (request.parent)
    validate(*request.parent, request.sampleRate);
  std::vector<Candidate> result;
  Random seeds(request.seed);
  const bool allLocked = request.parent && request.locks.sub &&
                         request.locks.attack && request.locks.body &&
                         request.locks.tail;
  for (int i = 0; i < request.count && !stop.stop_requested(); ++i) {
    Candidate best;
    double bestSimilarity = 2;
    const int tries = request.refine || allLocked ? 1 : 4;
    for (int attempt = 0; attempt < tries && !stop.stop_requested();
         ++attempt) {
      const auto seed = seeds.next();
      Candidate candidate;
      if (request.universal)
        candidate.genome =
            request.parent
                ? evolveUniversal(*request.parent, *request.universal, seed,
                                  request.novelty, request.locks,
                                  request.refine ? .1 : 1)
                : generateUniversal(*request.universal, seed, request.novelty);
      else
        candidate.genome =
            request.parent
                ? evolveGenome(*request.parent, request.mode, seed,
                               request.novelty, request.locks,
                               request.refine ? .1 : 1)
                : generateGenome(request.mode, seed, request.novelty);
      candidate.audio = render(candidate.genome, request.sampleRate, false);
      candidate.features = measure(candidate.audio.mix, request.sampleRate,
                                   request.universal.has_value());
      for (const auto &previous : result)
        candidate.nearestSimilarity =
            std::max(candidate.nearestSimilarity,
                     similarity(candidate.audio.mix, candidate.features,
                                previous.audio.mix, previous.features));
      candidate.attempts = attempt + 1;
      candidate.nearDuplicate = candidate.nearestSimilarity > .985;
      if (candidate.nearestSimilarity < bestSimilarity) {
        bestSimilarity = candidate.nearestSimilarity;
        best = std::move(candidate);
      }
      if (bestSimilarity <= .985)
        break;
    }
    if (best.genome.id.empty())
      break;
    result.push_back(std::move(best));
    if (progress)
      progress(static_cast<int>(result.size()), request.count);
  }
  return result;
}
std::size_t Library::append(const Genome &g) {
  auto checked = g;
  identify(checked);
  if (checked.id != g.id)
    throw std::invalid_argument("Genome identity mismatch");
  for (std::size_t i = 0; i < entries_.size(); ++i)
    if (entries_[i].id == g.id) {
      if (canonical(entries_[i]) != canonical(g))
        throw std::invalid_argument("Genome ID conflict");
      return i;
    }
  if (entries_.size() >= maximum)
    throw std::invalid_argument(
        "Library full (512 kicks). Save this session and start another.");
  entries_.push_back(g);
  return entries_.size() - 1;
}
void validateSession(const Session &session) {
  const auto size = static_cast<int>(session.library.entries().size());
  const auto index = [&](int value) { return value >= -1 && value < size; };
  if (!index(session.selected) || !index(session.compareA) ||
      !index(session.compareB) || session.visible.size() > Library::maximum)
    throw std::invalid_argument("Invalid library selection");
  for (auto value : session.visible)
    if (value < 0 || value >= size)
      throw std::invalid_argument("Invalid visible generation");
  if (!std::isfinite(session.bpm) || session.bpm < 80 || session.bpm > 220)
    throw std::invalid_argument("Invalid tempo");
  const auto &r = session.settings;
  if (r.universal) {
    validateProfile(r.universal->profile);
    auto p = r.universal->profile;
    identifyProfile(p);
    if (p.digest != r.universal->profile.digest ||
        r.universal->character < Character::neutral ||
        r.universal->character >= Character::count ||
        r.universal->overrideArchitecture < Architecture::analog ||
        r.universal->overrideArchitecture > Architecture::count ||
        (r.universal->note != 0 &&
         (r.universal->note < 24 || r.universal->note > 60)))
      throw std::invalid_argument("Invalid universal settings");
  }
  if (r.mode < Mode::deep || r.mode >= Mode::count ||
      !std::isfinite(r.novelty) || r.novelty < 0 || r.novelty > 1 ||
      (r.sampleRate != 44100 && r.sampleRate != 48000) ||
      (r.count != 1 && r.count != 4 && r.count != 8 && r.count != 12 &&
       r.count != 20))
    throw std::invalid_argument("Invalid generation settings");
  for (const auto &g : session.library.entries()) {
    validate(g);
    auto copy = g;
    identify(copy);
    if (copy.id != g.id)
      throw std::invalid_argument("Invalid saved identity");
    for (const auto &parent : g.parents)
      for (const auto &other : session.library.entries())
        if (other.id == parent && g.generation != other.generation + 1)
          throw std::invalid_argument("Invalid genealogy order");
  }
}
} // namespace kravatek::genetic
