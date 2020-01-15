/*****************************************************************************
 * VLCFSPanelController.m: macOS fullscreen controls window controller
 *****************************************************************************
 * Copyright (C) 2006-2019 VLC authors and VideoLAN
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

#import "extensions/NSString+Helpers.h"
#import "main/VLCMain.h"
#import "playlist/VLCPlaylistController.h"
#import "playlist/VLCPlayerController.h"
#import "windows/video/VLCVideoWindowCommon.h"
#import "windows/video/VLCWindow.h"
#import "views/VLCDefaultValueSlider.h"
#import "views/VLCTimeField.h"
#import "views/VLCSlider.h"
#import "library/VLCInputItem.h"

NSString *VLCFSPanelShouldBecomeActive = @"VLCFSPanelShouldBecomeActive";
NSString *VLCFSPanelShouldBecomeInactive = @"VLCFSPanelShouldBecomeInactive";

@interface VLCFSPanelController ()
{
    BOOL _isCounting;
    BOOL _isFadingIn;

    // Only used to track changes and trigger centering of FS panel
    NSRect _associatedVoutFrame;
    // Used to ask for current constraining rect on movement
    NSWindow *_associatedVoutWindow;

    VLCPlaylistController *_playlistController;
    VLCPlayerController *_playerController;
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

- (void)dealloc
{
    [self stopAutohideTimer];
    [[NSNotificationCenter defaultCenter] removeObserver:self];
}

- (void)windowDidLoad
{
    _playlistController = [[VLCMain sharedInstance] playlistController];
    _playerController = [_playlistController playerController];

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

    NSNotificationCenter *notificationCenter = [NSNotificationCenter defaultCenter];
    [notificationCenter addObserver:self selector:@selector(playbackStateChanged:) name:VLCPlayerStateChanged object:nil];
    [notificationCenter addObserver:self selector:@selector(updatePositionAndTime:) name:VLCPlayerTimeAndPositionChanged object:nil];
    [notificationCenter addObserver:self selector:@selector(capabilitiesChanged:) name:VLCPlayerCapabilitiesChanged object:nil];
    [notificationCenter addObserver:self selector:@selector(hasPreviousChanged:) name:VLCPlaybackHasPreviousChanged object:nil];
    [notificationCenter addObserver:self selector:@selector(hasNextChanged:) name:VLCPlaybackHasNextChanged object:nil];
    [notificationCenter addObserver:self selector:@selector(volumeChanged:) name:VLCPlayerVolumeChanged object:nil];
    [notificationCenter addObserver:self selector:@selector(inputItemChanged:) name:VLCPlayerCurrentMediaItemChanged object:nil];
    [notificationCenter addObserver:self selector:@selector(shouldBecomeActive:) name:VLCFSPanelShouldBecomeActive object:nil];
    [notificationCenter addObserver:self selector:@selector(shouldBecomeInactive:) name:VLCFSPanelShouldBecomeInactive object:nil];
    [notificationCenter addObserver:self selector:@selector(voutWasUpdated:) name:VLCVideoWindowDidEnterFullscreen object:nil];
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
    [_volumeSlider setMaxValue:VLCVolumeMaximum];
    [_volumeSlider setFloatValue:_playerController.volume];
    [_volumeSlider setDefaultValue:VLCVolumeDefault];

    /* Identifier to store the state of the remaining or total time label,
     * this is the same identifier as used for the window playback cotrols
     * so the state is shared between those.
     */
    [_remainingOrTotalTime setRemainingIdentifier:VLCTimeFieldDisplayTimeAsRemaining];

    [self inputItemChanged:nil];
}

#undef setupButton

#pragma mark -
#pragma mark Control Actions

- (IBAction)togglePlayPause:(id)sender
{
    [_playerController togglePlayPause];
}

- (IBAction)jumpForward:(id)sender
{
    static NSTimeInterval last_event = 0;
    if (([NSDate timeIntervalSinceReferenceDate] - last_event) > 0.16) {
        /* We just skipped 4 "continuous" events, otherwise we are too fast */
        [_playerController jumpForwardExtraShort];
        last_event = [NSDate timeIntervalSinceReferenceDate];
    }
}

- (IBAction)jumpBackward:(id)sender
{
    static NSTimeInterval last_event = 0;
    if (([NSDate timeIntervalSinceReferenceDate] - last_event) > 0.16) {
        /* We just skipped 4 "continuous" events, otherwise we are too fast */
        [_playerController jumpBackwardExtraShort];
        last_event = [NSDate timeIntervalSinceReferenceDate];
    }
}

- (IBAction)gotoPrevious:(id)sender
{
    [_playlistController playPreviousItem];
}

- (IBAction)gotoNext:(id)sender
{
    [_playlistController playNextItem];
}

- (IBAction)toggleFullscreen:(id)sender
{
    [_playerController toggleFullscreen];
}

- (IBAction)timeSliderUpdate:(id)sender
{
    float f_updatedDelta;

    switch([[NSApp currentEvent] type]) {
        case NSLeftMouseUp:
            /* Ignore mouse up, as this is a continous slider and
             * when the user does a single click to a position on the slider,
             * the action is called twice, once for the mouse down and once
             * for the mouse up event. This results in two short seeks one
             * after another to the same position, which results in weird
             * audio quirks.
             */
            return;
        case NSLeftMouseDown:
        case NSLeftMouseDragged:
            f_updatedDelta = [_timeSlider floatValue];
            break;
        case NSScrollWheel:
            f_updatedDelta = [_timeSlider floatValue];
            break;

        default:
            return;
    }

    [_playerController setPositionFast:f_updatedDelta / 10000.];
}

