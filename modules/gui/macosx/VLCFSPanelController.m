/*****************************************************************************
 * VLCFSPanelController.m: macOS fullscreen controls window controller
 *****************************************************************************
 * Copyright (C) 2006-2016 VLC authors and VideoLAN
 * $Id$
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
#import "VLCCoreInteraction.h"
#import "CompatibilityFixes.h"
#import "VLCMain.h"

@interface VLCFSPanelController () {
    BOOL _isCounting;
    CGDirectDisplayID _displayID;
}

@end

@implementation VLCFSPanelController

#pragma mark -
#pragma mark Initialization

- (id)init
{
    self = [super initWithWindowNibName:@"VLCFullScreenPanel"];
    return self;
}

- (void)windowDidLoad
{
    [super windowDidLoad];

    /* Do some window setup that is not possible in IB */
    [self.window setOpaque:NO];
    [self.window setAlphaValue:0.0f];
    [self.window setMovableByWindowBackground:YES];
    [self.window setLevel:NSModalPanelWindowLevel];
    [self.window setStyleMask:self.window.styleMask | NSResizableWindowMask];
    [self.window setBackgroundColor:[NSColor clearColor]];

#ifdef MAC_OS_X_VERSION_10_10
    /* Inject correct background view depending on OS support */
    if (OSX_YOSEMITE_OR_HIGHER) {
        [self injectVisualEffectView];
    } else {
        [self injectBackgroundView];
    }
#else
    /* Compiled with old SDK, always use legacy style */
    [self injectBackgroundView];
#endif

    /* TODO: Write custom Image-only button subclass to behave properly */
    [(NSButtonCell*)[_playPauseButton cell] setHighlightsBy:NSPushInCellMask];
    [(NSButtonCell*)[_playPauseButton cell] setShowsStateBy:NSContentsCellMask];

    [self setupControls];
}

#define setupButton(target, title, desc)                                              \
    [target accessibilitySetOverrideValue:title                                       \
                             forAttribute:NSAccessibilityTitleAttribute];             \
    [target accessibilitySetOverrideValue:desc                                        \
                             forAttribute:NSAccessibilityDescriptionAttribute];       \
    [target setToolTip:desc];

- (void)setupControls
{
    /* Setup translations for buttons */
    setupButton(_playPauseButton,
                _NS("Play/Pause"),
                _NS("Click to play or pause the current media."));
    setupButton(_nextButton,
                _NS("Next"),
                _NS("Click to go to the next playlist item."));
    setupButton(_previousButton,
                _NS("Previous"),
                _NS("Click to go to the previous playlist item."));
    setupButton(_forwardButton,
                _NS("Forward"),
                _NS("Click and hold to skip forward through the current media."));
    setupButton(_backwardButton,
                _NS("Backward"),
                _NS("Click and hold to skip backward through the current media."));
    setupButton(_fullscreenButton,
                _NS("Toggle Fullscreen mode"),
                _NS("Click to exit fullscreen playback."));
    setupButton(_volumeSlider,
                _NS("Volume"),
                _NS("Drag to adjust the volume."));
    setupButton(_timeSlider,
                _NS("Position"),
                _NS("Drag to adjust the current playback position."));

    /* Setup other controls */
    [_volumeSlider setMaxValue:[[VLCCoreInteraction sharedInstance] maxVolume]];
    [_volumeSlider setIntValue:AOUT_VOLUME_DEFAULT];
}

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
        /* We just skipped 4 "continous" events, otherwise we are too fast */
        [[VLCCoreInteraction sharedInstance] forwardExtraShort];
        last_event = [NSDate timeIntervalSinceReferenceDate];
    }
}

