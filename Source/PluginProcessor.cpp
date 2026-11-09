#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cstdint>

juce::AudioProcessorValueTreeState::ParameterLayout SkellCastProcessor::createParams() {
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID {"send", 1}, "Share audio", false));
    return {params.begin(), params.end()};
}
SkellCastProcessor::SkellCastProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                    .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      state(*this, nullptr, "SkellCastParams", createParams()),
      output_(*state.getRawParameterValue("send")) {}
SkellCastProcessor::~SkellCastProcessor() { output_.stop(); }
void SkellCastProcessor::prepareToPlay(double sampleRate, int) {
    // Windows WASAPI handles sample-rate conversion in shared mode.
    const auto rate = static_cast<std::uint32_t>(juce::jlimit(8000.0, 192000.0, sampleRate));
    output_.start(rate);
}
void SkellCastProcessor::releaseResources() { output_.stop(); }
bool SkellCastProcessor::isBusesLayoutSupported(const BusesLayout& layout) const {
    const auto input = layout.getMainInputChannelSet();
    return (input == juce::AudioChannelSet::mono() || input == juce::AudioChannelSet::stereo()) &&
           layout.getMainOutputChannelSet() == input;
}
void SkellCastProcessor::processBlock(juce::AudioBuffer<float>& audio, juce::MidiBuffer&) {
    juce::ScopedNoDenormals noDenormals;
    if (state.getRawParameterValue("send")->load(std::memory_order_relaxed) < 0.5f) return;
    if (audio.getNumSamples() == 0 || audio.getNumChannels() == 0) return;
    const float* left = audio.getReadPointer(0);
    const float* right = audio.getReadPointer(audio.getNumChannels() > 1 ? 1 : 0);
    output_.push(left, right, static_cast<std::size_t>(audio.getNumSamples()));
    // This effect intentionally leaves FL Studio's audio output unchanged.
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
