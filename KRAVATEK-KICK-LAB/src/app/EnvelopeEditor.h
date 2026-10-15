#pragma once
#include "InstrumentStyle.h"
#include "dsp/KickSynth.h"
#include <functional>

class EnvelopeEditor final : public juce::Component, public juce::SettableTooltipClient {
public:
    EnvelopeEditor();
    std::function<void(const kravatek::KickParameters&)> onEdit;
    void setKick(const kravatek::RenderedKick& kick);
    void setParameters(const kravatek::KickParameters& parameters);
    void setAmplitudeMode(bool amplitude);
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void dragHandle(int index, juce::Point<float> delta);
private:
    kravatek::KickParameters parameters_, dragStart_;
    std::vector<float> samples_;
    bool amplitude_ = false;
    int active_ = -1;
    juce::Point<float> dragOrigin_;
    juce::TextButton pitch_ {"PITCH"}, amp_ {"AMP"}, wave_ {"WAVE"};
    juce::Rectangle<float> graph() const;
    std::array<juce::Point<float>, 3> handles() const;
    void applyDrag(juce::Point<float> delta);
    float frequencyY(double frequency) const;
    double frequencyAtY(float y) const;
};
