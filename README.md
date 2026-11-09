# SkellCast v0.3 — **one-button VST3 only**

This replaces the old SkellCast sender/receiver setup. There is **NO receiver application** and **NO background executable**. The plugin itself writes FL Studio's stereo mix directly to the Windows **VB-CABLE** playback endpoint using shared-mode WASAPI. No UDP networking or cloud transport is involved.

**Requirements:** Windows 10/11 x64, a DAW supporting VST3 (FL Studio), and a third-party virtual audio cable **VB-CABLE** installed from [VB-Audio](https://vb-audio.com/Cable/). Windows must see the active render/playback device named **CABLE Input (VB-Audio Virtual Cable)**. This project does not install a virtual microphone driver. It also does not include any third-party drivers.

## How to use with Discord

1. Install VB-CABLE (restart Windows if prompted) from [VB-Audio](https://vb-audio.com/Cable/).
2. Build the plugin and install the entire `SkellCast.vst3` directory into `C:\Program Files\Common Files\VST3\`. Rescan plugins in FL Studio.
3. Put SkellCast as the **last effect on the FL Studio Master mixer**. Keep FL Studio's usual speakers/headphones or audio-interface output unchanged.
4. Open SkellCast. There is exactly **one ON/OFF button**: **green = sharing enabled; red = sharing disabled**. The plugin automatically finds and sends to the active VB-CABLE playback endpoint. There are no audio sliders, meters, or device selectors.
5. In Discord → Voice & Video, set the **input device** to **CABLE Output (VB-Audio Virtual Cable)**. Keep its output device on your headphones. Discord noise suppression, echo cancellation, and automatic gain can degrade music, so turn them off when appropriate.

For OBS Studio, add an **Audio Input Capture** source selecting **CABLE Output**. Do not duplicate the same audio via desktop capture unless you want doubled playback.

**If VB-CABLE is missing or not detected, no audio will be sent anywhere** (the plugin will NOT fall back to speakers). The green light reflects the user's ON selection, **not confirmed device connection**. The plugin retries discovery automatically. For Discord to hear your microphone as well, you need a separate microphone mixer such as Voicemeeter or route your mic into FL Studio deliberately (be careful about feedback).

## Build

Requires Windows x64, Visual Studio 2022/2026 with C++ desktop workload, CMake 3.25+, Git and an internet connection to fetch JUCE 8.0.17-security (or set `-DJUCE_DIR=...` to a local JUCE checkout). The specific previous error in JUCE's CMake configuration is fixed with `LANGUAGES C CXX`.

From PowerShell in the extracted `skellcast-source` directory:

```powershell
./scripts/Build-Windows.ps1
```

Or push the source to GitHub and use **Actions → Windows VST3 Only → Run workflow**, and download the `SkellCast-VST3-Windows-x64` artifact. There will be **no `SkellCastReceiver.exe`**. If you have a stale build tree configured with another Visual Studio generator, delete **only** the `build` directory then rebuild. Bundled JUCE source may require license compliance for distribution.

## Design and limitations

- **No change to FL Studio's master audio**: passthrough in `processBlock()`, with an independent copy to a nonblocking SPSC queue. The Windows WASAPI render client runs on its **own plugin-owned worker thread**. It uses Windows shared-mode audio conversion as necessary when FL sample rate differs from VB-CABLE's device mix format.
- Only transmits when the saved ON/OFF parameter is ON. Turning OFF stops writing and closes the output stream; Windows may have ~30 ms of already-buffered audio. Start state defaults to OFF and is saved with the project.
- Only auto-connects to the first active Windows render device matching a **VB-Audio CABLE Input** name. Renamed or nonstandard cables will not be detected without modifying source.
- Queued PCM floats are clipped to [-1, 1] for safe device output. FIFO is fixed-size (16384 frames), and backlog beyond about 100 ms is reduced to avoid progressively increasing delay. Underflow outputs silence.
- Best for **one SkellCast instance** on the Master mixer; multiple instances output into the same virtual device and may combine duplicate signals. On stop/start or device reconnect, short glitches are possible. Loopback routes that feed this signal back into FL Studio can create feedback.
- This source ZIP contains C++ **source**, not a precompiled Windows VST3. **Windows compilation and real-world FL Studio/Discord validation have not been run here.** Run the checklist in `TESTING.md`.

## Why VB-CABLE is still necessary

Discord receives signals from operating-system recording devices. VST3 plugins aren't recording devices and can't add a microphone input by themselves. Integrating a custom signed Windows audio driver is a separate, significantly larger project. This revision removes only the separate *SkellCast receiver application*, not the virtual audio device required to connect into Discord.
