#pragma once
#include <JuceHeader.h>
#include "BridgeProtocol.h"

class SkellCastProcessor final : public juce::AudioProcessor {
public:
    SkellCastProcessor();
    ~SkellCastProcessor() override;
    void prepareToPlay(double sampleRate, int maxBlockSize) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    juce::AudioProcessorValueTreeState state;
    std::atomic<std::uint64_t> droppedPackets {0};

private:
    class SenderThread final : private juce::Thread {
    public:
        explicit SenderThread(SkellCastProcessor& owner);
        ~SenderThread() override;
        void start();
        void stop();
        bool enqueue(const skellcast::Packet& p) noexcept { return queue_.push(p); }
    private:
        void run() override;
        SkellCastProcessor& owner_;
        skellcast::PacketQueue queue_;
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SenderThread)
    };
    static juce::AudioProcessorValueTreeState::ParameterLayout createParams();
    SenderThread sender_;
    std::uint32_t sequence_ = 0;
    std::uint32_t rate_ = 48000;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SkellCastProcessor)
};
