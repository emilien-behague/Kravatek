#include "WavWriter.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <stdexcept>

namespace kravatek {
namespace {
void littleEndian(std::ostream& out, std::uint32_t value, int bytes) {
    for (int i = 0; i < bytes; ++i)
        out.put(static_cast<char>((value >> (8 * i)) & 0xffU));
}
}

std::string describe(const RenderedKick& k) {
    const auto& p = k.parameters;
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::setprecision(17)
        << "{\"engine\":\"KRAVATEK-002\",\"seed\":" << p.seed
        << ",\"sampleRate\":" << k.sampleRate
        << ",\"startHz\":" << p.startHz << ",\"endHz\":" << p.endHz
        << ",\"pitchDropSeconds\":" << p.pitchDropSeconds
        << ",\"durationSeconds\":" << p.durationSeconds
        << ",\"attackSeconds\":" << p.attackSeconds
        << ",\"releaseSeconds\":" << p.releaseSeconds
        << ",\"peakDb\":" << p.peakDb
        << ",\"pitchCurve\":" << p.pitchCurve << ",\"ampCurve\":" << p.ampCurve
        << ",\"harmonics\":" << p.harmonics << ",\"fmAmount\":" << p.fmAmount
        << ",\"driveDb\":" << p.driveDb << ",\"subMix\":" << p.subMix
        << ",\"clickLevel\":" << p.clickLevel << ",\"clickSeconds\":" << p.clickSeconds
        << ",\"transientSeed\":" << p.transientSeed
        << ",\"fmSeconds\":" << p.fmSeconds << ",\"shape\":" << p.shape
        << ",\"tailLevel\":" << p.tailLevel << ",\"tailSeconds\":" << p.tailSeconds
        << ",\"tailRatio\":" << p.tailRatio << ",\"tailMotion\":" << p.tailMotion
        << ",\"filterHz\":" << p.filterHz << ",\"resonance\":" << p.resonance
        << ",\"filterSweep\":" << p.filterSweep << ",\"subSeconds\":" << p.subSeconds << "}";
    return out.str();
}

void writeWav24(const std::filesystem::path& path, const RenderedKick& k) {
    validate(k.parameters, k.sampleRate);
    writePcm24(path, k.samples, k.sampleRate, describe(k));
}

void writePcm24(const std::filesystem::path& path, std::span<const float> samples, double sampleRate, const std::string& metadata) {
    if (!std::isfinite(sampleRate) || sampleRate < 8000 || sampleRate > 192000 || metadata.size() > 16*1024*1024)
        throw std::invalid_argument("Invalid WAV rate or metadata");
    if (samples.empty() || samples.size() > 768000
        || std::round(sampleRate) != sampleRate)
        throw std::invalid_argument("Invalid WAV buffer or non-integer sample rate");
    for (const auto sample : samples)
        if (!std::isfinite(sample) || std::abs(sample) > 1.0f)
            throw std::invalid_argument("WAV contains invalid audio");

    const auto dataSize = static_cast<std::uint32_t>(samples.size() * 3);
    const auto comment = metadata + '\0';
    const auto commentSize = static_cast<std::uint32_t>(comment.size());
    const auto listSize = 4U + 8U + commentSize + (commentSize & 1U);
    const auto riffSize = 36U + dataSize + (dataSize & 1U) + 8U + listSize;
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.exceptions(std::ios::failbit | std::ios::badbit);
    out.write("RIFF", 4); littleEndian(out, riffSize, 4); out.write("WAVEfmt ", 8);
    littleEndian(out, 16, 4); littleEndian(out, 1, 2); littleEndian(out, 1, 2);
    const auto rate = static_cast<std::uint32_t>(sampleRate);
    littleEndian(out, rate, 4); littleEndian(out, rate * 3, 4);
    littleEndian(out, 3, 2); littleEndian(out, 24, 2);
    out.write("data", 4); littleEndian(out, dataSize, 4);
    for (const auto sample : samples) {
        const auto pcm = static_cast<std::int32_t>(std::llround(static_cast<double>(sample) * 8388607.0));
        littleEndian(out, static_cast<std::uint32_t>(pcm), 3);
    }
    if ((dataSize & 1U) != 0U) out.put('\0');
    out.write("LIST", 4); littleEndian(out, listSize, 4); out.write("INFOICMT", 8);
    littleEndian(out, commentSize, 4);
    out.write(comment.data(), static_cast<std::streamsize>(comment.size()));
    if ((commentSize & 1U) != 0U) out.put('\0');
    out.close();
}
} // namespace kravatek
