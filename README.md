# SkellCast v0.5 — automatic safe desktop-device selection

Windows x64 VST3 for FL Studio. No receiver executable and no background companion app.

## Controls

- **AUDIO SHARING**: single large ON/OFF control; green means enabled, red means disabled.
- **OUTPUT CABLE / DESKTOP**: two small destinations; CABLE is the default, including existing sessions and project presets.
- **AUTO status**: small line under the switch displays the actual Windows device receiving audio, or a missing-device warning. When ON, green status indicates the Windows audio stream opened; a red status means routing is unavailable.

## CABLE — microphone input for Discord

1. Install [VB-CABLE](https://vb-audio.com/Cable/) on Windows.
2. Insert SkellCast as the **last effect** on the FL Studio Master mixer.
3. Select CABLE, press ON. SkellCast detects the *playback* endpoint named CABLE Input (VB-Audio Virtual Cable).
4. In Discord > Voice & Video, select **CABLE Output** as the input device. For OBS, add Audio Input Capture using CABLE Output.

CABLE mode **never falls back to speakers** if VB-CABLE is missing.

## DESKTOP — spare Windows output for OBS, without doubling the default

Desktop mode now searches Windows playback devices for a **separate non-default, currently idle** output. It excludes all Windows default speaker/headphone output roles, and excludes endpoints with active Windows audio sessions. If multiple candidates exist, it prioritizes a virtual playback device. The selected output's Windows friendly name is shown under the switch.

**Critical:** DESKTOP no longer writes to Windows' default speakers/headphones. If your computer has only one playback device, you'll see **No spare output device**; it will send **silence** instead of doubling your audio. This is intentional.

In OBS, add **Audio Output Capture** for the **exact device named in SkellCast's AUTO status**; OBS Desktop Audio set to the normal/default speakers will not automatically capture audio going to a different device.

Unlike an actual virtual microphone driver, Desktop mode is not a Discord input. Use CABLE mode for Discord microphone sharing.

### Important limitations

Windows' shared-mode audio-session enumeration can help avoid active playback endpoints, but it cannot reliably identify FL Studio's ASIO hardware output, direct/exclusive playback, a physical device with multiple Windows endpoints, or other sessions that begin after selection. It is impossible for a regular VST3 to guarantee that a spare endpoint is truly unused in every configuration. The new policy substantially reduces double playback by excluding Windows' default endpoints, but **do not assume it can guarantee zero duplicate sound across ASIO setups**.

If an endpoint becomes a Windows default while Desktop mode is ON, SkellCast stops sending to it and searches for another spare endpoint. Windows default playback devices always remain excluded. Devices can reconnect on the worker thread. SkellCast does not mute or change FL Studio's original sound.

## Build and install

Requires Windows 10/11 x64, Visual Studio 2022/2026 C++ Desktop workload, CMake 3.25+, Git and the JUCE dependency. Build with:

```powershell
./scripts/Build-Windows.ps1
```

Or go to **GitHub Actions > Windows VST3 Only** to download the **SkellCast-VST3-Windows-x64** build artifact after the build succeeds. Install the complete `SkellCast.vst3` bundle under `C:\Program Files\Common Files\VST3\`, then rescan in FL Studio.

### Engineering notes

- One VST3 target, no receiver.
- Host audio passes through unchanged; a nonblocking stereo FIFO copies to a dedicated WASAPI thread.
- Device detection, session checks, stream setup, and reconnection never run in FL Studio's audio callback.
- Selection is fail-closed when the Windows default device roles or session activity cannot be checked.
- Automatic preference is virtual spare endpoint over physical spare endpoint. In-use Windows audio sessions disqualify endpoints; idle here means the API reported no active shared-mode session when scanned.
- The ON/OFF and output choice are saved in the project. The AUTO status is live and is not a third control.


## v0.6 — Skell Key Bone styling and global user preferences

- UI now uses Skell Key's **Bone** palette: near-black, ivory lettering, bone-style square cards, and green accents. `SKELL` is ivory and `CAST` is green.
- Compact 272 x 212 window (15% smaller in both directions from the previous 320 x 249 window).
- Output switch reads **CABLE / SPARE**. `SPARE` is the previous `DESKTOP` routing mode — it selects a separate idle, non-default Windows playback device, not the default speakers. The parameter ID stays `desktop` for existing FL Studio automation compatibility.
- Both the sharing toggle and output destination persist **globally per Windows user**, including across independent FL Studio projects and restarts. The last saved setting wins over historical per-project snapshots when they are loaded.
- Preferences use JUCE's per-user `PropertiesFile` under the Skelly / SkellCast app configuration location, not a companion application.
- Open SkellCast instances in the same FL Studio process synchronise their settings when the host's message loop is running (4 updates/sec). Changes made while the editor is closed are also captured through the parameter listener.
- **Important:** with global ON, adding SkellCast to a new Master mixer can begin sharing as soon as the host runs audio. Turn OFF when private audio should not be shared. Also avoid two projects/instances playing into the same destination simultaneously.
- The previous WASAPI output device selection and audio processing implementation are unchanged.

## v0.7 — final UI polish (Skelcast)

- **Visible title:** `Skelcast` is a single tightly spaced word. `Skel` is white, `cast` is the Skell Key neon green. Internal plugin identity `SkellCast` and saved parameter IDs are unchanged to preserve FL Studio project compatibility.
- Header key emblem replaced by a **small skull only**, adapted from Skell Key's skull silhouette.
- Midnight-black background and panels, white/light-white labels instead of ivory, slightly brighter green pilot-light glow. No extra control or meter.
- Compact **272 × 212** interface retained; working CABLE/SPARE routing and global ON/OFF behavior unchanged.

### Recording and performance

Skelcast is an audio-share **routing VST3**, not a file recorder. For recordings, insert it on the FL Studio Master, select **CABLE** and capture `CABLE Output` in OBS **Audio Input Capture**, or use **SPARE** and capture the displayed endpoint via OBS **Audio Output Capture**. Discord can similarly use CABLE as its input.

FL Studio's normal live recording and offline audio export continue through the unchanged Master stream: the plugin does **not** modify or mute audio samples. Capturing it in OBS/Discord is real-time and subject to the available output device and recording application's settings.

CPU usage is **not zero**. Stereo samples are copied into a bounded lock-free queue on the audio thread, and Windows WASAPI rendering happens on a separate thread; that avoids expensive device work and allocations in the real-time callback. The core tests now print a **non-gating FIFO throughput measurement**. Actual CPU %, glitches and sustained recording quality still require a Windows/FL Studio/OBS test.
