# SkellCast v0.5 test plan

## Automated tests
- [x] Pure routing selector tests added: default output exclusion, active endpoint rejection, inspection failure, no-spare case and virtual preference.
- [ ] Windows GitHub Actions VST3 build + CTest success (check latest run).
- [ ] Manual Windows/WASAPI integration and FL Studio / OBS tests.

## Windows / FL Studio / OBS checks
- [ ] **One Windows speaker/headphone only**: DESKTOP reports No spare output device and makes no duplicate sound.
- [ ] Two Windows playback devices: DESKTOP chooses an idle *non-default* device and displays its name.
- [ ] Busy non-default device: excludes endpoint if Windows lists an active shared-mode audio session.
- [ ] Multiple eligible endpoints: prioritizes idle virtual output; otherwise picks first idle physical.
- [ ] OBS Audio Output Capture can capture the **shown** non-default endpoint, not the default output.
- [ ] Change Windows default device while sharing: formerly spare default is disconnected, then a new spare is scanned.
- [ ] Disconnect/reconnect selected hardware endpoint, retry after it becomes available.
- [ ] ASIO/exclusive-host caveat: verify spare endpoint is not actually an alternate route to same hardware; session manager alone cannot detect this.
- [ ] CABLE mode: VB-CABLE present/absent, Discord input test, ON/OFF behavior.
- [ ] UI auto status updates, no clipping of label, button/switch works, project state persists.
- [ ] Stress: repeated toggles, 44.1/48/96 kHz, 64-2048 frame blocks, no host dropouts; validate no feedback.
- [ ] Confirm output remains bit-identical through FL Master and app closes without leaked threads.

## Portable core tests

```sh
cmake -S . -B build-core -DSKELLCAST_CORE_ONLY=ON
cmake --build build-core
ctest --test-dir build-core --output-on-failure
```

## v0.6 UI and global preference regression checks

- [ ] Verify Skell Key Bone colors and skull-key logo on actual Windows VST3 render, including ivory SKELL / green CAST.
- [ ] Verify 272x212 layout: no clipped text, click targets remain usable.
- [ ] Send ON in Project A, open Project B (new instance): it is ON without touching Project B.
- [ ] Send OFF in Project B, open Project C: it is OFF without touching Project C.
- [ ] Load previously saved Project A while global preference is OFF: old per-project ON must not turn sharing back on.
- [ ] Switch CABLE/SPARE and reopen FL Studio: last chosen destination is preserved.
- [ ] Confirm unrelated VST3 automation still uses existing parameter ID `desktop`.
- [ ] Close and reopen plugin UI without closing project: current state stays consistent.
- [ ] Run with several SkellCast instances in one process: same global UI state; no disk I/O in audio callback.
- [ ] Ensure no audible difference to pass-through audio, latency or WASAPI routing after UI/preferences update.

## v0.7 final release checklist

- [x] Maintains the existing CABLE/SPARE DSP and WASAPI implementation unchanged.
- [x] Preserves VST3 ID and parameters `send` / `desktop` for older projects.
- [x] CPU throughput non-gating FIFO smoke test added.
- [ ] GitHub Actions Windows Release + CTest must succeed on this exact commit.
- [ ] Render and inspect UI in FL Studio at 100% and high DPI: skull (no key), joined `Skelcast` title, white labels, black background and green accents.
- [ ] OBS record 1–2 minutes with CABLE mode: verify captured signal is normal volume, free of clipping/drops and plays back after the recording is saved.
- [ ] Repeat with SPARE output on a machine with a genuinely separate idle device; OBS must capture the shown endpoint.
- [ ] Verify existing FL Studio projects and global preference across FL Studio restarts, with ON then OFF.
- [ ] Compare FL Studio Master rendered output with Skelcast enabled vs disabled; it must remain unchanged.
- [ ] During a longer session, monitor CPU in FL Studio and Windows Task Manager; watch for dropouts, underruns and stuck shutdown.
- [ ] Confirm multiple windows/instances don't share the same audio route unintentionally.
