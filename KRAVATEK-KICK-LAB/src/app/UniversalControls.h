#pragma once
#include "ParameterControl.h"
#include "genetic/UniversalDsp.h"
class UniversalControls final : public juce::Component {
public:
  using Genome = kravatek::genetic::Genome;
  UniversalControls(const Genome &,
                    std::function<void(std::function<void(Genome &)>)>);
  void paint(juce::Graphics &) override;
  void resized() override;
  void setPage(int);
  void
  setValue(int,
           double); // Same controls exercised by the functional smoke test.
private:
  Genome snapshot_;
  std::function<void(std::function<void(Genome &)>)> edit_;
  std::array<ParameterControl, 12> knobs_;
  std::array<juce::TextButton, 3> pages_;
  juce::ComboBox kind_;
  int page_ = 0;
  void configure();
  void apply(int, double);
};
