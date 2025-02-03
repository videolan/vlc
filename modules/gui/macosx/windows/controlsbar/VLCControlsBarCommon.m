/*****************************************************************************
 * VLCControlsBarCommon.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2024 VLC authors and VideoLAN
 *
 * Authors: Felix Paul Kühne <fkuehne -at- videolan -dot- org>
 *          David Fuhrmann <david dot fuhrmann at googlemail dot com>
 *          Maxime Chapelet <umxprime at videolabs dot io>
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
#import "playqueue/VLCPlayQueueController.h"
#import "playqueue/VLCPlayQueueItem.h"
#import "playqueue/VLCPlayQueueModel.h"
#import "playqueue/VLCPlayerController.h"
#import "library/VLCInputItem.h"

#import "views/VLCBottomBarView.h"
#import "views/VLCDragDropView.h"
#import "views/VLCImageView.h"
#import "views/VLCPlaybackProgressSlider.h"
#import "views/VLCTimeField.h"
#import "views/VLCVolumeSlider.h"

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
    NSImage *_backwardImage;
    NSImage *_forwardImage;
    NSImage *_fullscreenImage;
    NSImage *_mutedVolumeImage;
    NSImage *_unmutedVolumeImage;

    NSTimeInterval last_fwd_event;
    NSTimeInterval last_bwd_event;
    BOOL just_triggered_next;
    BOOL just_triggered_previous;

    VLCPlayQueueController *_playQueueController;
    VLCPlayerController *_playerController;
}
@end

@implementation VLCControlsBarCommon

- (void)awakeFromNib
{
    [super awakeFromNib];

    _playQueueController = VLCMain.sharedInstance.playQueueController;
    _playerController = _playQueueController.playerController;

    NSNotificationCenter *notificationCenter = NSNotificationCenter.defaultCenter;
    [notificationCenter addObserver:self
                           selector:@selector(updateTimeSlider:)
                               name:VLCPlayerTimeAndPositionChanged
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(updateVolumeSlider:)
                               name:VLCPlayerVolumeChanged
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(updateVolumeSlider:)
                               name:VLCPlayerMuteChanged
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(updateMuteVolumeButton:)
                               name:VLCPlayerMuteChanged
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(playerStateUpdated:)
                               name:VLCPlayerStateChanged
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(updatePlaybackControls:)
                               name:VLCPlayerCurrentMediaItemChanged
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(fullscreenStateUpdated:)
                               name:VLCPlayerFullscreenChanged
                             object:nil];

    _nativeFullscreenMode = var_InheritBool(getIntf(), "macosx-nativefullscreenmode");

    self.dropView.drawBorder = NO;

    self.playButton.toolTip = _NS("Play");
    self.playButton.accessibilityLabel = self.playButton.toolTip;

    self.backwardButton.toolTip = _NS("Backward");
    self.backwardButton.accessibilityLabel = _NS("Seek backward");
    self.backwardButton.accessibilityTitle = self.backwardButton.toolTip;

    self.forwardButton.toolTip = _NS("Forward");
    self.forwardButton.accessibilityLabel = _NS("Seek forward");
    self.forwardButton.accessibilityTitle = self.forwardButton.toolTip;

    self.jumpBackwardButton.toolTip = _NS("Jump backwards");
    self.jumpBackwardButton.accessibilityLabel = _NS("Jump backwards in current item");
    self.jumpBackwardButton.accessibilityTitle = self.jumpBackwardButton.toolTip;

    self.jumpForwardButton.toolTip = _NS("Jump forwards");
    self.jumpForwardButton.accessibilityLabel = _NS("Jump forwards in current item");
    self.jumpForwardButton.accessibilityTitle = self.jumpForwardButton.toolTip;

    self.timeSlider.toolTip = _NS("Position");
    self.timeSlider.accessibilityLabel = _NS("Playback position");
    self.timeSlider.accessibilityTitle = self.timeSlider.toolTip;

    self.fullscreenButton.toolTip = _NS("Enter fullscreen");
    self.fullscreenButton.accessibilityLabel = self.fullscreenButton.toolTip;

    if (@available(macOS 11.0, *)) {
        _playImage = [NSImage imageWithSystemSymbolName:@"play.circle.fill"
                               accessibilityDescription:_NS("Play")];
        _pressedPlayImage = [NSImage imageWithSystemSymbolName:@"play.circle.fill"
                                      accessibilityDescription:_NS("Play")];
        _pauseImage = [NSImage imageWithSystemSymbolName:@"pause.circle.fill"
                                accessibilityDescription:_NS("Pause")];
        _pressedPauseImage = [NSImage imageWithSystemSymbolName:@"pause.circle.fill"
                                       accessibilityDescription:_NS("Pause")];
        _backwardImage = [NSImage imageWithSystemSymbolName:@"backward.fill"
                                   accessibilityDescription:_NS("Previous")];
        _forwardImage = [NSImage imageWithSystemSymbolName:@"forward.fill"
                                  accessibilityDescription:_NS("Next")];
        _fullscreenImage = [NSImage imageWithSystemSymbolName:@"arrow.up.backward.and.arrow.down.forward"
                                     accessibilityDescription:_NS("Fullscreen")];
        _mutedVolumeImage = [NSImage imageWithSystemSymbolName:@"speaker.slash.fill"
                                      accessibilityDescription:_NS("Muted")];
        _unmutedVolumeImage = [NSImage imageWithSystemSymbolName:@"speaker.wave.3.fill"
                                        accessibilityDescription:_NS("Unmuted")];

        const int64_t shortJumpSize = var_InheritInteger(getIntf(), "short-jump-size");
        NSString * const shortJumpSizeString = [NSString stringWithFormat:@"%lli", shortJumpSize];
        switch (shortJumpSize) {
            case 90:
            case 75:
            case 60:
            case 45:
            case 30:
            case 15:
            case 10:
            case 5:
            {
                NSString * const jumpForwardSymbolName =
                    [NSString stringWithFormat:@"%@.arrow.trianglehead.clockwise",shortJumpSizeString];
                NSString * const jumpBackwardSymbolName =
                    [NSString stringWithFormat:@"%@.arrow.trianglehead.counterclockwise", shortJumpSizeString];
                self.jumpForwardButton.image =
                    [NSImage imageWithSystemSymbolName:jumpForwardSymbolName
                              accessibilityDescription:_NS("Jump forward")];
                self.jumpBackwardButton.image =
                    [NSImage imageWithSystemSymbolName:jumpBackwardSymbolName
                              accessibilityDescription:_NS("Jump backward")];
            }
        }
    } else {
        _playImage = imageFromRes(@"VLCPlayTemplate");
        _pressedPlayImage = imageFromRes(@"VLCPlayTemplate");
        _pauseImage = imageFromRes(@"VLCPauseTemplate");
        _pressedPauseImage = imageFromRes(@"VLCPauseTemplate");
        _backwardImage = imageFromRes(@"VLCBackwardTemplate");
        _forwardImage = imageFromRes(@"VLCForwardTemplate");
        _fullscreenImage = imageFromRes(@"VLCFullscreenOffTemplate");
        _mutedVolumeImage = imageFromRes(@"VLCVolumeOffTemplate");
        _unmutedVolumeImage = imageFromRes(@"VLCVolumeOnTemplate");
    }

    self.backwardButton.image = _backwardImage;
    self.backwardButton.alternateImage = _backwardImage;
    self.forwardButton.image = _forwardImage;
    self.forwardButton.alternateImage = _forwardImage;

    self.fullscreenButton.image = _fullscreenImage;
    self.fullscreenButton.alternateImage = _fullscreenImage;
    self.playButton.image = _playImage;
    self.playButton.alternateImage = _pressedPlayImage;

    self.timeSlider.hidden = NO;

    self.volumeSlider.toolTip = [NSString stringWithFormat:_NS("Volume: %i %%"), 100];
    self.volumeSlider.accessibilityLabel = _NS("Volume");

    self.volumeSlider.maxValue = VLCVolumeMaximum;
    self.volumeSlider.defaultValue = VLCVolumeDefault;

    self.muteVolumeButton.toolTip = _NS("Mute");
    self.muteVolumeButton.accessibilityLabel = self.muteVolumeButton.toolTip;

    self.timeField.needsDisplay = YES;
    self.timeField.identifier = VLCTimeFieldDisplayTimeAsElapsed;
    self.trailingTimeField.isTimeRemaining = NO;
    self.timeField.accessibilityLabel = _NS("Playback time");

    self.trailingTimeField.isTimeRemaining = !self.timeField.isTimeRemaining;
    self.trailingTimeField.needsDisplay = YES;
    self.timeField.identifier = VLCTimeFieldDisplayTimeAsRemaining;
    self.trailingTimeField.isTimeRemaining = YES;
    self.trailingTimeField.accessibilityLabel = _NS("Playback time");

    // remove fullscreen button for lion fullscreen
    if (_nativeFullscreenMode) {
        self.fullscreenButtonWidthConstraint.constant = 0;
        self.fullscreenButton.hidden = YES;
    }

    self.backwardButton.accessibilityTitle = _NS("Previous");
    self.backwardButton.accessibilityLabel = _NS("Go to previous item");

    self.forwardButton.accessibilityTitle = _NS("Next");
    self.forwardButton.accessibilityLabel = _NS("Go to next item");

    [self.forwardButton setAction:@selector(fwd:)];
    [self.backwardButton setAction:@selector(bwd:)];

    [self playerStateUpdated:nil];

    self.artworkImageView.cropsImagesToRoundedCorners = YES;
    self.artworkImageView.image = [NSImage imageNamed:@"noart"];
    self.artworkImageView.contentGravity = VLCImageViewContentGravityResize;

    if (!NSClassFromString(@"PIPViewController")) {
        self.pipButtonWidthConstraint.constant = 0;
        self.pipButton.hidden = YES;
    }

    // Update verything post-init
    [self update];
}

- (void)dealloc
{
    [NSNotificationCenter.defaultCenter removeObserver:self];
}

- (CGFloat)height
{
    return self.bottomBarView.frame.size.height;
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
        [_playQueueController playPreviousItem];
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
    } else if (([NSDate timeIntervalSinceReferenceDate] - last_fwd_event) > 0.16) {
        // we just skipped 4 "continuous" events, otherwise we are too fast
        [_playerController jumpBackwardExtraShort];
        last_bwd_event = [NSDate timeIntervalSinceReferenceDate];
        [self performSelector:@selector(resetBackwardSkip)
                   withObject: NULL
                   afterDelay:0.40];
    }
}

- (void)resetNextButton
{
    if (([NSDate timeIntervalSinceReferenceDate] - last_fwd_event) >= 0.35) {
        // seems like no further event occurred, so let's switch the playback item
        [_playQueueController playNextItem];
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
    } else if (([NSDate timeIntervalSinceReferenceDate] - last_fwd_event) > 0.16) {
        // we just skipped 4 "continuous" events, otherwise we are too fast
        [_playerController jumpForwardExtraShort];
        last_fwd_event = [NSDate timeIntervalSinceReferenceDate];
        [self performSelector:@selector(resetForwardSkip)
                   withObject: NULL
                   afterDelay:0.40];
    }
}

- (IBAction)jumpBackward:(id)sender
{
    [_playerController jumpBackwardShort];
}

- (IBAction)jumpForward:(id)sender
{
    [_playerController jumpForwardShort];
}

- (IBAction)timeSliderAction:(id)sender
{
    if (![sender respondsToSelector:@selector(floatValue)]) {
        return;
    }

    switch (NSApp.currentEvent.type) {
        case NSEventTypeLeftMouseUp:
            /* Ignore mouse up, as this is a continuous slider and
             * when the user does a single click to a position on the slider,
             * the action is called twice, once for the mouse down and once
             * for the mouse up event. This results in two short seeks one
             * after another to the same position, which results in weird
             * audio quirks.
             */
            return;
        case NSEventTypeLeftMouseDown:
        case NSEventTypeLeftMouseDragged:
        case NSEventTypeScrollWheel:
        {
            const float newPosition = [sender floatValue];
            [_playerController setPositionFast:newPosition];
            self.timeSlider.floatValue = newPosition;
            break;
        }
        default:
            return;
    }
}

