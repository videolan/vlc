# Subtitle Drag Feature

## Overview
VLC now supports dragging subtitles up or down on screen to adjust their vertical position in real-time during playback.

## How to Use

1. **Play a video with subtitles** - Ensure you have subtitles enabled (either embedded or loaded from a file)

2. **Activate drag mode** - Hold down the **Ctrl** key on your keyboard

3. **Click and drag** - While holding Ctrl, click in the lower portion of the video (where subtitles typically appear) and drag:
   - **Drag UP** to move subtitles higher on screen
   - **Drag DOWN** to move subtitles lower on screen

4. **Visual feedback** - While dragging, a tooltip appears showing "Adjusting subtitle position..."

5. **Release** - Release the mouse button to complete the adjustment

## Technical Details

### Implementation
- The feature uses VLC's existing `sub-margin` variable to control subtitle vertical positioning
- Drag is detected only when Ctrl key is held and mouse is in the lower 60% of video area
- Mouse events propagate normally when Ctrl is not held, so regular playback controls work as expected
- Works with both primary and secondary subtitles

### Code Changes
- **C++ Backend**: Added `setSubtitleMargin()` and `setSecondarySubtitleMargin()` methods to PlayerController
- **QML Frontend**: New SubtitleDragOverlay component integrated into Player.qml
- Minimal changes to existing codebase for maintainability

### Compatibility
- Works with Qt-based VLC interface
- Requires video output to be active
- Compatible with all subtitle formats supported by VLC

## Notes
- The subtitle position adjustment is temporary and applies only to the current playback session
- To make permanent subtitle position changes, use VLC's preferences: Tools > Preferences > Subtitles/OSD
- This feature is designed to be non-intrusive and only activates when explicitly invoked with Ctrl+drag
