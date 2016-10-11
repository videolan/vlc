/*****************************************************************************
 * ControlsBar.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2012-2016 VLC authors and VideoLAN
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

    NSButton * _previousButton;
    NSButton * _nextButton;

    BOOL b_show_jump_buttons;
    BOOL b_show_playmode_buttons;
}

- (void)addJumpButtons:(BOOL)b_fast;
- (void)removeJumpButtons:(BOOL)b_fast;
- (void)addPlaymodeButtons:(BOOL)b_fast;
- (void)removePlaymodeButtons:(BOOL)b_fast;

@end

@implementation VLCMainWindowControlsBar

- (void)awakeFromNib
{
    [super awakeFromNib];

    [self.stopButton setToolTip: _NS("Stop")];
    [[self.stopButton cell] accessibilitySetOverrideValue:_NS("Click to stop playback.") forAttribute:NSAccessibilityDescriptionAttribute];
    [[self.stopButton cell] accessibilitySetOverrideValue:[self.stopButton toolTip] forAttribute:NSAccessibilityTitleAttribute];

    [self.playlistButton setToolTip: _NS("Show/Hide Playlist")];
    [[self.playlistButton cell] accessibilitySetOverrideValue:_NS("Click to switch between video output and playlist. If no video is shown in the main window, this allows you to hide the playlist.") forAttribute:NSAccessibilityDescriptionAttribute];
    [[self.playlistButton cell] accessibilitySetOverrideValue:[self.playlistButton toolTip] forAttribute:NSAccessibilityTitleAttribute];

    [self.repeatButton setToolTip: _NS("Repeat")];
    [[self.repeatButton cell] accessibilitySetOverrideValue:_NS("Click to change repeat mode. There are 3 states: repeat one, repeat all and off.") forAttribute:NSAccessibilityDescriptionAttribute];
    [[self.repeatButton cell] accessibilitySetOverrideValue:[self.repeatButton toolTip] forAttribute:NSAccessibilityTitleAttribute];

    [self.shuffleButton setToolTip: _NS("Shuffle")];
    [[self.shuffleButton cell] accessibilitySetOverrideValue:[self.shuffleButton toolTip] forAttribute:NSAccessibilityTitleAttribute];
    [[self.shuffleButton cell] accessibilitySetOverrideValue:_NS("Click to enable or disable random playback.") forAttribute:NSAccessibilityDescriptionAttribute];

    NSString *volumeTooltip = [NSString stringWithFormat:_NS("Volume: %i %%"), 100];
    [self.volumeSlider setToolTip: volumeTooltip];
    [[self.volumeSlider cell] accessibilitySetOverrideValue:_NS("Click and move the mouse while keeping the button pressed to use this slider to change the volume.") forAttribute:NSAccessibilityDescriptionAttribute];
    [[self.volumeSlider cell] accessibilitySetOverrideValue:[self.volumeSlider toolTip] forAttribute:NSAccessibilityTitleAttribute];
    [self.volumeDownButton setToolTip: _NS("Mute")];
    [[self.volumeDownButton cell] accessibilitySetOverrideValue:_NS("Click to mute or unmute the audio.") forAttribute:NSAccessibilityDescriptionAttribute];
    [[self.volumeDownButton cell] accessibilitySetOverrideValue:[self.volumeDownButton toolTip] forAttribute:NSAccessibilityTitleAttribute];
    [self.volumeUpButton setToolTip: _NS("Full Volume")];
    [[self.volumeUpButton cell] accessibilitySetOverrideValue:_NS("Click to play the audio at maximum volume.") forAttribute:NSAccessibilityDescriptionAttribute];
    [[self.volumeUpButton cell] accessibilitySetOverrideValue:[self.volumeUpButton toolTip] forAttribute:NSAccessibilityTitleAttribute];

    [self.effectsButton setToolTip: _NS("Audio Effects")];
    [[self.effectsButton cell] accessibilitySetOverrideValue:_NS("Click to show an Audio Effects panel featuring an equalizer and further filters.") forAttribute:NSAccessibilityDescriptionAttribute];
    [[self.effectsButton cell] accessibilitySetOverrideValue:[self.effectsButton toolTip] forAttribute:NSAccessibilityTitleAttribute];

    if (!self.darkInterface) {
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
        [self.volumeTrackImageView setImage: imageFromRes(@"volume-slider-track")];
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
    } else {
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
        [self.volumeTrackImageView setImage: imageFromRes(@"volume-slider-track_dark")];
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
    }
    [self.repeatButton setImage: _repeatImage];
    [self.repeatButton setAlternateImage: _pressedRepeatImage];
    [self.shuffleButton setImage: _shuffleImage];
    [self.shuffleButton setAlternateImage: _pressedShuffleImage];

    BOOL b_mute = ![[VLCCoreInteraction sharedInstance] mute];
    [self.volumeSlider setEnabled: b_mute];
    [self.volumeSlider setMaxValue: [[VLCCoreInteraction sharedInstance] maxVolume]];
    [self.volumeUpButton setEnabled: b_mute];

    // remove fullscreen button for lion fullscreen
    if (self.nativeFullscreenMode) {
        NSRect frame;

        // == [fullscreenButton frame].size.width;
        // button is already removed!
        float f_width = 29.;
#define moveItem(item) \
frame = [item frame]; \
frame.origin.x = f_width + frame.origin.x; \
[item setFrame: frame]

        moveItem(self.effectsButton);
        moveItem(self.volumeUpButton);
        moveItem(self.volumeSlider);
        moveItem(self.volumeTrackImageView);
        moveItem(self.volumeDownButton);
#undef moveItem

        // time field and progress bar are moved in super method!
    }


    b_show_jump_buttons = config_GetInt(getIntf(), "macosx-show-playback-buttons");
    if (b_show_jump_buttons)
        [self addJumpButtons:YES];

    b_show_playmode_buttons = config_GetInt(getIntf(), "macosx-show-playmode-buttons");
    if (!b_show_playmode_buttons)
        [self removePlaymodeButtons:YES];

    if (!config_GetInt(getIntf(), "macosx-show-effects-button"))
        [self removeEffectsButton:YES];

    [[[VLCMain sharedInstance] playlist] playbackModeUpdated];

}

#pragma mark -
#pragma mark interface customization


- (void)toggleEffectsButton
{
    if (config_GetInt(getIntf(), "macosx-show-effects-button"))
        [self addEffectsButton:NO];
    else
        [self removeEffectsButton:NO];
}

- (void)addEffectsButton:(BOOL)b_fast
{
    if (!self.effectsButton)
        return;

    if (b_fast) {
        [self.effectsButton setHidden: NO];
    } else {
        [[self.effectsButton animator] setHidden: NO];
    }

#define moveItem(item) \
frame = [item frame]; \
frame.origin.x = frame.origin.x - f_space; \
if (b_fast) \
[item setFrame: frame]; \
else \
[[item animator] setFrame: frame]

    NSRect frame;
    CGFloat f_space = [self.effectsButton frame].size.width;
    // extra margin between button and volume up button
    if (self.nativeFullscreenMode)
        f_space += 2;


    moveItem(self.volumeUpButton);
    moveItem(self.volumeSlider);
    moveItem(self.volumeTrackImageView);
    moveItem(self.volumeDownButton);
    moveItem(self.timeField);
#undef moveItem


    frame = [self.progressView frame];
    frame.size.width = frame.size.width - f_space;
    if (b_fast)
        [self.progressView setFrame: frame];
    else
        [[self.progressView animator] setFrame: frame];

    if (!self.nativeFullscreenMode) {
        if (self.darkInterface) {
            [self.fullscreenButton setImage: imageFromRes(@"fullscreen-double-buttons_dark")];
            [self.fullscreenButton setAlternateImage: imageFromRes(@"fullscreen-double-buttons-pressed_dark")];
        } else {
            [self.fullscreenButton setImage: imageFromRes(@"fullscreen-double-buttons")];
            [self.fullscreenButton setAlternateImage: imageFromRes(@"fullscreen-double-buttons-pressed")];
        }
    }

    [self.bottomBarView setNeedsDisplay:YES];
}

- (void)removeEffectsButton:(BOOL)b_fast
{
    if (!self.effectsButton)
        return;

    [self.effectsButton setHidden: YES];

#define moveItem(item) \
frame = [item frame]; \
frame.origin.x = frame.origin.x + f_space; \
if (b_fast) \
[item setFrame: frame]; \
else \
[[item animator] setFrame: frame]

    NSRect frame;
    CGFloat f_space = [self.effectsButton frame].size.width;
    // extra margin between button and volume up button
    if (self.nativeFullscreenMode)
        f_space += 2;

    moveItem(self.volumeUpButton);
    moveItem(self.volumeSlider);
    moveItem(self.volumeTrackImageView);
    moveItem(self.volumeDownButton);
    moveItem(self.timeField);
#undef moveItem


    frame = [self.progressView frame];
    frame.size.width = frame.size.width + f_space;
    if (b_fast)
        [self.progressView setFrame: frame];
    else
        [[self.progressView animator] setFrame: frame];

    if (!self.nativeFullscreenMode) {
        if (self.darkInterface) {
            [[self.fullscreenButton animator] setImage: imageFromRes(@"fullscreen-one-button_dark")];
            [[self.fullscreenButton animator] setAlternateImage: imageFromRes(@"fullscreen-one-button-pressed_dark")];
        } else {
            [[self.fullscreenButton animator] setImage: imageFromRes(@"fullscreen-one-button")];
            [[self.fullscreenButton animator] setAlternateImage: imageFromRes(@"fullscreen-one-button-pressed")];
        }
    }

    [self.bottomBarView setNeedsDisplay:YES];
}

- (void)toggleJumpButtons
{
    b_show_jump_buttons = config_GetInt(getIntf(), "macosx-show-playback-buttons");

    if (b_show_jump_buttons)
        [self addJumpButtons:NO];
    else
        [self removeJumpButtons:NO];
}

- (void)addJumpButtons:(BOOL)b_fast
{
    NSRect preliminaryFrame = [self.backwardButton frame];
    BOOL b_enabled = [self.backwardButton isEnabled];
    preliminaryFrame.size.width = 29.;
    _previousButton = [[NSButton alloc] initWithFrame:preliminaryFrame];
    [_previousButton setButtonType: NSMomentaryChangeButton];
    [_previousButton setBezelStyle:NSRegularSquareBezelStyle];
    [_previousButton setBordered:NO];
    [_previousButton setTarget:self];
    [_previousButton setAction:@selector(prev:)];
    [_previousButton setToolTip: _NS("Previous")];
    [[_previousButton cell] accessibilitySetOverrideValue:_NS("Previous") forAttribute:NSAccessibilityTitleAttribute];
    [[_previousButton cell] accessibilitySetOverrideValue:_NS("Click to go to the previous playlist item.") forAttribute:NSAccessibilityDescriptionAttribute];
    [_previousButton setEnabled: b_enabled];

    _nextButton = [[NSButton alloc] initWithFrame:preliminaryFrame];
    [_nextButton setButtonType: NSMomentaryChangeButton];
    [_nextButton setBezelStyle:NSRegularSquareBezelStyle];
    [_nextButton setBordered:NO];
    [_nextButton setTarget:self];
    [_nextButton setAction:@selector(next:)];
    [_nextButton setToolTip: _NS("Next")];
    [[_nextButton cell] accessibilitySetOverrideValue:_NS("Next") forAttribute:NSAccessibilityTitleAttribute];
    [[_nextButton cell] accessibilitySetOverrideValue:_NS("Click to go to the next playlist item.") forAttribute:NSAccessibilityDescriptionAttribute];
    [_nextButton setEnabled: b_enabled];

    if (self.darkInterface) {
        [_previousButton setImage: imageFromRes(@"previous-6btns-dark")];
        [_previousButton setAlternateImage: imageFromRes(@"previous-6btns-dark-pressed")];
        [_nextButton setImage: imageFromRes(@"next-6btns-dark")];
        [_nextButton setAlternateImage: imageFromRes(@"next-6btns-dark-pressed")];
    } else {
        [_previousButton setImage: imageFromRes(@"previous-6btns")];
        [_previousButton setAlternateImage: imageFromRes(@"previous-6btns-pressed")];
        [_nextButton setImage: imageFromRes(@"next-6btns")];
        [_nextButton setAlternateImage: imageFromRes(@"next-6btns-pressed")];
    }

    NSRect frame;
    frame = [self.backwardButton frame];
    frame.size.width--;
    [self.backwardButton setFrame:frame];
    frame = [self.forwardButton frame];
    frame.size.width--;
    [self.forwardButton setFrame:frame];

#define moveItem(item) \
frame = [item frame]; \
frame.origin.x = frame.origin.x + f_space; \
if (b_fast) \
    [item setFrame: frame]; \
else \
    [[item animator] setFrame: frame]

    float f_space = 29.;
    moveItem(self.backwardButton);
    f_space = 28.;
    moveItem(self.playButton);
    moveItem(self.forwardButton);
    f_space = 28. * 2;
    moveItem(self.stopButton);
    moveItem(self.playlistButton);
    moveItem(self.repeatButton);
    moveItem(self.shuffleButton);
#undef moveItem

    frame = [self.progressView frame];
    frame.size.width = frame.size.width - f_space;
    frame.origin.x = frame.origin.x + f_space;
    if (b_fast)
        [self.progressView setFrame: frame];
    else
        [[self.progressView animator] setFrame: frame];

    if (self.darkInterface) {
        [[self.forwardButton animator] setImage:imageFromRes(@"forward-6btns-dark")];
        [[self.forwardButton animator] setAlternateImage:imageFromRes(@"forward-6btns-dark-pressed")];
        [[self.backwardButton animator] setImage:imageFromRes(@"backward-6btns-dark")];
        [[self.backwardButton animator] setAlternateImage:imageFromRes(@"backward-6btns-dark-pressed")];
    } else {
        [[self.forwardButton animator] setImage:imageFromRes(@"forward-6btns")];
        [[self.forwardButton animator] setAlternateImage:imageFromRes(@"forward-6btns-pressed")];
        [[self.backwardButton animator] setImage:imageFromRes(@"backward-6btns")];
        [[self.backwardButton animator] setAlternateImage:imageFromRes(@"backward-6btns-pressed")];
    }

    preliminaryFrame.origin.x = [_previousButton frame].origin.x + [_previousButton frame].size.width + [self.backwardButton frame].size.width + [self.playButton frame].size.width + [self.forwardButton frame].size.width;
    [_nextButton setFrame: preliminaryFrame];

    // wait until the animation is done, if displayed
    if (b_fast) {
        [self.bottomBarView addSubview:_previousButton];
        [self.bottomBarView addSubview:_nextButton];
    } else {
        [self.bottomBarView performSelector:@selector(addSubview:) withObject:_previousButton afterDelay:.2];
        [self.bottomBarView performSelector:@selector(addSubview:) withObject:_nextButton afterDelay:.2];
    }

    [self toggleForwardBackwardMode: YES];
}

- (void)removeJumpButtons:(BOOL)b_fast
{
    if (!_previousButton || !_nextButton)
        return;

    if (b_fast) {
        [_previousButton setHidden: YES];
        [_nextButton setHidden: YES];
    } else {
        [[_previousButton animator] setHidden: YES];
        [[_nextButton animator] setHidden: YES];
    }
    [_previousButton removeFromSuperviewWithoutNeedingDisplay];
    [_nextButton removeFromSuperviewWithoutNeedingDisplay];
    _previousButton = nil;
    _nextButton = nil;

    NSRect frame;
    frame = [self.backwardButton frame];
    frame.size.width++;
    [self.backwardButton setFrame:frame];
    frame = [self.forwardButton frame];
    frame.size.width++;
    [self.forwardButton setFrame:frame];

#define moveItem(item) \
frame = [item frame]; \
frame.origin.x = frame.origin.x - f_space; \
if (b_fast) \
    [item setFrame: frame]; \
else \
    [[item animator] setFrame: frame]

    float f_space = 29.;
    moveItem(self.backwardButton);
    f_space = 28.;
    moveItem(self.playButton);
    moveItem(self.forwardButton);
    f_space = 28. * 2;
    moveItem(self.stopButton);
    moveItem(self.playlistButton);
    moveItem(self.repeatButton);
    moveItem(self.shuffleButton);
#undef moveItem

    frame = [self.progressView frame];
    frame.size.width = frame.size.width + f_space;
    frame.origin.x = frame.origin.x - f_space;
    if (b_fast)
        [self.progressView setFrame: frame];
    else
        [[self.progressView animator] setFrame: frame];

    if (self.darkInterface) {
        [[self.forwardButton animator] setImage:imageFromRes(@"forward-3btns-dark")];
        [[self.forwardButton animator] setAlternateImage:imageFromRes(@"forward-3btns-dark-pressed")];
        [[self.backwardButton animator] setImage:imageFromRes(@"backward-3btns-dark")];
        [[self.backwardButton animator] setAlternateImage:imageFromRes(@"backward-3btns-dark-pressed")];
    } else {
        [[self.forwardButton animator] setImage:imageFromRes(@"forward-3btns")];
        [[self.forwardButton animator] setAlternateImage:imageFromRes(@"forward-3btns-pressed")];
        [[self.backwardButton animator] setImage:imageFromRes(@"backward-3btns")];
        [[self.backwardButton animator] setAlternateImage:imageFromRes(@"backward-3btns-pressed")];
    }

    [self toggleForwardBackwardMode: NO];

    [self.bottomBarView setNeedsDisplay:YES];
}

- (void)togglePlaymodeButtons
{
    b_show_playmode_buttons = config_GetInt(getIntf(), "macosx-show-playmode-buttons");

    if (b_show_playmode_buttons)
        [self addPlaymodeButtons:NO];
    else
        [self removePlaymodeButtons:NO];
}

- (void)addPlaymodeButtons:(BOOL)b_fast
{
    NSRect frame;
    CGFloat f_space = [self.repeatButton frame].size.width + [self.shuffleButton frame].size.width - 6.;

    if (self.darkInterface) {
        [[self.playlistButton animator] setImage:imageFromRes(@"playlist_dark")];
        [[self.playlistButton animator] setAlternateImage:imageFromRes(@"playlist-pressed_dark")];
    } else {
        [[self.playlistButton animator] setImage:imageFromRes(@"playlist-btn")];
        [[self.playlistButton animator] setAlternateImage:imageFromRes(@"playlist-btn-pressed")];
    }
    frame = [self.playlistButton frame];
    frame.size.width--;
    [self.playlistButton setFrame:frame];

    if (b_fast) {
        [self.repeatButton setHidden: NO];
        [self.shuffleButton setHidden: NO];
    } else {
        [[self.repeatButton animator] setHidden: NO];
        [[self.shuffleButton animator] setHidden: NO];
    }

    frame = [self.progressView frame];
    frame.size.width = frame.size.width - f_space;
    frame.origin.x = frame.origin.x + f_space;
    if (b_fast)
        [self.progressView setFrame: frame];
    else
        [[self.progressView animator] setFrame: frame];
}

- (void)removePlaymodeButtons:(BOOL)b_fast
{
    NSRect frame;
    CGFloat f_space = [self.repeatButton frame].size.width + [self.shuffleButton frame].size.width - 6.;
    [self.repeatButton setHidden: YES];
    [self.shuffleButton setHidden: YES];

    if (self.darkInterface) {
        [[self.playlistButton animator] setImage:imageFromRes(@"playlist-1btn-dark")];
        [[self.playlistButton animator] setAlternateImage:imageFromRes(@"playlist-1btn-dark-pressed")];
    } else {
        [[self.playlistButton animator] setImage:imageFromRes(@"playlist-1btn")];
        [[self.playlistButton animator] setAlternateImage:imageFromRes(@"playlist-1btn-pressed")];
    }
    frame = [self.playlistButton frame];
    frame.size.width++;
    [self.playlistButton setFrame:frame];

    frame = [self.progressView frame];
    frame.size.width = frame.size.width + f_space;
    frame.origin.x = frame.origin.x - f_space;
    if (b_fast)
        [self.progressView setFrame: frame];
    else
        [[self.progressView animator] setFrame: frame];
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
        [self.shuffleButton setImage: _shuffleOnImage];
        [self.shuffleButton setAlternateImage: _pressedShuffleOnImage];
    } else {
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

    if (b_show_jump_buttons) {
        [_previousButton setEnabled: (b_seekable || b_plmul || b_chapters)];
        [_nextButton setEnabled: (b_seekable || b_plmul || b_chapters)];
    }

    [[[VLCMain sharedInstance] mainMenu] setRateControlsEnabled: b_control];
}

@end
