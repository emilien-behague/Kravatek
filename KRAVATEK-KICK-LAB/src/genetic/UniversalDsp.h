#pragma once
#include "Genome.h"
namespace kravatek::genetic {
// Per-layer state: never shared across layers, candidates or render calls.
class DistortionChain {
public:
  DistortionChain(const FxGenes &, double sampleRate);
  double process(double input, double seconds, double drive, double fold);

private:
  FxGenes genes_;
  double rate_, preAlpha_, postAlpha_, splitAlpha_;
  double pre_ = 0, post_ = 0, split_ = 0, held_ = 0, clock_ = 1;
};
Genome universalFoundation(Architecture);
double midiFrequency(int note);
void tune(Genome &, int note, Locks = {});
// Upgrade metadata only. Each layer retains its renderer until explicitly
// edited.
void upgrade(Genome &);
} // namespace kravatek::genetic
