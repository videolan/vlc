/*****************************************************************************
 * VLCControlsBarCommon.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2012-2016 VLC authors and VideoLAN
 *
 * Authors: Felix Paul Kühne <fkuehne -at- videolan -dot- org>
 *          David Fuhrmann <david dot fuhrmann at googlemail dot com>
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

#import "VLCControlsBarCommon.h"

#import "coreinteraction/VLCCoreInteraction.h"
#import "extensions/NSString+Helpers.h"
#import "main/VLCMain.h"
#import "main/CompatibilityFixes.h"
#import "menus/VLCMainMenu.h"
#import "playlist/VLCPlaylistController.h"
#import "playlist/VLCPlayerController.h"

/*****************************************************************************
 * VLCControlsBarCommon
 *
 *  Holds all outlets, actions and code common for controls bar in detached
 *  and in main window.
 *****************************************************************************/

@interface VLCControlsBarCommon ()
{
    NSImage * _pauseImage;
    NSImage * _pressedPauseImage;
    NSImage * _playImage;
    NSImage * _pressedPlayImage;

    NSTimeInterval last_fwd_event;
    NSTimeInterval last_bwd_event;
    BOOL just_triggered_next;
    BOOL just_triggered_previous;
}
@end

@implementation VLCControlsBarCommon

- (void)awakeFromNib
{
    [super awakeFromNib];

    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(updateTimeSlider:) name:VLCPlayerTimeAndPositionChanged object:nil];

    _nativeFullscreenMode = var_InheritBool(getIntf(), "macosx-nativefullscreenmode");

    [self.dropView setDrawBorder: NO];

    [self.playButton setToolTip: _NS("Play")];
    self.playButton.accessibilityLabel = self.playButton.toolTip;

    [self.backwardButton setToolTip: _NS("Backward")];
    self.backwardButton.accessibilityLabel = _NS("Seek backward");
    self.backwardButton.accessibilityTitle = self.backwardButton.toolTip;

    [self.forwardButton setToolTip: _NS("Forward")];
    self.forwardButton.accessibilityLabel = _NS("Seek forward");
    self.forwardButton.accessibilityTitle = self.forwardButton.toolTip;

    [self.timeSlider setToolTip: _NS("Position")];
    self.timeSlider.accessibilityLabel = _NS("Playback position");
    self.timeSlider.accessibilityTitle = self.timeSlider.toolTip;

    [self.fullscreenButton setToolTip: _NS("Enter fullscreen")];
    self.fullscreenButton.accessibilityLabel = self.fullscreenButton.toolTip;

    [self.backwardButton setImage: imageFromRes(@"backward-3btns")];
    [self.backwardButton setAlternateImage: imageFromRes(@"backward-3btns-pressed")];
    _playImage = imageFromRes(@"play");
    _pressedPlayImage = imageFromRes(@"play-pressed");
    _pauseImage = imageFromRes(@"pause");
    _pressedPauseImage = imageFromRes(@"pause-pressed");
    [self.forwardButton setImage: imageFromRes(@"forward-3btns")];
    [self.forwardButton setAlternateImage: imageFromRes(@"forward-3btns-pressed")];

    [self.fullscreenButton setImage: imageFromRes(@"fullscreen-one-button")];
    [self.fullscreenButton setAlternateImage: imageFromRes(@"fullscreen-one-button-pressed")];

    [self.playButton setImage: _playImage];
    [self.playButton setAlternateImage: _pressedPlayImage];

    NSColor *timeFieldTextColor = [NSColor colorWithCalibratedRed:0.64 green:0.64 blue:0.64 alpha:100.0];
    [self.timeField setTextColor: timeFieldTextColor];
    [self.timeField setFont:[NSFont titleBarFontOfSize:10.0]];
    [self.timeField setAlignment: NSCenterTextAlignment];
    [self.timeField setNeedsDisplay:YES];
    [self.timeField setRemainingIdentifier:@"DisplayTimeAsTimeRemaining"];
    self.timeField.accessibilityLabel = _NS("Playback time");

    // remove fullscreen button for lion fullscreen
    if (_nativeFullscreenMode) {
        self.fullscreenButtonWidthConstraint.constant = 0;
    }

    if (config_GetInt("macosx-show-playback-buttons"))
        [self toggleForwardBackwardMode: YES];
}

