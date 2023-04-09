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

#import "extensions/NSColor+VLCAdditions.h"
#import "extensions/NSString+Helpers.h"

#import "library/VLCInputItem.h"
#import "library/VLCLibraryWindow.h"

#import "main/VLCMain.h"

#import "playlist/VLCPlaylistController.h"
#import "playlist/VLCPlayerController.h"

#import "views/VLCTimeField.h"
#import "views/VLCVolumeSlider.h"
#import "views/VLCWrappableTextField.h"

/*****************************************************************************
 * VLCMainWindowControlsBar
 *
 *  Holds all specific outlets, actions and code for the main window controls bar.
 *****************************************************************************/

@interface VLCMainWindowControlsBar()
{
    NSImage *_repeatOffImage;
    NSImage *_repeatAllImage;
    NSImage *_repeatOneImage;
    NSImage *_shuffleOffImage;
    NSImage *_shuffleOnImage;

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
    [notificationCenter addObserver:self
                           selector:@selector(updatePlaybackControls:)
                               name:VLCPlaylistCurrentItemChanged
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(playbackStateChanged:)
                               name:VLCPlayerStateChanged
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(shuffleStateUpdated:)
                               name:VLCPlaybackOrderChanged
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(repeatStateUpdated:)
                               name:VLCPlaybackRepeatChanged
                             object:nil];

    _repeatAllImage = [NSImage imageNamed:@"repeatAll"];
    _repeatOffImage = [NSImage imageNamed:@"repeatOff"];
    _repeatOneImage = [NSImage imageNamed:@"repeatOne"];

    _shuffleOffImage = [NSImage imageNamed:@"shuffleOff"];
    _shuffleOnImage = [NSImage imageNamed:@"shuffleOn"];

    self.repeatButton.action = @selector(repeatAction:);
    self.shuffleButton.action = @selector(shuffleAction:);

    [self repeatStateUpdated:nil];
    [self shuffleStateUpdated:nil];

    [self.stopButton setToolTip: _NS("Stop")];
    self.stopButton.accessibilityLabel = self.stopButton.toolTip;
    
    [self.volumeUpButton setToolTip: _NS("Full Volume")];
    self.volumeUpButton.accessibilityLabel = self.volumeUpButton.toolTip;

    [self.stopButton setImage: imageFromRes(@"stop")];
    [self.stopButton setAlternateImage: imageFromRes(@"stop-pressed")];

    [self.volumeUpButton setImage: imageFromRes(@"VLCVolumeOnTemplate")];

    [self.fullscreenButton setImage: imageFromRes(@"VLCFullscreenOffTemplate")];
    [self.fullscreenButton setAlternateImage: imageFromRes(@"VLCFullscreenOffTemplate")];

    [self.prevButton setImage: imageFromRes(@"previous-6btns")];
    [self.prevButton setAlternateImage: imageFromRes(@"previous-6btns-pressed")];
    [self.nextButton setImage: imageFromRes(@"next-6btns")];
    [self.nextButton setAlternateImage: imageFromRes(@"next-6btns-pressed")];

    [self playbackStateChanged:nil];
    [self.stopButton setHidden:YES];

    [self.timeField setAlignment: NSCenterTextAlignment];
    [self.trailingTimeField setAlignment: NSCenterTextAlignment];
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
    if (sender == self.volumeUpButton) {
        [_playerController setVolume:VLCVolumeMaximum];
    } else {
        [super volumeAction:sender];
    }
}

- (IBAction)artworkButtonAction:(id)sender
{
    [[VLCMain sharedInstance].libraryWindow reopenVideoView];
}

- (IBAction)shuffleAction:(id)sender
{
    if (_playlistController.playbackOrder == VLC_PLAYLIST_PLAYBACK_ORDER_NORMAL) {
        _playlistController.playbackOrder = VLC_PLAYLIST_PLAYBACK_ORDER_RANDOM;
    } else {
        _playlistController.playbackOrder = VLC_PLAYLIST_PLAYBACK_ORDER_NORMAL;
    }
}

- (IBAction)repeatAction:(id)sender
{
    enum vlc_playlist_playback_repeat currentRepeatState = _playlistController.playbackRepeat;
    switch (currentRepeatState) {
        case VLC_PLAYLIST_PLAYBACK_REPEAT_ALL:
            _playlistController.playbackRepeat = VLC_PLAYLIST_PLAYBACK_REPEAT_NONE;
            break;
        case VLC_PLAYLIST_PLAYBACK_REPEAT_CURRENT:
            _playlistController.playbackRepeat = VLC_PLAYLIST_PLAYBACK_REPEAT_ALL;
            break;

        default:
            _playlistController.playbackRepeat = VLC_PLAYLIST_PLAYBACK_REPEAT_CURRENT;
            break;
    }
}

#pragma mark -
#pragma mark Extra updaters

- (void)updateVolumeSlider:(NSNotification *)aNotification
{
    [super updateVolumeSlider:aNotification];
    BOOL b_muted = _playerController.mute;
    [self.volumeUpButton setEnabled: !b_muted];
}

- (void)updateCurrentItemDisplayControls:(NSNotification *)aNotification;
{
    [super updateCurrentItemDisplayControls:aNotification];

    VLCInputItem * const inputItem = _playerController.currentMedia;
    if (!inputItem) {
        return;
    }

    NSFont * const boldSystemFont = [NSFont boldSystemFontOfSize:12.];
    NSDictionary<NSString *, id> * const boldAttribute = @{NSFontAttributeName: boldSystemFont};

    NSMutableAttributedString * const displayString = [[NSMutableAttributedString alloc] initWithString:inputItem.name attributes:boldAttribute];

    if (inputItem.artist.length != 0) {
        NSAttributedString * const separator = [[NSAttributedString alloc] initWithString:@" · " attributes:boldAttribute];
        [displayString appendAttributedString:separator];

        NSAttributedString * const artistString = [[NSAttributedString alloc] initWithString:inputItem.artist];
        [displayString appendAttributedString:artistString];
    }

    self.playingItemDisplayField.attributedStringValue = displayString;
}

- (void)updateMuteVolumeButtonImage
{
    self.muteVolumeButton.image = imageFromRes(@"VLCVolumeOffTemplate");
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

- (void)repeatStateUpdated:(NSNotification *)aNotification
{
    enum vlc_playlist_playback_repeat currentRepeatState = _playlistController.playbackRepeat;

    switch (currentRepeatState) {
        case VLC_PLAYLIST_PLAYBACK_REPEAT_CURRENT:
            self.repeatButton.image = _repeatOneImage;
            break;
        case VLC_PLAYLIST_PLAYBACK_REPEAT_ALL:
            self.repeatButton.image = _repeatAllImage;
            break;
        case VLC_PLAYLIST_PLAYBACK_REPEAT_NONE:
        default:
            self.repeatButton.image = _repeatOffImage;
            break;
    }

    if (@available(macOS 11.0, *)) {
        self.repeatButton.contentTintColor = currentRepeatState == VLC_PLAYLIST_PLAYBACK_REPEAT_NONE ?
            nil : [NSColor VLCAccentColor];
    }
}

- (void)shuffleStateUpdated:(NSNotification *)aNotification
{
    self.shuffleButton.image = _playlistController.playbackOrder == VLC_PLAYLIST_PLAYBACK_ORDER_NORMAL ?
        _shuffleOffImage : _shuffleOnImage;

    if (@available(macOS 11.0, *)) {
        self.shuffleButton.contentTintColor = _playlistController.playbackOrder == VLC_PLAYLIST_PLAYBACK_ORDER_NORMAL ?
            nil : [NSColor VLCAccentColor];
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
