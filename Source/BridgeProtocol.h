#pragma once

#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

namespace skellcast {
constexpr int kPort = 44991;
constexpr std::uint32_t kMagic = 0x534B4341; // 'SKCA'
constexpr std::uint32_t kVersion = 1;
constexpr std::size_t kPacketFrames = 128;
constexpr std::size_t kRingFrames = 65536;

struct StereoFrame { float left = 0.0f; float right = 0.0f; };

// Fixed layout for Windows x64, native little-endian floats. Localhost only.
struct Packet {
    std::uint32_t magic = kMagic;
    std::uint32_t version = kVersion;
    std::uint32_t sequence = 0;
    std::uint32_t sampleRate = 48000;
    std::uint16_t frames = 0;
    std::uint16_t channels = 2;
    StereoFrame audio[kPacketFrames] {};
};
static_assert(sizeof(StereoFrame) == 8);
static_assert(offsetof(Packet, audio) == 20);
static_assert(std::is_trivially_copyable_v<Packet>);

inline std::size_t packetBytes(const Packet& p) noexcept {
    return offsetof(Packet, audio) + static_cast<std::size_t>(p.frames) * sizeof(StereoFrame);
}
inline bool validPacket(const Packet& p, std::size_t bytes) noexcept {
    return p.magic == kMagic && p.version == kVersion && p.channels == 2 &&
           p.frames > 0 && p.frames <= kPacketFrames &&
           p.sampleRate >= 8000 && p.sampleRate <= 192000 &&
           bytes == packetBytes(p);
}

// Single producer / single consumer. Never allocate, lock, or wait in an audio callback.
// Capacity must be a power of two. One slot is intentionally reserved.
template <typename T, std::size_t Capacity>
class SpscQueue {
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be power of two");
public:
    bool push(const T& item) noexcept {
        const auto w = write_.load(std::memory_order_relaxed);
        if (w - read_.load(std::memory_order_acquire) >= Capacity - 1) return false;
        data_[w & (Capacity - 1)] = item;
        write_.store(w + 1, std::memory_order_release);
        return true;
    }
    bool pop(T& item) noexcept {
        const auto r = read_.load(std::memory_order_relaxed);
        if (r == write_.load(std::memory_order_acquire)) return false;
        item = data_[r & (Capacity - 1)];
        read_.store(r + 1, std::memory_order_release);
        return true;
    }
    std::size_t available() const noexcept {
        return static_cast<std::size_t>(write_.load(std::memory_order_acquire) -
                                        read_.load(std::memory_order_acquire));
    }
private:
    std::array<T, Capacity> data_ {};
    alignas(64) std::atomic<std::uint32_t> write_ {0};
    alignas(64) std::atomic<std::uint32_t> read_ {0};
};

using PacketQueue = SpscQueue<Packet, 128>;
using FrameQueue = SpscQueue<StereoFrame, kRingFrames>;

inline float saneSample(float v) noexcept {
    return std::isfinite(v) ? (v < -4.0f ? -4.0f : (v > 4.0f ? 4.0f : v)) : 0.0f;
}
} // namespace skellcast
