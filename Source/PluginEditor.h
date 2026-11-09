#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

// One interactive control: a two-state transmit button with a red/green pilot light.
class SkellCastPowerButton final : public juce::Button {
public:
    SkellCastPowerButton();
    void paintButton(juce::Graphics& g, bool highlighted, bool pressed) override;
};

class SkellCastEditor final : public juce::AudioProcessorEditor {
public:
    explicit SkellCastEditor(SkellCastProcessor&);
    ~SkellCastEditor() override = default;
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    SkellCastPowerButton powerButton_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> sendAttachment_;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SkellCastEditor)
};