- (IBAction)volumeAction:(id)sender
{
    if (sender == self.volumeSlider) {
        [_playerController setVolume:[sender floatValue]];
    } else if (sender == self.muteVolumeButton) {
        [_playerController toggleMute];
    }
}

- (IBAction)fullscreen:(id)sender
{
    [_playerController toggleFullscreen];
}

- (IBAction)onPipButtonClick:(id)sender
{
    [_playerController togglePictureInPicture];
}

#pragma mark -
#pragma mark Updaters

- (void)update 
{
    [self updateTimeSlider:nil];
    [self updateVolumeSlider:nil];
    [self updateMuteVolumeButtonImage];
    [self updatePlaybackControls:nil];
    [self updateCurrentItemDisplayControls:nil];
}

- (void)updateTimeSlider:(NSNotification *)aNotification;
{
    VLCInputItem * const inputItem = _playerController.currentMedia;

    const BOOL validInputItem = inputItem != nil;
    const vlc_tick_t duration = validInputItem ? inputItem.duration : -1;
    NSString * const timeString =
        [NSString stringWithDuration:duration currentTime:_playerController.time negative:NO];
    NSString * const remainingTime =
        [NSString stringWithDuration:duration currentTime:_playerController.time negative:YES];
    const BOOL buffering = _playerController.playerState == VLC_PLAYER_STATE_STARTED;

    self.timeSlider.hidden = !validInputItem;
    self.timeSlider.enabled = duration >= 0 && !buffering && _playerController.seekable;
    self.timeSlider.indefinite = buffering;
    self.timeSlider.floatValue = validInputItem ? _playerController.position : 0.;

    [self.timeField setTime:timeString withRemainingTime:remainingTime];
    [self.trailingTimeField setTime:timeString withRemainingTime:remainingTime];
}

