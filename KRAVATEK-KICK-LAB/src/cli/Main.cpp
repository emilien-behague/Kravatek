#include "dsp/KickSynth.h"
#include "dsp/WavWriter.h"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

int main(int argc, char** argv) {
    try {
        kravatek::KickParameters parameters;
        double rate = 48000;
        std::filesystem::path output = "kravatek-kick.wav";
        // Seed first, manual overrides second: command-line order is immaterial.
        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "--help") {
                std::cout << "kravatek-render [--seed UINT32] [--pitch-end HZ] [--decay MS] "
                             "[--sample-rate HZ] [--output FILE.wav]\n";
                return 0;
            }
            if (i + 1 >= argc) throw std::invalid_argument("Missing option value");
            const std::string value = argv[++i];
            if (arg == "--seed") {
                std::size_t used = 0;
                const auto seed = std::stoull(value, &used);
                if (used != value.size() || seed > 0xffffffffULL || value.front() == '-')
                    throw std::invalid_argument("Seed must be a uint32");
                parameters = kravatek::variationFromSeed(static_cast<std::uint32_t>(seed));
            } else if (arg != "--pitch-end" && arg != "--decay" && arg != "--sample-rate" && arg != "--output") {
                throw std::invalid_argument("Unknown option: " + arg);
            }
        }
        for (int i = 1; i + 1 < argc; i += 2) {
            const std::string arg = argv[i];
            const std::string value = argv[i + 1];
            if (arg == "--output") output = std::filesystem::path(std::u8string(value.begin(), value.end()));
            else if (arg != "--seed") {
                std::size_t used = 0;
                const auto number = std::stod(value, &used);
                if (used != value.size() || !std::isfinite(number))
                    throw std::invalid_argument("Invalid number: " + arg);
                if (arg == "--pitch-end") parameters.endHz = number;
                if (arg == "--decay") parameters.durationSeconds = number / 1000.0;
                if (arg == "--sample-rate") rate = number;
            }
        }
        const auto kick = kravatek::renderKick(parameters, rate);
        kravatek::writeWav24(output, kick);
        std::cout << output.string() << '\n' << kravatek::describe(kick) << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Export failed: " << error.what() << '\n';
        return 1;
    }
}
