#include "EnvelopeEditor.h"

EnvelopeEditor::EnvelopeEditor() {
    for (auto* button : {&pitch_, &amp_, &wave_}) addAndMakeVisible(button);
    pitch_.setClickingTogglesState(true); amp_.setClickingTogglesState(true); wave_.setClickingTogglesState(true);
    pitch_.setRadioGroupId(934); amp_.setRadioGroupId(934);
    pitch_.setToggleState(true, juce::dontSendNotification);
    wave_.setToggleState(true, juce::dontSendNotification);
    pitch_.onClick = [this] {setAmplitudeMode(false);};
    amp_.onClick = [this] {setAmplitudeMode(true);};
    wave_.onClick = [this] {repaint();};
    setTooltip(juce::String::fromUTF8("Glisse les trois points cyan. PITCH : départ, chute/courbure, fin. AMP : attaque, forme, extinction."));
}
void EnvelopeEditor::setKick(const kravatek::RenderedKick& kick) {samples_ = kick.samples; setParameters(kick.parameters);}
void EnvelopeEditor::setParameters(const kravatek::KickParameters& p) {parameters_ = p; repaint();}
void EnvelopeEditor::setAmplitudeMode(bool amplitude) {
    amplitude_ = amplitude;
    pitch_.setToggleState(!amplitude, juce::dontSendNotification);
    amp_.setToggleState(amplitude, juce::dontSendNotification);
    repaint();
}
juce::Rectangle<float> EnvelopeEditor::graph() const {
    return getLocalBounds().toFloat().reduced(24, 0).withTrimmedTop(66).withTrimmedBottom(37);
}
float EnvelopeEditor::frequencyY(double frequency) const {
    const auto relative = std::clamp(std::log(frequency / 20) / std::log(100.0), 0.0, 1.0);
    return graph().getBottom() - static_cast<float>(relative) * graph().getHeight();
}
double EnvelopeEditor::frequencyAtY(float y) const {
    const auto relative = std::clamp(static_cast<double>((graph().getBottom() - y) / graph().getHeight()), 0.0, 1.0);
    return 20 * std::pow(100.0, relative);
}
std::array<juce::Point<float>, 3> EnvelopeEditor::handles() const {
    const auto b = graph(); const auto& p = parameters_;
    if (!amplitude_) {
        const auto t = .35 * p.pitchDropSeconds;
        return {{{b.getX() + 2, frequencyY(p.startHz)},
                 {b.getX() + static_cast<float>(t / p.durationSeconds) * b.getWidth(), frequencyY(kravatek::pitchAt(p, t))},
                 {b.getRight() - 2, frequencyY(p.endHz)}}};
    }
    return {{{b.getX() + std::max(8.0f, static_cast<float>(p.attackSeconds / p.durationSeconds) * b.getWidth()), b.getY() + 2},
             {b.getX() + b.getWidth() * .35f, b.getBottom() - static_cast<float>(kravatek::amplitudeAt(p, p.durationSeconds * .35)) * b.getHeight()},
             {b.getRight() - static_cast<float>(p.releaseSeconds / p.durationSeconds) * b.getWidth(), b.getBottom() - 3}}};
}
void EnvelopeEditor::resized() {
    auto r = getLocalBounds().withHeight(32).reduced(7, 4);
    wave_.setBounds(r.removeFromRight(68)); r.removeFromRight(8);
    amp_.setBounds(r.removeFromRight(64)); r.removeFromRight(4);
    pitch_.setBounds(r.removeFromRight(72));
}
void EnvelopeEditor::paint(juce::Graphics& g) {
    krui::frame(g, getLocalBounds(), "OSC / BODY ENVELOPE - DRAG TO SCULPT");
    const auto b = graph();
    if (b.getWidth() < 1 || b.getHeight() < 1) return;
    g.setColour(krui::dark); g.fillRect(b);
    krui::grid(g, b.toNearestInt(), 24);
    g.setFont(krui::mono(11)); g.setColour(krui::muted);
    g.drawText(amplitude_ ? "BODY AMP / ATTACK - SHAPE - RELEASE" : "BODY PITCH / START - BEND - END",
               24, 37, getWidth() - 48, 23, juce::Justification::centredLeft);
    const auto middle = b.getCentreY();
    g.setColour(krui::cyan.withAlpha(.18f)); g.drawLine(b.getX(), middle, b.getRight(), middle);
    juce::Path waveform, envelope;
    const auto columns = std::max(2, static_cast<int>(b.getWidth()));
    for (int x = 0; x < columns; ++x) {
        const auto t = parameters_.durationSeconds * x / (columns - 1);
        const auto px = b.getX() + static_cast<float>(x);
        const auto py = amplitude_ ? b.getBottom() - static_cast<float>(kravatek::amplitudeAt(parameters_, t)) * b.getHeight()
                                  : frequencyY(kravatek::pitchAt(parameters_, t));
        if (x == 0) envelope.startNewSubPath(px, py); else envelope.lineTo(px, py);
        if (!samples_.empty()) {
            const auto first = std::min(samples_.size() - 1, static_cast<std::size_t>(x) * samples_.size() / static_cast<std::size_t>(columns));
            const auto last = std::min(samples_.size(), std::max(first + 1,
                static_cast<std::size_t>(x + 1) * samples_.size() / static_cast<std::size_t>(columns)));
            auto mm = std::minmax_element(samples_.begin() + static_cast<std::ptrdiff_t>(first), samples_.begin() + static_cast<std::ptrdiff_t>(last));
            if (wave_.getToggleState()) {
                g.setColour(krui::pink.withAlpha(.6f));
                g.drawLine(px, middle - *mm.first * b.getHeight() * .48f, px, middle - *mm.second * b.getHeight() * .48f);
            }
            const auto wy = middle - samples_[first] * b.getHeight() * .48f;
            if (x == 0) waveform.startNewSubPath(px, wy); else waveform.lineTo(px, wy);
        }
    }
    if (wave_.getToggleState()) {
        g.setColour(krui::pink.withAlpha(.06f)); g.strokePath(waveform, juce::PathStrokeType(6));
        g.setColour(krui::pink); g.strokePath(waveform, juce::PathStrokeType(1.3f));
    }
    g.setColour(krui::cyan.withAlpha(.08f)); g.strokePath(envelope, juce::PathStrokeType(7));
    g.setColour(krui::cyan); g.strokePath(envelope, juce::PathStrokeType(2));
    const auto points = handles();
    for (int i = 0; i < 3; ++i) {
        const auto point = points[static_cast<std::size_t>(i)];
        g.setColour(krui::cyan.withAlpha(.15f)); g.fillEllipse(point.x - 10, point.y - 10, 20, 20);
        g.setColour(active_ == i ? krui::pink : krui::ink); g.fillEllipse(point.x - 5, point.y - 5, 10, 10);
        g.setColour(krui::cyan); g.drawEllipse(point.x - 5, point.y - 5, 10, 10, 2);
    }
    g.setColour(krui::muted); g.setFont(krui::mono(10));
    for (int i = 0; i <= 4; ++i)
        g.drawText(juce::String(parameters_.durationSeconds * 1000 * i / 4, 0) + " ms",
            static_cast<int>(b.getX() + (b.getWidth() - 48) * i / 4), static_cast<int>(b.getBottom()) + 6, 60, 18,
            juce::Justification::centredLeft);
}
void EnvelopeEditor::mouseDown(const juce::MouseEvent& event) {
    active_ = -1; const auto points = handles();
    for (int i = 0; i < 3; ++i)
        if (event.position.getDistanceFrom(points[static_cast<std::size_t>(i)]) < 16) active_ = i;
    dragStart_ = parameters_; dragOrigin_ = event.position; repaint();
}
void EnvelopeEditor::mouseDrag(const juce::MouseEvent& event) {if (active_ >= 0) applyDrag(event.position - dragOrigin_);}
void EnvelopeEditor::mouseUp(const juce::MouseEvent&) {active_ = -1; repaint();}
void EnvelopeEditor::mouseMove(const juce::MouseEvent& event) {
    bool over = false;
    for (auto point : handles()) over = over || point.getDistanceFrom(event.position) < 16;
    setMouseCursor(over ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor);
}
void EnvelopeEditor::dragHandle(int index, juce::Point<float> delta) {
    active_ = index; dragStart_ = parameters_; applyDrag(delta); active_ = -1; repaint();
}
void EnvelopeEditor::applyDrag(juce::Point<float> delta) {
    if (active_ < 0 || active_ > 2 || graph().getHeight() <= 0) return;
    auto p = dragStart_; const auto b = graph();
    if (!amplitude_) {
        if (active_ == 0) p.startHz = std::clamp(frequencyAtY(frequencyY(dragStart_.startHz) + delta.y), 120.0, 1600.0);
        if (active_ == 2) p.endHz = std::clamp(frequencyAtY(frequencyY(dragStart_.endHz) + delta.y), 30.0, 100.0);
        if (active_ == 1) {
            const auto t = .35 * dragStart_.pitchDropSeconds;
            p.pitchDropSeconds = std::clamp(dragStart_.pitchDropSeconds
                + static_cast<double>(delta.x / b.getWidth()) * dragStart_.durationSeconds / .35, .005, .180);
            const auto frequency = frequencyAtY(frequencyY(kravatek::pitchAt(dragStart_, t)) + delta.y);
            const auto ratio = std::clamp((frequency - p.endHz) / (p.startHz - p.endHz), .0001, .999);
            p.pitchCurve = std::clamp(std::log(-std::log(ratio) / std::log(100.0)) / std::log(.35), .4, 2.4);
        }
    } else {
        if (active_ == 0) p.attackSeconds = std::clamp(dragStart_.attackSeconds + delta.x * .00005, .0001, .006);
        if (active_ == 2) p.releaseSeconds = std::clamp(dragStart_.releaseSeconds - delta.x * .0001, .002, std::min(.035, p.durationSeconds / 4));
        if (active_ == 1) {
            const auto original = kravatek::amplitudeAt(dragStart_, dragStart_.durationSeconds * .35);
            const auto amplitude = std::clamp(original - delta.y / b.getHeight(), .002, .98);
            p.ampCurve = std::clamp(std::log(-std::log(amplitude) / 6) / std::log(.35), .45, 2.3);
        }
    }
    parameters_ = p; if (onEdit) onEdit(p); repaint();
}
