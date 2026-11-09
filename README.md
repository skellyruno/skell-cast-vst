# SkellCast v0.4 — VST3 with CABLE / DESKTOP destination switch

SkellCast is a single Windows VST3 plugin for FL Studio. **No receiver app, extra executable or cloud service.** It copies the signal from the mixer into a nonblocking queue; a dedicated Windows WASAPI worker thread outputs that copy to the selected destination. The original audio reaching the DAW master output remains unchanged.

## Plugin controls

- **AUDIO SHARING ON/OFF** — big button; red means OFF and green means ON.
- **SEND TO: CABLE / DESKTOP** — a small two-way switch beneath the button. CABLE is the default, including for existing project presets.
- Switching the destination while ON closes the previous output and opens the selected device. Switch position and ON/OFF are saved with the FL Studio project.
- Green means the ON button is enabled, **not** a verified connection. If the selected device is unavailable, no fallback to speakers or another destination takes place.

### CABLE — Discord microphone / OBS audio input

1. Install [VB-CABLE](https://vb-audio.com/Cable/) (Windows 10/11 x64). Restart if necessary.
2. In FL Studio, insert SkellCast **last** on the Master mixer.
3. Choose **CABLE**, press **ON**.
4. Discord **User Settings > Voice & Video > Input Device**: select **CABLE Output (VB-Audio Virtual Cable)**. Keep Discord output on your normal headphones.
5. For OBS, add **Audio Input Capture** and select **CABLE Output** instead of capturing the same sound twice.

SkellCast looks for the Windows *playback* endpoint called **CABLE Input (VB-Audio Virtual Cable)**, which becomes the virtual recording source **CABLE Output** for Discord. Renamed/nonstandard virtual cables are not automatically detected. If VB-CABLE is unavailable, CABLE mode stays silent; it never redirects to your desktop speakers.

### DESKTOP — Windows system playback / OBS Desktop Audio

1. In FL Studio, insert SkellCast last on the Master mixer.
2. Choose **DESKTOP**, press **ON**.
3. SkellCast sends an extra copy of the FL signal to Windows' **default playback device** (the ordinary default speakers/headphones selected in Windows Sound settings, not the default communications device). OBS can capture it using **Desktop Audio / Audio Output Capture** for that exact output device.

**IMPORTANT: Avoid duplicate playback.** If FL Studio already sends its main audio to the **same** Windows default speakers/headphones, DESKTOP mode will play a **second copy**, potentially causing echo, phasing and a louder signal. In that case leave SkellCast OFF for desktop capture, or route FL Studio's primary output to a *different* audio interface/device before using DESKTOP. SkellCast does **not** silence or redirect FL Studio's original output. Do not feed captured desktop audio back into FL Studio; feedback can be loud.

DESKTOP is **not** a Discord microphone input and does not directly control Discord screen share. Discord's normal voice chat still needs CABLE mode or another audio-input setup. Screen sharing desktop audio depends on Discord's screen-share capture settings. This switch only selects where Windows plays the additional audio copy.

## Build Windows VST3

Requires Windows x64, Visual Studio 2022/2026 C++ Desktop workload, CMake 3.25+, Git, and network access for JUCE 8.0.17-security unless you specify a local JUCE folder.

```powershell
./scripts/Build-Windows.ps1
```

Or use GitHub **Actions > Windows VST3 Only** and download the **SkellCast-VST3-Windows-x64** artifact. Install the whole `SkellCast.vst3` bundle into `C:\Program Files\Common Files\VST3\` and rescan plugins in FL Studio. Both C and C++ are enabled in CMake (`LANGUAGES C CXX`).

### Internals and limitations

- The processor's audio callback never waits for Windows audio device access, allocates audio buffers, or selects an endpoint. Output uses a lock-free stereo SPSC FIFO.
- Output switching occurs entirely on the WASAPI worker. The old endpoint stops before the replacement is started; queued old audio is discarded on the transition.
- CABLE matches the first active VB-CABLE render endpoint; DESKTOP uses the Windows default eConsole playback endpoint, checks for default-device changes, and reconnects.
- A 30 ms WASAPI shared-mode buffer and ~100 ms FIFO backlog limit balance stream continuity and latency; some dropout or buffering is possible during device switches.
- Stereo audio is clipped to [-1,1] for the output copy and Windows shared-mode audio handles sample-rate conversion if supported by the destination.
- Avoid multiple SkellCast instances on the same master output; they would mix multiple copies into the same endpoint.
- Test Windows routing and host playback with `TESTING.md`. VST3 build success and core tests alone do not prove correct Discord/OBS routing.
