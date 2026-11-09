#include "BridgeProtocol.h"
#include <cstdlib>
#include <cmath>
#include <iostream>
#include <limits>
#include <thread>
#include <atomic>

#define CHECK(expr) do { if (!(expr)) { \
    std::cerr << "CHECK failed: " #expr " at line " << __LINE__ << "\n"; \
    std::exit(EXIT_FAILURE); \
} } while (false)

int main() {
    using namespace skellcast;
    Packet p;
    p.sampleRate = 44100;
    p.frames = 128;
    p.audio[0] = {0.5f, -0.5f};
    CHECK(packetBytes(p) == 20 + kPacketFrames * 8);
    CHECK(validPacket(p, packetBytes(p)));
    CHECK(!validPacket(p, packetBytes(p) - 1));
    p.magic = 0;
    CHECK(!validPacket(p, packetBytes(p)));
    p.magic = kMagic;
    p.frames = 129;
    CHECK(!validPacket(p, packetBytes(p)));
    CHECK(saneSample(std::numeric_limits<float>::infinity()) == 0.0f);
    CHECK(saneSample(std::numeric_limits<float>::quiet_NaN()) == 0.0f);
    CHECK(saneSample(10.0f) == 4.0f);
    SpscQueue<int, 8> q;
    for (int i = 0; i < 7; ++i) CHECK(q.push(i));
    CHECK(!q.push(99));
    for (int i = 0; i < 7; ++i) { int j=-1; CHECK(q.pop(j) && j == i); }
    int j = -1;
    CHECK(!q.pop(j));
    CHECK(q.push(42));
    CHECK(q.pop(j) && j == 42);
    FrameQueue frames;
    for (int i = 0; i < 1024; ++i) CHECK(frames.push({i / 1024.f, -i / 1024.f}));
    for (int i = 0; i < 1024; ++i) {
        StereoFrame f;
        CHECK(frames.pop(f));
        CHECK(std::abs(f.left - i / 1024.f) < 0.00001f);
    }
    SpscQueue<int, 512> concurrent;
    constexpr int total = 100000;
    std::atomic<bool> good {true};
    std::thread producer([&] {
        for (int i = 0; i < total; ++i) {
            while (!concurrent.push(i)) std::this_thread::yield();
        }
    });
    std::thread consumer([&] {
        for (int i = 0; i < total; ++i) {
            int value = -1;
            while (!concurrent.pop(value)) std::this_thread::yield();
            if (value != i) good.store(false);
        }
    });
    producer.join();
    consumer.join();
    CHECK(good.load());
    std::cout << "SkellCast packet, ring, 2-thread FIFO stress tests passed\n";
}
