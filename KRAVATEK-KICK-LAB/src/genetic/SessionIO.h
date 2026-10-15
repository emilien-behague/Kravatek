#pragma once
#include "Population.h"
#include <string_view>
namespace kravatek::genetic {
// Versioned JSON; Floating genes use 17-significant-digit decimal strings and
// std::from_chars; this avoids loss in JUCE JSON numeric conversion for tiny
// envelope values.
std::string encodeSession(const Session &);
Session decodeSession(std::string_view);
std::string
encodeSound(const Genome &, double sampleRate = 48000,
            const std::optional<UniversalRequest> &recipe = std::nullopt);
} // namespace kravatek::genetic
