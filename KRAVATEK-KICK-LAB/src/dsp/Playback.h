#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <memory>
#include <span>

namespace kravatek {

// One GUI producer, one audio consumer. All storage is allocated at construction.
// Slots are owned either by the producer, the mailbox, or the consumer.
class Playback {
public:
    static constexpr std::size_t maxSamples = 768000;
    Playback();
    bool queue(std::span<const float> samples, std::size_t loopPeriodFrames = 0) noexcept;
    void stop() noexcept;
    void process(float* const* outputs, int channels, int frames) noexcept;
    // Invoke only while the device callback is stopped. queue() may run concurrently.
    void reset() noexcept;
    void setSampleRate(double rate) noexcept;
private:
    struct Slot {
        std::unique_ptr<float[]> samples = std::make_unique<float[]>(maxSamples);
        std::size_t size = 0;
        std::size_t loopPeriod = 0;
        std::atomic<bool> free {true};
    };
    std::array<Slot, 3> slots_;
    std::atomic<int> mailbox_ {-1};
    int current_ = -1;
    int next_ = -1;
    std::size_t position_ = 0;
    int fadeSamples_ = 240;
    int fadeRemaining_ = 0;
    void release(int& index) noexcept;
    void promote() noexcept;
};
} // namespace kravatek
