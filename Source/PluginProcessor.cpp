#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "GlobalSettings.h"
#include <cstdint>

juce::AudioProcessorValueTreeState::ParameterLayout SkellCastProcessor::createParams() {
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    // Keep the exact historical IDs for existing FL Studio automation.
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID {"send", 1}, "Share audio", false));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID {"desktop", 1}, "Send to spare output", false));
    return {params.begin(), params.end()};
}

SkellCastProcessor::SkellCastProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                    .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      state(*this, nullptr, "SkellCastParams", createParams()),
      output_(*state.getRawParameterValue("send"),
              *state.getRawParameterValue("desktop")) {
    // Load the user's last actual choice, not this project's stored flags.
    applyGlobalSettings();
    state.addParameterListener("send", this);
    state.addParameterListener("desktop", this);
    if (juce::MessageManager::getInstanceWithoutCreating() != nullptr)
        startTimerHz(4); // Synchronise other open instances, with no audio-thread work.
}

SkellCastProcessor::~SkellCastProcessor() {
    stopTimer();
    state.removeParameterListener("send", this);
    state.removeParameterListener("desktop", this);
    cancelPendingUpdate();
    if (auto* mm = juce::MessageManager::getInstanceWithoutCreating();
        mm != nullptr && mm->isThisTheMessageThread())
        SkellCastGlobalSettings::instance().saveIfDirty();
    output_.stop();
}

void SkellCastProcessor::applyGlobalSettings() {
    const auto& settings = SkellCastGlobalSettings::instance();
    applyingGlobal_.store(true, std::memory_order_release);
    if (auto* parameter = state.getParameter("send")) {
        const float desired = settings.sendEnabled() ? 1.0f : 0.0f;
        if (parameter->getValue() != desired)
            parameter->setValueNotifyingHost(desired);
    }
    if (auto* parameter = state.getParameter("desktop")) {
        const float desired = settings.useSpareOutput() ? 1.0f : 0.0f;
        if (parameter->getValue() != desired)
            parameter->setValueNotifyingHost(desired);
    }
    applyingGlobal_.store(false, std::memory_order_release);
}

void SkellCastProcessor::parameterChanged(const juce::String& id, float value) {
    if (applyingGlobal_.load(std::memory_order_acquire))
        return;
    auto& settings = SkellCastGlobalSettings::instance();
    if (id == "send") settings.changeSend(value >= 0.5f);
    else if (id == "desktop") settings.changeSpare(value >= 0.5f);
    // No disk I/O on a DAW automation/audio thread.
    if (auto* mm = juce::MessageManager::getInstanceWithoutCreating();
        mm != nullptr && mm->isThisTheMessageThread())
        settings.saveIfDirty();
    else
        triggerAsyncUpdate();
}
void SkellCastProcessor::handleAsyncUpdate() {
    SkellCastGlobalSettings::instance().saveIfDirty();
}
void SkellCastProcessor::timerCallback() {
    applyGlobalSettings();
    SkellCastGlobalSettings::instance().saveIfDirty();
}

void SkellCastProcessor::prepareToPlay(double sampleRate, int) {
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
    // Pass-through untouched: no changes to the previously working audio engine.
}

juce::AudioProcessorEditor* SkellCastProcessor::createEditor() { return new SkellCastEditor(*this); }

void SkellCastProcessor::getStateInformation(juce::MemoryBlock& dest) {
    if (auto xml = state.copyState().createXml())
        copyXmlToBinary(*xml, dest);
}
void SkellCastProcessor::setStateInformation(const void* data, int size) {
    if (auto xml = getXmlFromBinary(data, size)) {
        if (xml->hasTagName(state.state.getType())) {
            auto loaded = juce::ValueTree::fromXml(*xml);
            const auto& settings = SkellCastGlobalSettings::instance();
            for (int i = 0; i < loaded.getNumChildren(); ++i) {
                auto entry = loaded.getChild(i);
                if (entry.getProperty("id").toString() == "send")
                    entry.setProperty("value", settings.sendEnabled() ? 1.0f : 0.0f, nullptr);
                else if (entry.getProperty("id").toString() == "desktop")
                    entry.setProperty("value", settings.useSpareOutput() ? 1.0f : 0.0f, nullptr);
            }
            applyingGlobal_.store(true, std::memory_order_release);
            state.replaceState(loaded);
            applyingGlobal_.store(false, std::memory_order_release);
        }
    }
    // This last step handles older project states missing one of the parameters.
    applyGlobalSettings();
}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SkellCastProcessor(); }
