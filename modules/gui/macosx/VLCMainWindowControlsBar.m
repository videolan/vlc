/*****************************************************************************
 * ControlsBar.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2012-2018 VLC authors and VideoLAN
 * $Id$
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
#import "VLCMainWindowControlsBar.h"
#import "VLCMain.h"
#import "VLCCoreInteraction.h"
#import "VLCMainMenu.h"
#import "VLCPlaylist.h"
#import "CompatibilityFixes.h"

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

    BOOL _shuffle;
    BOOL _repeatOne;
    BOOL _repeatAll;
}

@end

@implementation VLCMainWindowControlsBar

- (void)dealloc
{
    if (@available(macOS 10_14, *)) {
        [[NSApplication sharedApplication] removeObserver:self forKeyPath:@"effectiveAppearance"];
    }
}

- (void)awakeFromNib
{
    [super awakeFromNib];

    [self.stopButton setToolTip: _NS("Stop")];
    [[self.stopButton cell] accessibilitySetOverrideValue:[self.stopButton toolTip] forAttribute:NSAccessibilityDescriptionAttribute];

    [self.playlistButton setToolTip: _NS("Show/Hide Playlist")];
    [[self.playlistButton cell] accessibilitySetOverrideValue:[self.playlistButton toolTip] forAttribute:NSAccessibilityDescriptionAttribute];

    [self.repeatButton setToolTip: _NS("Repeat")];
    [[self.repeatButton cell] accessibilitySetOverrideValue:_NS("Change repeat mode. Modes: repeat one, repeat all and no repeat.") forAttribute:NSAccessibilityDescriptionAttribute];
    [[self.repeatButton cell] accessibilitySetOverrideValue:[self.repeatButton toolTip] forAttribute:NSAccessibilityTitleAttribute];

    [self.shuffleButton setToolTip: _NS("Shuffle")];
    [[self.shuffleButton cell] accessibilitySetOverrideValue:[self.shuffleButton toolTip] forAttribute:NSAccessibilityDescriptionAttribute];

    NSString *volumeTooltip = [NSString stringWithFormat:_NS("Volume: %i %%"), 100];
    [self.volumeSlider setToolTip: volumeTooltip];
    [[self.volumeSlider cell] accessibilitySetOverrideValue:_NS("Volume") forAttribute:NSAccessibilityDescriptionAttribute];
    
    [self.volumeDownButton setToolTip: _NS("Mute")];
    [[self.volumeDownButton cell] accessibilitySetOverrideValue:[self.volumeDownButton toolTip] forAttribute:NSAccessibilityDescriptionAttribute];
    
    [self.volumeUpButton setToolTip: _NS("Full Volume")];
    [[self.volumeUpButton cell] accessibilitySetOverrideValue:[self.volumeUpButton toolTip] forAttribute:NSAccessibilityDescriptionAttribute];

    [self.effectsButton setToolTip: _NS("Audio Effects")];
    [[self.effectsButton cell] accessibilitySetOverrideValue:_NS("Open Audio Effects window") forAttribute:NSAccessibilityDescriptionAttribute];
    [[self.effectsButton cell] accessibilitySetOverrideValue:[self.effectsButton toolTip] forAttribute:NSAccessibilityTitleAttribute];

    if (@available(macOS 10_14, *)) {
        [[NSApplication sharedApplication] addObserver:self
                                            forKeyPath:@"effectiveAppearance"
                                               options:0
                                               context:nil];
    }

    if (!self.darkInterface) {
        [self setBrightButtonImageSet];
    } else {
        [self setDarkButtonImageSet];
    }
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

    [[[VLCMain sharedInstance] playlist] playbackModeUpdated];
}

- (void)setBrightButtonImageSet
{
    [super setBrightButtonImageSet];

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
    [self.volumeSlider setUsesBrightArtwork: YES];

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

    [self updatePlaymodeButtonImages];
}

- (void)setDarkButtonImageSet
{
    [super setDarkButtonImageSet];

    [self.stopButton setImage: imageFromRes(@"stop_dark")];
    [self.stopButton setAlternateImage: imageFromRes(@"stop-pressed_dark")];

    [self.playlistButton setImage: imageFromRes(@"playlist_dark")];
    [self.playlistButton setAlternateImage: imageFromRes(@"playlist-pressed_dark")];
    _repeatImage = imageFromRes(@"repeat_dark");
    _pressedRepeatImage = imageFromRes(@"repeat-pressed_dark");
    _repeatAllImage  = imageFromRes(@"repeat-all-blue_dark");
    _pressedRepeatAllImage = imageFromRes(@"repeat-all-blue-pressed_dark");
    _repeatOneImage = imageFromRes(@"repeat-one-blue_dark");
    _pressedRepeatOneImage = imageFromRes(@"repeat-one-blue-pressed_dark");
    _shuffleImage = imageFromRes(@"shuffle_dark");
    _pressedShuffleImage = imageFromRes(@"shuffle-pressed_dark");
    _shuffleOnImage = imageFromRes(@"shuffle-blue_dark");
    _pressedShuffleOnImage = imageFromRes(@"shuffle-blue-pressed_dark");

    [self.volumeDownButton setImage: imageFromRes(@"volume-low_dark")];
    [self.volumeUpButton setImage: imageFromRes(@"volume-high_dark")];
    [self.volumeSlider setUsesBrightArtwork: NO];

    if (self.nativeFullscreenMode) {
        [self.effectsButton setImage: imageFromRes(@"effects-one-button_dark")];
        [self.effectsButton setAlternateImage: imageFromRes(@"effects-one-button-pressed-dark")];
    } else {
        [self.effectsButton setImage: imageFromRes(@"effects-double-buttons_dark")];
        [self.effectsButton setAlternateImage: imageFromRes(@"effects-double-buttons-pressed_dark")];
    }

    [self.fullscreenButton setImage: imageFromRes(@"fullscreen-double-buttons_dark")];
    [self.fullscreenButton setAlternateImage: imageFromRes(@"fullscreen-double-buttons-pressed_dark")];

    [self.prevButton setImage: imageFromRes(@"previous-6btns-dark")];
    [self.prevButton setAlternateImage: imageFromRes(@"previous-6btns-dark-pressed")];
    [self.nextButton setImage: imageFromRes(@"next-6btns-dark")];
    [self.nextButton setAlternateImage: imageFromRes(@"next-6btns-dark-pressed")];

    [self updatePlaymodeButtonImages];
}

- (void)updatePlaymodeButtonImages
{
    if (_shuffle) {
        self.shuffleButton.image = _shuffleOnImage;
        self.shuffleButton.alternateImage = _pressedShuffleImage;
    } else {
        self.shuffleButton.image = _shuffleImage;
        self.shuffleButton.alternateImage = _pressedShuffleImage;
    }
    if (_repeatOne) {
        self.repeatButton.image = _repeatOneImage;
        self.repeatButton.alternateImage = _pressedRepeatOneImage;
    } else if (_repeatAll) {
        self.repeatButton.image = _repeatAllImage;
        self.repeatButton.alternateImage = _pressedRepeatAllImage;
    } else {
        self.repeatButton.image = _repeatImage;
        self.repeatButton.alternateImage = _pressedRepeatImage;
    }
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
        if (self.darkInterface) {
            [button setImage: imageFromRes(@"fullscreen-double-buttons_dark")];
            [button setAlternateImage: imageFromRes(@"fullscreen-double-buttons-pressed_dark")];
        } else {
            [button setImage: imageFromRes(@"fullscreen-double-buttons")];
            [button setAlternateImage: imageFromRes(@"fullscreen-double-buttons-pressed")];
        }
    }
    [NSAnimationContext endGrouping];
}

- (void)removeEffectsButton:(BOOL)withAnimation
{
    [NSAnimationContext beginGrouping];
    [self hideButtonWithConstraint:self.effectsButtonWidthConstraint animation:withAnimation];

    id button = withAnimation ? self.fullscreenButton.animator : self.fullscreenButton;
    if (!self.nativeFullscreenMode) {
        if (self.darkInterface) {
            [button setImage: imageFromRes(@"fullscreen-one-button_dark")];
            [button setAlternateImage: imageFromRes(@"fullscreen-one-button-pressed_dark")];
        } else {
            [button setImage: imageFromRes(@"fullscreen-one-button")];
            [button setAlternateImage: imageFromRes(@"fullscreen-one-button-pressed")];
        }
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
    if (self.darkInterface) {
        [forwardButton setImage:imageFromRes(@"forward-6btns-dark")];
        [forwardButton setAlternateImage:imageFromRes(@"forward-6btns-dark-pressed")];
        [backwardButton setImage:imageFromRes(@"backward-6btns-dark")];
        [backwardButton setAlternateImage:imageFromRes(@"backward-6btns-dark-pressed")];
    } else {
        [forwardButton setImage:imageFromRes(@"forward-6btns")];
        [forwardButton setAlternateImage:imageFromRes(@"forward-6btns-pressed")];
        [backwardButton setImage:imageFromRes(@"backward-6btns")];
        [backwardButton setAlternateImage:imageFromRes(@"backward-6btns-pressed")];
    }
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
    if (self.darkInterface) {
        [forwardButton setImage:imageFromRes(@"forward-3btns-dark")];
        [forwardButton setAlternateImage:imageFromRes(@"forward-3btns-dark-pressed")];
        [backwardButton setImage:imageFromRes(@"backward-3btns-dark")];
        [backwardButton setAlternateImage:imageFromRes(@"backward-3btns-dark-pressed")];
    } else {
        [forwardButton setImage:imageFromRes(@"forward-3btns")];
        [forwardButton setAlternateImage:imageFromRes(@"forward-3btns-pressed")];
        [backwardButton setImage:imageFromRes(@"backward-3btns")];
        [backwardButton setAlternateImage:imageFromRes(@"backward-3btns-pressed")];
    }
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
    if (self.darkInterface) {
        [button setImage:imageFromRes(@"playlist_dark")];
        [button setAlternateImage:imageFromRes(@"playlist-pressed_dark")];
    } else {
        [button setImage:imageFromRes(@"playlist-btn")];
        [button setAlternateImage:imageFromRes(@"playlist-btn-pressed")];
    }
    [NSAnimationContext endGrouping];
}

- (void)removePlaymodeButtons:(BOOL)withAnimation
{
    [NSAnimationContext beginGrouping];

    [self hideButtonWithConstraint:self.repeatButtonWidthConstraint animation:withAnimation];
    [self hideButtonWithConstraint:self.shuffleButtonWidthConstraint animation:withAnimation];

    id button = withAnimation ? self.playlistButton.animator : self.playlistButton;
    if (self.darkInterface) {
        [button setImage:imageFromRes(@"playlist-1btn-dark")];
        [button setAlternateImage:imageFromRes(@"playlist-1btn-dark-pressed")];
    } else {
        [button setImage:imageFromRes(@"playlist-1btn")];
        [button setAlternateImage:imageFromRes(@"playlist-1btn-pressed")];
    }
    [NSAnimationContext endGrouping];
}

#pragma mark -
#pragma mark Extra button actions

- (IBAction)stop:(id)sender
{
    [[VLCCoreInteraction sharedInstance] stop];
}

// dynamically created next / prev buttons
- (IBAction)prev:(id)sender
{
    [[VLCCoreInteraction sharedInstance] previous];
}

- (IBAction)next:(id)sender
{
    [[VLCCoreInteraction sharedInstance] next];
}

- (void)setRepeatOne
{
    [self.repeatButton setImage: _repeatOneImage];
    [self.repeatButton setAlternateImage: _pressedRepeatOneImage];
    _repeatOne = YES;
    _repeatAll = NO;
}

- (void)setRepeatAll
{
    [self.repeatButton setImage: _repeatAllImage];
    [self.repeatButton setAlternateImage: _pressedRepeatAllImage];
    _repeatOne = NO;
    _repeatAll = YES;
}

- (void)setRepeatOff
{
    [self.repeatButton setImage: _repeatImage];
    [self.repeatButton setAlternateImage: _pressedRepeatImage];
    _repeatOne = NO;
    _repeatAll = NO;
}

- (IBAction)repeat:(id)sender
{
    vlc_value_t looping,repeating;
    intf_thread_t * p_intf = getIntf();
    playlist_t * p_playlist = pl_Get(p_intf);

    var_Get(p_playlist, "repeat", &repeating);
    var_Get(p_playlist, "loop", &looping);

    if (!repeating.b_bool && !looping.b_bool) {
        /* was: no repeating at all, switching to Repeat One */
        [[VLCCoreInteraction sharedInstance] repeatOne];
        [self setRepeatOne];
    }
    else if (repeating.b_bool && !looping.b_bool) {
        /* was: Repeat One, switching to Repeat All */
        [[VLCCoreInteraction sharedInstance] repeatAll];
        [self setRepeatAll];
    } else {
        /* was: Repeat All or bug in VLC, switching to Repeat Off */
        [[VLCCoreInteraction sharedInstance] repeatOff];
        [self setRepeatOff];
    }
}

