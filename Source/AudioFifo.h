#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace skellcast {
struct StereoFrame { float left; float right; };
static_assert(sizeof(StereoFrame) == sizeof(float) * 2, "Audio frames must be interleaved stereo floats");

// Lock-free single-producer/single-consumer FIFO. The DAW only writes; WASAPI only reads.
// No locks, allocations, OS calls or sleeps occur on the DAW audio callback.
template<std::size_t Capacity = 16384>
class AudioFifo final {
    static_assert(Capacity >= 2 && (Capacity & (Capacity - 1)) == 0, "Power-of-two capacity required");
public:
    std::size_t push(const float* left, const float* right, std::size_t count) noexcept {
        const auto write = write_.load(std::memory_order_relaxed);
        const auto read = read_.load(std::memory_order_acquire);
        const auto size = static_cast<std::size_t>(write - read);
        const auto copy = std::min(count, Capacity - 1 - std::min(size, Capacity - 1));
        for (std::size_t i = 0; i < copy; ++i)
            frames_[(write + i) & (Capacity - 1)] = {sanitize(left[i]), sanitize(right[i])};
        write_.store(write + static_cast<std::uint32_t>(copy), std::memory_order_release);
        return copy;
    }

    std::size_t popInterleaved(float* output, std::size_t count) noexcept {
        const auto read = read_.load(std::memory_order_relaxed);
        const auto write = write_.load(std::memory_order_acquire);
        const auto copy = std::min(count, static_cast<std::size_t>(write - read));
        for (std::size_t i = 0; i < copy; ++i) {
            const auto& sample = frames_[(read + i) & (Capacity - 1)];
            output[i * 2] = sample.left;
            output[i * 2 + 1] = sample.right;
        }
        read_.store(read + static_cast<std::uint32_t>(copy), std::memory_order_release);
        return copy;
    }

    std::size_t discard(std::size_t maximum) noexcept {
        const auto read = read_.load(std::memory_order_relaxed);
        const auto write = write_.load(std::memory_order_acquire);
        const auto count = std::min(maximum, static_cast<std::size_t>(write - read));
        read_.store(read + static_cast<std::uint32_t>(count), std::memory_order_release);
        return count;
    }

    void clearConsumer() noexcept { discard(Capacity); }
    std::size_t available() const noexcept {
        return static_cast<std::size_t>(write_.load(std::memory_order_acquire) - read_.load(std::memory_order_acquire));
    }
    static float sanitize(float f) noexcept {
        return std::isfinite(f) ? std::max(-1.0f, std::min(1.0f, f)) : 0.0f;
    }
private:
    std::array<StereoFrame, Capacity> frames_ {};
    alignas(64) std::atomic<std::uint32_t> write_ {0};
    alignas(64) std::atomic<std::uint32_t> read_ {0};
};
}
