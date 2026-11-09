# SkellCast v0.3 verification checklist

- [x] VST3-only source target. No receiver GUI target, UDP transport, or extra EXE.
- [x] CMake project enables `LANGUAGES C CXX` to address the reported JUCE error.
- [ ] Compile VST3 with Windows Visual Studio 2022 / 2026 and JUCE 8.0.17-security.
- [ ] Run standalone VST3 validator, inspect every plug-in entry point/bus/parameter/state restore.
- [ ] Verify ON with VB-CABLE routes audio to Discord's **CABLE Output** input; red/OFF stops it.
- [ ] Verify ON with VB-CABLE absent or unplugged sends **nothing to default speakers**, and reconnect after installation.
- [ ] Open/close FL Studio UI rapidly and unload plug-in repeatedly. No worker-thread shutdown hangs.
- [ ] Verify FL Studio Master native output is bitwise unchanged when VST3 is enabled or disabled.
- [ ] Test mono/stereo bus layouts, 44.1/48/96/192 kHz, 64/128/512/2048 block sizes, FL stop/start.
- [ ] Test on Windows where VB-CABLE playback device format differs from FL Studio's sample rate.
- [ ] Test red/OFF after green/ON: no lingering signal beyond a short Windows audio buffer.
- [ ] Test a 30-minute session, device disconnect/reconnect, host transport seek and pause, heavy CPU load.
- [ ] Test other Windows capture software (OBS) and confirm no doubled monitor channels.
- [ ] Check old Receiver installation is unused and no `SkellCastReceiver.exe` is shipped.
- [ ] Run `ctest --test-dir build -C Release --output-on-failure` on Windows.

Core FIFO multithreaded tests can run without JUCE on Linux/macOS:

```bash
cmake -S . -B build-core -DSKELLCAST_CORE_ONLY=ON
cmake --build build-core
ctest --test-dir build-core --output-on-failure
```

**Known limitations:** green means **user-enabled**, not device-connected. First active device with standard VB-Audio CABLE Input name is selected automatically. Windows WASAPI shared mode and third-party cable driver determine the final output latency and device format. The Windows integration remains unverified until the above manual tests are done.
