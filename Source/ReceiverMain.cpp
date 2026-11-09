#include <JuceHeader.h>
#include "BridgeProtocol.h"
#include <algorithm>
#include <cmath>

namespace {
const juce::Colour background {0xff101310};
const juce::Colour panel {0xff1d251e};
const juce::Colour accent {0xffb5ff43};
const juce::Colour subdued {0xff9eb09a};

class BridgeReceiver final : public juce::AudioIODeviceCallback, private juce::Thread {
public:
    BridgeReceiver() : juce::Thread("SkellCast UDP localhost receiver") { startThread(); }
    ~BridgeReceiver() override { signalThreadShouldExit(); stopThread(1500); }

    void setEnabled(bool b) noexcept { enabled_.store(b, std::memory_order_release); }
    bool isBound() const noexcept { return bound_.load(std::memory_order_relaxed); }
    bool hasFreshAudio() const noexcept {
        return rxCount_.load(std::memory_order_relaxed) != 0 &&
               (juce::Time::getMillisecondCounter() - lastPacketMs_.load(std::memory_order_relaxed)) < 1200;
    }
    std::uint32_t rate() const noexcept { return senderRate_.load(std::memory_order_relaxed); }
    std::uint64_t packetCount() const noexcept { return rxCount_.load(std::memory_order_relaxed); }
    std::uint64_t missedPackets() const noexcept { return missed_.load(std::memory_order_relaxed); }
    std::uint64_t overflows() const noexcept { return overflow_.load(std::memory_order_relaxed); }

    void audioDeviceAboutToStart(juce::AudioIODevice* d) override {
        outputRate_.store(d ? d->getCurrentSampleRate() : 48000.0, std::memory_order_release);
        primed_ = false;
        phase_ = 0.0;
    }
    void audioDeviceStopped() override { primed_ = false; }
    void audioDeviceIOCallbackWithContext(const float* const*, int, float* const* outputs,
                                          int outputChannels, int frames,
                                          const juce::AudioIODeviceCallbackContext&) override {
        for (int channel = 0; channel < outputChannels; ++channel)
            if (outputs[channel]) juce::FloatVectorOperations::clear(outputs[channel], frames);

        if (!enabled_.load(std::memory_order_acquire)) {
            skellcast::StereoFrame discard;
            while (frames_.pop(discard)) {} // reset queued latency when bridge is off
            primed_ = false;
            phase_ = 0.0;
            return;
        }
        const auto sourceRate = static_cast<double>(senderRate_.load(std::memory_order_acquire));
        const auto outputRate = outputRate_.load(std::memory_order_acquire);
        if (sourceRate < 8000.0 || outputRate < 8000.0) return;
        const auto queued = frames_.available();
        const auto target = static_cast<std::size_t>(sourceRate * 0.06); // ~60 ms prefill
        if (!primed_) {
            if (queued < target || !frames_.pop(prev_) || !frames_.pop(next_)) return;
            primed_ = true;
            phase_ = 0.0;
        }
        // Small drift compensation keeps independent DAW and WASAPI clocks aligned.
        const auto drift = std::clamp((static_cast<double>(queued) - static_cast<double>(target)) /
                                         std::max(1.0, static_cast<double>(target)) * 0.003,
                                      -0.003, 0.003);
        const auto step = (sourceRate / outputRate) * (1.0 + drift);
        for (int i = 0; i < frames; ++i) {
            if (!primed_) break;
            const float l = static_cast<float>(prev_.left + (next_.left - prev_.left) * phase_);
            const float r = static_cast<float>(prev_.right + (next_.right - prev_.right) * phase_);
            if (outputChannels > 0 && outputs[0]) outputs[0][i] = std::clamp(l, -1.0f, 1.0f);
            if (outputChannels > 1 && outputs[1]) outputs[1][i] = std::clamp(r, -1.0f, 1.0f);
            phase_ += step;
            while (phase_ >= 1.0 && primed_) {
                phase_ -= 1.0;
                prev_ = next_;
                if (!frames_.pop(next_)) {
                    primed_ = false;
                    phase_ = 0.0;
                }
            }
        }
    }
private:
    void run() override {
        juce::DatagramSocket socket(false);
        if (!socket.bindToPort(skellcast::kPort, "127.0.0.1")) return;
        bound_.store(true, std::memory_order_release);
        std::uint32_t lastSeq = 0;
        bool haveSeq = false;
        while (!threadShouldExit()) {
            if (socket.waitUntilReady(true, 100) <= 0) continue;
            skellcast::Packet packet;
            const int received = socket.read(&packet, static_cast<int>(sizeof(packet)), false);
            if (received <= 0 || !skellcast::validPacket(packet, static_cast<std::size_t>(received))) continue;
            if (haveSeq && packet.sequence != lastSeq + 1) missed_.fetch_add(1, std::memory_order_relaxed);
            haveSeq = true;
            lastSeq = packet.sequence;
            senderRate_.store(packet.sampleRate, std::memory_order_release);
            for (unsigned i = 0; i < packet.frames; ++i) {
                const skellcast::StereoFrame f {skellcast::saneSample(packet.audio[i].left),
                                                skellcast::saneSample(packet.audio[i].right)};
                if (!frames_.push(f)) overflow_.fetch_add(1, std::memory_order_relaxed);
            }
            rxCount_.fetch_add(1, std::memory_order_relaxed);
            lastPacketMs_.store(juce::Time::getMillisecondCounter(), std::memory_order_relaxed);
        }
        bound_.store(false, std::memory_order_release);
    }
    skellcast::FrameQueue frames_;
    std::atomic<bool> enabled_ {false}, bound_ {false};
    std::atomic<double> outputRate_ {48000.0};
    std::atomic<std::uint32_t> senderRate_ {48000}, lastPacketMs_ {0};
    std::atomic<std::uint64_t> rxCount_ {0}, missed_ {0}, overflow_ {0};
    // Audio thread exclusively owns interpolation state.
    bool primed_ = false;
    double phase_ = 0.0;
    skellcast::StereoFrame prev_, next_;
};

class ReceiverContent final : public juce::Component, private juce::Timer {
public:
    ReceiverContent() {
        auto file = settingsFile();
        auto previousSettings = juce::XmlDocument::parse(file);
        devices_.initialise(0, 2, previousSettings.get(), true);
        devices_.addAudioCallback(&receiver_);

        selector_ = std::make_unique<juce::AudioDeviceSelectorComponent>(
            devices_, 0, 0, 1, 2, false, false, true, false);
        addAndMakeVisible(*selector_);

        start_.setButtonText("START ROUTING  /  OFF");
        start_.setClickingTogglesState(true);
        start_.setColour(juce::TextButton::buttonColourId, panel);
        start_.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff375824));
        start_.setColour(juce::TextButton::textColourOffId, accent);
        start_.setColour(juce::TextButton::textColourOnId, accent);
        start_.onClick = [this] {
            receiver_.setEnabled(start_.getToggleState());
            start_.setButtonText(start_.getToggleState() ? "ROUTING LIVE  /  ON" : "START ROUTING  /  OFF");
        };
        addAndMakeVisible(start_);

