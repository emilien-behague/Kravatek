#include "app/PresetState.h"
#include "dsp/WavWriter.h"
#include <stdexcept>
void legacyRoundtrip() {
  for (std::uint32_t seed = 0; seed < 100; ++seed) {
    PresetState s;
    s.style = static_cast<int>(seed % 8);
    s.parameters =
        kravatek::seededPreset(static_cast<kravatek::KickStyle>(s.style), seed);
    const auto loaded = decodePreset(encodePreset(s));
    if (kravatek::describe({s.parameters, 48000, {}}) !=
        kravatek::describe({loaded.parameters, 48000, {}}))
      throw std::runtime_error(
          "Legacy 002 parameters did not round-trip exactly");
    if (seed < 4 && kravatek::renderKick(s.parameters).samples !=
                        kravatek::renderKick(loaded.parameters).samples)
      throw std::runtime_error("Legacy 002 audio changed on reload");
  }
}
