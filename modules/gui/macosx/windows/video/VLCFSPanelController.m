/*****************************************************************************
 * VLCFSPanelController.m: macOS fullscreen controls window controller
 *****************************************************************************
 * Copyright (C) 2006-2016 VLC authors and VideoLAN
 *
 * Authors: Jérôme Decoodt <djc at videolan dot org>
 *          Felix Paul Kühne <fkuehne at videolan dot org>
 *          David Fuhrmann <david dot fuhrmann at googlemail dot com>
 *          Marvin Scholz <epirat07 at gmail dot com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#import "VLCFSPanelController.h"

#import <vlc_aout.h>

#import "coreinteraction/VLCCoreInteraction.h"
#import "main/CompatibilityFixes.h"
#import "main/VLCMain.h"

@interface VLCFSPanelController () {
    BOOL _isCounting;

    // Only used to track changes and trigger centering of FS panel
    NSRect _associatedVoutFrame;
    // Used to ask for current constraining rect on movement
    NSWindow *_associatedVoutWindow;
}

@end

@implementation VLCFSPanelController

static NSString *kAssociatedFullscreenRect = @"VLCFullscreenAssociatedWindowRect";

+ (void)initialize
{
    NSDictionary *appDefaults = [NSDictionary dictionaryWithObjectsAndKeys: NSStringFromRect(NSZeroRect), kAssociatedFullscreenRect, nil];

    [[NSUserDefaults standardUserDefaults] registerDefaults:appDefaults];
}


#pragma mark -
#pragma mark Initialization

- (id)init
{
    self = [super initWithWindowNibName:@"VLCFullScreenPanel"];
    if (self) {
        NSString *rectStr = [[NSUserDefaults standardUserDefaults] stringForKey:kAssociatedFullscreenRect];
        _associatedVoutFrame = NSRectFromString(rectStr);
    }
    return self;
}

- (void)windowDidLoad
{
    [super windowDidLoad];

    /* Do some window setup that is not possible in IB */
    [self.window setOpaque:NO];
    [self.window setAlphaValue:0.0f];
    [self.window setMovableByWindowBackground:NO];
    [self.window setLevel:NSModalPanelWindowLevel];
    [self.window setStyleMask:self.window.styleMask | NSResizableWindowMask];
    [self.window setBackgroundColor:[NSColor clearColor]];

    /* Set autosave name after we changed window mask to resizable */
    [self.window setFrameAutosaveName:@"VLCFullscreenControls"];

    /* Inject correct background view depending on OS support */
    [self injectVisualEffectView];

    [self setupControls];
}

#define setupButton(target, title, desc)            \
    target.accessibilityTitle = title;              \
    target.accessibilityLabel = desc;               \
    [target setToolTip:title];

- (void)setupControls
{
    /* Setup translations for buttons */
    setupButton(_playPauseButton,
                _NS("Play/Pause"),
                _NS("Play/Pause the current media"));
    setupButton(_nextButton,
                _NS("Next"),
                _NS("Go to next item"));
    setupButton(_previousButton,
                _NS("Previous"),
                _NS("Go to the previous item"));
    setupButton(_forwardButton,
                _NS("Forward"),
                _NS("Seek forward"));
    setupButton(_backwardButton,
                _NS("Backward"),
                _NS("Seek backward"));
    setupButton(_fullscreenButton,
                _NS("Leave fullscreen"),
                _NS("Leave fullscreen"));
    setupButton(_volumeSlider,
                _NS("Volume"),
                _NS("Adjust the volume"));
    setupButton(_timeSlider,
                _NS("Position"),
                _NS("Adjust the current playback position"));

    /* Setup other controls */
    [_volumeSlider setMaxValue:[[VLCCoreInteraction sharedInstance] maxVolume]];
    [_volumeSlider setIntValue:AOUT_VOLUME_DEFAULT];
    [_volumeSlider setDefaultValue:AOUT_VOLUME_DEFAULT];

    /* Identifier to store the state of the remaining or total time label,
     * this is the same identifier as used for the window playback cotrols
     * so the state is shared between those.
     */
    [_remainingOrTotalTime setRemainingIdentifier:@"DisplayTimeAsTimeRemaining"];
}

