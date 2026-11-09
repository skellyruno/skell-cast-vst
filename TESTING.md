# SkellCast — verification checklist

## Automated
- [x] Core packet layout, length/magic validation and ring round-trip (`Tests/core_tests.cpp`, Linux GCC).
- [ ] Windows x64 JUCE VST3 and receiver compile via GitHub Actions (not yet run for this delivery).
- [ ] Plugin inspector / vst3validator for parameters, state restore, bus layouts and host reload.

## FL Studio (manual Windows test)
- [ ] Load on Master; monitor original master. Confirm **no attenuation** and exact rendered output with Transmit OFF and ON.
- [ ] Play 44.1, 48, 88.2, 96, 192 kHz sessions with buffers 64, 128, 256, 512 and 2048 samples; check level and dropouts.
- [ ] Pause, resume, reposition song and switch transport; inspect receiver for leftover audio and stuttering.
- [ ] Click the only ON/OFF button and automate the send parameter; verify green = ON and red = OFF, and ensure FL Studio master audio never changes.
- [ ] Open and close plugin UI repeatedly and shut down FL Studio; check no hangs and persistent send state after save/reload. Confirm there are no gain sliders, meters, or extra plugin controls.
- [ ] Test receiver launched before and after FL Studio; stop/restart receiver and switch VB-CABLE output.
- [ ] 30-minute loop plus sample rate/device change stress test (latency, drift, overflow, device reacquire).

## Routing (manual Windows test)
- [ ] Discord Input Device = **CABLE Output**; verify friend hears only FL Studio and not microphone by default.
- [ ] Discord noise reduction, echo cancellation, automatic gain and input sensitivity tailored for music.
- [ ] OBS Audio Input Capture = **CABLE Output**; verify levels and no duplicate captures.
- [ ] Confirm only one receiver can bind to UDP port 44991 and that a clear error appears if port is occupied.
- [ ] Check VB-CABLE routing remains silent until Start Routing and plugin ON; allow for a brief prebuffered tail when switching OFF.
- [ ] Confirm no accidental feedback when FL Studio input source is an independent hardware microphone.

## Later engineering milestones
- [ ] Bandlimited/sinc sample-rate conversion with measured clock-drift management.
- [ ] Smooth fade-in/out and packet discontinuity handling; packet timeline / sequence-aware jitter control.
- [ ] Receiver device auto-detection and safe default / clear accidental-speaker warnings.
- [ ] Explicit connection handshake, robust multiple-instance routing, stable receiver shutdown/reconnect.
- [ ] Optional mic blend in receiver with echo/feedback avoidance.
- [ ] User-level Windows installer, verified code-signed builds, complete GUI accessibility.