        status_.setColour(juce::Label::textColourId, accent);
        status_.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(status_);
        details_.setColour(juce::Label::textColourId, subdued);
        details_.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(details_);
        warning_.setText("Receiver output: CABLE Input  •  Discord / OBS input: CABLE Output",
                         juce::dontSendNotification);
        warning_.setColour(juce::Label::textColourId, subdued);
        warning_.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(warning_);
        startTimerHz(2);
        setSize(710, 650);
    }
    ~ReceiverContent() override {
        stopTimer();
        receiver_.setEnabled(false);
        devices_.removeAudioCallback(&receiver_);
        if (auto settings = devices_.createStateXml()) {
            auto file = settingsFile();
            file.getParentDirectory().createDirectory();
            file.replaceWithText(settings->toString());
        }
        devices_.closeAudioDevice();
    }
    void resized() override {
        const int w = getWidth();
        start_.setBounds(30, 109, w - 60, 45);
        status_.setBounds(31, 162, w - 62, 28);
        details_.setBounds(31, 193, w - 62, 28);
        selector_->setBounds(26, 243, w - 52, getHeight() - 328);
        warning_.setBounds(31, getHeight() - 78, w - 62, 48);
    }
    void paint(juce::Graphics& g) override {
        g.fillAll(background);
        g.setColour(accent);
        g.setFont(juce::Font(juce::FontOptions(31.0f, juce::Font::bold)));
        g.drawText("SKELL / CAST", 29, 19, getWidth() - 60, 48, juce::Justification::centredLeft);
        g.setColour(subdued);
        g.setFont(juce::Font(juce::FontOptions(13.0f)));
        g.drawText("RECEIVER  •  CHOOSE A WINDOWS AUDIO OUTPUT DEVICE BELOW", 31, 67,
                   getWidth() - 60, 20, juce::Justification::centredLeft);
        g.setColour(accent.withAlpha(.28f));
        g.drawLine(30, 97, getWidth() - 30.0f, 97, 1);
    }
private:
    static juce::File settingsFile() {
        return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
            .getChildFile("SkellCast").getChildFile("receiver-audio.xml");
    }
    void timerCallback() override {
        const auto device = devices_.getCurrentAudioDevice();
        const bool audio = receiver_.hasFreshAudio();
        status_.setText(!receiver_.isBound() ? "PORT 44991 UNAVAILABLE — ANOTHER RECEIVER MAY BE OPEN" :
                        audio ? "INCOMING AUDIO DETECTED  •  LOCALHOST CONNECTED" :
                                "WAITING FOR SKELLCAST VST3 IN FL STUDIO", juce::dontSendNotification);
        details_.setText("SOURCE " + juce::String(receiver_.rate()) + " Hz   |   PACKETS " +
                         juce::String(static_cast<juce::int64>(receiver_.packetCount())) +
                         "   |   GAPS " + juce::String(static_cast<juce::int64>(receiver_.missedPackets())) +
                         "   |   OUTPUT " + (device ? device->getName() : juce::String("none")),
                         juce::dontSendNotification);
    }
    BridgeReceiver receiver_;
    juce::AudioDeviceManager devices_;
    std::unique_ptr<juce::AudioDeviceSelectorComponent> selector_;
    juce::TextButton start_;
    juce::Label status_, details_, warning_;
};

class ReceiverApp final : public juce::JUCEApplication {
public:
    const juce::String getApplicationName() override { return "SkellCast Receiver"; }
    const juce::String getApplicationVersion() override { return "0.1.0"; }
    bool moreThanOneInstanceAllowed() override { return false; }
    void initialise(const juce::String&) override {
        window_ = std::make_unique<MainWindow>();
    }
    void shutdown() override { window_.reset(); }
    void systemRequestedQuit() override { quit(); }
private:
    class MainWindow final : public juce::DocumentWindow {
    public:
        MainWindow()
            : juce::DocumentWindow("SkellCast Receiver", background, juce::DocumentWindow::allButtons) {
            setUsingNativeTitleBar(true);
            setContentOwned(new ReceiverContent(), true);
            setResizable(true, false);
            setResizeLimits(650, 580, 1150, 950);
            centreWithSize(710, 650);
            setVisible(true);
        }
        void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
    };
    std::unique_ptr<MainWindow> window_;
};
} // namespace
START_JUCE_APPLICATION(ReceiverApp)
