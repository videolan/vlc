/*****************************************************************************
 * ControlsBar.m: MacOS X interface module
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

#import <vlc_aout.h>

#import "coreinteraction/VLCCoreInteraction.h"
#import "main/CompatibilityFixes.h"
#import "main/VLCMain.h"
#import "menus/VLCMainMenu.h"
#import "windows/mainwindow/VLCMainWindowControlsBar.h"
#import "playlist/VLCPlaylistController.h"
#import "playlist/VLCPlayerController.h"

/*****************************************************************************
 * VLCMainWindowControlsBar
 *
 *  Holds all specific outlets, actions and code for the main window controls bar.
 *****************************************************************************/

@interface VLCMainWindowControlsBar()
{
    NSImage * _repeatImage;
    NSImage * _pressedRepeatImage;
    NSImage * _repeatAllImage;
    NSImage * _pressedRepeatAllImage;
    NSImage * _repeatOneImage;
    NSImage * _pressedRepeatOneImage;
    NSImage * _shuffleImage;
    NSImage * _pressedShuffleImage;
    NSImage * _shuffleOnImage;
    NSImage * _pressedShuffleOnImage;
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
    [notificationCenter addObserver:self selector:@selector(playbackOrderUpdated:) name:VLCPlaybackOrderChanged object:nil];
    [notificationCenter addObserver:self selector:@selector(playbackRepeatChanged:) name:VLCPlaybackRepeatChanged object:nil];

    [self.stopButton setToolTip: _NS("Stop")];
    self.stopButton.accessibilityLabel = self.stopButton.toolTip;

    [self.playlistButton setToolTip: _NS("Show/Hide Playlist")];
    self.playlistButton.accessibilityLabel = self.playlistButton.toolTip;

    [self.repeatButton setToolTip: _NS("Repeat")];
    self.repeatButton.accessibilityLabel = _NS("Change repeat mode. Modes: repeat one, repeat all and no repeat.");
    self.repeatButton.accessibilityTitle = self.repeatButton.toolTip;

    [self.shuffleButton setToolTip: _NS("Shuffle")];
    self.shuffleButton.accessibilityLabel = self.shuffleButton.toolTip;

    NSString *volumeTooltip = [NSString stringWithFormat:_NS("Volume: %i %%"), 100];
    [self.volumeSlider setToolTip: volumeTooltip];
    self.volumeSlider.accessibilityLabel = _NS("Volume");
    
    [self.volumeDownButton setToolTip: _NS("Mute")];
    self.volumeDownButton.accessibilityLabel = self.volumeDownButton.toolTip;
    
    [self.volumeUpButton setToolTip: _NS("Full Volume")];
    self.volumeUpButton.accessibilityLabel = self.volumeUpButton.toolTip;

    [self.effectsButton setToolTip: _NS("Audio Effects")];
    self.effectsButton.accessibilityTitle = _NS("Open Audio Effects window");
    self.effectsButton.accessibilityLabel = self.effectsButton.toolTip;

    [self.stopButton setImage: imageFromRes(@"stop")];
    [self.stopButton setAlternateImage: imageFromRes(@"stop-pressed")];

    [self.playlistButton setImage: imageFromRes(@"playlist-btn")];
    [self.playlistButton setAlternateImage: imageFromRes(@"playlist-btn-pressed")];
    _repeatImage = imageFromRes(@"repeat");
    _pressedRepeatImage = imageFromRes(@"repeat-pressed");
    _repeatAllImage  = imageFromRes(@"repeat-all");
    _pressedRepeatAllImage = imageFromRes(@"repeat-all-pressed");
    _repeatOneImage = imageFromRes(@"repeat-one");
    _pressedRepeatOneImage = imageFromRes(@"repeat-one-pressed");
    _shuffleImage = imageFromRes(@"shuffle");
    _pressedShuffleImage = imageFromRes(@"shuffle-pressed");
    _shuffleOnImage = imageFromRes(@"shuffle-blue");
    _pressedShuffleOnImage = imageFromRes(@"shuffle-blue-pressed");

