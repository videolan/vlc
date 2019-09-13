/*****************************************************************************
 * VLCControlsBarCommon.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2012-2019 VLC authors and VideoLAN
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

#import "extensions/NSString+Helpers.h"
#import "main/VLCMain.h"
#import "playlist/VLCPlaylistController.h"
#import "playlist/VLCPlayerController.h"
#import "library/VLCInputItem.h"

#import "views/VLCBottomBarView.h"
#import "views/VLCDragDropView.h"
#import "views/VLCTimeField.h"
#import "views/VLCSlider.h"

/*****************************************************************************
 * VLCControlsBarCommon
 *
 *  Holds all outlets, actions and code common for controls bar in detached
 *  and in main window.
 *****************************************************************************/

@interface VLCControlsBarCommon ()
{
    NSImage *_pauseImage;
    NSImage *_pressedPauseImage;
    NSImage *_playImage;
    NSImage *_pressedPlayImage;

    NSTimeInterval last_fwd_event;
    NSTimeInterval last_bwd_event;
    BOOL just_triggered_next;
    BOOL just_triggered_previous;

    VLCPlaylistController *_playlistController;
    VLCPlayerController *_playerController;
}
@end

@implementation VLCControlsBarCommon

- (void)awakeFromNib
{
    [super awakeFromNib];

    _playlistController = [[VLCMain sharedInstance] playlistController];
    _playerController = _playlistController.playerController;

    NSNotificationCenter *notificationCenter = [NSNotificationCenter defaultCenter];
    [notificationCenter addObserver:self selector:@selector(updateTimeSlider:) name:VLCPlayerTimeAndPositionChanged object:nil];
    [notificationCenter addObserver:self selector:@selector(playerStateUpdated:) name:VLCPlayerStateChanged object:nil];
    [notificationCenter addObserver:self selector:@selector(updatePlaybackControls:) name:VLCPlaylistCurrentItemChanged object:nil];
    [notificationCenter addObserver:self selector:@selector(fullscreenStateUpdated:) name:VLCPlayerFullscreenChanged object:nil];

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

    [self.timeSlider setHidden:NO];
    [self updateTimeSlider:nil];

    NSColor *timeFieldTextColor = [NSColor controlTextColor];
    [self.timeField setTextColor: timeFieldTextColor];
    [self.timeField setFont:[NSFont titleBarFontOfSize:10.0]];
    [self.timeField setAlignment: NSCenterTextAlignment];
    [self.timeField setNeedsDisplay:YES];
    [self.timeField setRemainingIdentifier:VLCTimeFieldDisplayTimeAsRemaining];
    self.timeField.accessibilityLabel = _NS("Playback time");

    // remove fullscreen button for lion fullscreen
    if (_nativeFullscreenMode) {
        self.fullscreenButtonWidthConstraint.constant = 0;
    }

    self.backwardButton.accessibilityTitle = _NS("Previous");
    self.backwardButton.accessibilityLabel = _NS("Go to previous item");

    self.forwardButton.accessibilityTitle = _NS("Next");
    self.forwardButton.accessibilityLabel = _NS("Go to next item");

    [self.forwardButton setAction:@selector(fwd:)];
    [self.backwardButton setAction:@selector(bwd:)];

    [self playerStateUpdated:nil];
}

- (void)dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver:self];
}

- (CGFloat)height
{
    return [self.bottomBarView frame].size.height;
}

#pragma mark -
#pragma mark Button Actions

- (IBAction)play:(id)sender
{
    [_playerController togglePlayPause];
}

- (void)resetPreviousButton
{
    if (([NSDate timeIntervalSinceReferenceDate] - last_bwd_event) >= 0.35) {
        // seems like no further event occurred, so let's switch the playback item
        [_playlistController playPreviousItem];
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
            [_playerController jumpBackwardExtraShort];
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
        [_playlistController playNextItem];
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
            [_playerController jumpForwardExtraShort];
            last_fwd_event = [NSDate timeIntervalSinceReferenceDate];
            [self performSelector:@selector(resetForwardSkip)
                       withObject: NULL
                       afterDelay:0.40];
        }
    }
}

- (IBAction)timeSliderAction:(id)sender
{
    float newPosition;
    NSEvent *theEvent = [NSApp currentEvent];
    NSEventType theEventType = [theEvent type];

    switch (theEventType) {
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
            newPosition = [sender floatValue];
            break;
        case NSScrollWheel:
            newPosition = [sender floatValue];
            break;

        default:
            return;
    }

    [_playerController setPositionFast:newPosition];
    [self.timeSlider setFloatValue:newPosition];
}

- (IBAction)fullscreen:(id)sender
{
    [_playerController toggleFullscreen];
}

#pragma mark -
#pragma mark Updaters

- (void)updateTimeSlider:(NSNotification *)aNotification;
{
    VLCInputItem *inputItem = _playerController.currentMedia;

    if (!inputItem) {
        // Nothing playing
        [self.timeSlider setKnobHidden:YES];
        [self.timeSlider setFloatValue: 0.0];
        [self.timeField setStringValue: @"00:00"];
        [self.timeSlider setIndefinite:NO];
        [self.timeSlider setEnabled:NO];
        return;
    }

    [self.timeSlider setKnobHidden:NO];
    [self.timeSlider setFloatValue:_playerController.position];

    vlc_tick_t duration = inputItem.duration;
    bool buffering = _playerController.playerState == VLC_PLAYER_STATE_STARTED;
    if (duration == -1) {
        // No duration, disable slider
        [self.timeSlider setEnabled:NO];
    } else if (buffering) {
        [self.timeSlider setEnabled:NO];
        [self.timeSlider setIndefinite:buffering];
    } else {
        [self.timeSlider setEnabled:_playerController.seekable];
    }

    NSString *time = [NSString stringWithDuration:duration
                                      currentTime:_playerController.time
                                         negative:self.timeField.timeRemaining];
    [self.timeField setStringValue:time];
    [self.timeField setNeedsDisplay:YES];
}

- (void)playerStateUpdated:(NSNotification *)aNotification
{
    if (_playerController.playerState == VLC_PLAYER_STATE_PLAYING) {
        [self setPause];
    } else {
        [self setPlay];
    }
}

- (void)updatePlaybackControls:(NSNotification *)aNotification
{
    bool b_seekable = _playerController.seekable;
    bool b_chapters = [_playerController numberOfChaptersForCurrentTitle] > 0;

    [self.timeSlider setEnabled: b_seekable];

    [self.forwardButton setEnabled: (b_seekable || _playlistController.hasNextPlaylistItem || b_chapters)];
    [self.backwardButton setEnabled: (b_seekable || _playlistController.hasPreviousPlaylistItem || b_chapters)];
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

- (void)fullscreenStateUpdated:(NSNotification *)aNotification
{
    if (!self.nativeFullscreenMode) {
        [self.fullscreenButton setState:_playerController.fullscreen];
    }
}

@end