- (void)setShuffle
{
    bool b_value;
    playlist_t *p_playlist = pl_Get(getIntf());
    b_value = var_GetBool(p_playlist, "random");

    if (b_value) {
        _shuffle = YES;
        [self.shuffleButton setImage: _shuffleOnImage];
        [self.shuffleButton setAlternateImage: _pressedShuffleOnImage];
    } else {
        _shuffle = NO;
        [self.shuffleButton setImage: _shuffleImage];
        [self.shuffleButton setAlternateImage: _pressedShuffleImage];
    }
}

- (IBAction)shuffle:(id)sender
{
    [[VLCCoreInteraction sharedInstance] shuffle];
    [self setShuffle];
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

- (void)updateVolumeSlider
{
    int i_volume = [[VLCCoreInteraction sharedInstance] volume];
    BOOL b_muted = [[VLCCoreInteraction sharedInstance] mute];

    if (b_muted)
        i_volume = 0;

    [self.volumeSlider setIntValue: i_volume];

    i_volume = (i_volume * 200) / AOUT_VOLUME_MAX;
    NSString *volumeTooltip = [NSString stringWithFormat:_NS("Volume: %i %%"), i_volume];
    [self.volumeSlider setToolTip:volumeTooltip];

    [self.volumeSlider setEnabled: !b_muted];
    [self.volumeUpButton setEnabled: !b_muted];
}

- (void)updateControls
{
    [super updateControls];

    bool b_input = false;
    bool b_seekable = false;
    bool b_plmul = false;
    bool b_control = false;
    bool b_chapters = false;

    playlist_t * p_playlist = pl_Get(getIntf());

    PL_LOCK;
    b_plmul = playlist_CurrentSize(p_playlist) > 1;
    PL_UNLOCK;

    input_thread_t * p_input = playlist_CurrentInput(p_playlist);
    if ((b_input = (p_input != NULL))) {
        /* seekable streams */
        b_seekable = var_GetBool(p_input, "can-seek");

        /* check whether slow/fast motion is possible */
        b_control = var_GetBool(p_input, "can-rate");

        /* chapters & titles */
        //FIXME! b_chapters = p_input->stream.i_area_nb > 1;

        vlc_object_release(p_input);
    }

    [self.stopButton setEnabled: b_input];
    [self.prevButton setEnabled: (b_seekable || b_plmul || b_chapters)];
    [self.nextButton setEnabled: (b_seekable || b_plmul || b_chapters)];

    [[[VLCMain sharedInstance] mainMenu] setRateControlsEnabled: b_control];
}

- (void)observeValueForKeyPath:(NSString *)keyPath
                      ofObject:(id)object
                        change:(NSDictionary<NSKeyValueChangeKey,id> *)change
                       context:(void *)context
{
    if (@available(macOS 10_14, *)) {
        if ([[NSApplication sharedApplication].effectiveAppearance.name isEqualToString:NSAppearanceNameDarkAqua]) {
            [self setDarkButtonImageSet];
        } else {
            [self setBrightButtonImageSet];
        }
    }
}

@end
