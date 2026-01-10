# Intro/Credits Skip Detector

## Overview

The **introskip** module is an automatic intro and credits detection feature for VLC media player. Similar to Netflix's "Skip Intro" button, this module analyzes audio patterns to identify and notify users about potential intro/credits segments.

## Features

- **Automatic Detection**: Analyzes audio silence patterns to identify intro boundaries
- **User Notification**: Displays message when intro is detected
- **Configurable**: Adjustable sensitivity and detection parameters
- **Non-intrusive**: Runs as an audio filter without affecting playback

## How It Works

The module uses silence detection to identify potential intro/credits segments:

1. **Audio Analysis**: Monitors audio RMS (Root Mean Square) levels in real-time
2. **Silence Detection**: Identifies periods where audio falls below threshold
3. **Pattern Recognition**: Long silence periods (typically 2+ seconds) indicate intro/credits boundaries
4. **User Notification**: When detected, displays message suggesting skip

## Usage

### Enable the Feature

To use the intro skip detector, add it to your audio filters:

```bash
vlc --audio-filter=introskip your-video.mp4
```

Or via GUI:
- Tools → Preferences → Audio → Filters
- Check "Intro/Credits skip detector"

### Configuration Options

#### `--introskip-enabled` (boolean, default: true)
Enable or disable the intro skip detection module.

```bash
vlc --introskip-enabled=true video.mp4
```

#### `--introskip-threshold` (float, 0.0-1.0, default: 0.01)
Silence detection threshold. Lower values detect quieter sounds as silence.
- **0.0**: Maximum sensitivity (detects very quiet audio as silence)
- **0.01**: Default (good for most content)
- **0.1**: Less sensitive (only loud silence gaps)

```bash
vlc --introskip-threshold=0.02 video.mp4
```

#### `--introskip-min-silence` (integer, milliseconds, default: 2000)
Minimum duration of silence to consider as intro/credits boundary.
- **1000**: 1 second (faster detection, more false positives)
- **2000**: 2 seconds (default, balanced)
- **5000**: 5 seconds (stricter detection)

```bash
vlc --introskip-min-silence=3000 video.mp4
```

### Example Commands

**Basic usage:**
```bash
vlc --audio-filter=introskip movie.mkv
```

**High sensitivity for quiet intros:**
```bash
vlc --audio-filter=introskip --introskip-threshold=0.005 episode.mp4
```

**Strict detection (avoid false positives):**
```bash
vlc --audio-filter=introskip --introskip-min-silence=4000 show.avi
```

## How to Skip

When the module detects an intro, you'll see a message:
```
*** INTRO DETECTED: 0s to 45s - Press 'I' to skip! ***
```

To skip the intro:
1. **Manual skip**: Use the seek bar or forward button
2. **Keyboard**: Press `→` to skip forward
3. **Future enhancement**: Dedicated skip button (in development)

## Technical Details

### Algorithm

1. **Audio Processing**:
   - Processes audio blocks in real-time
   - Calculates RMS (Root Mean Square) of audio samples
   - Compares against configurable threshold

2. **Silence Tracking**:
   - Tracks consecutive silent audio buffers
   - Measures duration of silence periods
   - Identifies patterns typical of intro/credits

3. **Detection Logic**:
   - Focuses on first 3 minutes of playback (typical intro location)
   - Requires minimum silence duration (default 2 seconds)
   - Marks intro end when audio resumes after silence

### Limitations

- **Audio-based only**: Currently analyzes audio, not video frames
- **Pattern dependent**: Works best with content that has clear silence gaps
- **Early playback**: Focuses on first few minutes (intros typically occur early)
- **Single detection**: Currently detects one intro per playback session

## Future Enhancements

Planned improvements:
- [ ] Video-based detection (black frames, static screens)
- [ ] Machine learning pattern recognition
- [ ] Credits detection at video end
- [ ] Automatic skip with user preference
- [ ] Skip button in UI
- [ ] Database of known intro/credits timestamps
- [ ] Multiple language support for on-screen text detection

## Debugging

Enable debug messages to see detection progress:

```bash
vlc -vv --audio-filter=introskip video.mp4 2>&1 | grep introskip
```

Debug output includes:
- RMS audio levels every 10 seconds
- Silence detection events
- Intro detection confirmation

## Troubleshooting

**Intro not detected?**
- Lower the threshold: `--introskip-threshold=0.005`
- Reduce minimum silence: `--introskip-min-silence=1000`
- Check if intro actually has silence gaps

**Too many false positives?**
- Raise the threshold: `--introskip-threshold=0.05`
- Increase minimum silence: `--introskip-min-silence=4000`

**Feature not working?**
- Ensure module is loaded: `--audio-filter=introskip`
- Check if feature is enabled: `--introskip-enabled=true`
- Verify audio track is present in video

## Contributing

To improve this feature:
1. Test with different content types (TV shows, movies, anime)
2. Report false positives/negatives with logs
3. Suggest improvements to detection algorithm
4. Help add video-based detection

## License

This module is part of VLC media player and is licensed under LGPL 2.1+.

## Author

- Ashutosh Mishra (2026)

## See Also

- VLC Audio Filters: https://wiki.videolan.org/Documentation:Modules/audio_filter/
- VLC Command Line: https://wiki.videolan.org/VLC_command-line_help/
