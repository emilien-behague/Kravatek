#include "GeneticGraph.h"
using namespace kravatek::genetic;
GeneticGraph::GeneticGraph() {
  for (auto *b : {&pitch_, &amp_, &wave_, &zoom_}) {
    addAndMakeVisible(b);
    b->setClickingTogglesState(true);
    b->setWantsKeyboardFocus(false);
  }
  pitch_.setRadioGroupId(9003);
  amp_.setRadioGroupId(9003);
  pitch_.setToggleState(true, juce::dontSendNotification);
  wave_.setToggleState(true, juce::dontSendNotification);
  pitch_.onClick = [this] { setAmplitudeMode(false); };
  amp_.onClick = [this] { setAmplitudeMode(true); };
  wave_.onClick = [this] { repaint(); };
  zoom_.onClick = [this] { repaint(); };
  zoom_.setTooltip(
      juce::String::fromUTF8("Agrandir la descente de pitch. Les points "
                             "modifient réellement le body."));
}
void GeneticGraph::setSound(const Genome &g, std::span<const float> samples,
                            double rate) {
  body_ = g.body;
  rate_ = rate;
  samples_.assign(samples.begin(), samples.end());
  duration_ =
      std::max(g.body.duration, static_cast<double>(samples.size()) / rate);
  repaint();
}
void GeneticGraph::setAmplitudeMode(bool value) {
  amplitude_ = value;
  pitch_.setToggleState(!value, juce::dontSendNotification);
  amp_.setToggleState(value, juce::dontSendNotification);
  repaint();
}
juce::Rectangle<float> GeneticGraph::graph() const {
  return getLocalBounds()
      .toFloat()
      .reduced(42, 0)
      .withTrimmedTop(52)
      .withTrimmedBottom(28);
}
double GeneticGraph::span() const {
  return zoom_.getToggleState() ? std::max(.06, body_.settleTime * 1.4)
                                : duration_;
}
float GeneticGraph::frequencyY(double hz) const {
  return graph().getBottom() -
         static_cast<float>(
             std::clamp(std::log(hz / 25) / std::log(80.0), 0.0, 1.0)) *
             graph().getHeight();
}
double GeneticGraph::frequency(float y) const {
  return 25 * std::pow(80.0, std::clamp(
                                 static_cast<double>((graph().getBottom() - y) /
                                                     graph().getHeight()),
                                 0.0, 1.0));
}
std::array<juce::Point<float>, 3> GeneticGraph::points() const {
  auto b = graph();
  const auto x = [&](double t) {
    return b.getX() + static_cast<float>(t / span()) * b.getWidth();
  };
  if (!amplitude_)
    return {{{x(0), frequencyY(body_.startHz)},
             {x(body_.pivotTime), frequencyY(body_.pivotHz)},
             {x(body_.settleTime), frequencyY(body_.endHz)}}};
  return {{{x(body_.attack), b.getBottom() - static_cast<float>(bodyAmplitudeAt(
                                                 body_, body_.attack)) *
                                                 b.getHeight()},
           {x(body_.duration * .35),
            b.getBottom() - static_cast<float>(
                                bodyAmplitudeAt(body_, body_.duration * .35)) *
                                b.getHeight()},
           {x(body_.duration - body_.release),
            b.getBottom() - static_cast<float>(bodyAmplitudeAt(
                                body_, body_.duration - body_.release)) *
                                b.getHeight()}}};
}
void GeneticGraph::resized() {
  auto r = getLocalBounds().withHeight(29).reduced(5, 2);
  for (auto *button : {&zoom_, &wave_, &amp_, &pitch_}) {
    button->setBounds(r.removeFromRight(62));
    r.removeFromRight(4);
  }
}
void GeneticGraph::paint(juce::Graphics &g) {
  krui::frame(g, getLocalBounds(), "PITCH / ENVELOPE EDITOR");
  const auto b = graph();
  if (b.getWidth() < 2 || b.getHeight() < 2)
    return;
  g.setColour(krui::dark);
  g.fillRect(b);
  krui::grid(g, b.toNearestInt(), 24);
  g.setColour(krui::muted);
  g.setFont(krui::mono(10));
  g.drawText(amplitude_ ? "BODY AMP / ATTACK - SHAPE - RELEASE"
                        : "BODY PITCH / START - INTERMEDIATE - END",
             42, 31, getWidth() - 84, 18, juce::Justification::centredLeft);
  for (auto hz : {25., 100., 400., 1600.}) {
    const auto y = frequencyY(hz);
    if (!amplitude_) {
      g.setColour(krui::cyan.withAlpha(.18f));
      g.drawLine(b.getX(), y, b.getRight(), y);
      g.setColour(krui::muted);
      g.drawText(juce::String(static_cast<int>(hz)), 3, static_cast<int>(y) - 7,
                 35, 14, juce::Justification::centredRight);
    }
  }
  juce::Path curve;
  const auto width = std::max(2, static_cast<int>(b.getWidth()));
  const auto centre = b.getCentreY();
  for (int i = 0; i < width; ++i) {
    const auto t = span() * i / (width - 1);
    const auto x = b.getX() + i;
    const auto y =
        amplitude_
            ? b.getBottom() -
                  static_cast<float>(bodyAmplitudeAt(body_, t)) * b.getHeight()
            : frequencyY(bodyPitchAt(body_, t));
    if (i == 0)
      curve.startNewSubPath(x, y);
    else
      curve.lineTo(x, y);
    if (wave_.getToggleState() && !samples_.empty()) {
      const auto first =
          std::min(samples_.size(), static_cast<std::size_t>(t * rate_));
      const auto end = std::min(
          samples_.size(),
          std::max(first + 1,
                   static_cast<std::size_t>(span() * (i + 1) / width * rate_)));
      if (first >= samples_.size())
        continue;
      const auto mm = std::minmax_element(
          samples_.begin() + static_cast<std::ptrdiff_t>(first),
          samples_.begin() + static_cast<std::ptrdiff_t>(end));
      g.setColour(krui::pink);
      g.drawLine(x, centre - *mm.first * b.getHeight() * .48f, x,
                 centre - *mm.second * b.getHeight() * .48f, 1);
    }
  }
  g.setColour(krui::cyan.withAlpha(.16f));
  g.strokePath(curve, juce::PathStrokeType(6));
  g.setColour(krui::cyan);
  g.strokePath(curve, juce::PathStrokeType(2));
  for (auto point : points()) {
    if (b.expanded(8).contains(point)) {
      g.setColour(krui::ink);
      g.fillEllipse(point.x - 4, point.y - 4, 8, 8);
      g.setColour(krui::cyan);
      g.drawEllipse(point.x - 5, point.y - 5, 10, 10, 2);
    }
  }
  g.setColour(krui::muted);
  g.setFont(krui::mono(10));
  for (int i = 0; i < 5; ++i)
    g.drawText(
        juce::String(static_cast<int>(std::lround(span() * 1000 * i / 4))) +
            " ms",
        static_cast<int>(b.getX() + (b.getWidth() - 45) * i / 4),
        static_cast<int>(b.getBottom()) + 5, 65, 16,
        juce::Justification::centredLeft);
}
void GeneticGraph::mouseDown(const juce::MouseEvent &e) {
  handle_ = -1;
  auto p = points();
  float distance = 18;
  for (int i = 0; i < 3; ++i)
    if (e.position.getDistanceFrom(p[static_cast<std::size_t>(i)]) < distance) {
      distance = e.position.getDistanceFrom(p[static_cast<std::size_t>(i)]);
      handle_ = i;
    }
  before_ = body_;
  origin_ = e.position;
}
void GeneticGraph::mouseDrag(const juce::MouseEvent &e) {
  drag(e.position - origin_);
}
void GeneticGraph::mouseUp(const juce::MouseEvent &) { handle_ = -1; }
void GeneticGraph::mouseMove(const juce::MouseEvent &e) {
  bool over = false;
  for (auto p : points())
    over |= e.position.getDistanceFrom(p) < 18;
  setMouseCursor(over ? juce::MouseCursor::DraggingHandCursor
                      : juce::MouseCursor::NormalCursor);
}
void GeneticGraph::dragHandle(int index, juce::Point<float> delta) {
  handle_ = index;
  before_ = body_;
  drag(delta);
  handle_ = -1;
}
void GeneticGraph::drag(juce::Point<float> delta) {
  if (handle_ < 0 || graph().getHeight() < 1)
    return;
  auto b = before_;
  const auto box = graph();
  if (!amplitude_) {
    if (handle_ == 0)
      b.startHz = std::clamp(frequency(frequencyY(before_.startHz) + delta.y),
                             30.0, 2000.0);
    if (handle_ == 1) {
      b.pivotHz = std::clamp(frequency(frequencyY(before_.pivotHz) + delta.y),
                             25.0, 2000.0);
      b.pivotTime =
          std::clamp(before_.pivotTime + delta.x / box.getWidth() * span(),
                     .001, std::min(.12, b.settleTime - .001));
    }
    if (handle_ == 2) {
      b.endHz = std::clamp(frequency(frequencyY(before_.endHz) + delta.y), 25.0,
                           150.0);
      b.settleTime =
          std::clamp(before_.settleTime + delta.x / box.getWidth() * span(),
                     b.pivotTime + .001, .3);
    }
  } else {
    if (handle_ == 0)
      b.attack = std::clamp(before_.attack + delta.x / box.getWidth() * span(),
                            .0001, std::min(.020, b.duration * .3));
    if (handle_ == 1) {
      const auto level =
          std::clamp(bodyAmplitudeAt(before_, before_.duration * .35) -
                         delta.y / box.getHeight(),
                     .002, .98);
      b.ampCurve =
          std::clamp(std::log(-std::log(level) / 6) / std::log(.35), .45, 2.3);
    }
    if (handle_ == 2)
      b.release =
          std::clamp(before_.release - delta.x / box.getWidth() * span(), .0003,
                     b.duration * .3);
  }
  body_ = b;
  if (onEdit)
    onEdit(b);
  repaint();
}
