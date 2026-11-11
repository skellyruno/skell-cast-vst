#include "PluginEditor.h"

namespace {
// Final Skelcast skin: midnight black, white labels, signature neon green.
const juce::Colour background {0xff050607};
const juce::Colour panel      {0xff090c0a};
const juce::Colour raised     {0xff101612};
const juce::Colour line       {0xff223127};
const juce::Colour bone       {0xfff6fff8}; // white, not ivory
const juce::Colour boneDim    {0xffc1cbc3}; // white at lower emphasis
const juce::Colour accent     {0xff39ff14}; // Skell Key signature neon
const juce::Colour onInk      {0xff050807};
const juce::Colour offRed     {0xffff5c61};

juce::Font labelFont(float size) {
    return juce::Font(juce::FontOptions(size, juce::Font::bold));
}

// Skull silhouette taken from the skull portion of Skell Key's mark.
// No key shaft, teeth or extra header decoration. Vector drawn at any DPI.
void drawSkullMark(juce::Graphics& g, float x, float y) {
    const juce::Graphics::ScopedSaveState saved(g);
    g.addTransform(juce::AffineTransform::scale(0.92f, 0.92f).translated(x, y));

    g.setColour(bone);
    g.fillEllipse(1.0f, 0.0f, 25.0f, 23.0f);
    g.fillRoundedRectangle(6.5f, 17.0f, 14.0f, 11.0f, 3.0f);

    g.setColour(background);
    g.fillEllipse(6.2f, 8.4f, 6.4f, 6.8f);
    g.fillEllipse(14.6f, 8.4f, 6.4f, 6.8f);
    juce::Path nose;
    nose.addTriangle(13.5f, 15.0f, 11.6f, 19.0f, 15.4f, 19.0f);
    g.fillPath(nose);
    for (int i = 0; i < 3; ++i)
        g.fillRect(9.6f + 3.4f * static_cast<float>(i), 22.0f, 1.0f, 5.0f);
}

// Flat square cards and short corner details echo the Skell Key UI.
void drawBonePanel(juce::Graphics& g, juce::Rectangle<float> r) {
    g.setColour(panel);
    g.fillRect(r);
    g.setColour(line);
    g.drawRect(r, 1.0f);
    const float arm = 6.0f;
    juce::Path corner;
    corner.startNewSubPath(r.getX(), r.getY() + arm);
    corner.lineTo(r.getX(), r.getY());
    corner.lineTo(r.getX() + arm, r.getY());
    corner.startNewSubPath(r.getRight() - arm, r.getY());
    corner.lineTo(r.getRight(), r.getY());
    corner.lineTo(r.getRight(), r.getY() + arm);
    corner.startNewSubPath(r.getX(), r.getBottom() - arm);
    corner.lineTo(r.getX(), r.getBottom());
    corner.lineTo(r.getX() + arm, r.getBottom());
    corner.startNewSubPath(r.getRight() - arm, r.getBottom());
    corner.lineTo(r.getRight(), r.getBottom());
    corner.lineTo(r.getRight(), r.getBottom() - arm);
    g.setColour(boneDim.withAlpha(0.70f));
    g.strokePath(corner, juce::PathStrokeType(1.1f));
}
} // namespace

SkellCastPowerButton::SkellCastPowerButton() : juce::Button("Share audio") {
    setClickingTogglesState(true);
    setTooltip("Send FL Studio audio to the selected Windows output (ON/OFF)");
}

void SkellCastPowerButton::paintButton(juce::Graphics& g, bool highlighted, bool pressed) {
    const bool active = getToggleState();
    const auto light = active ? accent : offRed;
    const auto bounds = getLocalBounds().toFloat().reduced(0.5f);
    g.setColour(pressed ? raised.darker(0.14f) : (highlighted ? raised.brighter(0.10f) : raised));
    g.fillRect(bounds);
    g.setColour(active ? accent.withAlpha(0.68f) : line);
    g.drawRect(bounds, 1.0f);
    if (active) {
        // A single quiet accent line, not a bright illuminated frame.
        g.setColour(accent);
        g.fillRect(bounds.getX(), bounds.getY(), 2.0f, bounds.getHeight());
        g.setColour(accent.withAlpha(0.13f));
        g.drawRect(bounds.expanded(1.0f), 1.0f);
    }

    const float cx = bounds.getX() + 28.0f;
    const float cy = bounds.getCentreY();
    g.setGradientFill(juce::ColourGradient(
        light.withAlpha(active ? 0.24f : 0.13f), cx, cy,
        light.withAlpha(0.0f), cx + 18.0f, cy + 18.0f, true));
    g.fillEllipse(cx - 18.0f, cy - 18.0f, 36.0f, 36.0f);
    g.setColour(light);
    g.fillEllipse(cx - 7.0f, cy - 7.0f, 14.0f, 14.0f);

    g.setColour(boneDim);
    g.setFont(labelFont(9.5f));
    g.drawText("AUDIO SHARING", 58, 7, getWidth() - 68, 14,
               juce::Justification::centredLeft);
    g.setColour(active ? accent : bone);
    g.setFont(labelFont(24.0f));
    g.drawText(active ? "ON" : "OFF", 58, 22, getWidth() - 68, 32,
               juce::Justification::centredLeft);
}

