#include "PresetState.h"
#include "dsp/WavWriter.h"
#include <cmath>
#include <stdexcept>

namespace {
double number(const juce::var& object, const char* name) {
    const auto value = object[juce::Identifier(name)];
    if (!(value.isDouble() || value.isInt() || value.isInt64()) || !std::isfinite(static_cast<double>(value)))
        throw std::invalid_argument(std::string("Missing or invalid preset value: ") + name);
    return static_cast<double>(value);
}
std::uint32_t seed(const juce::var& object, const char* name) {
    const auto value = number(object, name);
    if (value < 0 || value > 4294967295.0 || value != std::floor(value))
        throw std::invalid_argument("Invalid preset seed");
    return static_cast<std::uint32_t>(value);
}
bool boolean(const juce::var& object, const char* name) {
    const auto value = object[juce::Identifier(name)];
    if (!value.isBool()) throw std::invalid_argument("Invalid DNA lock");
    return static_cast<bool>(value);
}
}

juce::String encodePreset(const PresetState& state) {
    kravatek::validate(state.parameters, 48000);
    juce::DynamicObject::Ptr root = new juce::DynamicObject;
    root->setProperty("schema", "KRAVATEK_PRESET_002");
    const auto description = kravatek::describe({state.parameters, 48000, {}});
    root->setProperty("parameters", juce::JSON::parse(juce::String::fromUTF8(description.c_str())));
    root->setProperty("style", state.style);
    root->setProperty("mutation", state.mutation); root->setProperty("novelty", state.novelty);
    root->setProperty("bpm", state.bpm);
    juce::DynamicObject::Ptr locks = new juce::DynamicObject;
    locks->setProperty("pitch", state.locks.pitch); locks->setProperty("amplitude", state.locks.amplitude);
    locks->setProperty("tone", state.locks.tone); locks->setProperty("transient", state.locks.transient);
    locks->setProperty("tail", state.locks.tail); locks->setProperty("filter", state.locks.filter); locks->setProperty("sub", state.locks.sub);
    root->setProperty("locks", juce::var(locks.get()));
    return juce::JSON::toString(juce::var(root.get()), false, 17);
}

PresetState decodePreset(const juce::String& json) {
    if (json.getNumBytesAsUTF8() > 65536) throw std::invalid_argument("Preset is too large");
    const auto root = juce::JSON::parse(json);
    if (!root.isObject() || root["schema"].toString() != "KRAVATEK_PRESET_002")
        throw std::invalid_argument("Not a KRAVATEK 002 preset");
    const auto object = root["parameters"];
    if (!object.isObject()) throw std::invalid_argument("Preset parameters are missing");
    PresetState state;
    auto& p = state.parameters;
    p.startHz = number(object, "startHz"); p.endHz = number(object, "endHz");
    p.pitchDropSeconds = number(object, "pitchDropSeconds"); p.durationSeconds = number(object, "durationSeconds");
    p.attackSeconds = number(object, "attackSeconds"); p.releaseSeconds = number(object, "releaseSeconds");
    p.peakDb = number(object, "peakDb"); p.seed = seed(object, "seed");
    p.pitchCurve = number(object, "pitchCurve"); p.ampCurve = number(object, "ampCurve");
    p.harmonics = number(object, "harmonics"); p.fmAmount = number(object, "fmAmount");
    p.driveDb = number(object, "driveDb"); p.subMix = number(object, "subMix");
    p.clickLevel = number(object, "clickLevel"); p.clickSeconds = number(object, "clickSeconds");
    p.transientSeed = seed(object, "transientSeed");
    p.fmSeconds = number(object, "fmSeconds"); p.shape = number(object, "shape");
    p.tailLevel = number(object, "tailLevel"); p.tailSeconds = number(object, "tailSeconds");
    p.tailRatio = number(object, "tailRatio"); p.tailMotion = number(object, "tailMotion");
    p.filterHz = number(object, "filterHz"); p.resonance = number(object, "resonance");
    p.filterSweep = number(object, "filterSweep"); p.subSeconds = number(object, "subSeconds");
    kravatek::validate(p, 48000);
    // Reject values that the editor cannot represent, rather than silently clamping its display.
    if (p.startHz < 120 || p.startHz > 1600 || p.endHz < 30 || p.endHz > 100
        || p.pitchDropSeconds > .180 || p.pitchCurve < .4 || p.pitchCurve > 2.4
        || p.durationSeconds < .080 || p.durationSeconds > 1 || p.attackSeconds > .006
        || p.releaseSeconds < .002 || p.releaseSeconds > .035 || p.ampCurve < .45 || p.ampCurve > 2.3
        || p.clickSeconds > .020 || p.peakDb != -3)
        throw std::invalid_argument("Preset is outside this instrument's editing ranges");
    const auto style = number(root, "style");
    if (style < 0 || style >= static_cast<int>(kravatek::KickStyle::count) || style != std::floor(style))
        throw std::invalid_argument("Invalid preset family");
    state.style = static_cast<int>(style);
    state.novelty = number(root, "novelty");
    state.mutation = number(root, "mutation"); state.bpm = number(root, "bpm");
    if (state.novelty < 0 || state.novelty > 1 || state.mutation < 0 || state.mutation > 1 || state.bpm < 80 || state.bpm > 220)
        throw std::invalid_argument("Invalid preview or mutation setting");
    const auto locks = root["locks"];
    state.locks = {boolean(locks, "pitch"), boolean(locks, "amplitude"), boolean(locks, "tone"), boolean(locks, "transient"),
                   boolean(locks, "tail"), boolean(locks, "filter"), boolean(locks, "sub")};
    return state;
}