    [self.volumeDownButton setImage: imageFromRes(@"volume-low")];
    [self.volumeUpButton setImage: imageFromRes(@"volume-high")];

    if (self.nativeFullscreenMode) {
        [self.effectsButton setImage: imageFromRes(@"effects-one-button")];
        [self.effectsButton setAlternateImage: imageFromRes(@"effects-one-button-pressed")];
    } else {
        [self.effectsButton setImage: imageFromRes(@"effects-double-buttons")];
        [self.effectsButton setAlternateImage: imageFromRes(@"effects-double-buttons-pressed")];
    }

    [self.fullscreenButton setImage: imageFromRes(@"fullscreen-double-buttons")];
    [self.fullscreenButton setAlternateImage: imageFromRes(@"fullscreen-double-buttons-pressed")];

    [self.prevButton setImage: imageFromRes(@"previous-6btns")];
    [self.prevButton setAlternateImage: imageFromRes(@"previous-6btns-pressed")];
    [self.nextButton setImage: imageFromRes(@"next-6btns")];
    [self.nextButton setAlternateImage: imageFromRes(@"next-6btns-pressed")];
    [self.repeatButton setImage: _repeatImage];
    [self.repeatButton setAlternateImage: _pressedRepeatImage];
    [self.shuffleButton setImage: _shuffleImage];
    [self.shuffleButton setAlternateImage: _pressedShuffleImage];

    BOOL b_mute = ![[VLCCoreInteraction sharedInstance] mute];
    [self.volumeSlider setEnabled: b_mute];
    [self.volumeSlider setMaxValue: [[VLCCoreInteraction sharedInstance] maxVolume]];
    [self.volumeSlider setDefaultValue: AOUT_VOLUME_DEFAULT];
    [self.volumeUpButton setEnabled: b_mute];

    // configure optional buttons
    if (!var_InheritBool(getIntf(), "macosx-show-effects-button"))
        [self removeEffectsButton:NO];

    if (!var_InheritBool(getIntf(), "macosx-show-playmode-buttons"))
        [self removePlaymodeButtons:NO];

    if (!var_InheritBool(getIntf(), "macosx-show-playback-buttons"))
        [self removeJumpButtons:NO];

    // FIXME: make sure that buttons appear in the correct state
}

#pragma mark -
#pragma mark interface customization


- (void)hideButtonWithConstraint:(NSLayoutConstraint *)constraint animation:(BOOL)animation
{
    NSAssert([constraint.firstItem isKindOfClass:[NSButton class]], @"Constraint must be for NSButton object");

    NSLayoutConstraint *animatedConstraint = animation ? constraint.animator : constraint;
    animatedConstraint.constant = 0;
}

- (void)showButtonWithConstraint:(NSLayoutConstraint *)constraint animation:(BOOL)animation
{
    NSAssert([constraint.firstItem isKindOfClass:[NSButton class]], @"Constraint must be for NSButton object");

    NSLayoutConstraint *animatedConstraint = animation ? constraint.animator : constraint;
    animatedConstraint.constant = ((NSButton *)constraint.firstItem).image.size.width;
}

- (void)toggleEffectsButton
{
    if (var_InheritBool(getIntf(), "macosx-show-effects-button"))
        [self addEffectsButton:YES];
    else
        [self removeEffectsButton:YES];
}

- (void)addEffectsButton:(BOOL)withAnimation
{
    [NSAnimationContext beginGrouping];
    [self showButtonWithConstraint:self.effectsButtonWidthConstraint animation:withAnimation];

    id button = withAnimation ? self.fullscreenButton.animator : self.fullscreenButton;
    if (!self.nativeFullscreenMode) {
        [button setImage: imageFromRes(@"fullscreen-double-buttons")];
        [button setAlternateImage: imageFromRes(@"fullscreen-double-buttons-pressed")];
    }
    [NSAnimationContext endGrouping];
}

