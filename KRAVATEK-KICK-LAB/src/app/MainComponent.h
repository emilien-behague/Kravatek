#pragma once

#include "dsp/KickSynth.h"
#include "dsp/Playback.h"
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_gui_basics/juce_gui_basics.h>

class WaveformComponent final : public juce::Component {
public:
    void setKick(const kravatek::RenderedKick& kick);
    void paint(juce::Graphics& graphics) override;
private:
    std::vector<float> samples_;
    double duration_ = .320;
};

class MainComponent final : public juce::Component,
                            private juce::AudioIODeviceCallback,
                            private juce::AsyncUpdater {
public:
    MainComponent();
    ~MainComponent() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    // Exercises the same GUI handlers and export path as normal use; no dialogs.
    void runSmokeTest(const juce::File& directory);
private:
    kravatek::Playback playback_;
    juce::AudioDeviceManager deviceManager_;
    std::atomic<double> deviceRate_ {48000.0};
    std::atomic<std::uint64_t> callbackCount_ {0};
    std::atomic<bool> audioActive_ {false};
    kravatek::KickParameters parameters_;
    kravatek::RenderedKick kick_;
    std::vector<float> deviceSamples_;
    double renderedDeviceRate_ = 0;
    juce::LookAndFeel_V4 lookAndFeel_;
    juce::Label title_, subtitle_, pitchLabel_, decayLabel_, seedLabel_, status_, format_;
    juce::TextButton play_ {"PLAY"}, generate_ {"GENERATE"}, export_ {"EXPORT WAV"}, audio_ {"AUDIO"};
    juce::Slider pitch_, decay_;
    juce::TextEditor seed_;
    WaveformComponent waveform_;
    std::unique_ptr<juce::FileChooser> chooser_;

    void regenerate();
    void syncControls();
    void playKick();
    void applySeed();
    void chooseExport();
    void exportTo(const juce::File& file);
    void chooseAudio();
    void setStatus(const juce::String& text, bool error = false);
    void handleAsyncUpdate() override;
    void audioDeviceAboutToStart(juce::AudioIODevice*) override;
    void audioDeviceStopped() override;
    void audioDeviceIOCallbackWithContext(const float* const*, int, float* const*, int, int,
                                        const juce::AudioIODeviceCallbackContext&) override;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
