#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <algorithm>
#include <cmath>

SkellCastProcessor::SenderThread::SenderThread(SkellCastProcessor& owner)
    : juce::Thread("SkellCast localhost audio sender"), owner_(owner) {}
SkellCastProcessor::SenderThread::~SenderThread() { stop(); }
void SkellCastProcessor::SenderThread::start() { if (!isThreadRunning()) startThread(); }
void SkellCastProcessor::SenderThread::stop() { signalThreadShouldExit(); stopThread(1500); }
void SkellCastProcessor::SenderThread::run() {
    juce::DatagramSocket socket(false);
    skellcast::Packet p;
    while (!threadShouldExit()) {
        if (queue_.pop(p)) {
            // Do not transmit queued frames after the user switches sharing off.
            if (owner_.state.getRawParameterValue("send")->load(std::memory_order_relaxed) < 0.5f)
                continue;
            if (socket.write("127.0.0.1", skellcast::kPort, &p,
                             static_cast<int>(skellcast::packetBytes(p))) < 0)
                owner_.droppedPackets.fetch_add(1, std::memory_order_relaxed);
        } else {
            juce::Thread::sleep(1);
        }
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout SkellCastProcessor::createParams() {
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID {"send", 1}, "Share to receiver", false));
    return {params.begin(), params.end()};
}
SkellCastProcessor::SkellCastProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                    .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      state(*this, nullptr, "SkellCastParams", createParams()), sender_(*this) {}
SkellCastProcessor::~SkellCastProcessor() { sender_.stop(); }
void SkellCastProcessor::prepareToPlay(double sampleRate, int) {
    rate_ = static_cast<std::uint32_t>(juce::jlimit(8000.0, 192000.0, sampleRate));
    sender_.start();
}
void SkellCastProcessor::releaseResources() { sender_.stop(); }
bool SkellCastProcessor::isBusesLayoutSupported(const BusesLayout& l) const {
    const auto input = l.getMainInputChannelSet();
    return (input == juce::AudioChannelSet::mono() || input == juce::AudioChannelSet::stereo()) &&
           l.getMainOutputChannelSet() == input;
}
void SkellCastProcessor::processBlock(juce::AudioBuffer<float>& audio, juce::MidiBuffer&) {
    juce::ScopedNoDenormals noDenormals;
    const int count = audio.getNumSamples();
    const int chans = audio.getNumChannels();
    if (count == 0 || chans == 0) return;
    const auto* l = audio.getReadPointer(0);
    const auto* r = audio.getReadPointer(chans > 1 ? 1 : 0);
    if (state.getRawParameterValue("send")->load(std::memory_order_relaxed) < 0.5f) return;
    for (int offset = 0; offset < count; offset += static_cast<int>(skellcast::kPacketFrames)) {
        skellcast::Packet p;
        p.sequence = sequence_++;
        p.sampleRate = rate_;
        p.frames = static_cast<std::uint16_t>(std::min(static_cast<int>(skellcast::kPacketFrames), count - offset));
        for (int f = 0; f < p.frames; ++f) {
            p.audio[f] = {skellcast::saneSample(l[offset + f]),
                          skellcast::saneSample(r[offset + f])};
        }
        if (!sender_.enqueue(p)) droppedPackets.fetch_add(1, std::memory_order_relaxed);
    }
    // Intentionally no modification to the FL Studio audio passing through this effect.
}
juce::AudioProcessorEditor* SkellCastProcessor::createEditor() { return new SkellCastEditor(*this); }
void SkellCastProcessor::getStateInformation(juce::MemoryBlock& dest) {
    if (auto xml = state.copyState().createXml()) copyXmlToBinary(*xml, dest);
}
void SkellCastProcessor::setStateInformation(const void* data, int size) {
    if (auto xml = getXmlFromBinary(data, size))
        if (xml->hasTagName(state.state.getType())) state.replaceState(juce::ValueTree::fromXml(*xml));
}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SkellCastProcessor(); }
