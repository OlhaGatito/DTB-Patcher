# Changelog

## Unreleased — Native C++ UI

### Added

- Semantic comparison blocks with Doador/Receptor side-by-side values.
- Controls view reduced to physical button GPIO transfers instead of raw GPIO/pinctrl noise.
- Compatibility filtering so only existing Receiver properties can be transferred.
- Semantic transfer model smoke test in CI.
- Native C++/Win32 graphical interface.
- Native DTC bridge.
- Structural DTB comparison engine.
- Doador/Receptor swap action.
- Native Saruê project branding.
- About dialog.
- Improved visual hierarchy, header, source selectors, status area and side-by-side transfer tables.
- Explicit Receptor-as-base generation flow with round-trip validation.
- Fixed a string-buffer overflow in the Windows text-field reader that could corrupt the heap when generating a DTB.
- Native DTC smoke test in CI.
- Documentation for architecture, security and contribution workflow.
- Final DTS preview showing the complete Receptor-based tree before compilation.
- Persistent diagnostics log with native DTC failure trapping.
- Native build smoke test now re-compiles the DTS produced by DTB -> DTS -> DTB -> DTS round-trip.
- DTS model now preserves node/property labels and memory reservations needed by real-world Device Trees.

### Validation

The native implementation is maintained on `main`. CI validates the native DTC bridge, the semantic transfer model, standalone runtime dependencies and the Windows executable. Real hardware DTBs still require device-level validation before use.

## Previous

The project migrated from the earlier Python implementation to the native C++ implementation. The current repository contains only the native implementation and its reference/smoke-test DTS fixtures.
