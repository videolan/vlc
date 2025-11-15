# Subtitle Drag Feature - Visual Demo

## What This Feature Does

This feature allows you to **click and drag subtitles** to reposition them anywhere vertically on the video screen in real-time.

## Before This Feature

Previously, to adjust subtitle position in VLC, you had to:
1. Go to **Tools** → **Preferences**
2. Navigate to **Subtitles/OSD** section
3. Adjust numeric values
4. Click **Save**
5. Hope you got the position right
6. If not, repeat steps 1-5

❌ **Problem:** Time-consuming, not intuitive, requires leaving the video

## After This Feature

Now you can:
1. Hold **Ctrl** key
2. **Click and drag** the subtitle area
3. See subtitles move **instantly** as you drag
4. Release when positioned perfectly

✅ **Solution:** Instant, visual, intuitive adjustment while watching

## Visual Example

```
┌─────────────────────────────────────────────────────┐
│                                                     │
│                    Video Playing                    │
│                                                     │
│                                                     │
│                 [Action Scene]                      │
│                                                     │
│                                                     │
│  ┌────────────────────────────────────────────┐   │
│  │ Adjusting subtitle position...              │   │  ← Tooltip (only during drag)
│  └────────────────────────────────────────────┘   │
│                                                     │
│            Subtitles appear here ←─────────────────┼─── You can drag this up or down
│                                                     │     with Ctrl held
└─────────────────────────────────────────────────────┘
```

## Step-by-Step Usage

### Step 1: Start Playing Video
```
┌─────────────────────────────────────────┐
│                                         │
│          [Video Content]                │
│                                         │
│     "Hello, how are you?"               │  ← Subtitle at default position
└─────────────────────────────────────────┘
```

### Step 2: Hold Ctrl and Click
```
┌─────────────────────────────────────────┐
│                                         │
│          [Video Content]                │
│                                         │
│     "Hello, how are you?"               │  ← Click here while holding Ctrl
│            ↑                            │
│      (Your mouse)                       │
└─────────────────────────────────────────┘
```

### Step 3: Drag Up to Move Subtitles Higher
```
┌─────────────────────────────────────────┐
│                                         │
│          [Video Content]                │  ← Drag upward
│     "Hello, how are you?"               │  ← Subtitles move up with your mouse
│              ↑                          │
│        [Tooltip shown]                  │
└─────────────────────────────────────────┘
```

### Step 4: Release - Done!
```
┌─────────────────────────────────────────┐
│                                         │
│     "Hello, how are you?"               │  ← Subtitles now at new position
│          [Video Content]                │
│                                         │
└─────────────────────────────────────────┘
```

## Drag Down Example
```
Before:                          After dragging down:

┌───────────────────┐           ┌───────────────────┐
│                   │           │                   │
│   [Video]         │           │   [Video]         │
│                   │           │                   │
│   Subtitle here   │           │                   │
└───────────────────┘           │   Subtitle here   │
                                 └───────────────────┘
```

## Multiple Adjustments

Each drag session is **cumulative**:

```
1st Drag (move up):
┌─────────────────┐
│  Subtitle       │  ← Moved up
│  [Video]        │
└─────────────────┘

2nd Drag (move up more):
┌─────────────────┐
│                 │
│  Subtitle       │  ← Moved up further from previous position
│  [Video]        │
└─────────────────┘

3rd Drag (move down):
┌─────────────────┐
│  Subtitle       │  ← Moved down a bit, still higher than original
│  [Video]        │
└─────────────────┘
```

## Use Cases

### 1. Subtitle Blocking Important Content
```
Problem:                         Solution (drag up):
┌─────────────────┐             ┌─────────────────┐
│                 │             │  [Score: 3-2]   │  ← Now visible!
│  [Score: 3-2]   │             │                 │
│  "Great play!"  │  ← Blocking │                 │
└─────────────────┘             │  "Great play!"  │
                                 └─────────────────┘
```

### 2. Subtitle Too Low (Cut Off)
```
Problem:                         Solution (drag up):
┌─────────────────┐             ┌─────────────────┐
│  [Video]        │             │  [Video]        │
│  "This is a..."│              │  "This is a     │  ← Fully visible!
└─────────────────┘             │   long subtitle"│
  (cut off)                      └─────────────────┘
```

### 3. Personal Preference
```
Some people prefer:              Others prefer:
┌─────────────────┐             ┌─────────────────┐
│  [Video]        │             │  Subtitle       │  ← At top
│                 │             │  [Video]        │
│  Subtitle       │  ← At bottom│                 │
└─────────────────┘             └─────────────────┘
```

## Features

✅ **Real-time adjustment** - See changes instantly  
✅ **Visual feedback** - Tooltip shows you're adjusting  
✅ **Non-intrusive** - Only activates with Ctrl key  
✅ **Cumulative** - Multiple drags stack  
✅ **Smooth animation** - Professional feel  
✅ **Works everywhere** - Any video, any subtitle format  

## Keyboard Shortcut Reference

| Action | Keys |
|--------|------|
| Activate drag mode | Hold **Ctrl** |
| Drag subtitle up | **Ctrl + Click + Drag Up** |
| Drag subtitle down | **Ctrl + Click + Drag Down** |
| Cancel drag | Release **Ctrl** or click |

## Tips

💡 **Tip 1:** Hold Ctrl first, then click and drag  
💡 **Tip 2:** Drag in the lower 60% of video for best results  
💡 **Tip 3:** Each drag is cumulative - you can adjust multiple times  
💡 **Tip 4:** The tooltip shows you when adjustment is active  
💡 **Tip 5:** Position resets when you open a new video  

## Comparison with Other Methods

| Method | Speed | Precision | Real-time Preview | While Playing |
|--------|-------|-----------|-------------------|---------------|
| **This Feature** | ⚡ Instant | 🎯 Pixel-perfect | ✅ Yes | ✅ Yes |
| Preferences Menu | 🐌 Slow | 📊 Numeric only | ❌ No | ❌ No |
| Keyboard Shortcuts | ⚡ Fast | 🎚️ Step-based | ✅ Yes | ✅ Yes |

## Limitations

- Position is **temporary** (resets on new video)
- Requires **mouse/touchpad** (keyboard shortcuts still available)
- Only adjusts **vertical position** (horizontal may come in future)
- Minimum position is **top of video** (cannot go above)

## Future Enhancements (Ideas)

- 💾 Save position permanently in preferences
- ↔️ Horizontal positioning with Shift+Ctrl
- 🔄 Reset to default button
- 📌 Per-video position memory
- 📱 Touch screen support

---

## Quick Start

**Want to try it now?**

1. Open VLC and play a video with subtitles
2. Hold down the **Ctrl** key
3. Click anywhere in the bottom half of the video
4. Drag your mouse up or down
5. Watch the subtitles move!

**That's it!** 🎉