SkellCastDestinationSwitch::SkellCastDestinationSwitch() : juce::Button("Output destination") {
    setClickingTogglesState(true);
    setTooltip("CABLE: VB-CABLE for Discord; SPARE: auto-detected separate Windows output for OBS");
}

void SkellCastDestinationSwitch::paintButton(juce::Graphics& g, bool highlighted, bool pressed) {
    const auto full = getLocalBounds().toFloat().reduced(0.5f);
    const bool spare = getToggleState();
    g.setColour(panel);
    g.fillRect(full);
    g.setColour(line);
    g.drawRect(full, 1.0f);
    const auto middle = full.getCentreX();
    auto choice = spare ? full.withLeft(middle) : full.withRight(middle);
    choice = choice.reduced(2.0f);
    g.setColour(accent.withAlpha(pressed ? 0.85f : (highlighted ? 0.78f : 0.67f)));
    g.fillRect(choice);

    const int half = getWidth() / 2;
    g.setFont(labelFont(11.0f));
    g.setColour(spare ? boneDim : onInk);
    g.drawText("CABLE", 0, 0, half, getHeight(), juce::Justification::centred);
    g.setColour(spare ? onInk : boneDim);
    g.drawText("SPARE", half, 0, getWidth() - half, getHeight(),
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
    setSize(272, 212); // Keep the tested compact dimensions.
    startTimerHz(5);
}

void SkellCastEditor::timerCallback() {
    repaint(12, 176, getWidth() - 24, 26);
}

void SkellCastEditor::resized() {
    powerButton_.setBounds(14, 65, getWidth() - 28, 61);
    destinationSwitch_.setBounds(14, 153, getWidth() - 28, 27);
}

void SkellCastEditor::paint(juce::Graphics& g) {
    g.fillAll(background);
    drawBonePanel(g, getLocalBounds().toFloat().reduced(6.0f));

    drawSkullMark(g, 16.0f, 16.0f);

    // Proportional glyph measurement makes Skelcast ONE tight word,
    // not two fixed text boxes separated by a large artificial gap.
    const auto font = labelFont(21.0f);
    juce::GlyphArrangement glyphs;
    glyphs.addLineOfText(font, "Skel", 0.0f, 0.0f);
    const float firstWidth = glyphs.getBoundingBox(0, -1, true).getWidth();
    constexpr float titleX = 48.0f, titleY = 15.0f, titleH = 29.0f;
    const float castX = titleX + firstWidth + 0.5f;
    g.setFont(font);
    g.setColour(bone);
    g.drawText("Skel", juce::Rectangle<float>(titleX, titleY, firstWidth + 2.0f, titleH),
               juce::Justification::centredLeft, false);
    auto castArea = juce::Rectangle<float>(castX, titleY, 70.0f, titleH);
    g.setColour(accent.withAlpha(0.14f));
    g.drawText("cast", castArea.expanded(0.7f, 0.0f),
               juce::Justification::centredLeft, false);
    g.setColour(accent);
    g.drawText("cast", castArea, juce::Justification::centredLeft, false);

    g.setFont(labelFont(9.0f));
    g.setColour(boneDim);
    g.drawText("STREAM / SHARE", 48, 43, getWidth() - 64, 13,
               juce::Justification::centredLeft);

    g.setColour(boneDim);
    g.setFont(labelFont(9.0f));
    g.drawText("OUTPUT", 15, 134, getWidth() - 30, 15, juce::Justification::centredLeft);

    const auto status = processor_.outputDeviceStatus();
    juce::String label("AUTO DETECT");
    juce::Colour colour = boneDim;
    if (powerButton_.getToggleState()) {
        switch (status.state) {
            case DirectCableOutput::DeviceState::Connected:
                label = "AUTO: " + juce::String(status.name.c_str());
                colour = accent;
                break;
            case DirectCableOutput::DeviceState::NotFound:
            case DirectCableOutput::DeviceState::Error:
                label = juce::String(status.name.c_str());
                colour = offRed;
                break;
            case DirectCableOutput::DeviceState::Off:
            case DirectCableOutput::DeviceState::Searching:
                label = "AUTO: DETECTING";
                break;
        }
    }
    g.setColour(colour);
    g.fillEllipse(16.0f, 191.0f, 5.0f, 5.0f);
    g.setFont(labelFont(9.0f));
    g.drawFittedText(label, 28, 185, getWidth() - 43, 17,
                     juce::Justification::centredLeft, 1, 0.65f);
}