- (void)updateVolumeSlider:(NSNotification *)aNotification
{
    const BOOL muted = _playerController.mute;
    const float volume = muted ? 0. : _playerController.volume;

    self.volumeSlider.enabled = !muted;
    self.volumeSlider.floatValue = volume;
    self.volumeSlider.toolTip =
        [NSString stringWithFormat:_NS("Volume: %i %%"), (int)(volume * 100.)];
}

- (void)updateMuteVolumeButton:(NSNotification*)aNotification
{
    [self updateMuteVolumeButtonImage];
}

- (void)updateMuteVolumeButtonImage
{
    _muteVolumeButton.image = _playerController.mute ? _mutedVolumeImage : _unmutedVolumeImage;
}

- (void)playerStateUpdated:(NSNotification *)aNotification
{
    if (_playerController.playerState == VLC_PLAYER_STATE_PLAYING) {
        [self updatePlayButtonWithPauseState];
    } else {
        [self updatePlayButtonWithPlayState];
    }
}

- (void)updatePlaybackControls:(NSNotification *)notification
{
    const BOOL seekable = _playerController.seekable;
    const BOOL chapters = _playerController.numberOfChaptersForCurrentTitle > 0;

    self.timeSlider.enabled = seekable;
    self.forwardButton.enabled = seekable || _playQueueController.hasNextPlayQueueItem || chapters;
    self.backwardButton.enabled = seekable || _playQueueController.hasPreviousPlayQueueItem || chapters;

    [self updateCurrentItemDisplayControls:notification];
}

