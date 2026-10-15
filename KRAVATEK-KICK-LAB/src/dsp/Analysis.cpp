#include "Analysis.h"
#include <algorithm>
#include <cmath>
#include <complex>
#include <numbers>
#include <stdexcept>
#include <vector>
namespace kravatek {
namespace {
double db(double value) {return 20 * std::log10(std::max(value, 1e-6));}
}
double comparisonRms(std::span<const float> samples, double rate) noexcept {
    const auto count = std::min(samples.size(), static_cast<std::size_t>(rate * .4));
    double sum = 0; for (std::size_t i=0; i<count; ++i) sum += samples[i] * static_cast<double>(samples[i]);
    return std::sqrt(sum / std::max(1.0, rate * .4)); // Fixed 400 ms window, with silence padding.
}
AudioAnalysis analyse(std::span<const float> samples, double rate) {
    if (!std::isfinite(rate) || rate<8000 || rate>192000 || samples.empty())
        throw std::invalid_argument("Invalid analysis buffer or rate");
    AudioAnalysis result; double peak=0, energy=0, sum=0;
    for (auto value:samples) {
        if (!std::isfinite(value)) throw std::invalid_argument("Non-finite analysis input");
        peak=std::max(peak,std::abs(static_cast<double>(value))); energy+=value*static_cast<double>(value); sum+=value;
    }
    result.peakDb=db(peak); result.rmsDb=db(std::sqrt(energy / static_cast<double>(samples.size())));
    result.dc=sum/static_cast<double>(samples.size()); result.durationSeconds=static_cast<double>(samples.size())/rate;
    constexpr std::size_t n=8192;
    std::vector<std::complex<double>> fft(n);
    const auto count=std::min(n,samples.size()); double windowSum=0;
    for (std::size_t i=0;i<count;++i) {
        const auto window=count > 1 ? .5-.5*std::cos(2*std::numbers::pi*static_cast<double>(i)/static_cast<double>(count-1)) : 1;
        fft[i]=samples[i]*window; windowSum+=window;
    }
    for (std::size_t i=1,j=0;i<n;++i) {
        auto bit=n>>1; for (;j&bit;bit>>=1) j^=bit; j^=bit;
        if (i<j) std::swap(fft[i],fft[j]);
    }
    for (std::size_t length=2;length<=n;length<<=1) {
        const auto angle=-2*std::numbers::pi/static_cast<double>(length);
        const std::complex<double> step(std::cos(angle),std::sin(angle));
        for (std::size_t i=0;i<n;i+=length) {
            std::complex<double> w=1;
            for (std::size_t j=0;j<length/2;++j) {
                const auto a=fft[i+j], b=w*fft[i+j+length/2]; fft[i+j]=a+b; fft[i+j+length/2]=a-b; w*=step;
            }
        }
    }
    const auto high=std::min(18000.0,rate*.48);
    for (std::size_t i=0;i<result.spectrumDb.size();++i) {
        const auto lowFrequency=20*std::pow(high/20,static_cast<double>(i)/64);
        const auto highFrequency=20*std::pow(high/20,static_cast<double>(i+1)/64);
        const auto first=std::clamp(static_cast<std::size_t>(std::floor(lowFrequency*n/rate)),std::size_t(1),n/2-1);
        const auto last=std::clamp(static_cast<std::size_t>(std::ceil(highFrequency*n/rate)),first+1,n/2);
        double maximum=0; for (auto k=first;k<last;++k) maximum=std::max(maximum,std::abs(fft[k]));
        result.spectrumDb[i]=db(2*maximum/std::max(1.0,windowSum));
    }
    return result;
}
}
