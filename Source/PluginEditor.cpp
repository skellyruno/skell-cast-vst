#include "PluginEditor.h"

namespace {
// Deliberately understated SKELLY palette: near-black metal, neon green, one red OFF LED.
const juce::Colour black        {0xff080b09};
const juce::Colour panel        {0xff101510};
const juce::Colour buttonFace   {0xff151c16};
const juce::Colour border       {0xff27342a};
const juce::Colour neon         {0xffadff3d};
const juce::Colour offRed       {0xffff5d62};
const juce::Colour white        {0xffedf5ee};
const juce::Colour muted        {0xff89988b};

// Original tiny vector skull mark. No textures, images, bitmaps, or extra assets.
void drawSkellyMark(juce::Graphics& g) {
    g.setColour(neon.withAlpha(0.92f));
    g.fillEllipse(22.0f, 22.0f, 25.0f, 23.0f);
    g.fillRoundedRectangle(27.0f, 38.0f, 15.0f, 10.0f, 3.0f);
    g.setColour(black);
    g.fillEllipse(27.0f, 32.0f, 6.0f, 7.0f);
    g.fillEllipse(37.0f, 32.0f, 6.0f, 7.0f);
    juce::Path nose;
    nose.addTriangle(35.0f, 40.0f, 32.0f, 43.5f, 38.0f, 43.5f);
    g.fillPath(nose);
    g.setColour(black.withAlpha(0.8f));
    g.drawLine(32.0f, 43.5f, 32.0f, 47.0f, 1.0f);
    g.drawLine(37.0f, 43.5f, 37.0f, 47.0f, 1.0f);
}
}

SkellCastPowerButton::SkellCastPowerButton() : juce::Button("Share audio") {
    setClickingTogglesState(true);
    setTooltip("Turn SkellCast audio sharing ON or OFF");
}

void SkellCastPowerButton::paintButton(juce::Graphics& g, bool highlighted, bool pressed) {
    const bool on = getToggleState();
    const auto light = on ? neon : offRed;
    const auto bounds = getLocalBounds().toFloat().reduced(1.0f);

    g.setColour(buttonFace.brighter(highlighted ? 0.06f : 0.0f)
                          .darker(pressed ? 0.08f : 0.0f));
    g.fillRoundedRectangle(bounds, 11.0f);
    g.setColour(on ? neon.withAlpha(0.60f) : border);
    g.drawRoundedRectangle(bounds.reduced(0.5f), 11.0f, 1.2f);

    const auto cx = bounds.getX() + 37.0f;
    const auto cy = bounds.getCentreY();

    // A soft, inexpensive pilot light: red = disabled, green = selected ON.
    g.setGradientFill(juce::ColourGradient(light.withAlpha(on ? 0.25f : 0.17f),
                                           cx, cy, light.withAlpha(0.0f),
                                           cx + 23.0f, cy + 23.0f, true));
    g.fillEllipse(cx - 23.0f, cy - 23.0f, 46.0f, 46.0f);
    g.setColour(light.withAlpha(0.24f));
    g.fillEllipse(cx - 13.0f, cy - 13.0f, 26.0f, 26.0f);
    g.setColour(light);
    g.fillEllipse(cx - 7.0f, cy - 7.0f, 14.0f, 14.0f);

    const auto labelX = static_cast<int>(bounds.getX() + 72.0f);
    const auto labelWidth = static_cast<int>(bounds.getWidth() - 83.0f);

    g.setColour(muted);
    g.setFont(juce::Font(juce::FontOptions(10.5f, juce::Font::bold)));
    g.drawText("AUDIO SHARING", labelX, static_cast<int>(bounds.getY() + 9.0f),
               labelWidth, 18, juce::Justification::centredLeft);

    g.setColour(on ? neon : offRed);
    g.setFont(juce::Font(juce::FontOptions(26.0f, juce::Font::bold)));
    g.drawText(on ? "ON" : "OFF", labelX, static_cast<int>(bounds.getY() + 28.0f),
               labelWidth, 34, juce::Justification::centredLeft);
}

SkellCastDestinationSwitch::SkellCastDestinationSwitch() : juce::Button("Output destination") {
    setClickingTogglesState(true);
    setTooltip("CABLE for Discord microphone input / DESKTOP for Windows playback");
}

