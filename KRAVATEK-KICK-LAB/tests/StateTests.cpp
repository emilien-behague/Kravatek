#include "dsp/WavWriter.h"
#include "genetic/SessionIO.h"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <map>
#include <stdexcept>
void legacyRoundtrip();
using namespace kravatek::genetic;
namespace {
void check(bool v, const char *text) {
  if (!v)
    throw std::runtime_error(text);
}
template <class F> void rejects(F f) {
  bool failed = false;
  try {
    f();
  } catch (const std::invalid_argument &) {
    failed = true;
  }
  check(failed, "Malformed session accepted");
}
Session example() {
  Session s;
  auto parent = generateGenome(Mode::experimental, 101, .75);
  s.library.append(parent);
  auto child =
      evolveGenome(parent, Mode::mutant, 102, 1, {true, true, false, false});
  s.library.append(child);
  s.selected = 1;
  s.visible = {0, 1};
  s.compareA = 0;
  s.compareB = 1;
  s.settings.seed = 0xffffffffU;
  s.settings.locks = {true, true, false, false};
  s.settings.count = 8;
  s.settings.novelty = .81;
  return s;
}
void roundtrip() {
  auto s = example();
  for (std::uint32_t i = 0; i < 50; ++i)
    s.library.append(generateGenome(static_cast<Mode>(i % 5), i, i / 49.0));
  const auto text = encodeSession(s);
  auto loaded = decodeSession(text);
  check(encodeSession(loaded) == text, "Session JSON is not exactly stable");
  check(loaded.library.entries() == s.library.entries(),
        "Saved parameters/provenance changed");
  for (std::size_t i = 0; i < 2; ++i)
    for (auto rate : {44100., 48000.})
      check(render(s.library.at(i), rate).mix ==
                render(loaded.library.at(i), rate).mix,
            "Reload changed audio");
  auto single = decodeSession(encodeSound(s.library.at(1), 44100));
  check(single.library.at(0) == s.library.at(1) &&
            single.settings.sampleRate == 44100,
        "Associated sound file changed parameters");
}
void invalid() {
  auto text = encodeSession(example());
  rejects([] { decodeSession("{}"); });
  auto bad = text;
  auto pos = bad.find(engineVersion);
  bad.replace(pos, std::string(engineVersion).size(), "FUTURE");
  rejects([&] { decodeSession(bad); });
  bad = text;
  pos = bad.find("\"startHz\"");
  pos = bad.find(':', pos);
  const auto end = bad.find(',', pos);
  bad.replace(pos + 1, end - pos - 1, " 999999");
  rejects([&] { decodeSession(bad); });
  bad = text;
  pos = bad.find("\"id\"");
  pos = bad.find("K3-", pos);
  bad[pos + 3] = bad[pos + 3] == 'a' ? 'b' : 'a';
  rejects([&] { decodeSession(bad); });
}
void wav() {
  auto g = generateGenome(Mode::mutant, 12345, .95);
  auto audio = render(g, 44100);
  const auto json = encodeSound(g, 44100);
  const auto path = std::filesystem::path("genetic-export.wav");
  kravatek::writePcm24(path, audio.mix, 44100, json);
  std::ifstream in(path, std::ios::binary);
  std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(in)), {});
  in.close();
  const auto le = [&](std::size_t at, int count) {
    std::uint32_t v = 0;
    for (int i = 0; i < count; ++i)
      v |=
          static_cast<std::uint32_t>(bytes.at(at + static_cast<std::size_t>(i)))
          << (8 * i);
    return v;
  };
  check(le(4, 4) + 8 == bytes.size() && le(24, 4) == 44100 && le(34, 2) == 24,
        "Invalid genetic WAV header");
  for (std::size_t i = 0; i < audio.mix.size(); ++i) {
    auto raw = le(44 + 3 * i, 3);
    auto value =
        static_cast<std::int32_t>(raw) - ((raw & 0x800000U) ? 0x1000000 : 0);
    check(std::abs(static_cast<double>(value) / 8388607 - audio.mix[i]) <
              .000000061,
          "Genetic PCM mismatch");
  }
  const std::string content(bytes.begin(), bytes.end());
  check(content.find(json) != std::string::npos, "WAV genome metadata missing");
  std::filesystem::remove(path);
}
void history() {
  auto session = example();
  const auto parent = session.library.at(1), snapshot = parent;
  for (auto count : {4, 8, 12, 20}) {
    BatchRequest r;
    r.parent = parent;
    r.count = count;
    r.mode = Mode::mutant;
    r.novelty = 1;
    r.locks = {true, true, false, false};
    r.seed = static_cast<std::uint32_t>(count);
    auto children = population(r);
    check(children.size() == static_cast<std::size_t>(count),
          "Incorrect population count");
    for (const auto &child : children) {
      session.library.append(child.genome);
      check(child.genome.parents == std::vector<std::string>{parent.id},
            "Population lost parent");
      check(child.genome.sub == parent.sub &&
                child.genome.attack == parent.attack,
            "Population changed DNA");
    }
    check(parent == snapshot && session.library.at(1) == snapshot,
          "Population overwrote parent");
  }
  const auto saved = encodeSession(session);
  check(decodeSession(saved).library.entries() == session.library.entries(),
        "Genealogy history not restored");
  BatchRequest r;
  r.count = 4;
  r.seed = 77;
  const auto a = population(r), b = population(r);
  for (std::size_t i = 0; i < a.size(); ++i)
    check(a[i].genome == b[i].genome && a[i].audio.mix == b[i].audio.mix,
          "Population is not reproducible");
  std::stop_source stop;
  stop.request_stop();
  check(population(r, stop.get_token()).empty(),
        "Population ignores cancellation");
}
} // namespace
int main(int argc, char **argv) {
  try {
    const std::map<std::string, std::function<void()>> tests{
        {"legacy-preset-roundtrip", legacyRoundtrip},
        {"session-roundtrip", roundtrip},
        {"session-invalid", invalid},
        {"session-wav", wav},
        {"population-history", history}};
    if (argc != 2 || !tests.contains(argv[1]))
      throw std::runtime_error("Choose state test");
    tests.at(argv[1])();
    std::cout << "PASS " << argv[1] << '\n';
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "FAIL " << e.what() << '\n';
    return 1;
  }
}
