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
