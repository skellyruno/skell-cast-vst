#include "PluginEditor.h"

namespace {
const juce::Colour background {0xff101411};
const juce::Colour surface {0xff1b231d};
const juce::Colour green {0xffa6ff48};
const juce::Colour red {0xffff555b};
const juce::Colour text {0xffe8f2e9};
const juce::Colour secondary {0xff809287};
}

SkellCastPowerButton::SkellCastPowerButton() : juce::Button("Share audio") {
    setClickingTogglesState(true);
    setTooltip("Send FL Studio audio to the SkellCast receiver (ON/OFF)");
}

void SkellCastPowerButton::paintButton(juce::Graphics& g, bool highlighted, bool pressed) {
    const auto on = getToggleState();
    const auto light = on ? green : red;
    auto b = getLocalBounds().toFloat().reduced(2.0f);

    // Large high-contrast single action. All visual parts belong to the same button.
    g.setColour(surface.brighter(highlighted ? 0.09f : 0.0f).darker(pressed ? 0.12f : 0.0f));
    g.fillRoundedRectangle(b, 13.0f);
    g.setColour(light.withAlpha(on ? 0.75f : 0.52f));
    g.drawRoundedRectangle(b.reduced(0.6f), 13.0f, on ? 1.7f : 1.2f);

    const float cx = b.getX() + 39.0f;
    const float cy = b.getCentreY();
    // Diffuse pilot light and solid centre (green when transmitting, red when off).
    g.setGradientFill(juce::ColourGradient(light.withAlpha(0.35f), cx, cy,
                                           light.withAlpha(0.0f), cx + 23.0f, cy + 23.0f, true));
    g.fillEllipse(cx - 23.0f, cy - 23.0f, 46.0f, 46.0f);
    g.setColour(light.darker(0.48f));
    g.fillEllipse(cx - 12.0f, cy - 12.0f, 24.0f, 24.0f);
    g.setColour(light);
    g.fillEllipse(cx - 9.0f, cy - 9.0f, 18.0f, 18.0f);
    g.setColour(juce::Colours::white.withAlpha(0.48f));
    g.fillEllipse(cx - 5.0f, cy - 7.0f, 5.0f, 5.0f);

    const auto labelX = static_cast<int>(b.getX() + 78.0f);
    const auto labelWidth = static_cast<int>(b.getWidth() - 87.0f);
    g.setColour(secondary);
    g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
    g.drawText("AUDIO SHARING", labelX, static_cast<int>(b.getY() + 11.0f),
               labelWidth, 16, juce::Justification::centredLeft);
    g.setColour(light);
    g.setFont(juce::Font(juce::FontOptions(24.0f, juce::Font::bold)));
    g.drawText(on ? "ON" : "OFF", labelX, static_cast<int>(b.getY() + 29.0f),
               labelWidth, 30, juce::Justification::centredLeft);
}

SkellCastEditor::SkellCastEditor(SkellCastProcessor& p) : juce::AudioProcessorEditor(&p) {
    addAndMakeVisible(powerButton_);
    sendAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        p.state, "send", powerButton_);
    setResizable(false, false);
    setSize(340, 184);
}

void SkellCastEditor::resized() {
    powerButton_.setBounds(24, 83, getWidth() - 48, 77);
}

void SkellCastEditor::paint(juce::Graphics& g) {
    g.fillAll(background);
    auto bounds = getLocalBounds().toFloat().reduced(8.0f);
    g.setColour(surface);
    g.fillRoundedRectangle(bounds, 14.0f);
    g.setColour(green.withAlpha(0.37f));
    g.drawRoundedRectangle(bounds, 14.0f, 1.0f);

    g.setColour(text);
    g.setFont(juce::Font(juce::FontOptions(25.0f, juce::Font::bold)));
    g.drawText("SKELLCAST", 25, 19, getWidth() - 50, 34, juce::Justification::centredLeft);
    g.setColour(secondary);
    g.setFont(juce::Font(juce::FontOptions(11.0f)));
    g.drawText("FL STUDIO  /  STREAM + DISCORD", 26, 56, getWidth() - 52, 16,
               juce::Justification::centredLeft);
}
