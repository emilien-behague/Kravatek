#pragma once
#include "dsp/KickSynth.h"
#include <juce_core/juce_core.h>

struct PresetState {
    kravatek::KickParameters parameters = kravatek::factoryPreset(kravatek::KickStyle::hardTechno);
    kravatek::DnaLocks locks;
    int style = static_cast<int>(kravatek::KickStyle::hardTechno);
    double mutation = .4;
    double novelty = .35;
    double bpm = 150;
};

juce::String encodePreset(const PresetState& state);
PresetState decodePreset(const juce::String& json);
