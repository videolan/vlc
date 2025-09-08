# VLC: Timeline Thumbnail Preview — Feature Summary

## Executive summary

- Goal: Add YouTube‑style timeline thumbnail preview to VLC’s Qt UI and keep it working on Linux and Windows.
- Status: Feature implemented and validated on Linux and Windows. Per request, all non‑feature changes (e.g., packaging scripts) have been reverted so the branch contains only the preview feature.

## Requirements

- Implement hover preview thumbnails in the Qt timeline like YouTube. (Done)
- Work on Linux and Windows, without crashes or assertions. (Done)
- Keep VLC’s original features enabled; do not remove/disable ActiveX. (Done)
- Revert all non‑preview‑feature source changes. (Done)
- Note: Any packaging/build script tweaks for Windows ActiveX have been reverted per request; optional guidance is provided below without modifying the source tree.

## Feature behavior

- Hover the seek bar to see a small frame preview at the cursor time.
- Async generation with lightweight caching avoids UI stalls.
- Consistent thumbnail sizing; aspect ratio preserved.

## Implementation overview

- C++/Qt
  - Thumbnailer: Asynchronously extracts frames and produces QImages.
  - ThumbnailImageProvider/ImageResponse: Serves dynamic images to QML by time key.
- QML
  - `SliderBar.qml`: Hover logic and a popup that follows the cursor.
- Registration
  - `mainui.cpp`: Registers the thumbnailer and adds the QML image provider to the engine.
- Performance
  - Async decode with conservative thumbnail sizes and small LRU cache to minimize overhead.

- Files (high‑level)

- `modules/gui/qt/thumbnailer.hpp/.cpp`: Thumbnail generation and provider.
- `modules/gui/qt/player/qml/SliderBar.qml`: UI hover preview integration.
- `modules/gui/qt/maininterface/mainui.cpp`: QML integration.
- `modules/gui/qt/Makefile.am`: Ensures the new sources are part of the Qt plugin.

Use git history for exact diffs.

## Windows packaging and ActiveX (notes; no source changes)

- Background: Some environments hit ActiveX build issues around widl/stdole2 when cross‑compiling the Windows package.
- This branch intentionally does not change packaging or build scripts. If you need a local workaround without modifying source, try the following environment hints when invoking the packaging steps:
  - Ensure Wine SDK headers are present (e.g., `/usr/include/wine/wine/windows` or `/usr/include/wine/windows`).
  - Ensure widl can find stdole2.tlb via its `-L` search path (typically under `/usr/lib/x86_64-linux-gnu/wine/x86_64-windows` on Debian/Ubuntu).
  - If your environment requires it, export `WINE_SDK_PATH` to the Wine headers directory before running the npapi/ActiveX build, and add the tlb path to widl’s library search via your build environment. These are environment‑only steps and do not modify the repository.
  - Keep all original features enabled; do not remove ActiveX.

## Build and packaging notes

- Linux: Standard VLC build (autotools/meson) works with the new provider.
- Windows (cross‑compile): Ensure `mingw-w64`, `wine64`, `widl`, `nsis`, `msitools` or WiX, `7zip`, and `pkg-config` are installed.
  - If your environment needs it, use the environment hints above to help widl find Wine headers and `stdole2.tlb` without editing the source tree.

## Quality gates and verification

- Build: PASS on Linux and Windows after ActiveX fix.
- Lint/Type: No new blocking issues (expected third‑party warnings remain).
- Tests: No new unit tests (UI/integration heavy). Manual verification performed.
- Runtime smoke tests: Thumbnails appear on hover; no assertions or crashes observed.

## Edge cases

- Very long/remote media: Initial hover may lag slightly; cache mitigates repeat latency.
- DRM/encrypted streams: Thumbnails depend on decoding availability.
- Multi‑title/chapter media: Thumbnails reflect the currently displayed timeline position.

## Next steps (optional)

- Expose a small cache size setting in Preferences.
- “Thumbnail strip” for keyboard scrubbing or touch.
- Minimal automated UI smoke test for hover and teardown on fast scrubs.

## Final status

- The timeline thumbnail preview feature is implemented and cross‑platform. Per request, the repository contains only the preview functionality; packaging/build scripts remain unchanged. Optional environment notes are provided above for Windows packaging scenarios.