- (void)updateCurrentItemDisplayControls:(NSNotification *)notification
{
    VLCInputItem * const inputItem = _playerController.currentMedia;
    VLCMediaLibraryMediaItem * const mediaItem =
        [VLCMediaLibraryMediaItem mediaItemForURL:_playerController.URLOfCurrentMediaItem];

    self.playingItemDisplayField.stringValue = inputItem.name ?: _NS("No current item");
    self.detailLabel.hidden =
        mediaItem == nil ||
        [mediaItem.primaryDetailString isEqualToString:@""] ||
        [mediaItem.primaryDetailString isEqualToString:mediaItem.durationString];
    self.detailLabel.stringValue = mediaItem.primaryDetailString ?: @"";

    NSURL * const artworkURL = inputItem.artworkURL;
    NSImage * const placeholderImage = [NSImage imageNamed:@"noart"];
    if (artworkURL) {
        [self.artworkImageView setImageURL:inputItem.artworkURL placeholderImage:placeholderImage];
    } else {
        self.artworkImageView.image = placeholderImage;
    }
}

- (void)updatePlayButtonWithPauseState
{
    self.playButton.image = _pauseImage;
    self.playButton.alternateImage = _pressedPauseImage;
    self.playButton.toolTip = _NS("Pause");
    self.playButton.accessibilityLabel = self.playButton.toolTip;
}

- (void)updatePlayButtonWithPlayState
{
    self.playButton.image = _playImage;
    self.playButton.alternateImage = _pressedPlayImage;
    self.playButton.toolTip = _NS("Play");
    self.playButton.accessibilityLabel = self.playButton.toolTip;
}

- (void)fullscreenStateUpdated:(NSNotification *)aNotification
{
    if (!self.nativeFullscreenMode) {
        [self.fullscreenButton setState:_playerController.fullscreen];
    }
}

@end