- (void)removeEffectsButton:(BOOL)withAnimation
{
    [NSAnimationContext beginGrouping];
    [self hideButtonWithConstraint:self.effectsButtonWidthConstraint animation:withAnimation];

    id button = withAnimation ? self.fullscreenButton.animator : self.fullscreenButton;
    if (!self.nativeFullscreenMode) {
        [button setImage: imageFromRes(@"fullscreen-one-button")];
        [button setAlternateImage: imageFromRes(@"fullscreen-one-button-pressed")];
    }
    [NSAnimationContext endGrouping];
}

- (void)toggleJumpButtons
{
    if (var_InheritBool(getIntf(), "macosx-show-playback-buttons"))
        [self addJumpButtons:YES];
    else
        [self removeJumpButtons:YES];
}

- (void)addJumpButtons:(BOOL)withAnimation
{
    [NSAnimationContext beginGrouping];
    [self showButtonWithConstraint:self.prevButtonWidthConstraint animation:withAnimation];
    [self showButtonWithConstraint:self.nextButtonWidthConstraint animation:withAnimation];

    id backwardButton = withAnimation ? self.backwardButton.animator : self.backwardButton;
    id forwardButton = withAnimation ? self.forwardButton.animator : self.forwardButton;
    [forwardButton setImage:imageFromRes(@"forward-6btns")];
    [forwardButton setAlternateImage:imageFromRes(@"forward-6btns-pressed")];
    [backwardButton setImage:imageFromRes(@"backward-6btns")];
    [backwardButton setAlternateImage:imageFromRes(@"backward-6btns-pressed")];

    [NSAnimationContext endGrouping];

    [self toggleForwardBackwardMode: YES];
}

- (void)removeJumpButtons:(BOOL)withAnimation
{
    [NSAnimationContext beginGrouping];
    [self hideButtonWithConstraint:self.prevButtonWidthConstraint animation:withAnimation];
    [self hideButtonWithConstraint:self.nextButtonWidthConstraint animation:withAnimation];

    id backwardButton = withAnimation ? self.backwardButton.animator : self.backwardButton;
    id forwardButton = withAnimation ? self.forwardButton.animator : self.forwardButton;
    [forwardButton setImage:imageFromRes(@"forward-3btns")];
    [forwardButton setAlternateImage:imageFromRes(@"forward-3btns-pressed")];
    [backwardButton setImage:imageFromRes(@"backward-3btns")];
    [backwardButton setAlternateImage:imageFromRes(@"backward-3btns-pressed")];
    [NSAnimationContext endGrouping];

    [self toggleForwardBackwardMode: NO];
}

- (void)togglePlaymodeButtons
{
    if (var_InheritBool(getIntf(), "macosx-show-playmode-buttons"))
        [self addPlaymodeButtons:YES];
    else
        [self removePlaymodeButtons:YES];
}

- (void)addPlaymodeButtons:(BOOL)withAnimation
{
    [NSAnimationContext beginGrouping];
    [self showButtonWithConstraint:self.repeatButtonWidthConstraint animation:withAnimation];
    [self showButtonWithConstraint:self.shuffleButtonWidthConstraint animation:withAnimation];

    id button = withAnimation ? self.playlistButton.animator : self.playlistButton;
    [button setImage:imageFromRes(@"playlist-btn")];
    [button setAlternateImage:imageFromRes(@"playlist-btn-pressed")];
    [NSAnimationContext endGrouping];
}

