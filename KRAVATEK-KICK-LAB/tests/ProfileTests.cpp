#include "dsp/WavWriter.h"
#include "genetic/ProfileIO.h"
#include "genetic/SessionIO.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <set>
using namespace kravatek::genetic;
namespace {
void check(bool v, const char *m) {
  if (!v)
    throw std::runtime_error(m);
}
template <class F> void rejects(F f) {
  bool caught = false;
  try {
    f();
  } catch (const std::invalid_argument &) {
    caught = true;
  }
  check(caught, "Malformed profile accepted");
}
void catalog() {
  auto ps = bundledProfiles();
  check(ps.size() == 8, "Expected eight operational profiles");
  for (const auto &p : ps) {
    check(decodeProfile(encodeProfile(p)) == p,
          "Profile snapshot loses precision");
    auto r = UniversalRequest{p};
    for (int i = 0; i < 40; ++i) {
      auto g = generateUniversal(r, static_cast<std::uint32_t>(i), i / 39.);
      validate(g);
      check(g == generateUniversal(r, static_cast<std::uint32_t>(i), i / 39.),
            "Profile parameters nondeterministic");
    }
  }
  auto p = ps.front();
  p.ranges.front().low = -1;
  rejects([&] { validateProfile(p); });
  p = ps.front();
  p.ranges.front().parameter = "unknown";
  rejects([&] { validateProfile(p); });
  p = ps.front();
  p.architectures[0] = .1;
  rejects([&] { validateProfile(p); });
  rejects([] { decodeCatalog("{}"); });
  rejects([] { decodeProfile("{}"); });
  auto r = UniversalRequest{ps[0]};
  r.profile.ranges[0].high += .01;
  rejects([&] { generateUniversal(r, 1, .5); });
}
void campaign() {
  auto ps = bundledProfiles();
  std::vector<Features> bases;
  std::set<std::string> hashes;
  const auto root = std::filesystem::path("universal-reference");
  std::filesystem::create_directories(root);
  std::ofstream report(root / "measurements.csv");
  report << "profile,seed,rate,seconds,effectiveSeconds,f0,confidence,peakDb,"
            "rmsDb,centroidHz,lowFraction,midFraction,highFraction,"
            "transientFraction,safetyGain\n";
  int count = 0;
  for (const auto &p : ps) {
    UniversalRequest request{p};
    for (int i = 0; i < 3; ++i) {
      auto g = generateUniversal(request, static_cast<std::uint32_t>(i + 1),
                                 .2 + .4 * i);
      for (auto rate : {44100., 48000.}) {
        auto r = render(g, rate);
        auto f = measure(r.mix, rate, true);
        check(f.basic.peakDb <= -.999 && f.basic.peakDb > -60,
              "Invalid profile energy/headroom");
        check(std::abs(f.basic.dc) < 2e-7, "Profile DC offset");
        check(r.mix.front() == 0 && r.mix.back() == 0,
              "Profile edge discontinuity");
        for (auto x : r.mix)
          check(std::isfinite(x), "Profile nonfinite sample");
        check(
            r.mix ==
                render(generateUniversal(request, g.seed, g.novelty), rate).mix,
            "Profile signal not repeatable");
        auto saved = decodeSession(encodeSound(g, rate));
        check(render(saved.library.at(0), rate).mix == r.mix,
              "Profile saved signal changed");
        const auto stem = p.id + "-" + std::to_string(i + 1) + "-" +
                          std::to_string(static_cast<int>(rate));
        kravatek::writePcm24(root / (stem + ".wav"), r.mix, rate,
                             encodeSound(g, rate));
        std::ofstream(root / (stem + ".krava.json")) << encodeSound(g, rate);
        report << p.id << ',' << g.seed << ',' << rate << ','
               << f.basic.durationSeconds << ',' << f.effectiveSeconds << ','
               << f.fundamentalHz << ',' << f.fundamentalConfidence << ','
               << f.basic.peakDb << ',' << f.basic.rmsDb << ',' << f.centroidHz
               << ',' << f.bandEnergy[0] << ',' << f.bandEnergy[1] << ','
               << f.bandEnergy[2] << ',' << f.transientFraction << ','
               << r.safetyGain << '\n';
        if (rate == 48000) {
          hashes.insert(
              sha256(std::string(reinterpret_cast<const char *>(r.mix.data()),
                                 r.mix.size() * sizeof(float))));
          if (i == 1)
            bases.push_back(f);
        }
        ++count;
      }
    }
  }
  check(hashes.size() == 24, "Profiles contain exact duplicate sounds");
  std::ofstream distance(root / "profile-distances.csv");
  distance << "profileA,profileB,descriptorDistance\n";
  double total = 0;
  int pairs = 0;
  for (std::size_t i = 0; i < ps.size(); ++i)
    for (std::size_t j = 0; j < i; ++j) {
      auto d = featureDistance(bases[i], bases[j]);
      check(d > .015, "Reference profile descriptors collapsed");
      distance << ps[i].id << ',' << ps[j].id << ',' << d << '\n';
      total += d;
      ++pairs;
    }
  check(bases[6].effectiveSeconds > bases[2].effectiveSeconds * 2,
        "Long 808 not distinct from short psy");
  check(bases[5].bandEnergy[1] > bases[0].bandEnergy[1],
        "Hardcore not richer in midrange than house reference");
  check(report.good() && distance.good(),
        "Reference reports could not be written");
  std::cout << count
            << " renders; 24 distinct audios at 48 kHz; mean profile distance "
            << total / pairs << '\n';
}
void locks() {
  auto profiles = bundledProfiles();
  for (const auto &p : profiles) {
    auto parent = generateUniversal({p}, 31, .6), snapshot = parent;
    for (auto rate : {44100., 48000.}) {
      auto before = render(parent, rate);
      for (auto l :
           {Locks{true, true, false, false}, Locks{false, false, true, true},
            Locks{true, true, true, true}}) {
        auto child = evolveUniversal(parent, {profiles[5]}, 55, 1, l);
        auto r = render(child, rate);
        check(child.parents == std::vector<std::string>{parent.id},
              "Universal parent lost");
        if (l.sub)
          check(child.sub == parent.sub && r.layers[0] == before.layers[0],
                "Locked sub changed");
        if (l.attack)
          check(child.attack == parent.attack &&
                    r.layers[1] == before.layers[1],
                "Locked attack changed");
        if (l.body)
          check(child.body == parent.body && r.layers[2] == before.layers[2],
                "Locked body changed");
        if (l.tail)
          check(child.tail == parent.tail && r.layers[3] == before.layers[3],
                "Locked tail changed");
      }
      check(render(evolveUniversal(parent, {profiles[4]}, 99, 1, {}, 0), rate)
                    .mix == before.mix,
            "Zero evolution changes universal signal");
    }
    check(parent == snapshot, "Universal parent overwritten");
  }
  auto old = generateGenome(Mode::experimental, 90, .7);
  auto child =
      evolveUniversal(old, {profiles[7]}, 91, 1, {true, true, false, false});
  check(render(old).layers[0] == render(child).layers[0] &&
            render(old).layers[1] == render(child).layers[1],
        "Cross-version locks changed samples");
}
void persistence() {
  auto ps = bundledProfiles();
  Session s;
  for (const auto &p : ps)
    s.visible.push_back(
        static_cast<int>(s.library.append(generateUniversal({p}, 71, .75))));
  s.selected = 6;
  s.settings.universal =
      UniversalRequest{ps[6], Architecture::count, Character::warm, 33};
  s.settings.count = 4;
  s.settings.seed = 77;
  auto json = encodeSession(s);
  auto loaded = decodeSession(json);
  check(encodeSession(loaded) == json &&
            loaded.settings.universal == s.settings.universal,
        "Universal recipe not exactly persisted");
  auto a = population(s.settings), b = population(loaded.settings);
  for (std::size_t i = 0; i < a.size(); ++i)
    check(a[i].genome == b[i].genome && a[i].audio.mix == b[i].audio.mix,
          "Saved profile recipe not repeatable");
  // Snapshot is independent of a subsequent catalogue edit.
  auto changed = ps[6];
  changed.ranges[0].high *= .9;
  identifyProfile(changed);
  check(changed.digest != loaded.settings.universal->profile.digest,
        "Catalogue edit invisible to fingerprint");
}
void characters() {
  auto ps = bundledProfiles();
  auto base = generateUniversal({ps[0]}, 99, .5);
  const auto audio = render(base).mix;
  for (int i = 1; i < static_cast<int>(Character::count); ++i) {
    UniversalRequest r{ps[0]};
    r.character = static_cast<Character>(i);
    auto g = generateUniversal(r, 99, .5);
    check(render(g).mix != audio, "Character has no audio effect");
  }
  for (int i = 0; i < 6; ++i) {
    UniversalRequest r{ps[0]};
    r.overrideArchitecture = static_cast<Architecture>(i);
    auto g = generateUniversal(r, 1, .4);
    validate(g);
    if (i == 4)
      check(g.body.kind == BodyKind::membrane, "Architecture override ignored");
  }
}
} // namespace
int main(int argc, char **argv) {
  try {
    std::map<std::string, std::function<void()>> t{
        {"profile-catalog", catalog},
        {"profile-campaign", campaign},
        {"profile-locks", locks},
        {"profile-persistence", persistence},
        {"profile-characters", characters}};
    if (argc != 2)
      throw std::invalid_argument("Choose test");
    t.at(argv[1])();
    std::cout << "PASS " << argv[1] << '\n';
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "FAIL " << e.what() << '\n';
    return 1;
  }
}
