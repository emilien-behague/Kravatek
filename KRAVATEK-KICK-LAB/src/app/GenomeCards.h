#pragma once
#include "InstrumentStyle.h"
#include "genetic/Population.h"
#include <cmath>
struct CardView {
  int libraryIndex = -1;
  kravatek::genetic::Genome genome;
  kravatek::genetic::Features features;
  std::array<float, 100> wave{};
  bool close = false;
};
class GenomeCards final : public juce::Component {
public:
  std::vector<CardView> cards;
  int selected = -1;
  std::function<void(int, bool)> onSelect;
  void layout(int width) {
    const int columns = std::max(1, width / 260);
    columns_ = columns;
    setSize(width, std::max(1, static_cast<int>(
                                   (cards.size() +
                                    static_cast<std::size_t>(columns) - 1) /
                                   static_cast<std::size_t>(columns))) *
                       92);
    repaint();
  }
  void paint(juce::Graphics &g) override {
    const auto width = getWidth() / columns_;
    for (std::size_t i = 0; i < cards.size(); ++i) {
      const auto &c = cards[i];
      auto r =
          juce::Rectangle<int>(static_cast<int>(i) % columns_ * width,
                               static_cast<int>(i) / columns_ * 92, width, 92)
              .reduced(4);
      const bool active = c.libraryIndex == selected;
      g.setColour(active ? juce::Colour(0xff2228a0) : krui::dark);
      g.fillRoundedRectangle(r.toFloat(), 3);
      g.setColour(active ? krui::cyan : krui::pink.withAlpha(.55f));
      g.drawRoundedRectangle(r.toFloat(), 3, active ? 2 : 1);
      g.setColour(krui::ink);
      g.setFont(krui::mono(11, true));
      g.drawText(juce::String::fromUTF8(c.genome.name.c_str()),
                 r.reduced(8, 0).withY(r.getY() + 4).withHeight(18),
                 juce::Justification::centredLeft);
      auto wave = r.reduced(8, 0).withY(r.getY() + 25).withHeight(29);
      juce::Path p;
      for (std::size_t j = 0; j < c.wave.size(); ++j) {
        const auto x = wave.getX() +
                       static_cast<float>(j) * wave.getWidth() / 99,
                   y = wave.getCentreY() - c.wave[j] * wave.getHeight() * .55f;
        if (j == 0)
          p.startNewSubPath(x, y);
        else
          p.lineTo(x, y);
      }
      g.setColour(krui::pink);
      g.strokePath(p, juce::PathStrokeType(1));
      g.setColour(krui::muted);
      g.setFont(krui::mono(9.5f));
      g.drawText("G" + juce::String(static_cast<int>(c.genome.generation)) +
                     " / " +
                     juce::String(static_cast<int>(std::lround(
                         c.features.basic.durationSeconds * 1000))) +
                     "ms / " + juce::String(c.features.basic.rmsDb, 1) +
                     " dB RMS" + (c.close ? " / SIMILAR" : ""),
                 r.reduced(8, 0).withY(r.getBottom() - 26).withHeight(13),
                 juce::Justification::centredLeft);
      g.drawText("SEED " +
                     juce::String(static_cast<juce::int64>(c.genome.seed)) +
                     " / " + juce::String(c.genome.id.substr(3, 8)),
                 r.reduced(8, 0).withY(r.getBottom() - 14).withHeight(12),
                 juce::Justification::centredLeft);
    }
  }
  void mouseDown(const juce::MouseEvent &e) override {
    const auto column = e.x / std::max(1, getWidth() / columns_),
               row = e.y / 92, index = row * columns_ + column;
    if (index >= 0 && index < static_cast<int>(cards.size()) && onSelect)
      onSelect(cards[static_cast<std::size_t>(index)].libraryIndex, false);
  }
  void mouseDoubleClick(const juce::MouseEvent &e) override {
    const auto index =
        e.y / 92 * columns_ + e.x / std::max(1, getWidth() / columns_);
    if (index >= 0 && index < static_cast<int>(cards.size()) && onSelect)
      onSelect(cards[static_cast<std::size_t>(index)].libraryIndex, true);
  }

private:
  int columns_ = 4;
};