- (void)dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver:self];
}

- (CGFloat)height
{
    return [self.bottomBarView frame].size.height;
}

- (void)toggleForwardBackwardMode:(BOOL)b_alt
{
    if (b_alt == YES) {
        /* change the accessibility help for the backward/forward buttons accordingly */
        self.backwardButton.accessibilityTitle = _NS("Backward");
        self.backwardButton.accessibilityLabel = _NS("Seek backward");

        self.forwardButton.accessibilityTitle = _NS("Forward");
        self.forwardButton.accessibilityLabel = _NS("Seek forward");

        [self.forwardButton setAction:@selector(alternateForward:)];
        [self.backwardButton setAction:@selector(alternateBackward:)];

    } else {
        /* change the accessibility help for the backward/forward buttons accordingly */
        self.backwardButton.accessibilityTitle = _NS("Previous");
        self.backwardButton.accessibilityLabel = _NS("Go to previous item");

        self.forwardButton.accessibilityTitle = _NS("Next");
        self.forwardButton.accessibilityLabel = _NS("Go to next item");

        [self.forwardButton setAction:@selector(fwd:)];
        [self.backwardButton setAction:@selector(bwd:)];
    }
}

#pragma mark -
#pragma mark Button Actions

- (IBAction)play:(id)sender
{
    [[VLCCoreInteraction sharedInstance] playOrPause];
}

- (void)resetPreviousButton
{
    if (([NSDate timeIntervalSinceReferenceDate] - last_bwd_event) >= 0.35) {
        // seems like no further event occurred, so let's switch the playback item
        [[VLCCoreInteraction sharedInstance] previous];
        just_triggered_previous = NO;
    }
}

- (void)resetBackwardSkip
{
    // the user stopped skipping, so let's allow him to change the item
    if (([NSDate timeIntervalSinceReferenceDate] - last_bwd_event) >= 0.35)
        just_triggered_previous = NO;
}

- (IBAction)bwd:(id)sender
{
    if (!just_triggered_previous) {
        just_triggered_previous = YES;
        [self performSelector:@selector(resetPreviousButton)
                   withObject: NULL
                   afterDelay:0.40];
    } else {
        if (([NSDate timeIntervalSinceReferenceDate] - last_fwd_event) > 0.16) {
            // we just skipped 4 "continous" events, otherwise we are too fast
            [[VLCCoreInteraction sharedInstance] backwardExtraShort];
            last_bwd_event = [NSDate timeIntervalSinceReferenceDate];
            [self performSelector:@selector(resetBackwardSkip)
                       withObject: NULL
                       afterDelay:0.40];
        }
    }
}

- (void)resetNextButton
{
    if (([NSDate timeIntervalSinceReferenceDate] - last_fwd_event) >= 0.35) {
        // seems like no further event occurred, so let's switch the playback item
        [[VLCCoreInteraction sharedInstance] next];
        just_triggered_next = NO;
    }
}

- (void)resetForwardSkip
{
    // the user stopped skipping, so let's allow him to change the item
    if (([NSDate timeIntervalSinceReferenceDate] - last_fwd_event) >= 0.35)
        just_triggered_next = NO;
}

- (IBAction)fwd:(id)sender
{
    if (!just_triggered_next) {
        just_triggered_next = YES;
        [self performSelector:@selector(resetNextButton)
                   withObject: NULL
                   afterDelay:0.40];
    } else {
        if (([NSDate timeIntervalSinceReferenceDate] - last_fwd_event) > 0.16) {
            // we just skipped 4 "continous" events, otherwise we are too fast
            [[VLCCoreInteraction sharedInstance] forwardExtraShort];
            last_fwd_event = [NSDate timeIntervalSinceReferenceDate];
            [self performSelector:@selector(resetForwardSkip)
                       withObject: NULL
                       afterDelay:0.40];
        }
    }
}

