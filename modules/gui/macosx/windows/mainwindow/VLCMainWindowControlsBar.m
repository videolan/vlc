/*****************************************************************************
 * VLCMainWindowControlsBar.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2012-2019 VLC authors and VideoLAN
 *
 * Authors: Felix Paul Kühne <fkuehne # videolan dot org>
 *          David Fuhrmann <dfuhrmann # videolan dot org>
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

#import "VLCMainWindowControlsBar.h"
#import "VLCControlsBarCommon.h"

#import "extensions/NSString+Helpers.h"
#import "main/VLCMain.h"
#import "playlist/VLCPlaylistController.h"
#import "playlist/VLCPlayerController.h"
#import "views/VLCVolumeSlider.h"

/*****************************************************************************
 * VLCMainWindowControlsBar
 *
 *  Holds all specific outlets, actions and code for the main window controls bar.
 *****************************************************************************/

@interface VLCMainWindowControlsBar()
{
    VLCPlaylistController *_playlistController;
    VLCPlayerController *_playerController;
}
@end

@implementation VLCMainWindowControlsBar

- (void)awakeFromNib
{
    [super awakeFromNib];
    _playlistController = [[VLCMain sharedInstance] playlistController];
    _playerController = _playlistController.playerController;

    NSNotificationCenter *notificationCenter = [NSNotificationCenter defaultCenter];
    [notificationCenter addObserver:self selector:@selector(updatePlaybackControls:) name:VLCPlaylistCurrentItemChanged object:nil];
    [notificationCenter addObserver:self selector:@selector(updateVolumeSlider:) name:VLCPlayerVolumeChanged object:nil];
    [notificationCenter addObserver:self selector:@selector(updateVolumeSlider:) name:VLCPlayerMuteChanged object:nil];
    [notificationCenter addObserver:self selector:@selector(playbackStateChanged:) name:VLCPlayerStateChanged object:nil];

    [self.stopButton setToolTip: _NS("Stop")];
    self.stopButton.accessibilityLabel = self.stopButton.toolTip;

    NSString *volumeTooltip = [NSString stringWithFormat:_NS("Volume: %i %%"), 100];
    [self.volumeSlider setToolTip: volumeTooltip];
    self.volumeSlider.accessibilityLabel = _NS("Volume");
    
    [self.volumeDownButton setToolTip: _NS("Mute")];
    self.volumeDownButton.accessibilityLabel = self.volumeDownButton.toolTip;
    
    [self.volumeUpButton setToolTip: _NS("Full Volume")];
    self.volumeUpButton.accessibilityLabel = self.volumeUpButton.toolTip;

    [self.stopButton setImage: imageFromRes(@"stop")];
    [self.stopButton setAlternateImage: imageFromRes(@"stop-pressed")];

    [self.volumeDownButton setImage: imageFromRes(@"volume-low")];
    [self.volumeUpButton setImage: imageFromRes(@"volume-high")];

    [self.fullscreenButton setImage: imageFromRes(@"fullscreen-double-buttons")];
    [self.fullscreenButton setAlternateImage: imageFromRes(@"fullscreen-double-buttons-pressed")];

    [self.prevButton setImage: imageFromRes(@"previous-6btns")];
    [self.prevButton setAlternateImage: imageFromRes(@"previous-6btns-pressed")];
    [self.nextButton setImage: imageFromRes(@"next-6btns")];
    [self.nextButton setAlternateImage: imageFromRes(@"next-6btns-pressed")];

    [self.volumeSlider setMaxValue: VLCVolumeMaximum];
    [self.volumeSlider setDefaultValue: VLCVolumeDefault];
    [self updateVolumeSlider:nil];

    [self playbackStateChanged:nil];
}

#pragma mark -
#pragma mark Extra button actions

- (IBAction)stop:(id)sender
{
    [_playlistController stopPlayback];
}

// dynamically created next / prev buttons
- (IBAction)prev:(id)sender
{
    [_playlistController playPreviousItem];
}

- (IBAction)next:(id)sender
{
    [_playlistController playNextItem];
}

- (IBAction)volumeAction:(id)sender
{
    if (sender == self.volumeSlider)
        [_playerController setVolume:[sender floatValue]];
    else if (sender == self.volumeDownButton)
        [_playerController toggleMute];
    else
        [_playerController setVolume:VLCVolumeMaximum];
}

#pragma mark -
#pragma mark Extra updaters

- (void)updateVolumeSlider:(NSNotification *)aNotification
{
    float f_volume = _playerController.volume;
    BOOL b_muted = _playerController.mute;

    if (b_muted)
        f_volume = 0.;

    [self.volumeSlider setFloatValue: f_volume];
    NSString *volumeTooltip = [NSString stringWithFormat:_NS("Volume: %i %%"), f_volume * 100];
    [self.volumeSlider setToolTip:volumeTooltip];

    [self.volumeSlider setEnabled: !b_muted];
    [self.volumeUpButton setEnabled: !b_muted];
}

- (void)playbackStateChanged:(NSNotification *)aNotification
{
    switch (_playerController.playerState) {
        case VLC_PLAYER_STATE_STOPPING:
        case VLC_PLAYER_STATE_STOPPED:
            self.stopButton.enabled = NO;
            break;

        default:
            self.stopButton.enabled = YES;
            break;
    }
}

- (void)updatePlaybackControls:(NSNotification *)aNotification
{
    bool b_seekable = _playerController.seekable;
    // FIXME: re-add chapter navigation as needed
    bool b_chapters = false;

    [self.prevButton setEnabled: (b_seekable || _playlistController.hasPreviousPlaylistItem || b_chapters)];
    [self.nextButton setEnabled: (b_seekable || _playlistController.hasNextPlaylistItem || b_chapters)];
}

@end
