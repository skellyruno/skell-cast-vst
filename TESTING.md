# SkellCast v0.4 validation checklist

- [x] JUCE CMake project enables C and C++.
- [x] VST3-only build and packaging workflow has no standalone receiver requirement.
- [x] Destination parameter defaults to CABLE; routing decision has portable unit coverage.
- [ ] Windows CI compiles the VST3 and runs core tests.
- [ ] Confirm ON/OFF: red OFF sends no signal; green ON uses selected destination.
- [ ] CABLE: VB-CABLE present -> Discord input / OBS Audio Input Capture gets clean stereo.
- [ ] CABLE: VB-CABLE absent -> no speaker fallback, no crash; reconnect when installed.
- [ ] DESKTOP: a **different Windows default playback device** receives the extra audio copy.
- [ ] DESKTOP: OBS Desktop Audio / Audio Output Capture picks it up, not twice.
- [ ] Warn if FL Studio already plays directly to those same default speakers: double playback is expected.
- [ ] Toggle CABLE <-> DESKTOP during playback; no stale audio, stuck thread, memory growth or crash.
- [ ] Flip ON/OFF repeatedly during output routing changes; close/reopen VST editor.
- [ ] Save/reload project and automation state; existing CABLE projects stay on CABLE by default.
- [ ] Change Windows default playback device while DESKTOP is ON: reconnect to new device.
- [ ] Remove/disconnect default playback device and re-enable it; reconnect automatically.
- [ ] 44.1/48/96/192 kHz; 64/128/512/2048 frame blocks; mono/stereo; pause/seek/stop in FL Studio.
- [ ] Verify dry FL Studio output is unchanged in CABLE, DESKTOP, and OFF.
- [ ] Test Windows output device formats different from FL Studio's sample rate.
- [ ] Test with OBS / Discord voice-processing features toggled as needed for music.
- [ ] 30 minute stress test; high CPU; DAW close; plugin unload; no leaked workers.
- [ ] Validate VST3 host compatibility in addition to CMake and FIFO unit tests.

Portable tests without Windows/JUCE:

```sh
cmake -S . -B build-core -DSKELLCAST_CORE_ONLY=ON
cmake --build build-core
ctest --test-dir build-core --output-on-failure
```

**Note:** CABLE mode sends sound to a virtual input. DESKTOP mode sends a **second copy** to Windows playback, which OBS can capture. It is not a selectable Discord microphone in DESKTOP mode.