// alternative actions for forward / backward buttons when next / prev are activated
- (IBAction)alternateForward:(id)sender
{
    [[VLCCoreInteraction sharedInstance] forwardExtraShort];
}

- (IBAction)alternateBackward:(id)sender
{
    [[VLCCoreInteraction sharedInstance] backwardExtraShort];
}

- (IBAction)timeSliderAction:(id)sender
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
            f_updatedDelta = [sender floatValue];
            break;
        case NSScrollWheel:
            f_updatedDelta = [sender floatValue];
            break;

        default:
            return;
    }

    VLCPlayerController *playerController = [[[VLCMain sharedInstance] playlistController] playerController];
    [playerController setPositionFast:f_updatedDelta / 10000.];
    [self.timeSlider setFloatValue:f_updatedDelta];
}

- (IBAction)fullscreen:(id)sender
{
    [[VLCCoreInteraction sharedInstance] toggleFullscreen];
}

#pragma mark -
#pragma mark Updaters

- (void)updateTimeSlider:(NSNotification *)aNotification;
{
    VLCPlayerController *playerController = aNotification.object;
    input_item_t *p_item = playerController.currentMedia;

    [self.timeSlider setHidden:NO];

    if (!p_item) {
        // Nothing playing
        [self.timeSlider setKnobHidden:YES];
        [self.timeSlider setFloatValue: 0.0];
        [self.timeField setStringValue: @"00:00"];
        [self.timeSlider setIndefinite:NO];
        [self.timeSlider setEnabled:NO];
        return;
    }

    [self.timeSlider setKnobHidden:NO];
    [self.timeSlider setFloatValue:(10000. * playerController.position)];

    vlc_tick_t duration = input_item_GetDuration(p_item);
    bool buffering = playerController.playerState == VLC_PLAYER_STATE_STARTED;
    if (duration == -1) {
        // No duration, disable slider
        [self.timeSlider setEnabled:NO];
    } else if (buffering) {
        [self.timeSlider setEnabled:NO];
        [self.timeSlider setIndefinite:buffering];
    } else {
        [self.timeSlider setEnabled:playerController.seekable];
    }

    NSString *time = [NSString stringWithDuration:duration
                                      currentTime:playerController.time
                                         negative:self.timeField.timeRemaining];
    [self.timeField setStringValue:time];
    [self.timeField setNeedsDisplay:YES];
}

- (void)updateControls
{
    bool b_plmul = false;
    bool b_seekable = false;
    bool b_chapters = false;

    playlist_t * p_playlist = pl_Get(getIntf());

    PL_LOCK;
    b_plmul = playlist_CurrentSize(p_playlist) > 1;
    PL_UNLOCK;

    input_thread_t * p_input = playlist_CurrentInput(p_playlist);

    if (p_input) {
        /* seekable streams */
        b_seekable = var_GetBool(p_input, "can-seek");

        /* chapters & titles */
        //FIXME! b_chapters = p_input->stream.i_area_nb > 1;

        vlc_object_release(p_input);
    }

    [self.timeSlider setEnabled: b_seekable];

    [self.forwardButton setEnabled: (b_seekable || b_plmul || b_chapters)];
    [self.backwardButton setEnabled: (b_seekable || b_plmul || b_chapters)];
}

- (void)setPause
{
    [self.playButton setImage: _pauseImage];
    [self.playButton setAlternateImage: _pressedPauseImage];
    [self.playButton setToolTip: _NS("Pause")];
    self.playButton.accessibilityLabel = self.playButton.toolTip;
}

- (void)setPlay
{
    [self.playButton setImage: _playImage];
    [self.playButton setAlternateImage: _pressedPlayImage];
    [self.playButton setToolTip: _NS("Play")];
    self.playButton.accessibilityLabel = self.playButton.toolTip;
}

- (void)setFullscreenState:(BOOL)b_fullscreen
{
    if (!self.nativeFullscreenMode)
        [self.fullscreenButton setState:b_fullscreen];
}

@end
