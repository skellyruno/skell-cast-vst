#pragma once
#include "AudioFifo.h"
#include <atomic>
#include <cstdint>
#include <thread>

class DirectCableOutput final {
public:
    explicit DirectCableOutput(std::atomic<float>& sendParam);
    ~DirectCableOutput();
    void start(std::uint32_t rate);
    void stop();
    void push(const float* left, const float* right, std::size_t count) noexcept;
    std::uint64_t overflowCount() const noexcept { return overflows_.load(std::memory_order_relaxed); }
private:
    void run() noexcept;
    bool runCableStream() noexcept;
    bool sharing() const noexcept { return send_.load(std::memory_order_relaxed) >= 0.5f; }

    std::atomic<float>& send_;
    std::atomic<bool> quit_ {false};
    std::atomic<std::uint32_t> sampleRate_ {48000};
    std::atomic<std::uint64_t> overflows_ {0};
    skellcast::AudioFifo<> fifo_;
    std::thread thread_;
    DirectCableOutput(const DirectCableOutput&) = delete;
    DirectCableOutput& operator=(const DirectCableOutput&) = delete;
};