#undef setupButton

#pragma mark -
#pragma mark Control Actions

- (IBAction)togglePlayPause:(id)sender
{
    [[VLCCoreInteraction sharedInstance] playOrPause];
}

- (IBAction)jumpForward:(id)sender
{
    static NSTimeInterval last_event = 0;
    if (([NSDate timeIntervalSinceReferenceDate] - last_event) > 0.16) {
        /* We just skipped 4 "continuous" events, otherwise we are too fast */
        [[VLCCoreInteraction sharedInstance] forwardExtraShort];
        last_event = [NSDate timeIntervalSinceReferenceDate];
    }
}

- (IBAction)jumpBackward:(id)sender
{
    static NSTimeInterval last_event = 0;
    if (([NSDate timeIntervalSinceReferenceDate] - last_event) > 0.16) {
        /* We just skipped 4 "continuous" events, otherwise we are too fast */
        [[VLCCoreInteraction sharedInstance] backwardExtraShort];
        last_event = [NSDate timeIntervalSinceReferenceDate];
    }
}

- (IBAction)gotoPrevious:(id)sender
{
    [[VLCCoreInteraction sharedInstance] previous];
}

- (IBAction)gotoNext:(id)sender
{
    [[VLCCoreInteraction sharedInstance] next];
}

- (IBAction)toggleFullscreen:(id)sender
{
    [[VLCCoreInteraction sharedInstance] toggleFullscreen];
}

- (IBAction)timeSliderUpdate:(id)sender
{
    switch([[NSApp currentEvent] type]) {
        case NSLeftMouseUp:
            /* Ignore mouse up, as this is a continuous slider and
             * when the user does a single click to a position on the slider,
             * the action is called twice, once for the mouse down and once
             * for the mouse up event. This results in two short seeks one
             * after another to the same position, which results in weird
             * audio quirks.
             */
            return;
        case NSLeftMouseDown:
        case NSLeftMouseDragged:
            break;

        default:
            return;
    }
    input_thread_t *p_input;
    p_input = pl_CurrentInput(getIntf());

    if (p_input) {
        vlc_value_t pos;
        pos.f_float = [_timeSlider floatValue] / 10000.;
        var_Set(p_input, "position", pos);
        input_Release(p_input);
    }
    [[[VLCMain sharedInstance] mainWindow] updateTimeSlider];
}

- (IBAction)volumeSliderUpdate:(id)sender
{
    [[VLCCoreInteraction sharedInstance] setVolume:[sender intValue]];
}

#pragma mark -
#pragma mark Metadata and state updates

- (void)setPlay
{
    [_playPauseButton setState:NSOffState];
    [_playPauseButton setToolTip: _NS("Play")];
}

- (void)setPause
{
    [_playPauseButton setState:NSOnState];
    [_playPauseButton setToolTip: _NS("Pause")];
}

- (void)setStreamTitle:(NSString *)title
{
    [_mediaTitle setStringValue:title];
}

