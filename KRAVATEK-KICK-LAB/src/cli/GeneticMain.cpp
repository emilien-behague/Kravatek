#include "dsp/WavWriter.h"
#include "genetic/ProfileIO.h"
#include "genetic/SessionIO.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <stdexcept>
using namespace kravatek::genetic;
namespace {
std::filesystem::path utf8Path(const std::string &text) {
  return std::filesystem::path(std::u8string(text.begin(), text.end()));
}
} // namespace
int main(int argc, char **argv) {
  try {
    std::map<std::string, std::string> args;
    for (int i = 1; i < argc; ++i) {
      std::string key = argv[i];
      if (key == "--list-profiles") {
        args.emplace(key, "");
        continue;
      }
      if (key == "--help") {
        std::cout
            << "krava-genome-render [--profile ID] [--profiles CATALOG.json] "
               "[--list-profiles] [--architecture "
               "auto|analog|psy-punch|distorted-hard|tonal|acoustic|"
               "experimental] [--character "
               "profile|deep|punchy|warm|clean|distorted|metallic|organic|"
               "bouncy|tonal|raw|experimental] [--note 24..60] [--mode "
               "deep|hard-psy|schranz|experimental|mutant] [--seed "
               "UINT32] [--novelty 0..1] [--sample-rate 44100|48000] "
               "[--batch 1|4|8|12|20] [--input SESSION.json] [--index N] "
               "--output FILE.wav\n";
        return 0;
      }
      if (key != "--mode" && key != "--seed" && key != "--novelty" &&
          key != "--sample-rate" && key != "--batch" && key != "--input" &&
          key != "--index" && key != "--output" && key != "--profile" &&
          key != "--profiles" && key != "--architecture" &&
          key != "--character" && key != "--note")
        throw std::invalid_argument("Unknown option: " + key);
      if (i + 1 >= argc || !args.emplace(key, argv[++i]).second)
        throw std::invalid_argument("Missing or repeated option");
    }
    const auto numeric = [&](const char *key, double fallback) {
      if (!args.contains(key))
        return fallback;
      std::size_t used = 0;
      auto v = std::stod(args.at(key), &used);
      if (used != args.at(key).size() || !std::isfinite(v))
        throw std::invalid_argument("Invalid numeric option");
      return v;
    };
    auto catalog = bundledProfiles();
    if (args.contains("--profiles")) {
      const auto path = utf8Path(args.at("--profiles"));
      if (std::filesystem::file_size(path) > 1024 * 1024)
        throw std::invalid_argument("Catalog too large");
      std::ifstream in(path, std::ios::binary);
      if (!in)
        throw std::runtime_error("Cannot read catalog");
      catalog =
          decodeCatalog(std::string((std::istreambuf_iterator<char>(in)), {}));
    }
    if (args.contains("--list-profiles")) {
      for (const auto &p : catalog)
        std::cout << p.id << " / " << p.family << " / " << p.name << '\n';
      return 0;
    }
    if (!args.contains("--profile") &&
        (args.contains("--architecture") || args.contains("--character") ||
         args.contains("--note")))
      throw std::invalid_argument(
          "Architecture/character/note require --profile");
    Session session;
    if (args.contains("--input")) {
      const auto path = utf8Path(args.at("--input"));
      if (std::filesystem::file_size(path) > 16 * 1024 * 1024)
        throw std::invalid_argument("Session too large");
      std::ifstream in(path, std::ios::binary);
      if (!in)
        throw std::runtime_error("Cannot read session");
      const std::string data((std::istreambuf_iterator<char>(in)), {});
      session = decodeSession(data);
      const auto idx = numeric("--index", session.selected);
      if (idx < 0 ||
          idx >= static_cast<double>(session.library.entries().size()) ||
          idx != std::floor(idx))
        throw std::invalid_argument("Invalid sound index");
      session.selected = static_cast<int>(idx);
      if (args.contains("--sample-rate"))
        session.settings.sampleRate = numeric("--sample-rate", 48000);
      validateSession(session);
    } else {
      const std::map<std::string, Mode> modes{
          {"deep", Mode::deep},
          {"hard-psy", Mode::hardPsy},
          {"schranz", Mode::schranz},
          {"experimental", Mode::experimental},
          {"mutant", Mode::mutant}};
      session.settings.mode = args.contains("--mode")
                                  ? modes.at(args.at("--mode"))
                                  : Mode::experimental;
      if (args.contains("--profile")) {
        if (args.contains("--mode"))
          throw std::invalid_argument("Choose profile or legacy mode");
        UniversalRequest request;
        bool found = false;
        for (const auto &p : catalog)
          if (p.id == args.at("--profile")) {
            request.profile = p;
            found = true;
          }
        if (!found)
          throw std::invalid_argument("Unknown genre profile");
        if (args.contains("--architecture")) {
          const std::map<std::string, Architecture> kinds{
              {"auto", Architecture::count},
              {"analog", Architecture::analog},
              {"psy-punch", Architecture::psyPunch},
              {"distorted-hard", Architecture::distortedHard},
              {"tonal", Architecture::tonal},
              {"acoustic", Architecture::acoustic},
              {"experimental", Architecture::experimental}};
          request.overrideArchitecture = kinds.at(args.at("--architecture"));
        }
        if (args.contains("--character")) {
          const std::vector<std::string> names{
              "profile", "deep",      "punchy",   "warm",
              "clean",   "distorted", "metallic", "organic",
              "bouncy",  "tonal",     "raw",      "experimental"};
          const auto at =
              std::find(names.begin(), names.end(), args.at("--character"));
          if (at == names.end())
            throw std::invalid_argument("Unknown sound character");
          request.character = static_cast<Character>(at - names.begin());
        }
        const auto note = numeric("--note", 0);
        if (note != std::floor(note) || (note != 0 && (note < 24 || note > 60)))
          throw std::invalid_argument("Invalid tuning note");
        request.note = static_cast<int>(note);
        session.settings.universal = request;
      }
      const auto seed = numeric("--seed", 1), count = numeric("--batch", 1);
      if (seed < 0 || seed > 4294967295.0 || seed != std::floor(seed) ||
          count != std::floor(count) || count < 1 || count > 20)
        throw std::invalid_argument("Invalid seed or count");
      session.settings.seed = static_cast<std::uint32_t>(seed);
      session.settings.count = static_cast<int>(count);
      session.settings.novelty = numeric("--novelty", .75);
      session.settings.sampleRate = numeric("--sample-rate", 48000);
      if (count == 1) {
        session.selected = static_cast<int>(session.library.append(
            session.settings.universal
                ? generateUniversal(*session.settings.universal,
                                    session.settings.seed,
                                    session.settings.novelty)
                : generateGenome(session.settings.mode, session.settings.seed,
                                 session.settings.novelty)));
        session.visible = {session.selected};
      } else
        for (const auto &c : population(session.settings)) {
          session.visible.push_back(
              static_cast<int>(session.library.append(c.genome)));
          session.selected = session.visible.front();
        }
    }
    validateSession(session);
    const auto output = utf8Path(args.contains("--output") ? args.at("--output")
                                                           : "krava-kick.wav");
    const auto write = [&](const Genome &g, const std::filesystem::path &path) {
      auto r = render(g, session.settings.sampleRate, false);
      const auto json = encodeSound(g, session.settings.sampleRate,
                                    session.settings.universal);
      kravatek::writePcm24(path, r.mix, r.sampleRate, json);
      auto sidecar = path;
      sidecar.replace_extension(".krava.json");
      std::ofstream out(sidecar, std::ios::binary);
      out.exceptions(std::ios::failbit | std::ios::badbit);
      out << json;
    };
    if (args.contains("--batch") && session.settings.count > 1) {
      int number = 0;
      for (auto index : session.visible) {
        auto path = output.parent_path() / (output.stem().string() + "-" +
                                            std::to_string(++number) + ".wav");
        write(session.library.at(static_cast<std::size_t>(index)), path);
      }
    } else
      write(session.library.at(static_cast<std::size_t>(session.selected)),
            output);
    auto sessionPath = output;
    sessionPath.replace_extension(".session.json");
    std::ofstream out(sessionPath, std::ios::binary);
    out.exceptions(std::ios::failbit | std::ios::badbit);
    out << encodeSession(session);
    std::cout
        << "Exported " << output.string() << " / "
        << session.library.at(static_cast<std::size_t>(session.selected)).id
        << '\n';
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "Export failed: " << e.what() << '\n';
    return 1;
  }
}