- (IBAction)jumpBackward:(id)sender
{
    static NSTimeInterval last_event = 0;
    if (([NSDate timeIntervalSinceReferenceDate] - last_event) > 0.16) {
        /* We just skipped 4 "continous" events, otherwise we are too fast */
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
    input_thread_t *p_input;
    p_input = pl_CurrentInput(getIntf());

    if (p_input) {
        vlc_value_t pos;
        pos.f_float = [_timeSlider floatValue] / 10000.;
        var_Set(p_input, "position", pos);
        vlc_object_release(p_input);
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
}

- (void)setPause
{
    [_playPauseButton setState:NSOnState];
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


    int64_t t = var_GetInteger(p_input, "time");
    mtime_t dur = input_item_GetDuration(input_GetItem(p_input));

    /* Update total duration (right field) */
    if (dur <= 0) {
        [_remainingOrTotalTime setHidden:YES];
    } else {
        [_remainingOrTotalTime setHidden:NO];

        NSString *totalTime;

        if ([_remainingOrTotalTime timeRemaining]) {
            mtime_t remaining = 0;
            if (dur > t)
                remaining = dur - t;
            totalTime = [NSString stringWithFormat:@"-%s", secstotimestr(psz_time, (remaining / 1000000))];
        } else {
            totalTime = toNSStr(secstotimestr(psz_time, (dur / 1000000)));
        }
        [_remainingOrTotalTime setStringValue:totalTime];
    }

    /* Update current position (left field) */
    NSString *playbackPosition = toNSStr(secstotimestr(psz_time, t / CLOCK_FREQ));

    [_elapsedTime setStringValue:playbackPosition];
    vlc_object_release(p_input);
}

- (void)setSeekable:(BOOL)seekable
{
    [_timeSlider setEnabled:seekable];
    [_forwardButton setEnabled:seekable];
    [_backwardButton setEnabled:seekable];
}

- (void)setVolumeLevel:(int)value
{
    [_volumeSlider setIntValue:value];
}

#pragma mark -
#pragma mark Window interactions

- (void)fadeIn
{
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

- (void)centerWindowOnScreen:(CGDirectDisplayID)screenID
{
    /* Find screen by its ID */
    NSScreen *screen = [NSScreen screenWithDisplayID:screenID];

    /* Check screen validity, fallback to mainScreen */
    if (!screen)
        screen = [NSScreen mainScreen];

    NSRect screenFrame = [screen frame];
    NSRect windowFrame = [self.window frame];

    /* Calculate coordinates for new NSWindow position */
    NSPoint coordinates;
    coordinates.x = (screenFrame.size.width - windowFrame.size.width) / 2 + screenFrame.origin.x;
    coordinates.y = (screenFrame.size.height / 3) - windowFrame.size.height + screenFrame.origin.y;

    [self.window setFrameTopLeftPoint:coordinates];
}

- (void)center
{
    [self centerWindowOnScreen:_displayID];
}

- (void)setNonActive
{
    [self.window orderOut:self];
}

- (void)setActive
{
    [self.window orderFront:self];
}

#pragma mark -
#pragma mark Misc interactions

- (void)hideMouse
{
    [NSCursor setHiddenUntilMouseMoves:YES];
}

- (void)setVoutWasUpdated:(VLCWindow *)voutWindow
{
    _voutWindow = voutWindow;
    int newDisplayID = [[self.window screen] displayID];

    if (_displayID != newDisplayID) {
        _displayID = newDisplayID;
        [self center];
    }
}

#pragma mark -
#pragma mark Autohide timer management

- (void)startAutohideTimer
{
    /* Do nothing if timer is already in place */
    if (_isCounting)
        return;

    int _timeToKeepVisibleInSec = var_CreateGetInteger(getIntf(), "mouse-hide-timeout") / 1000;
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

#ifdef MAC_OS_X_VERSION_10_10
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
 
 \warning Never call both, \c injectVisualEffectView and \c injectBackgroundView
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
#endif

/**
 Injects the standard background view in the Windows view hierarchy
 
 This is necessary on macOS versions that do not support the
 NSVisualEffectView that usually is injected.
 
 \warning Never call both, \c injectVisualEffectView and \c injectBackgroundView
 */
- (void)injectBackgroundView
{
    /* Setup the view */
    CGColorRef color = CGColorCreateGenericGray(0.0, 0.8);
    NSView *view = [[NSView alloc] initWithFrame:self.window.contentView.frame];
    [view setWantsLayer:YES];
    [view.layer setBackgroundColor:color];
    [view.layer setCornerRadius:8.0];
    [view setAutoresizesSubviews:YES];
    CGColorRelease(color);

    /* Inject view in view hierarchy */
    [self.window setContentView:view];
    [self.window.contentView addSubview:_controlsView];

    /* Disable adjusting height to workaround autolayout problems */
    [_heightMaxConstraint setConstant:42.0];
    [self.window setMaxSize:NSMakeSize(4068, 80)];
    [self.window setMinSize:NSMakeSize(480, 80)];
}

- (void)dealloc
{
    [self stopAutohideTimer];
}


@end
