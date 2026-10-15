#pragma once

#include "KickSynth.h"
#include <filesystem>
#include <string>
#include <span>

namespace kravatek {
std::string describe(const RenderedKick& kick);
// Mono, little-endian, PCM 24-bit; seed and all parameters in a LIST/ICMT chunk.
// File I/O belongs on a non-audio thread. Throws on write failure.
void writePcm24(const std::filesystem::path&, std::span<const float>, double sampleRate, const std::string& metadata);
void writeWav24(const std::filesystem::path& path, const RenderedKick& kick);
} // namespace kravatek
