#include "AudioFifo.h"
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <thread>

#define CHECK(expr) do { if (!(expr)) { \
    std::cerr << "CHECK failed: " #expr " at line " << __LINE__ << "\n"; \
    std::exit(EXIT_FAILURE); \
} } while (false)

int main() {
    using skellcast::AudioFifo;
    CHECK(AudioFifo<>::sanitize(std::numeric_limits<float>::infinity()) == 0.0f);
    CHECK(AudioFifo<>::sanitize(std::numeric_limits<float>::quiet_NaN()) == 0.0f);
    CHECK(AudioFifo<>::sanitize(2.0f) == 1.0f);
    CHECK(AudioFifo<>::sanitize(-3.0f) == -1.0f);
    AudioFifo<8> fifo;
    const float left[10] = {0.1f,0.2f,0.3f,0.4f,0.5f,0.6f,0.7f,0.8f,0.9f,1};
    const float right[10] = {-0.1f,-0.2f,-0.3f,-0.4f,-0.5f,-0.6f,-0.7f,-0.8f,-0.9f,-1};
    CHECK(fifo.push(left,right,10) == 7);
    CHECK(fifo.available() == 7);
    CHECK(fifo.push(left,right,1) == 0);
    float out[20] {};
    CHECK(fifo.popInterleaved(out,3) == 3);
    CHECK(std::abs(out[0]-0.1f)<0.00001f && std::abs(out[1]+0.1f)<0.00001f);
    CHECK(fifo.push(left+7,right+7,3) == 3); // wrap-around
    CHECK(fifo.available() == 7);
    CHECK(fifo.discard(6) == 6);
    CHECK(fifo.popInterleaved(out,1) == 1);
    CHECK(std::abs(out[0]-1.0f)<0.00001f);
    CHECK(fifo.available() == 0);
    CHECK(fifo.popInterleaved(out,1) == 0);

    AudioFifo<1024> concurrent;
    constexpr int total = 180000;
    std::atomic<bool> good {true};
    std::thread producer([&] {
        for (int i = 0; i < total; ++i) {
            const float value = static_cast<float>(i % 8000) / 8000.0f;
            while (concurrent.push(&value, &value, 1) == 0) std::this_thread::yield();
        }
    });
    std::thread consumer([&] {
        float s[2] {};
        for (int i = 0; i < total; ++i) {
            while (concurrent.popInterleaved(s, 1) == 0) std::this_thread::yield();
            const float expected = static_cast<float>(i % 8000) / 8000.0f;
            if (std::abs(s[0]-expected)>0.00001f || std::abs(s[1]-expected)>0.00001f)
                good.store(false);
        }
    });
    producer.join();
    consumer.join();
    CHECK(good.load());
    CHECK(concurrent.available() == 0);
    std::cout << "SkellCast FIFO wrap, overflow, sanitization, and two-thread stress passed\n";
}
