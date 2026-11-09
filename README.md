# SKELL / CAST

**SkellCast — FL Studio master audio → Discord / OBS**, Windows x64 JUCE VST3 and receiver companion app. Independent original implementation; no Muse code or branding is used.

> **Status: v0.2 SOURCE PROJECT / PRE-RELEASE.** Portable Windows binaries are produced by the included GitHub Actions workflow or `scripts/Build-Windows.ps1`. Core protocol tests have been run; the Windows VST3 and actual FL Studio, Discord and OBS device routing require Windows verification. This version is not a Windows virtual audio driver and is not a cloud collaboration platform.

## What it does

- **Stereo uncompressed 32-bit floating-point local transport.** Audio goes from the plugin to `127.0.0.1:44991` via UDP, not onto the public Internet.
- **Nonblocking FL Studio processing.** Plugin queues audio to a sender thread rather than performing socket I/O inside the realtime audio callback. Sends a copy; the FL Studio master track is not changed.
- **Exactly one plugin control:** a large ON/OFF button. Its pilot light glows **green = sharing enabled** or **red = sharing disabled**. Unity-level audio is sent; there is no gain slider or meter.
- **Receiver app** shows input activity, source sample rate, packet gaps, and a standard JUCE Windows device selector; 60 ms input prebuffer and linear resampling for different DAW/device rates.
- **Output routing** through a preinstalled virtual audio device like VB-CABLE to Discord or OBS.
- **Vector-only compact dark/neon-green UI (340 × 184).** Fixed-size VST3 editor, persistent ON/OFF switch. Receiver retains its Windows device setup screen.

## Setup for FL Studio → Discord

1. Install **VB-CABLE** from its official developer, [VB-Audio](https://vb-audio.com/Cable/), then restart Windows if the installer requests it. VB-CABLE is a third-party virtual audio driver **not included** in SkellCast.
2. Obtain the Windows build artifact from GitHub Actions or build from source (below). Copy the **entire `SkellCast.vst3` folder** to `C:\Program Files\Common Files\VST3\` (Administrator permissions may be required). Put `SkellCastReceiver.exe` anywhere you can run it.
3. In FL Studio, rescan VST3 plugins. Add **SkellCast** to the last effect slot of the **Master mixer track**, after effects you wish to broadcast.
4. Open **SkellCastReceiver.exe**. In its audio-device area, choose the regular Windows shared-audio system if offered (not an ASIO device already owned by FL Studio). Choose **CABLE Input (VB-Audio Virtual Cable)** as the *output*. Click **START ROUTING**. Be careful not to leave the output on your speakers; the app would play the same audio there again.
5. In the SkellCast VST3, press the single button until its indicator is **green / ON**, and play a project. When the receiver detects frames, it should say **INCOMING AUDIO DETECTED**. Press the button again for **red / OFF** to stop sending new frames.
6. Discord → **User Settings → Voice & Video → Input Device → CABLE Output (VB-Audio Virtual Cable)**. Leave your Discord *Output Device* as your headphones. Music is frequently damaged by Discord's processing: disable noise suppression, echo cancellation and automatic gain control when practical; use a voice activity / input sensitivity suitable for music.
7. You can keep your FL Studio output set to your original audio interface. The plugin copies the signal without changing FL Studio's usual playback.

**Important:** If you want to speak through your actual microphone **at the same time as the FL master stream**, you must mix the mic into the same virtual input (e.g. via Voicemeeter) or a future mic-mixing mode. Discord typically accepts only one mic/input device at a time. Also, Discord still compresses its voice channels: SkellCast's *local transport* is uncompressed, not the entire Discord connection.

## Setup for FL Studio → OBS

1. Follow setup steps 1–5 above.
2. OBS Studio → **Sources → + → Audio Input Capture → Create New**, and select **CABLE Output (VB-Audio Virtual Cable)**.
3. Use OBS's Audio Mixer to check for duplicate audio. Add a *separate* microphone source if needed. In Advanced Audio Properties, decide if/where you want to monitor.

## Building on Windows

Requirements: Windows 10/11 x64, Visual Studio 2022 with **Desktop development with C++**, CMake 3.25+, Git, Internet connectivity during initial JUCE download. The CMake build fetches JUCE **8.0.16-security**, or accepts a local JUCE checkout with `-DJUCE_DIR=C:/path/to/JUCE`.

From PowerShell inside the source directory:

```powershell
./scripts/Build-Windows.ps1
```

Release files are assembled into `out/SkellCast-Windows-x64/`. Or push the repository to GitHub and trigger **Actions → Windows VST3 + Receiver → Run workflow**; download the `SkellCast-Windows-x64` artifact.

The scripts **build** a `.vst3` and `.exe`; this source ZIP does **not** contain compiled Windows binaries. They are not code-signed. Commercial redistribution must comply with the relevant [JUCE licensing terms](https://juce.com/legal/juce-8-licence/) and applicable third-party software licenses.

## Technical notes and boundaries

| Feature | v0.2 |
| --- | --- |
| 1 FL Studio master / 2-channel stereo | Yes |
| FL host output unaffected | Yes |
| Local transmission | uncompressed float32 PCM over 127.0.0.1 UDP |
| Network/cloud transmission | No |
| Windows Discord and OBS | Via separate VB-CABLE virtual driver |
| Audio resampling | Linear (quality can be improved) |
| Receiver prebuffer | Approximately 60 ms plus device/host buffering |
| Auto-installed virtual mic | No |
| Integrated mic + DAW mixing | Not yet |
| Multiple concurrent plugin instances | Not supported as a mixed bus; place only one on Master |
| End-to-end Windows routing verified | Requires local test |

**The red/green light indicates only the VST3 ON/OFF state, not whether Discord, OBS, VB-CABLE, or the receiver is connected.** A brief buffered tail may play after switching OFF. Adjust receiver/Discord levels in their respective apps.

The current receiver is deliberately simple. Heavy CPU stalls or audio-device changes may cause discontinuities; restarting `START ROUTING` is advised after changing FL Studio's sample rate or switching output devices. For extremely demanding professional use, follow the regression checklist in `TESTING.md` and replace the linear resampler with a windowed-sinc implementation.
