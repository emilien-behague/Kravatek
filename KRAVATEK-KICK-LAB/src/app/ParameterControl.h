#pragma once
#include "InstrumentStyle.h"

class ParameterControl final : public juce::Component {
public:
    juce::Slider slider;
    ParameterControl() {
        addAndMakeVisible(label_); addAndMakeVisible(slider);
        label_.setFont(juce::Font(juce::FontOptions(10.5f, juce::Font::bold)));
        label_.setColour(juce::Label::textColourId, krui::ink);
        label_.setJustificationType(juce::Justification::centred);
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 85, 22);
        slider.setRotaryParameters(juce::MathConstants<float>::pi * 1.2f,
                                  juce::MathConstants<float>::pi * 2.8f, true);
    }
    void configure(const juce::String& title, double low, double high, double step,
                   const juce::String& suffix, const juce::String& explanation) {
        label_.setText(title, juce::dontSendNotification);
        slider.setRange(low, high, step); slider.setTextValueSuffix(suffix);
        slider.setTooltip(explanation);
    }
    void resized() override {
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, std::min(85, getWidth() - 6), 22);
        auto r = getLocalBounds(); label_.setBounds(r.removeFromTop(17)); slider.setBounds(r);
    }
private:
    juce::Label label_;
};