- (void)removePlaymodeButtons:(BOOL)withAnimation
{
    [NSAnimationContext beginGrouping];

    [self hideButtonWithConstraint:self.repeatButtonWidthConstraint animation:withAnimation];
    [self hideButtonWithConstraint:self.shuffleButtonWidthConstraint animation:withAnimation];

    id button = withAnimation ? self.playlistButton.animator : self.playlistButton;
    [button setImage:imageFromRes(@"playlist-1btn")];
    [button setAlternateImage:imageFromRes(@"playlist-1btn-pressed")];
    [NSAnimationContext endGrouping];
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

- (void)setRepeatOne
{
    [self.repeatButton setImage: _repeatOneImage];
    [self.repeatButton setAlternateImage: _pressedRepeatOneImage];
}

- (void)setRepeatAll
{
    [self.repeatButton setImage: _repeatAllImage];
    [self.repeatButton setAlternateImage: _pressedRepeatAllImage];
}

- (void)setRepeatOff
{
    [self.repeatButton setImage: _repeatImage];
    [self.repeatButton setAlternateImage: _pressedRepeatImage];
}

- (IBAction)repeat:(id)sender
{
    enum vlc_playlist_playback_repeat repeatState = _playlistController.playbackRepeat;
    switch (repeatState) {
        case VLC_PLAYLIST_PLAYBACK_REPEAT_NONE:
            /* was: no repeating at all, switching to Repeat One */
            _playlistController.playbackRepeat = VLC_PLAYLIST_PLAYBACK_REPEAT_CURRENT;
            break;
        case VLC_PLAYLIST_PLAYBACK_REPEAT_CURRENT:
            /* was: Repeat One, switching to Repeat All */
            _playlistController.playbackRepeat = VLC_PLAYLIST_PLAYBACK_REPEAT_ALL;
            break;

        default:
            /* was: Repeat All, switching to Repeat Off */
            _playlistController.playbackRepeat = VLC_PLAYLIST_PLAYBACK_REPEAT_NONE;
            break;
    }
}

- (void)playbackOrderUpdated:(NSNotification *)aNotification
{
    if (_playlistController.playbackOrder == VLC_PLAYLIST_PLAYBACK_ORDER_NORMAL) {
        [self.shuffleButton setImage: _shuffleImage];
        [self.shuffleButton setAlternateImage: _pressedShuffleImage];
    } else {
        [self.shuffleButton setImage: _shuffleOnImage];
        [self.shuffleButton setAlternateImage: _pressedShuffleOnImage];
    }
}

- (void)playbackRepeatChanged:(NSNotification *)aNotification
{
    enum vlc_playlist_playback_repeat repeatState = _playlistController.playbackRepeat;
    switch (repeatState) {
        case VLC_PLAYLIST_PLAYBACK_REPEAT_ALL:
            [self setRepeatAll];
            break;

        case VLC_PLAYLIST_PLAYBACK_REPEAT_CURRENT:
            [self setRepeatOne];
            break;

        default:
            [self setRepeatOff];
            break;
    }
}

- (IBAction)shuffle:(id)sender
{
    if (_playlistController.playbackOrder == VLC_PLAYLIST_PLAYBACK_ORDER_NORMAL) {
        _playlistController.playbackOrder = VLC_PLAYLIST_PLAYBACK_ORDER_RANDOM;
    } else {
        _playlistController.playbackOrder = VLC_PLAYLIST_PLAYBACK_ORDER_NORMAL;
    }
}

- (IBAction)togglePlaylist:(id)sender
{
    [[[VLCMain sharedInstance] mainWindow] changePlaylistState: psUserEvent];
}

- (IBAction)volumeAction:(id)sender
{
    if (sender == self.volumeSlider)
        [[VLCCoreInteraction sharedInstance] setVolume: [sender intValue]];
    else if (sender == self.volumeDownButton)
        [[VLCCoreInteraction sharedInstance] toggleMute];
    else
        [[VLCCoreInteraction sharedInstance] setVolume: AOUT_VOLUME_MAX];
}

- (IBAction)effects:(id)sender
{
    [[[VLCMain sharedInstance] mainMenu] showAudioEffects: sender];
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

- (void)updatePlaybackControls:(NSNotification *)aNotification
{
    bool b_input = false;
    bool b_seekable = _playerController.seekable;
    bool b_control = _playerController.rateChangable;
    // FIXME: re-add chapter navigation as needed
    bool b_chapters = false;

    input_item_t *p_item = _playerController.currentMedia;
    b_input = p_item != NULL;
    if (p_item) {
        b_input = YES;
        input_item_Release(p_item);
    }

    [self.stopButton setEnabled: b_input];
    [self.prevButton setEnabled: (b_seekable || _playlistController.hasPreviousPlaylistItem || b_chapters)];
    [self.nextButton setEnabled: (b_seekable || _playlistController.hasNextPlaylistItem || b_chapters)];

    [[[VLCMain sharedInstance] mainMenu] setRateControlsEnabled: b_control];
}

@end