- (void)updatePositionAndTime
{
    input_thread_t *p_input = pl_CurrentInput(getIntf());

    /* If nothing is playing, reset times and slider */
    if (!p_input) {
        [_timeSlider setFloatValue:0.0];
        [_elapsedTime setStringValue:@""];
        [_remainingOrTotalTime setHidden:YES];
        return;
    }

    vlc_value_t pos;
    char psz_time[MSTRTIME_MAX_SIZE];

    var_Get(p_input, "position", &pos);
    float f_updated = 10000. * pos.f_float;
    [_timeSlider setFloatValue:f_updated];


    vlc_tick_t t = var_GetInteger(p_input, "time");
    vlc_tick_t dur = input_item_GetDuration(input_GetItem(p_input));

    /* Update total duration (right field) */
    if (dur <= 0) {
        [_remainingOrTotalTime setHidden:YES];
    } else {
        [_remainingOrTotalTime setHidden:NO];

        NSString *totalTime;

        if ([_remainingOrTotalTime timeRemaining]) {
            vlc_tick_t remaining = 0;
            if (dur > t)
                remaining = dur - t;
            totalTime = [NSString stringWithFormat:@"-%s", secstotimestr(psz_time, (int)SEC_FROM_VLC_TICK(remaining))];
        } else {
            totalTime = toNSStr(secstotimestr(psz_time, (int)SEC_FROM_VLC_TICK(dur)));
        }
        [_remainingOrTotalTime setStringValue:totalTime];
    }

    /* Update current position (left field) */
    NSString *playbackPosition = toNSStr(secstotimestr(psz_time, (int)SEC_FROM_VLC_TICK(t)));

    [_elapsedTime setStringValue:playbackPosition];
    input_Release(p_input);
}

- (void)setSeekable:(BOOL)seekable
{
    // Workaround graphical issues in Mojave.
    // TODO: This needs a proper fix
    [_forwardButton setEnabled:NO];
    [_backwardButton setEnabled:NO];
    [_nextButton setEnabled:NO];
    [_nextButton setEnabled:YES];
    [_previousButton setEnabled:NO];
    [_previousButton setEnabled:YES];
    [_fullscreenButton setEnabled:NO];
    [_fullscreenButton setEnabled:YES];

    [_timeSlider setEnabled:seekable];
    [_forwardButton setEnabled:seekable];
    [_backwardButton setEnabled:seekable];
}

- (void)setVolumeLevel:(int)value
{
    [_volumeSlider setIntValue:value];
    [_volumeSlider setToolTip: [NSString stringWithFormat:_NS("Volume: %i %%"), (value*200)/AOUT_VOLUME_MAX]];
}

#pragma mark -
#pragma mark Window interactions

- (void)fadeIn
{
    if (!var_InheritBool(getIntf(), "macosx-fspanel"))
        return;

    [NSAnimationContext beginGrouping];
    [[NSAnimationContext currentContext] setDuration:0.4f];
    [[self.window animator] setAlphaValue:1.0f];
    [NSAnimationContext endGrouping];

    [self startAutohideTimer];
}

- (void)fadeOut
{
    [NSAnimationContext beginGrouping];
    [[NSAnimationContext currentContext] setDuration:0.4f];
    [[self.window animator] setAlphaValue:0.0f];
    [NSAnimationContext endGrouping];
}

- (void)centerPanel
{
    NSRect windowFrame = [self.window frame];
    windowFrame = [self contrainFrameToAssociatedVoutWindow:windowFrame];

    /* Calculate coordinates for centered position */
    NSRect limitFrame = _associatedVoutWindow.frame;
    windowFrame.origin.x = (limitFrame.size.width - windowFrame.size.width) / 2 + limitFrame.origin.x;
    windowFrame.origin.y = (limitFrame.size.height / 5) - windowFrame.size.height + limitFrame.origin.y;

    [self.window setFrame:windowFrame display:YES animate:NO];
}

- (NSRect)contrainFrameToAssociatedVoutWindow:(NSRect)frame
{
    NSRect limitFrame = _associatedVoutWindow.frame;

    // Limit rect to limitation view
    if (frame.origin.x < limitFrame.origin.x)
        frame.origin.x = limitFrame.origin.x;
    if (frame.origin.y < limitFrame.origin.y)
        frame.origin.y = limitFrame.origin.y;

    // Limit size (could be needed after resolution changes)
    if (frame.size.height > limitFrame.size.height)
        frame.size.height = limitFrame.size.height;
    if (frame.size.width > limitFrame.size.width)
        frame.size.width = limitFrame.size.width;

    if (frame.origin.x + frame.size.width > limitFrame.origin.x + limitFrame.size.width)
        frame.origin.x = limitFrame.origin.x + limitFrame.size.width - frame.size.width;
    if (frame.origin.y + frame.size.height > limitFrame.origin.y + limitFrame.size.height)
        frame.origin.y = limitFrame.origin.y + limitFrame.size.height - frame.size.height;

    return frame;
}

