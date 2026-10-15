#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <cmath>

namespace neon {
inline const juce::Colour blue {0xff090d71}, dark {0xff080b48}, panel {0xff10145d};
inline const juce::Colour cyan {0xff09e9ff}, pink {0xffff72ee}, ink {0xfff3e8ff}, muted {0xffb6b9f0};

inline juce::Font mono(float size, bool bold = false) {
    return juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), size,
                                      bold ? juce::Font::bold : juce::Font::plain));
}

inline void grid(juce::Graphics& g, juce::Rectangle<int> bounds, int step = 24) {
    g.setColour(cyan.withAlpha(.10f));
    for (int x = bounds.getX(); x < bounds.getRight(); x += step)
        g.drawVerticalLine(x, static_cast<float>(bounds.getY()), static_cast<float>(bounds.getBottom()));
    for (int y = bounds.getY(); y < bounds.getBottom(); y += step)
        g.drawHorizontalLine(y, static_cast<float>(bounds.getX()), static_cast<float>(bounds.getRight()));
}

inline void frame(juce::Graphics& g, juce::Rectangle<int> bounds, const juce::String& title) {
    auto r = bounds.toFloat().reduced(2);
    g.setColour(pink.withAlpha(.2f)); g.fillRoundedRectangle(r.translated(3, 4), 5);
    g.setColour(panel); g.fillRoundedRectangle(r, 5);
    g.setGradientFill(juce::ColourGradient(cyan, r.getX(), r.getY(), pink, r.getRight(), r.getY() + 25, false));
    g.fillRoundedRectangle(r.withHeight(32), 4);
    g.setColour(cyan); g.drawRoundedRectangle(r, 5, 2);
    g.setColour(pink); g.drawRoundedRectangle(r.reduced(3), 3, 1);
    g.setColour(dark); g.setFont(mono(13, true));
    g.drawText(title, bounds.reduced(14, 0).withY(bounds.getY() + 4).withHeight(26), juce::Justification::centredLeft);
}

inline void star(juce::Graphics& g, juce::Point<float> centre, float size) {
    juce::Path p;
    p.startNewSubPath(centre.x, centre.y - size);
    p.quadraticTo(centre.x + size * .15f, centre.y - size * .15f, centre.x + size, centre.y);
    p.quadraticTo(centre.x + size * .15f, centre.y + size * .15f, centre.x, centre.y + size);
    p.quadraticTo(centre.x - size * .15f, centre.y + size * .15f, centre.x - size, centre.y);
    p.quadraticTo(centre.x - size * .15f, centre.y - size * .15f, centre.x, centre.y - size);
    g.fillPath(p);
}

class Brand final : public juce::Component {
public:
    void paint(juce::Graphics& g) override {
        const auto b = getLocalBounds();
        frame(g, b, "KRAVATEK.EXE / PROTOTYPE 001");
        auto face = b.reduced(7).withTrimmedTop(28);
        g.setGradientFill(juce::ColourGradient(pink, 0, static_cast<float>(face.getY()),
            juce::Colour(0xffe2aaff), static_cast<float>(getWidth()), static_cast<float>(getHeight()), false));
        g.fillRect(face);
        constexpr std::array<const char*, 8> letters {{
            "10001100101010011000101001001010001", // K
            "11110100011000111110101001001010001", // R
            "01110100011000111111100011000110001", // A
            "10001100011000110001100010101000100", // V
            "01110100011000111111100011000110001", // A
            "11111001000010000100001000010000100", // T
            "11111100001000011110100001000011111", // E
            "10001100101010011000101001001010001"  // K
        }};
        const auto scale = std::min(7.0f, static_cast<float>(face.getWidth() - 24) / 47.0f);
        const auto originX = static_cast<float>(face.getCentreX()) - 47.0f * scale * .5f;
        const auto originY = static_cast<float>(face.getY()) + 9;
        g.setColour(blue);
        for (std::size_t letter = 0; letter < letters.size(); ++letter)
            for (int y = 0; y < 7; ++y)
                for (int x = 0; x < 5; ++x)
                    if (letters[letter][y * 5 + x] == '1')
                        g.fillRect(originX + static_cast<float>(letter * 6 + static_cast<std::size_t>(x)) * scale,
                                   originY + static_cast<float>(y) * scale, scale, scale);
        g.setFont(mono(18, true));
        g.drawText("K I C K   L A B", face.withTrimmedTop(static_cast<int>(7 * scale) + 11), juce::Justification::centred);
    }
};

