#include "dsp/KickSynth.h"
#include "dsp/WavWriter.h"
#include <cmath>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>

int main(int argc, char** argv) {
    try {
        using P = kravatek::KickParameters;
        const std::map<std::string, kravatek::KickStyle> styles {
            {"reference", kravatek::KickStyle::reference}, {"hard-techno", kravatek::KickStyle::hardTechno},
            {"hard-psy", kravatek::KickStyle::psy}, {"schranz", kravatek::KickStyle::schranz},
            {"hardcore", kravatek::KickStyle::hardcore}, {"deep", kravatek::KickStyle::deep},
            {"experimental", kravatek::KickStyle::experimental}, {"mutant", kravatek::KickStyle::mutant}
        };
        const std::map<std::string, std::pair<double P::*, double>> controls {
            {"--pitch-start", {&P::startHz,1}}, {"--pitch-end", {&P::endHz,1}},
            {"--pitch-drop", {&P::pitchDropSeconds,.001}}, {"--pitch-curve", {&P::pitchCurve,1}},
            {"--decay", {&P::durationSeconds,.001}}, {"--attack", {&P::attackSeconds,.001}},
            {"--amp-curve", {&P::ampCurve,1}}, {"--release", {&P::releaseSeconds,.001}},
            {"--drive", {&P::driveDb,1}}, {"--fm", {&P::fmAmount,1}}, {"--fm-time", {&P::fmSeconds,.001}},
            {"--harmonics", {&P::harmonics,1}}, {"--shape", {&P::shape,1}},
            {"--tail", {&P::tailLevel,1}}, {"--tail-time", {&P::tailSeconds,.001}},
            {"--tail-ratio", {&P::tailRatio,1}}, {"--tail-motion", {&P::tailMotion,1}},
            {"--filter", {&P::filterHz,1}}, {"--resonance", {&P::resonance,1}}, {"--sweep", {&P::filterSweep,1}},
            {"--click", {&P::clickLevel,1}}, {"--click-time", {&P::clickSeconds,.001}},
            {"--sub", {&P::subMix,1}}, {"--sub-time", {&P::subSeconds,.001}}
        };
        std::map<std::string,std::string> options;
        for (int i=1; i<argc; ++i) {
            const std::string arg=argv[i];
            if (arg=="--help") {
                std::cout << "kravatek-render --style reference|hard-techno|hard-psy|schranz|hardcore|deep|experimental|mutant\n"
                    "  [--seed UINT32] [--novelty 0..1] [--sample-rate HZ] [--output FILE.wav]\n"
                    "Times are milliseconds; levels/morphs are 0..1. Available manual overrides:\n";
                for (const auto& entry:controls) std::cout << "  " << entry.first << '\n';
                return 0;
            }
            if (i+1>=argc) throw std::invalid_argument("Missing option value: "+arg);
            if (!controls.contains(arg) && arg!="--seed" && arg!="--style" && arg!="--novelty"
                && arg!="--sample-rate" && arg!="--output") throw std::invalid_argument("Unknown option: "+arg);
            if (!options.emplace(arg,argv[++i]).second) throw std::invalid_argument("Repeated option: "+arg);
        }
        const auto numeric=[](const std::string& value) {
            std::size_t used=0; const auto number=std::stod(value,&used);
            if (used!=value.size() || !std::isfinite(number)) throw std::invalid_argument("Invalid numeric option");
            return number;
        };
        std::uint32_t seed=1;
        if (options.contains("--seed")) {
            const auto& text=options.at("--seed"); std::size_t used=0; const auto value=std::stoull(text,&used);
            if (used!=text.size() || value>0xffffffffULL || text.front()=='-') throw std::invalid_argument("Seed must be UINT32");
            seed=static_cast<std::uint32_t>(value);
        }
        const auto style=options.contains("--style") ? styles.at(options.at("--style")) : kravatek::KickStyle::reference;
        auto parameters=options.contains("--novelty") ? kravatek::generate(style,seed,numeric(options.at("--novelty")))
            : kravatek::seededPreset(style,seed);
        for (const auto& [name,control]:controls)
            if (options.contains(name)) parameters.*control.first=numeric(options.at(name))*control.second;
        const auto rate=options.contains("--sample-rate") ? numeric(options.at("--sample-rate")) : 48000;
        const auto outputText=options.contains("--output") ? options.at("--output") : "kravatek-kick.wav";
        const auto output=std::filesystem::path(std::u8string(outputText.begin(),outputText.end()));
        const auto kick=kravatek::renderKick(parameters,rate); kravatek::writeWav24(output,kick);
        std::cout << output.string() << '\n' << kravatek::describe(kick) << '\n'; return 0;
    } catch (const std::exception& error) {std::cerr << "Export failed: " << error.what() << '\n'; return 1;}
}
