#include "AudioFifo.h"
#include "OutputRouting.h"
#include "OutputSelection.h"
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
    using skellcast::OutputCandidate;
    // A single/default output must never be used as a second audio copy.
    constexpr OutputCandidate onlyDefault[] = {{true, false, false, true}};
    CHECK(skellcast::selectSpareOutput(onlyDefault, 1) == -1);
    CHECK(skellcast::selectSpareOutput(nullptr, 0) == -1);
    constexpr OutputCandidate activeOnly[] = {{false, true, false, true}};
    CHECK(skellcast::selectSpareOutput(activeOnly, 1) == -1);
    constexpr OutputCandidate unknownOnly[] = {{false, false, true, false}};
    CHECK(skellcast::selectSpareOutput(unknownOnly, 1) == -1);
    constexpr OutputCandidate withSpare[] = {
        {true, false, false, true}, // default speaker, forbidden
        {false, true, true, true}, // virtual endpoint already in use, forbidden
        {false, false, false, true}, // spare headphones, eligible
        {false, false, true, true}  // spare virtual cable, prefer this for OBS
    };
    CHECK(skellcast::selectSpareOutput(withSpare, 4) == 3);
    constexpr OutputCandidate physicalOnly[] = {
        {true, false, false, true},
        {false, false, false, true}
    };
    CHECK(skellcast::selectSpareOutput(physicalOnly, 2) == 1);
    CHECK(skellcast::outputTargetFromParameter(0.0f) == skellcast::OutputTarget::Cable);
    CHECK(skellcast::outputTargetFromParameter(1.0f) == skellcast::OutputTarget::Desktop);
    CHECK(skellcast::outputTargetFromParameter(0.49f) == skellcast::OutputTarget::Cable);
    CHECK(skellcast::outputTargetFromParameter(0.51f) == skellcast::OutputTarget::Desktop);
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
