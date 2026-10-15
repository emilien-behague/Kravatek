#pragma once
#include <cmath>
#include <juce_gui_basics/juce_gui_basics.h>

namespace krui {
inline const juce::Colour blue{0xff090d71}, dark{0xff070b45}, panel{0xff11145e};
inline const juce::Colour cyan{0xff09e9ff}, pink{0xffff72ee}, ink{0xfff3e8ff},
    muted{0xffb6b9f0};
inline const juce::Colour yellow{0xffff72ee}, line{0xff36449e};
inline juce::Font mono(float size, bool bold = false) {
  return juce::Font(
      juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), size,
                        bold ? juce::Font::bold : juce::Font::plain));
}
inline void grid(juce::Graphics &g, juce::Rectangle<int> bounds,
                 int step = 24) {
  g.setColour(cyan.withAlpha(.06f));
  for (int x = bounds.getX(); x < bounds.getRight(); x += step)
    g.drawVerticalLine(x, static_cast<float>(bounds.getY()),
                       static_cast<float>(bounds.getBottom()));
  for (int y = bounds.getY(); y < bounds.getBottom(); y += step)
    g.drawHorizontalLine(y, static_cast<float>(bounds.getX()),
                         static_cast<float>(bounds.getRight()));
}
inline void frame(juce::Graphics &g, juce::Rectangle<int> bounds,
                  const juce::String &title) {
  const auto r = bounds.toFloat().reduced(2);
  g.setColour(pink.withAlpha(.15f));
  g.fillRoundedRectangle(r.translated(2, 3), 4);
  g.setColour(panel);
  g.fillRoundedRectangle(r, 4);
  g.setGradientFill(juce::ColourGradient(cyan, r.getX(), r.getY(), pink,
                                         r.getRight(), r.getY() + 26, false));
  g.fillRoundedRectangle(r.withHeight(29), 3);
  g.setColour(cyan);
  g.drawRoundedRectangle(r, 4, 1.5f);
  g.setColour(pink);
  g.drawRoundedRectangle(r.reduced(3), 2, 1);
  g.setColour(blue);
  g.setFont(mono(12, true));
  g.drawText(title, bounds.reduced(12, 0).withHeight(29),
             juce::Justification::centredLeft);
}
class Brand final : public juce::Component {
  void paint(juce::Graphics &g) override {
    auto b = getLocalBounds();
    frame(g, b, "KRAVA-KICK.EXE / GENETIC LAB");
    auto face = b.reduced(7).withTrimmedTop(25);
    g.setGradientFill(juce::ColourGradient(
        pink, 0, static_cast<float>(face.getY()), juce::Colour(0xffdfaeff),
        static_cast<float>(face.getRight()),
        static_cast<float>(face.getBottom()), false));
    g.fillRect(face);
    const auto bitmap = [](char c) -> const char * {
      switch (c) {
      case 'K':
        return "10001100101010011000101001001010001";
      case 'R':
        return "11110100011000111110101001001010001";
      case 'A':
        return "01110100011000111111100011000110001";
      case 'V':
        return "10001100011000110001100010101000100";
      case 'I':
        return "11111001000010000100001000010011111";
      case 'C':
        return "01111100001000010000100001000001111";
      default:
        return "00000000000000011111000000000000000";
      }
    };
    const juce::String word = "KRAVA-KICK";
    const auto scale = std::min(static_cast<float>(face.getWidth() - 16) / 59,
                                (face.getHeight() - 23) / 7.0f);
    const auto x0 = face.getCentreX() - 59 * scale * .5f,
               y0 = face.getY() + 5.0f;
    g.setColour(blue);
    for (int letter = 0; letter < word.length(); ++letter) {
      const auto *pixels = bitmap(static_cast<char>(word[letter]));
      for (int y = 0; y < 7; ++y)
        for (int x = 0; x < 5; ++x)
          if (pixels[y * 5 + x] == '1')
            g.fillRect(x0 + (letter * 6 + x) * scale, y0 + y * scale, scale,
                       scale);
    }
    g.setFont(mono(11, true));
    g.drawText("L A B   /   G E N E T I C  0.3",
               face.withY(face.getBottom() - 18).withHeight(16),
               juce::Justification::centred);
  }
};
class WindowButton final : public juce::Button {
public:
  explicit WindowButton(int type) : Button("Window control"), type_(type) {}
  void paintButton(juce::Graphics &g, bool over, bool down) override {
    auto b = getLocalBounds().toFloat().reduced(6);
    g.setColour(over || down ? yellow : muted);
    if (type_ == juce::DocumentWindow::closeButton) {
      g.drawLine(b.getX(), b.getY(), b.getRight(), b.getBottom(), 1.5f);
      g.drawLine(b.getRight(), b.getY(), b.getX(), b.getBottom(), 1.5f);
    } else if (type_ == juce::DocumentWindow::minimiseButton)
      g.drawLine(b.getX(), b.getBottom(), b.getRight(), b.getBottom(), 1.5f);
    else
      g.drawRect(b, 1.5f);
  }

private:
  int type_;
};
class LookAndFeel final : public juce::LookAndFeel_V4 {
public:
  LookAndFeel() {
    setColour(juce::ResizableWindow::backgroundColourId, blue);
    setColour(juce::Slider::textBoxTextColourId, ink);
    setColour(juce::Slider::textBoxBackgroundColourId, dark);
    setColour(juce::Slider::textBoxOutlineColourId, line);
    setColour(juce::Slider::thumbColourId, yellow);
    setColour(juce::Slider::trackColourId, cyan);
    setColour(juce::Slider::backgroundColourId, line);
    setColour(juce::PopupMenu::backgroundColourId, dark);
    setColour(juce::PopupMenu::textColourId, ink);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, yellow);
    setColour(juce::PopupMenu::highlightedTextColourId, blue);
    setColour(juce::TooltipWindow::backgroundColourId, ink);
    setColour(juce::TooltipWindow::textColourId, blue);
    setColour(juce::TooltipWindow::outlineColourId, yellow);
  }
  void drawButtonBackground(juce::Graphics &g, juce::Button &b,
                            const juce::Colour &, bool over,
                            bool down) override {
    const bool action =
        b.getButtonText() == "GENERATE" || b.getButtonText() == "EVOLVE";
    const bool selected = b.getToggleState();
    auto r = b.getLocalBounds().toFloat().reduced(2);
    auto fill = action ? yellow : selected ? cyan : juce::Colour(0xff25278c);
    if (!b.isEnabled())
      fill = panel;
    else if (over || down)
      fill = fill.brighter(down ? .1f : .18f);
    if (b.isEnabled() && (action || b.getHeight() > 60))
      g.setGradientFill(juce::ColourGradient(
          cyan, r.getX(), r.getY(), pink, r.getRight(), r.getBottom(), false));
    else
      g.setColour(fill);
    g.fillRoundedRectangle(r, 2);
    g.setColour(b.isEnabled() && selected ? cyan : line);
    g.drawRoundedRectangle(r, 2, 1);
  }
  void drawButtonText(juce::Graphics &g, juce::TextButton &b, bool,
                      bool) override {
    auto r = b.getLocalBounds().toFloat().reduced(7);
    const auto text = b.getButtonText();
    const bool tall = r.getHeight() > 50;
    const bool filled =
        b.getToggleState() || text == "GENERATE" || text == "EVOLVE";
    g.setColour(!b.isEnabled() ? line : filled ? blue : ink);
    if (tall) {
      const auto c = juce::Point<float>(r.getCentreX(), r.getY() + 20);
      if (text == "PLAY") {
        juce::Path p;
        p.addTriangle(c.x - 8, c.y - 11, c.x - 8, c.y + 11, c.x + 11, c.y);
        g.fillPath(p);
      } else if (text == "STOP")
        g.fillRect(c.x - 9, c.y - 9, 18.0f, 18.0f);
      else if (text == "GENERATE") {
        g.drawRoundedRectangle(c.x - 12, c.y - 12, 24, 24, 3, 1.5f);
        for (auto p : {juce::Point<float>(-6, -6), juce::Point<float>(0, 0),
                       juce::Point<float>(6, 6)})
          g.fillEllipse(c.x + p.x - 2, c.y + p.y - 2, 4, 4);
      } else if (text == "EVOLVE") {
        juce::Path p;
        for (int i = 0; i < 30; ++i) {
          const auto x = c.x - 14 + i;
          const auto y = c.y + std::sin(i * .33f) * 10;
          if (i == 0)
            p.startNewSubPath(x, y);
          else
            p.lineTo(x, y);
        }
        g.strokePath(p, juce::PathStrokeType(2));
        g.drawLine(c.x + 11, c.y - 12, c.x + 11, c.y + 12, 1);
        g.fillEllipse(c.x + 8, c.y - 7, 6, 6);
      } else {
        g.drawRect(c.x - 12, c.y - 1, 24.0f, 14.0f, 1.5f);
        g.drawLine(c.x, c.y - 14, c.x, c.y + 5, 2);
        g.drawLine(c.x - 5, c.y - 8, c.x, c.y - 14, 2);
        g.drawLine(c.x + 5, c.y - 8, c.x, c.y - 14, 2);
      }
    }
    g.setFont(mono(tall ? 11.5f : 12, true));
    g.drawText(text, r.toNearestInt().withTrimmedTop(tall ? 44 : 0),
               juce::Justification::centred);
  }
  void drawRotarySlider(juce::Graphics &g, int x, int y, int w, int h,
                        float position, float start, float end,
                        juce::Slider &) override {
    const auto diameter = static_cast<float>(std::min(w, h)) - 8;
    if (diameter < 8)
      return;
    const auto c = juce::Point<float>(x + w * .5f, y + h * .5f);
    const auto radius = diameter * .5f;
    juce::Path base, value;
    base.addCentredArc(c.x, c.y, radius, radius, 0, start, end, true);
    value.addCentredArc(c.x, c.y, radius, radius, 0, start,
                        start + position * (end - start), true);
    g.setColour(line);
    g.strokePath(base, juce::PathStrokeType(3));
    g.setColour(cyan);
    g.strokePath(value, juce::PathStrokeType(3));
    for (int i = 0; i < 9; ++i) {
      const auto a =
          start + i * (end - start) / 8 - juce::MathConstants<float>::halfPi;
      g.setColour(muted.withAlpha(.55f));
      g.drawLine(c.x + std::cos(a) * (radius - 7),
                 c.y + std::sin(a) * (radius - 7),
                 c.x + std::cos(a) * (radius - 10),
                 c.y + std::sin(a) * (radius - 10), 1);
    }
    const auto knob =
        juce::Rectangle<float>(diameter - 20, diameter - 20).withCentre(c);
    g.setColour(dark);
    g.fillEllipse(knob);
    g.setColour(line);
    g.drawEllipse(knob, 1);
    const auto a =
        start + position * (end - start) - juce::MathConstants<float>::halfPi;
    g.setColour(ink);
    g.drawLine(c.x, c.y, c.x + std::cos(a) * (radius - 13),
               c.y + std::sin(a) * (radius - 13), 2);
  }
  void drawDocumentWindowTitleBar(juce::DocumentWindow &window,
                                  juce::Graphics &g, int width, int height,
                                  int x, int w, const juce::Image *,
                                  bool) override {
    g.fillAll(blue);
    g.setColour(muted);
    g.setFont(mono(11));
    g.drawText(window.getName(), x + 8, 0, w - 16, height,
               juce::Justification::centredLeft);
    g.setColour(line);
    g.fillRect(0, height - 1, width, 1);
  }
  juce::Font getComboBoxFont(juce::ComboBox &) override { return mono(11.5f); }
  void drawToggleButton(juce::Graphics &g, juce::ToggleButton &button,
                        bool over, bool) override {
    const auto y = (button.getHeight() - 14) * .5f;
    g.setColour(dark);
    g.fillRoundedRectangle(4, y, 14, 14, 2);
    g.setColour(button.isEnabled() ? (over ? pink : cyan) : line);
    g.drawRoundedRectangle(4, y, 14, 14, 2, 1);
    if (button.getToggleState()) {
      juce::Path p;
      p.startNewSubPath(7, y + 7);
      p.lineTo(10, y + 10);
      p.lineTo(15, y + 4);
      g.strokePath(p, juce::PathStrokeType(1.8f));
    }
    g.setColour(button.isEnabled() ? ink : muted.withAlpha(.5f));
    g.setFont(mono(10.5f));
    g.drawText(button.getButtonText(),
               button.getLocalBounds().withTrimmedLeft(23),
               juce::Justification::centredLeft);
  }
  juce::Button *createDocumentWindowButton(int type) override {
    return new WindowButton(type);
  }
};
} // namespace krui
