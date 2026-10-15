#pragma once
#include "InstrumentStyle.h"
#include "genetic/LayerRenderer.h"
#include <functional>
#include <span>
class GeneticGraph final : public juce::Component {
public:
  GeneticGraph();
  std::function<void(const kravatek::genetic::BodyGenes &)> onEdit;
  void setSound(const kravatek::genetic::Genome &, std::span<const float>,
                double rate);
  void setAmplitudeMode(bool);
  void dragHandle(int, juce::Point<float>);
  void paint(juce::Graphics &) override;
  void resized() override;
  void mouseDown(const juce::MouseEvent &) override;
  void mouseDrag(const juce::MouseEvent &) override;
  void mouseUp(const juce::MouseEvent &) override;
  void mouseMove(const juce::MouseEvent &) override;

private:
  kravatek::genetic::BodyGenes body_, before_;
  std::vector<float> samples_;
  double duration_ = 1, rate_ = 48000;
  bool amplitude_ = false;
  int handle_ = -1;
  juce::Point<float> origin_;
  juce::TextButton pitch_{"PITCH"}, amp_{"AMP"}, wave_{"WAVE"}, zoom_{"ZOOM"};
  juce::Rectangle<float> graph() const;
  double span() const;
  float frequencyY(double) const;
  double frequency(float) const;
  std::array<juce::Point<float>, 3> points() const;
  void drag(juce::Point<float>);
};