void SkellCastDestinationSwitch::paintButton(juce::Graphics& g, bool highlighted, bool pressed) {
    const bool desktop = getToggleState();
    const auto bounds = getLocalBounds().toFloat().reduced(1.0f);
    const auto midpoint = bounds.getCentreX();

    g.setColour(panel.brighter(highlighted ? 0.06f : 0.0f));
    g.fillRoundedRectangle(bounds, 9.0f);
    g.setColour(border);
    g.drawRoundedRectangle(bounds.reduced(0.5f), 9.0f, 1.0f);

    auto active = desktop ? bounds.withLeft(midpoint) : bounds.withRight(midpoint);
    active = active.reduced(2.5f);
    g.setColour(neon.withAlpha(pressed ? 0.10f : 0.16f));
    g.fillRoundedRectangle(active, 7.0f);
    g.setColour(neon.withAlpha(0.62f));
    g.drawRoundedRectangle(active, 7.0f, 1.0f);

    g.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
    const int half = getWidth() / 2;
    g.setColour(desktop ? muted : neon);
    g.drawText("CABLE", 0, 0, half, getHeight(), juce::Justification::centred);
    g.setColour(desktop ? neon : muted);
    g.drawText("DESKTOP", half, 0, getWidth() - half, getHeight(),
               juce::Justification::centred);
}

SkellCastEditor::SkellCastEditor(SkellCastProcessor& p)
    : juce::AudioProcessorEditor(&p), processor_(p) {
    addAndMakeVisible(powerButton_);
    addAndMakeVisible(destinationSwitch_);
    sendAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        p.state, "send", powerButton_);
    destinationAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        p.state, "desktop", destinationSwitch_);
    setResizable(false, false);
    setSize(320, 249);
    startTimerHz(5); // 5 Hz UI status, not an audio-thread operation.
}

void SkellCastEditor::timerCallback() {
    repaint(20, 215, getWidth() - 40, 26);
}

void SkellCastEditor::resized() {
    powerButton_.setBounds(20, 81, getWidth() - 40, 72);
    destinationSwitch_.setBounds(20, 176, getWidth() - 40, 34);
}

void SkellCastEditor::paint(juce::Graphics& g) {
    g.fillAll(black);

    auto frame = getLocalBounds().toFloat().reduced(7.0f);
    g.setColour(panel);
    g.fillRoundedRectangle(frame, 12.0f);
    g.setColour(border);
    g.drawRoundedRectangle(frame, 12.0f, 1.0f);

    // One restrained SKELLY signature. No distracting decoration or meters.
    drawSkellyMark(g);
    g.setColour(white);
    g.setFont(juce::Font(juce::FontOptions(22.0f, juce::Font::bold)));
    g.drawText("SKELLCAST", 55, 18, getWidth() - 76, 33,
               juce::Justification::centredLeft);

    g.setColour(neon.withAlpha(0.86f));
    g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
    g.drawText("STREAM  /  SHARE", 56, 49, getWidth() - 80, 18,
               juce::Justification::centredLeft);

    g.setColour(muted);
    g.setFont(juce::Font(juce::FontOptions(10.5f, juce::Font::bold)));
    g.drawText("OUTPUT", 22, 159, getWidth() - 44, 14,
               juce::Justification::centredLeft);

    // Show the actual Windows endpoint picked by the routing worker.
    const auto status = processor_.outputDeviceStatus();
    const bool enabled = powerButton_.getToggleState();
    juce::String statusLine = "AUTO DETECT";
    juce::Colour statusColour = muted;
    if (enabled) {
        switch (status.state) {
            case DirectCableOutput::DeviceState::Connected:
                statusLine = "AUTO: " + juce::String(status.name.c_str());
                statusColour = neon;
                break;
            case DirectCableOutput::DeviceState::NotFound:
                statusLine = juce::String(status.name.c_str());
                statusColour = offRed;
                break;
            case DirectCableOutput::DeviceState::Error:
                statusLine = juce::String(status.name.c_str());
                statusColour = offRed;
                break;
            case DirectCableOutput::DeviceState::Searching:
            case DirectCableOutput::DeviceState::Off:
                statusLine = "AUTO: DETECTING";
                break;
        }
    }
    g.setColour(statusColour);
    g.fillEllipse(23.0f, 223.0f, 6.0f, 6.0f);
    g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
    g.drawFittedText(statusLine, 34, 218, getWidth() - 56, 17,
                     juce::Justification::centredLeft, 1, 0.7f);
}
