#pragma once
#include "EnvelopeEditor.h"
#include "ParameterControl.h"
#include "PresetState.h"
#include "dsp/Playback.h"
#include "dsp/Analysis.h"
#include <juce_audio_devices/juce_audio_devices.h>
#include <optional>

class MainComponent final : public juce::Component, private juce::AudioIODeviceCallback,
                            private juce::AsyncUpdater, private juce::Timer {
public:
    MainComponent();
    ~MainComponent() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress&) override;
    void runSmokeTest(const juce::File& directory);
private:
    enum Control {start, end, drop, bend, length, attack, ampShape, release,
                  drive, fm, fmTime, harmonics, tailLevel, tailTime, tailRatio, tailMotion,
                  cutoff, resonance, sweep, shape, click, clickLength, sub, subTime, controlCount};
    PresetState state_;
    kravatek::Playback playback_;
    juce::AudioDeviceManager devices_;
    std::atomic<double> deviceRate_ {48000};
    std::atomic<std::uint64_t> callbacks_ {0};
    std::atomic<bool> audioActive_ {false};
    kravatek::RenderedKick kick_;
    std::vector<float> deviceSamples_;
    double renderedDeviceRate_ = 0;
    kravatek::AudioAnalysis analysis_;
    double comparisonGain_ = 1;
    std::vector<PresetState> history_;
    bool pendingRender_ = false, looping_ = false, showingA_ = false;
    std::optional<PresetState> comparisonA_, comparisonB_;
    krui::LookAndFeel lookAndFeel_;
    krui::Brand brand_;
    EnvelopeEditor editor_;
    std::array<ParameterControl, controlCount> controls_;
    juce::ComboBox factory_, historyBox_;
    juce::TextButton play_ {"PLAY"}, generate_ {"GENERATE"}, evolve_ {"EVOLVE"}, export_ {"EXPORT WAV"}, audio_ {"AUDIO"};
    juce::TextButton open_ {"OPEN"}, save_ {"SAVE"}, restore_ {"REGENERATE"};
    juce::TextButton storeA_ {"STORE A"}, compare_ {"A / B"}, loop_ {"LOOP"};
    std::array<juce::ToggleButton, 6> locks_;
    juce::ToggleButton subLock_ {"LOCK SUB"};
    juce::Slider mutation_, bpm_, novelty_;
    juce::TextEditor seed_;
    juce::Label presetLabel_, noveltyLabel_, mutationLabel_, bpmLabel_, seedLabel_, status_, compareLabel_, format_, help_, previewHelp_;
    std::array<juce::Rectangle<int>, 6> moduleBounds_;
    juce::Rectangle<int> presetBounds_, sideBounds_, previewBounds_, spectrumBounds_;
    std::unique_ptr<juce::FileChooser> chooser_;
    juce::TooltipWindow tooltips_ {this, 700};
    void syncControls();
    void controlChanged(Control);
    void requestRender();
    void renderNow();
    void timerCallback() override;
    void playKick();
    void queuePreview();
    void mutateKick();
    void generateKick();
    void remember();
    void reseed();
    void storeComparison();
    void toggleComparison();
    void exportTo(const juce::File&);
    void saveTo(const juce::File&);
    void loadFrom(const juce::File&);
    void chooseFile(int);
    void chooseAudio();
    void setStatus(const juce::String&, bool error = false);
    void handleAsyncUpdate() override;
    void audioDeviceAboutToStart(juce::AudioIODevice*) override;
    void audioDeviceStopped() override;
    void audioDeviceIOCallbackWithContext(const float* const*, int, float* const*, int, int,
                                         const juce::AudioIODeviceCallbackContext&) override;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
