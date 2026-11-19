# Changelog

## 0.8.0-preview — 2026-11-19

- Re-check destination, backup and staging path chains after directory creation and immediately before install/rollback directory swaps.
- Extend Windows installer integration tests to reject junctions in package and backup trees and at VST3/backup roots.
- Preserve the existing VST3 bundle, global audio-sharing OFF default, parameter IDs, automation IDs and saved host state.

## 0.8.0-preview — 2026-11-18

- Added a PowerShell 7+ private-preview Windows installer for the single x64 VST3 bundle.
- Added per-user install, idempotent update, checksum-verified backup and explicit rollback.
- Added a -WhatIf preview path; machine-wide installation is never the default.
- Added package, provenance, strict JSON, VST3 class identity, AMD64 PE32+ and checksum validation before installation writes.
- Added Windows integration coverage for install/update/rollback, backup integrity, path traversal, duplicate and non-finite JSON, package tampering and invalid existing targets.
- Preserved audio DSP, the global audio-sharing OFF default, VST3 identity, parameter IDs, automation IDs, presets and saved host state.

This is an unsigned private preview, not a public or commercial release. FL Studio, OBS/WASAPI, real audio A/B, target-hardware CPU/latency, licensing, vendor/support details and code signing remain separate acceptance gates.