- (IBAction)volumeSliderUpdate:(id)sender
{
    _playerController.volume = [sender floatValue];
}

#pragma mark -
#pragma mark Metadata and state updates

- (void)playbackStateChanged:(NSNotification *)aNotification
{
    if (_playerController.playerState == VLC_PLAYER_STATE_PLAYING) {
        [self setPause];
    } else {
        [self setPlay];
    }
}

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

- (void)inputItemChanged:(NSNotification *)aNotification
{
    NSString *title;
    NSString *nowPlaying;
    VLCInputItem *inputItem = _playerController.currentMedia;

    if (inputItem) {
        /* Something is playing */
        title = inputItem.title;
        nowPlaying = inputItem.nowPlaying;
    } else {
        /* Nothing playing */
        title = _NS("VLC media player");
    }

    if (nowPlaying.length > 0) {
        [_mediaTitle setStringValue:nowPlaying];
    } else {
        [_mediaTitle setStringValue:title];
    }
}

- (void)updatePositionAndTime:(NSNotification *)aNotification
{
    VLCPlayerController *playerController = aNotification.object;
    VLCInputItem *inputItem = playerController.currentMedia;

    /* If nothing is playing, reset times and slider */
    if (!inputItem) {
        [_timeSlider setFloatValue:0.0];
        [_elapsedTime setStringValue:@""];
        [_remainingOrTotalTime setHidden:YES];
        return;
    }

    [_timeSlider setFloatValue:(10000. * playerController.position)];

    vlc_tick_t time =_playerController.time;
    vlc_tick_t duration = inputItem.duration;

    bool buffering = playerController.playerState == VLC_PLAYER_STATE_STARTED;
    if (duration == -1) {
        // No duration, disable slider
        [_timeSlider setEnabled:NO];
    } else if (buffering) {
        [_timeSlider setEnabled:NO];
        [_timeSlider setIndefinite:buffering];
    } else {
        [_timeSlider setEnabled:playerController.seekable];
    }

    /* Update total duration (right field) */
    NSString *timeString = [NSString stringWithDuration:duration
                                            currentTime:time
                                               negative:_remainingOrTotalTime.timeRemaining];
    [_remainingOrTotalTime setStringValue:timeString];
    [_remainingOrTotalTime setNeedsDisplay:YES];
    [_remainingOrTotalTime setHidden:duration <= 0];

    /* Update current position (left field) */
    char psz_time[MSTRTIME_MAX_SIZE];
    NSString *playbackPosition = toNSStr(secstotimestr(psz_time, (int)SEC_FROM_VLC_TICK(time)));

    [_elapsedTime setStringValue:playbackPosition];
}

- (void)capabilitiesChanged:(NSNotification *)aNotification
{
    [self setSeekable:_playerController.seekable];
}

- (void)setSeekable:(BOOL)seekable
{
    // Workaround graphical issues in Mojave.
    // TODO: This needs a proper fix
    [_forwardButton setEnabled:NO];
    [_backwardButton setEnabled:NO];
    [_fullscreenButton setEnabled:NO];
    [_fullscreenButton setEnabled:YES];

    [_timeSlider setEnabled:seekable];
    [_forwardButton setEnabled:seekable];
    [_backwardButton setEnabled:seekable];
}

- (void)hasPreviousChanged:(NSNotification *)aNotification
{
    [_previousButton setEnabled:NO];
    [_previousButton setEnabled:YES];
    [_previousButton setEnabled:_playlistController.hasPreviousPlaylistItem];
}

- (void)hasNextChanged:(NSNotification *)aNotification
{
    [_nextButton setEnabled:NO];
    [_nextButton setEnabled:YES];
    [_nextButton setEnabled:_playlistController.hasNextPlaylistItem];
}

- (void)volumeChanged:(NSNotification *)aNotification
{
    float volume = _playerController.volume;
    [_volumeSlider setFloatValue:volume];
    [_volumeSlider setToolTip: [NSString stringWithFormat:_NS("Volume: %i %%"), volume * 100.]];
}

#pragma mark -
#pragma mark Window interactions

- (void)fadeIn
{
    if (!var_InheritBool(getIntf(), "macosx-fspanel"))
        return;

    if (_isFadingIn)
        return;

    [self stopAutohideTimer];
    [NSAnimationContext runAnimationGroup:^(NSAnimationContext * _Nonnull context) {
        _isFadingIn = YES;
        [context setDuration:0.4f];
        [[self.window animator] setAlphaValue:1.0f];
    } completionHandler:^{
        _isFadingIn = NO;
        [self startAutohideTimer];
    }];
}

- (void)fadeOut
{
    [NSAnimationContext runAnimationGroup:^(NSAnimationContext * _Nonnull context) {
        [context setDuration:0.4f];
        [[self.window animator] setAlphaValue:0.0f];
    } completionHandler:nil];
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

- (void)shouldBecomeInactive:(NSNotification *)aNotification
{
    [self.window orderOut:self];
}

- (void)shouldBecomeActive:(NSNotification *)aNotification
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

- (void)voutWasUpdated:(NSNotification *)aNotification
{
    VLCWindow *voutWindow = aNotification.object;
    _associatedVoutWindow = voutWindow;

    NSRect voutRect = voutWindow.frame;

    // In some cases, the FSPanel frame has moved outside of the
    // vout view --> Also re-center in this case
    NSRect currentFrame = [self.window frame];
    NSRect constrainedFrame = [self contrainFrameToAssociatedVoutWindow: currentFrame];

    if (!NSEqualRects(_associatedVoutFrame, voutRect) ||
        !NSEqualRects(currentFrame, constrainedFrame)) {
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

@end
