# SkellCast release baseline — 2026-11-16

Baseline Actions run #43776801697, artifact #14186867360, source commit `04ecc85099c17f3c1302d313015094a70bb99038`. Archive SHA-256: `8704574f6e1faceb67d63071991244bd0c53dc63db8dd9b1489430611f81d7fc` (2,490,074 bytes).

The package contains `SkellCast.vst3/Contents/x86_64-win/SkellCast.vst3` (5,473,792 bytes; SHA-256 `a0cd341fe1baa7aa24e43de41695287b722570ac15cc54fb73f2bda04ed7e185`) and `moduleinfo.json` (1,028 bytes; SHA-256 `2273784a4f863049745f7d6ce25755bdd419ebc2e6b520598cd917cde14e1af4`). The binary is AMD64 PE32+. Metadata is SkellCast 0.8.0 / Skelly with two distinct VST3 class IDs.

Confirmed defects: JUCE trailing commas make the baseline moduleinfo invalid strict JSON; no provenance or SHA-256 manifest exists; the packaging script selects the first matching Release bundle instead of rejecting duplicates; vendor URL and support email are blank and require owner-approved values.

The original Actions artifact remains preserved. This packaging work does not change DSP, UI, global preferences, parameter IDs or project state. FL Studio, OBS/WASAPI, pass-through A/B, CPU/latency, licensing, signing, installer, updater and rollback remain open. This is a preview candidate, not a commercial release.
\n\nAdditional host-compatibility finding: the baseline source advertises one program but returned an empty program name. PR #4 changes only the host-visible program label to `Default`; no program count, preset state, parameter IDs or audio processing changes. Pluginval strictness 5 must confirm this on the exact head.\n