#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class SkellCastPowerButton final : public juce::Button {
public:
    SkellCastPowerButton();
    void paintButton(juce::Graphics& g, bool highlighted, bool pressed) override;
};

class SkellCastDestinationSwitch final : public juce::Button {
public:
    SkellCastDestinationSwitch();
    void paintButton(juce::Graphics& g, bool highlighted, bool pressed) override;
};

class SkellCastEditor final : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit SkellCastEditor(SkellCastProcessor&);
    ~SkellCastEditor() override = default;
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    void timerCallback() override;
    SkellCastProcessor& processor_;
    SkellCastPowerButton powerButton_;
    SkellCastDestinationSwitch destinationSwitch_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> sendAttachment_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> destinationAttachment_;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SkellCastEditor)
};
