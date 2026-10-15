#include "Playback.h"

#include <algorithm>
#include <cmath>

namespace kravatek {
static_assert(std::atomic<int>::is_always_lock_free);
static_assert(std::atomic<bool>::is_always_lock_free);

Playback::Playback() = default;

bool Playback::queue(std::span<const float> samples) noexcept {
    if (samples.empty() || samples.size() > maxSamples)
        return false;
    for (int i = 0; i < static_cast<int>(slots_.size()); ++i) {
        auto& slot = slots_[static_cast<std::size_t>(i)];
        bool free = true;
        if (!slot.free.compare_exchange_strong(free, false, std::memory_order_acquire))
            continue;
        std::copy(samples.begin(), samples.end(), slot.samples.get());
        slot.size = samples.size();
        const auto replaced = mailbox_.exchange(i, std::memory_order_acq_rel);
        if (replaced >= 0)
            slots_[static_cast<std::size_t>(replaced)].free.store(true, std::memory_order_release);
        return true;
    }
    return false; // Bounded, nonblocking overload policy: leave the pending command intact.
}

void Playback::release(int& index) noexcept {
    if (index >= 0)
        slots_[static_cast<std::size_t>(index)].free.store(true, std::memory_order_release);
    index = -1;
}

void Playback::promote() noexcept {
    current_ = next_;
    next_ = -1;
    position_ = 0;
    fadeRemaining_ = 0;
}

void Playback::process(float* const* outputs, int channels, int frames) noexcept {
    const auto pending = mailbox_.exchange(-1, std::memory_order_acq_rel);
    if (pending >= 0) {
        release(next_);
        next_ = pending;
        if (current_ < 0) promote();
        else if (fadeRemaining_ == 0) fadeRemaining_ = fadeSamples_;
    }
    for (int i = 0; i < frames; ++i) {
        float value = 0.0f;
        if (current_ >= 0) {
            auto& slot = slots_[static_cast<std::size_t>(current_)];
            value = slot.samples[position_++];
            if (fadeRemaining_ > 0) {
                value *= static_cast<float>(fadeRemaining_ - 1) / static_cast<float>(fadeSamples_);
                --fadeRemaining_;
                if (fadeRemaining_ == 0) {
                    release(current_);
                    promote();
                }
            }
            if (current_ >= 0 && position_ >= slots_[static_cast<std::size_t>(current_)].size) {
                release(current_);
                promote();
            }
        }
        for (int channel = 0; channel < channels; ++channel)
            if (outputs[channel] != nullptr) outputs[channel][i] = value;
    }
}

void Playback::reset() noexcept {
    auto pending = mailbox_.exchange(-1, std::memory_order_acq_rel);
    release(pending);
    release(current_);
    release(next_);
    position_ = 0;
    fadeRemaining_ = 0;
}

void Playback::setSampleRate(double rate) noexcept {
    fadeSamples_ = std::isfinite(rate) && rate >= 8000.0 && rate <= 192000.0
        ? static_cast<int>(rate * 0.005) : 240;
}
} // namespace kravatek
