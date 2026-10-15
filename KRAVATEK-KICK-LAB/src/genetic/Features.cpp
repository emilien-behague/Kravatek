#include "Features.h"
#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdint>
#include <numbers>
#include <vector>
namespace kravatek::genetic {
namespace {
void fft(std::vector<std::complex<double>> &f) {
  const auto n = f.size();
  for (std::size_t i = 1, j = 0; i < n; ++i) {
    auto bit = n >> 1;
    for (; j & bit; bit >>= 1)
      j ^= bit;
    j ^= bit;
    if (i < j)
      std::swap(f[i], f[j]);
  }
  for (std::size_t len = 2; len <= n; len <<= 1) {
    const auto angle = -2 * std::numbers::pi / static_cast<double>(len);
    const std::complex<double> step(std::cos(angle), std::sin(angle));
    for (std::size_t i = 0; i < n; i += len) {
      std::complex<double> w = 1;
      for (std::size_t j = 0; j < len / 2; ++j) {
        auto a = f[i + j], b = w * f[i + j + len / 2];
        f[i + j] = a + b;
        f[i + j + len / 2] = a - b;
        w *= step;
      }
    }
  }
}
} // namespace
Features measure(std::span<const float> samples, double rate,
                 bool completeEdges) {
  Features result;
  result.basic = analyse(samples, rate);
  const auto count = samples.size();
  double energy = 0, transient = 0, peak = 0;
  std::size_t effectiveEnd = 0;
  const auto threshold = std::pow(10.0, result.basic.peakDb / 20) * .001;
  std::size_t first10 = count, first90 = count;
  const auto amplitude = std::pow(10.0, result.basic.peakDb / 20);
  for (std::size_t i = 0; i < count; ++i) {
    const auto value = static_cast<double>(samples[i]), power = value * value;
    energy += power;
    result.timeEnergy[std::min(std::size_t(7), i * 8 / count)] += power;
    if (std::abs(value) > threshold)
      effectiveEnd = i + 1;
    if (i < static_cast<std::size_t>(rate * .015)) {
      transient += power;
      peak = std::max(peak, std::abs(value));
    }
    if (first10 == count && std::abs(value) >= amplitude * .1)
      first10 = i;
    if (first90 == count && std::abs(value) >= amplitude * .9)
      first90 = i;
  }
  result.effectiveSeconds = static_cast<double>(effectiveEnd) / rate;
  result.transientFraction = energy > 0 ? transient / energy : 0;
  result.transientPeakDb = 20 * std::log10(std::max(peak, 1e-6));
  result.attackSeconds =
      first90 >= first10 ? static_cast<double>(first90 - first10) / rate : 0;
  for (auto &v : result.timeEnergy)
    v = energy > 0 ? v / energy : 0;
  constexpr std::size_t n = 8192, hop = 4096;
  double spectralEnergy = 0, weighted = 0;
  std::vector<std::complex<double>> spectrum(n);
  // UKE: centered square-root periodic Hann, 50% overlap and zero padding.
  // Sum of squared windows is one, including the transient at time zero.
  // Legacy analysis remains unchanged for deterministic GSE retry decisions.
  const auto first = completeEdges ? -static_cast<std::int64_t>(hop) : 0;
  for (auto start = first; start < static_cast<std::int64_t>(count);
       start += static_cast<std::int64_t>(hop)) {
    std::fill(spectrum.begin(), spectrum.end(), 0);
    for (std::size_t i = 0; i < n; ++i) {
      const auto j = start + static_cast<std::int64_t>(i);
      if (j < 0 || j >= static_cast<std::int64_t>(count))
        continue;
      const auto w =
          completeEdges
              ? std::sqrt(.5 - .5 * std::cos(2 * std::numbers::pi *
                                             static_cast<double>(i) / n))
              : (.5 -
                 .5 * std::cos(2 * std::numbers::pi * static_cast<double>(i) /
                               static_cast<double>(n - 1)));
      spectrum[i] = samples[static_cast<std::size_t>(j)] * w;
    }
    fft(spectrum);
    for (std::size_t i = completeEdges ? 0 : 1;
         i < (completeEdges ? n / 2 + 1 : n / 2); ++i) {
      const auto hz = static_cast<double>(i) * rate / n,
                 power = std::norm(spectrum[i]) *
                         (completeEdges && (i == 0 || i == n / 2) ? .5 : 1.);
      spectralEnergy += power;
      weighted += hz * power;
      result.bandEnergy[hz < 200 ? 0 : hz < 2000 ? 1 : 2] += power;
    }
  }
  result.centroidHz = spectralEnergy > 0 ? weighted / spectralEnergy : 0;
  for (auto &v : result.bandEnergy)
    v = spectralEnergy > 0 ? v / spectralEnergy : 0;
  // Low-pass before decimating to ~2 kHz. Autocorrelation is an estimate of the
  // dominant low-frequency periodicity, with confidence; complex tails may be
  // ambiguous.
  const auto step = std::max(1, static_cast<int>(rate / 2000));
  const auto lowRate = rate / step;
  const auto alpha = 1 - std::exp(-2 * std::numbers::pi * 280 / rate);
  double p1 = 0, p2 = 0, p3 = 0;
  std::vector<double> low;
  const auto stop = std::min(count, static_cast<std::size_t>(rate * .4));
  for (std::size_t i = 0; i < stop; ++i) {
    p1 += alpha * (samples[i] - p1);
    p2 += alpha * (p1 - p2);
    p3 += alpha * (p2 - p3);
    if (i % static_cast<std::size_t>(step) == 0 &&
        i > static_cast<std::size_t>(rate * .025))
      low.push_back(p3);
  }
  const auto minLag = std::max(2, static_cast<int>(lowRate / 150)),
             maxLag = static_cast<int>(lowRate / 25);
  if (low.size() > static_cast<std::size_t>(maxLag * 3)) {
    double mean = 0;
    for (auto v : low)
      mean += v;
    mean /= static_cast<double>(low.size());
    for (auto &v : low)
      v -= mean;
    std::vector<double> correlation(static_cast<std::size_t>(maxLag + 2));
    double best = 0;
    int bestLag = 0;
    for (int lag = minLag; lag <= maxLag; ++lag) {
      double dot = 0, a = 0, b = 0;
      for (std::size_t i = static_cast<std::size_t>(lag); i < low.size(); ++i) {
        auto x = low[i], y = low[i - static_cast<std::size_t>(lag)];
        dot += x * y;
        a += x * x;
        b += y * y;
      }
      correlation[static_cast<std::size_t>(lag)] =
          a * b > 1e-18 ? dot / std::sqrt(a * b) : 0;
    }
    for (int lag = minLag + 1; lag < maxLag; ++lag) {
      const auto c = correlation[static_cast<std::size_t>(lag)];
      if (c > best && c >= correlation[static_cast<std::size_t>(lag - 1)] &&
          c >= correlation[static_cast<std::size_t>(lag + 1)]) {
        best = c;
        bestLag = lag;
      }
    }
    if (best > .55 && bestLag > 0) {
      for (int lag = minLag + 1; lag < bestLag; ++lag)
        if (correlation[static_cast<std::size_t>(lag)] > best * .96 &&
            correlation[static_cast<std::size_t>(lag)] >=
                correlation[static_cast<std::size_t>(lag - 1)] &&
            correlation[static_cast<std::size_t>(lag)] >=
                correlation[static_cast<std::size_t>(lag + 1)]) {
          bestLag = lag;
          break;
        }
      const auto y0 = correlation[static_cast<std::size_t>(bestLag - 1)],
                 y1 = correlation[static_cast<std::size_t>(bestLag)],
                 y2 = correlation[static_cast<std::size_t>(bestLag + 1)];
      const auto denominator = y0 - 2 * y1 + y2;
      const auto offset =
          std::abs(denominator) > 1e-12
              ? std::clamp(.5 * (y0 - y2) / denominator, -.5, .5)
              : 0;
      result.fundamentalHz = lowRate / (bestLag + offset);
      result.fundamentalConfidence = std::clamp(y1, 0.0, 1.0);
    }
  }
  return result;
}
double featureDistance(const Features &a, const Features &b) noexcept {
  double distance =
      std::min(1.0, std::abs(a.basic.rmsDb - b.basic.rmsDb) / 30) * .1;
  distance +=
      std::min(1.0, std::abs(a.effectiveSeconds - b.effectiveSeconds) / .8) *
      .15;
  distance +=
      std::min(1.0,
               std::abs(std::log2((a.centroidHz + 20) / (b.centroidHz + 20))) /
                   5) *
      .2;
  if (a.fundamentalHz > 0 && b.fundamentalHz > 0)
    distance +=
        std::min(1.0,
                 std::abs(std::log2(a.fundamentalHz / b.fundamentalHz)) / 2) *
        .1;
  for (std::size_t i = 0; i < 3; ++i)
    distance += std::abs(a.bandEnergy[i] - b.bandEnergy[i]) * .1;
  for (std::size_t i = 0; i < 8; ++i)
    distance += std::abs(a.timeEnergy[i] - b.timeEnergy[i]) * .075;
  distance += std::abs(a.transientFraction - b.transientFraction) * .1;
  return std::clamp(distance, 0.0, 1.0);
}
double similarity(std::span<const float> a, const Features &fa,
                  std::span<const float> b, const Features &fb) noexcept {
  double dot = 0, aa = 0, bb = 0;
  for (auto x : a)
    aa += x * static_cast<double>(x);
  for (auto y : b)
    bb += y * static_cast<double>(y);
  for (std::size_t i = 0; i < std::min(a.size(), b.size()); ++i)
    dot += a[i] * static_cast<double>(b[i]);
  const auto correlation = aa * bb > 0
                               ? std::clamp(dot / std::sqrt(aa * bb), 0.0, 1.0)
                               : (aa == bb ? 1 : 0);
  return std::clamp(.6 * (1 - featureDistance(fa, fb)) + .4 * correlation, 0.0,
                    1.0);
}
} // namespace kravatek::genetic
