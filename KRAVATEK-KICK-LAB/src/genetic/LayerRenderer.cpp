#include "LayerRenderer.h"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace kravatek::genetic {
namespace {
constexpr double pi = std::numbers::pi, twoPi = 2 * pi;
struct Oscillator {
  double phase, rate;
  double next(double hz) {
    const auto v = std::sin(phase);
    phase += twoPi * hz / rate;
    if (phase >= twoPi)
      phase -= twoPi;
    return v;
  }
};
struct Filter {
  double s1 = 0, s2 = 0;
  double process(double input, double cutoff, double resonance, double rate) {
    const auto g = std::tan(pi * cutoff / rate), k = 2 - 1.9 * resonance,
               a = 1 / (1 + g * (g + k));
    const auto v1 = a * (s1 + g * (input - s2)), v2 = s2 + g * v1;
    s1 = 2 * v1 - s1;
    s2 = 2 * v2 - s2;
    return v2;
  }
};
double edge(double t, double duration, double attack, double release) {
  return .25 * (1 - std::cos(pi * std::clamp(t / attack, 0.0, 1.0))) *
         (1 - std::cos(pi * std::clamp((duration - t) / release, 0.0, 1.0)));
}
double shape(double input, double drive, double fold) {
  const auto gain = std::pow(10.0, drive / 20);
  return (1 - fold) * std::tanh(input * gain) / std::tanh(gain) +
         fold * std::sin(input * (1 + gain * .28));
}
std::vector<double> kernel(int factor) {
  const int count = 32 * factor + 1, centre = (count - 1) / 2;
  const auto cutoff = .4 / factor;
  std::vector<double> f(static_cast<std::size_t>(count));
  double sum = 0;
  for (int i = 0; i < count; ++i) {
    const auto x = i - centre;
    f[static_cast<std::size_t>(i)] =
        (x == 0 ? 2 * cutoff : std::sin(twoPi * cutoff * x) / (pi * x)) *
        (.42 - .5 * std::cos(twoPi * i / (count - 1)) +
         .08 * std::cos(2 * twoPi * i / (count - 1)));
    sum += f[static_cast<std::size_t>(i)];
  }
  for (auto &v : f)
    v /= sum;
  return f;
}
template <class Sample>
std::vector<float> layer(double duration, double rate, Sample sample) {
  const int factor = rate < 24000 ? 8 : 4;
  const auto highRate = rate * factor;
  static const auto filter4 = kernel(4), filter8 = kernel(8);
  const auto &f = factor == 4 ? filter4 : filter8;
  const auto half = f.size() / 2,
             count = static_cast<std::size_t>(std::llround(duration * rate));
  std::vector<float> high(count * static_cast<std::size_t>(factor) + 2 * half,
                          0),
      output(count);
  for (std::size_t i = 0; i < count * static_cast<std::size_t>(factor); ++i)
    high[half + i] =
        static_cast<float>(sample(static_cast<double>(i) / highRate, highRate));
  double sum = 0, weightSum = 0;
  for (std::size_t i = 0; i < count; ++i) {
    double v = 0;
    for (std::size_t k = 0; k < f.size(); ++k)
      v += high[i * static_cast<std::size_t>(factor) + k] * f[k];
    const auto weight = std::pow(
        std::sin(pi * static_cast<double>(i) / static_cast<double>(count - 1)),
        2);
    const auto taper = edge(static_cast<double>(i) / rate,
                            static_cast<double>(count - 1) / rate, .0003, .001);
    output[i] = static_cast<float>(v * taper);
    sum += output[i];
    weightSum += weight;
  }
  const auto dc = sum / weightSum;
  for (std::size_t i = 0; i < count; ++i) {
    output[i] = static_cast<float>(
        output[i] - dc * std::pow(std::sin(pi * static_cast<double>(i) /
                                           static_cast<double>(count - 1)),
                                  2));
    if (!std::isfinite(output[i]))
      throw std::runtime_error("Non-finite genetic layer");
  }
  output.front() = output.back() = 0;
  return output;
}
} // namespace
Render render(const Genome &genome, double rate, bool retainLayers) {
  validate(genome, rate);
  Render result;
  result.sampleRate = rate;
  const auto highRate = rate * (rate < 24000 ? 8 : 4);
  const auto &s = genome.sub;
  Oscillator sub{s.phase, highRate};
  result.layers[0] = layer(s.duration, rate, [&](double t, double) {
    return sub.next(s.hz) * s.level * edge(t, s.duration, s.attack, s.release) *
           std::exp(-t / s.decay);
  });
  const auto &a = genome.attack;
  Oscillator attack{a.phase, highRate};
  Random attackNoise(a.seed);
  double low = 0, high = 0;
  const auto lowAlpha =
      1 - std::exp(-twoPi * (300 + 2300 * a.colour) / highRate);
  const auto highAlpha =
      1 -
      std::exp(-twoPi * std::min(3500 + 8500 * a.colour, rate * .4) / highRate);
  result.layers[1] = layer(a.duration, rate, [&](double t, double) {
    const auto phase = attack.phase, tone = attack.next(a.hz);
    const auto noise = attackNoise.between(-1, 1);
    low += lowAlpha * (noise - low);
    high += highAlpha * (noise - low - high);
    double signal = high * 2;
    if (a.kind == AttackKind::tonal)
      signal = tone;
    if (a.kind == AttackKind::harmonic)
      signal =
          .6 * tone + .25 * std::sin(phase * 3) + .15 * std::sin(phase * 5);
    if (a.kind == AttackKind::hybrid)
      signal = .55 * tone + high * 1.5;
    return std::tanh(signal) * a.level *
           edge(t, a.duration, a.attack, a.release) * std::exp(-t / a.decay);
  });
  const auto &b = genome.body;
  Oscillator body{b.phase, highRate}, bodyMod{0, highRate},
      bodyExtra{0, highRate};
  Filter bodyFilter;
  Random bodyRandom(b.seed);
  const auto ratio = bodyRandom.between(1.4, 3.8);
  result.layers[2] = layer(b.duration, rate, [&](double t, double hr) {
    const auto hz = bodyPitchAt(b, t), phase = body.phase,
               mod = bodyMod.next(hz * ratio),
               depth = b.fm * std::exp(-t / b.fmTime);
    auto signal =
        body.next(hz * (b.kind == BodyKind::fm ? 1 + .18 * depth * mod : 1));
    if (b.kind == BodyKind::additive)
      signal +=
          b.harmonics * (.34 * std::sin(phase * 3) + .16 * std::sin(phase * 5) +
                         .08 * std::sin(phase * 7));
    if (b.kind == BodyKind::fm)
      signal = std::sin(phase + depth * mod) +
               .3 * b.harmonics * std::sin(phase * 3);
    if (b.kind == BodyKind::ring)
      signal = signal * ((1 - b.harmonics) +
                         b.harmonics * bodyExtra.next(hz * ratio)) +
               depth * .1 * mod;
    signal = shape(signal * bodyAmplitudeAt(b, t), b.drive, b.fold);
    const auto cutoff =
        std::clamp(b.cutoff * std::pow(2.0, b.sweep * std::exp(-t / .08)), 40.0,
                   std::min(18000.0, rate * .4));
    const auto filtered = bodyFilter.process(signal, cutoff, b.resonance, hr);
    return std::tanh(filtered) * b.level *
           edge(t, b.duration, .0001, b.release);
  });
  const auto &q = genome.tail;
  Oscillator tail{q.phase, highRate}, tailMod{0, highRate};
  Filter tailFilter;
  Random tailNoise(q.seed);
  result.layers[3] = layer(q.duration, rate, [&](double t, double hr) {
    const auto progress = 1 - std::exp(-t / .09),
               base = q.hz * std::pow(q.endHz / q.hz, progress);
    const auto movement =
        q.motion * std::sin(twoPi * q.speed * t) * (1 - std::exp(-t / .02));
    const auto hz = q.kind == TailKind::elastic
                        ? base * std::pow(2.0, 1.4 * movement)
                        : base;
    const auto phase = tail.phase;
    auto signal = tail.next(hz);
    const auto modulation = tailMod.next(base * 1.5 + 7);
    if (q.kind == TailKind::resonant)
      signal = .7 * signal + .3 * tailNoise.between(-1, 1);
    if (q.kind == TailKind::phaseWarp)
      signal = std::sin(phase + 3 * q.motion * modulation);
    auto envelope =
        edge(t, q.duration, q.attack, q.release) * std::exp(-t / q.decay);
    if (q.kind == TailKind::elastic)
      envelope *=
          1 - .65 * q.motion +
          .65 * q.motion * std::pow(.5 + .5 * std::cos(twoPi * q.speed * t), 2);
    signal = shape(signal * envelope, q.drive * (.4 + .6 * progress), 0);
    const auto cutoff =
        std::clamp(q.cutoff * std::pow(2.0, q.sweep * std::exp(-t / .12) +
                                                (q.kind == TailKind::resonant
                                                     ? movement * 2
                                                     : 0)),
                   40.0, std::min(18000.0, rate * .4));
    return std::tanh(tailFilter.process(signal, cutoff, q.resonance, hr)) *
           q.level * edge(t, q.duration, .0001, q.release);
  });
  std::size_t count = 0;
  for (const auto &v : result.layers)
    count = std::max(count, v.size());
  result.mix.assign(count, 0);
  for (const auto &v : result.layers)
    for (std::size_t i = 0; i < v.size(); ++i)
      result.mix[i] += .35f * v[i];
  double peak = 0;
  for (auto v : result.mix)
    peak = std::max(peak, std::abs(static_cast<double>(v)));
  // Attenuation only, never normalise a quiet candidate upward. Layer buffers
  // stay unchanged.
  result.safetyGain = peak > .891250938 ? .891250938 / peak : 1;
  if (result.safetyGain < 1)
    for (auto &v : result.mix)
      v *= static_cast<float>(result.safetyGain);
  if (!retainLayers)
    for (auto &v : result.layers)
      std::vector<float>().swap(v);
  return result;
}
} // namespace kravatek::genetic