class WindowButton final : public juce::Button {
public:
    explicit WindowButton(int type) : Button("Window control"), type_(type) {}
    void paintButton(juce::Graphics& g, bool over, bool down) override {
        auto b = getLocalBounds().toFloat().reduced(3);
        g.setColour(over || down ? cyan : pink); g.fillRect(b);
        g.setColour(blue); g.drawRect(b, 1.5f);
        b = b.reduced(5);
        if (type_ == juce::DocumentWindow::closeButton) {
            g.drawLine(b.getX(), b.getY(), b.getRight(), b.getBottom(), 2);
            g.drawLine(b.getRight(), b.getY(), b.getX(), b.getBottom(), 2);
        } else if (type_ == juce::DocumentWindow::minimiseButton) {
            g.drawLine(b.getX(), b.getBottom(), b.getRight(), b.getBottom(), 2);
        } else g.drawRect(b, 2);
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
        setColour(juce::Slider::textBoxOutlineColourId, cyan.withAlpha(.4f));
        setColour(juce::PopupMenu::backgroundColourId, dark);
        setColour(juce::PopupMenu::textColourId, ink);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, pink);
        setColour(juce::PopupMenu::highlightedTextColourId, blue);
    }
    void drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour&, bool over, bool down) override {
        auto r = button.getLocalBounds().toFloat().reduced(3);
        g.setColour(pink.withAlpha(.3f)); g.fillRoundedRectangle(r.translated(2, 3), 4);
        if (down) r.translate(1, 2);
        g.setGradientFill(juce::ColourGradient(over ? ink : cyan, r.getX(), r.getY(),
            pink, r.getRight(), r.getBottom(), false));
        g.fillRoundedRectangle(r, 4);
        g.setColour(cyan); g.drawRoundedRectangle(r, 4, 2);
        g.setColour(pink); g.drawRoundedRectangle(r.reduced(3), 2, 1);
    }
    void drawButtonText(juce::Graphics& g, juce::TextButton& button, bool, bool down) override {
        auto b = button.getLocalBounds().toFloat().reduced(8);
        if (down) b.translate(1, 2);
        const auto text = button.getButtonText();
        const auto tall = b.getHeight() >= 45;
        g.setColour(blue);
        if (tall) {
            const auto centre = juce::Point<float>(b.getCentreX(), b.getY() + 20);
            if (text == "PLAY") {
                juce::Path p; p.addTriangle(centre.x - 10, centre.y - 13, centre.x - 10, centre.y + 13, centre.x + 13, centre.y);
                g.fillPath(p);
            } else if (text == "GENERATE") star(g, centre, 17);
            else if (text == "EXPORT WAV") {
                g.drawRoundedRectangle(centre.x - 13, centre.y - 2, 26, 16, 2, 2.5f);
                g.drawLine(centre.x, centre.y - 14, centre.x, centre.y + 5, 3);
                g.drawLine(centre.x - 6, centre.y - 7, centre.x, centre.y - 14, 3);
                g.drawLine(centre.x + 6, centre.y - 7, centre.x, centre.y - 14, 3);
            } else {
                g.drawEllipse(centre.x - 13, centre.y - 13, 26, 26, 3);
                g.drawEllipse(centre.x - 4, centre.y - 4, 8, 8, 2);
                for (int i = 0; i < 8; ++i) {
                    const auto a = static_cast<float>(i) * juce::MathConstants<float>::twoPi / 8;
                    g.drawLine(centre.x + std::cos(a) * 12, centre.y + std::sin(a) * 12,
                               centre.x + std::cos(a) * 18, centre.y + std::sin(a) * 18, 3);
                }
            }
        }
        g.setFont(mono(tall ? 11.5f : 12.5f, true));
        g.drawText(text, b.toNearestInt().withTrimmedTop(tall ? 44 : 0), juce::Justification::centred);
    }
    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height, float position,
                          float start, float end, juce::Slider&) override {
        const auto size = static_cast<float>(std::min(width, height)) - 16;
        const auto centre = juce::Point<float>(static_cast<float>(x) + static_cast<float>(width) * .5f,
                                              static_cast<float>(y) + static_cast<float>(height) * .5f);
        const auto radius = size * .5f;
        juce::Path base, value;
        base.addCentredArc(centre.x, centre.y, radius, radius, 0, start, end, true);
        value.addCentredArc(centre.x, centre.y, radius, radius, 0, start, start + position * (end - start), true);
        g.setColour(juce::Colour(0xff303a9e)); g.strokePath(base, juce::PathStrokeType(5));
        g.setColour(cyan.withAlpha(.22f)); g.strokePath(value, juce::PathStrokeType(10));
        g.setColour(cyan); g.strokePath(value, juce::PathStrokeType(5));
        auto knob = juce::Rectangle<float>(size - 18, size - 18).withCentre(centre);
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff282780), knob.getX(), knob.getY(),
                                              dark, knob.getRight(), knob.getBottom(), false));
        g.fillEllipse(knob); g.setColour(blue); g.drawEllipse(knob, 2);
        const auto angle = start + position * (end - start) - juce::MathConstants<float>::halfPi;
        const auto pointer = centre + juce::Point<float>(std::cos(angle), std::sin(angle)) * (radius - 16);
        g.setColour(pink.withAlpha(.25f)); g.drawLine(juce::Line<float>(centre, pointer), 7);
        g.setColour(pink); g.drawLine(juce::Line<float>(centre, pointer), 3);
    }
    void drawDocumentWindowTitleBar(juce::DocumentWindow& window, juce::Graphics& g, int width, int height,
                                    int titleX, int titleWidth, const juce::Image*, bool) override {
        g.setGradientFill(juce::ColourGradient(cyan, 0, 0, pink, static_cast<float>(width), static_cast<float>(height), false));
        g.fillAll();
        g.setColour(blue); g.setFont(mono(12, true));
        g.drawText(window.getName(), titleX + 8, 0, titleWidth - 16, height, juce::Justification::centredLeft);
        g.setColour(cyan); g.fillRect(0, height - 2, width, 2);
    }
    juce::Button* createDocumentWindowButton(int type) override { return new WindowButton(type); }
};
} // namespace neon
