#pragma once
#include <JuceHeader.h>
#include <atomic>

// Process-wide settings shared by SkellCast plugin instances. The preferences
// survive FL Studio restarts in the current user's Windows profile. All disk
// operations are on the message thread, never on the audio callback.
class SkellCastGlobalSettings final {
public:
    static SkellCastGlobalSettings& instance() {
        static SkellCastGlobalSettings settings;
        return settings;
    }

    bool sendEnabled() const noexcept { return send_.load(std::memory_order_acquire); }
    bool useSpareOutput() const noexcept { return spare_.load(std::memory_order_acquire); }

    void changeSend(bool value) noexcept {
        if (send_.exchange(value, std::memory_order_acq_rel) != value)
            dirty_.store(true, std::memory_order_release);
    }
    void changeSpare(bool value) noexcept {
        if (spare_.exchange(value, std::memory_order_acq_rel) != value)
            dirty_.store(true, std::memory_order_release);
    }

    // Call on the JUCE message thread only; uses a single user preferences file.
    void saveIfDirty() {
        if (!dirty_.exchange(false, std::memory_order_acq_rel))
            return;

        juce::PropertiesFile settings { makeOptions() };
        settings.setValue("send", sendEnabled());
        settings.setValue("spare", useSpareOutput());
        if (!settings.saveIfNeeded())
            dirty_.store(true, std::memory_order_release);
    }

private:
    static juce::PropertiesFile::Options makeOptions() {
        juce::PropertiesFile::Options options;
        options.applicationName = "SkellCast";
        options.folderName = "Skelly";
        options.filenameSuffix = ".settings";
        options.storageFormat = juce::PropertiesFile::storeAsXML;
        options.commonToAllUsers = false;
        return options;
    }

    SkellCastGlobalSettings() {
        juce::PropertiesFile settings { makeOptions() };
        send_.store(settings.getBoolValue("send", false), std::memory_order_relaxed);
        spare_.store(settings.getBoolValue("spare", false), std::memory_order_relaxed);
    }
    ~SkellCastGlobalSettings() = default;

    std::atomic<bool> send_ {false};
    std::atomic<bool> spare_ {false};
    std::atomic<bool> dirty_ {false};
};