- (void)setNonActive
{
    [self.window orderOut:self];
}

- (void)setActive
{
    [self.window orderFront:self];
    [self fadeIn];
}

#pragma mark -
#pragma mark Misc interactions

- (void)hideMouse
{
    [NSCursor setHiddenUntilMouseMoves:YES];
}

- (void)setVoutWasUpdated:(VLCWindow *)voutWindow
{
    _associatedVoutWindow = voutWindow;

    NSRect voutRect = voutWindow.frame;
    if (!NSEqualRects(_associatedVoutFrame, voutRect)) {
        _associatedVoutFrame = voutRect;
        [[NSUserDefaults standardUserDefaults] setObject:NSStringFromRect(_associatedVoutFrame) forKey:kAssociatedFullscreenRect];

        [self centerPanel];
    }

}

#pragma mark -
#pragma mark Autohide timer management

- (void)startAutohideTimer
{
    /* Do nothing if timer is already in place */
    if (_isCounting)
        return;

    /* Get timeout and make sure it is not lower than 1 second */
    long long _timeToKeepVisibleInSec = MAX(var_CreateGetInteger(getIntf(), "mouse-hide-timeout") / 1000, 1);

    _hideTimer = [NSTimer scheduledTimerWithTimeInterval:_timeToKeepVisibleInSec
                                                  target:self
                                                selector:@selector(autohideCallback:)
                                                userInfo:nil
                                                 repeats:NO];
    _isCounting = YES;
}

- (void)stopAutohideTimer
{
    [_hideTimer invalidate];
    _isCounting = NO;
}

- (void)autohideCallback:(NSTimer *)timer
{
    if (!NSMouseInRect([NSEvent mouseLocation], [self.window frame], NO)) {
        [self fadeOut];
        [self hideMouse];
    }
    _isCounting = NO;
}

#pragma mark -
#pragma mark Helpers

/**
 Create an image mask for the NSVisualEffectView
 with rounded corners in the given rect

 This is necessary as clipping the VisualEffectView using the layers
 rounded corners is not possible when using the NSColor clearColor
 as background color.

 \note  The returned image will have the necessary \c capInsets and
        \c capResizingMode set.

 \param bounds  The rect for the image size
 */
- (NSImage *)maskImageWithBounds:(NSRect)bounds
{
    static const float radius = 8.0;
    NSImage *img = [NSImage imageWithSize:bounds.size flipped:YES drawingHandler:^BOOL(NSRect dstRect) {
        NSBezierPath *path = [NSBezierPath bezierPathWithRoundedRect:bounds xRadius:radius yRadius:radius];
        [[NSColor blackColor] setFill];
        [path fill];
        return YES;
    }];
    [img setCapInsets:NSEdgeInsetsMake(radius, radius, radius, radius)];
    [img setResizingMode:NSImageResizingModeStretch];
    return img;
}

/**
 Injects the visual effect view in the Windows view hierarchy

 This is necessary as we can't use the NSVisualEffect view on
 all macOS Versions and therefore need to dynamically insert it.

 */
- (void)injectVisualEffectView
{
    /* Setup the view */
    NSVisualEffectView *view = [[NSVisualEffectView alloc] initWithFrame:self.window.contentView.frame];
    [view setMaskImage:[self maskImageWithBounds:self.window.contentView.bounds]];
    [view setBlendingMode:NSVisualEffectBlendingModeBehindWindow];
    [view setMaterial:NSVisualEffectMaterialDark];
    [view setState:NSVisualEffectStateActive];
    [view setAutoresizesSubviews:YES];

    /* Inject view in view hierarchy */
    [self.window setContentView:view];
    [_controlsView setAppearance:[NSAppearance appearanceNamed:NSAppearanceNameVibrantDark]];
    [self.window.contentView addSubview:_controlsView];
}

- (void)dealloc
{
    [self stopAutohideTimer];
}


@end
