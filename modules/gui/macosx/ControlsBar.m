/*****************************************************************************
 * ControlsBar.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2012-2013 VLC authors and VideoLAN
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

#import "ControlsBar.h"
#import "intf.h"
#import "CoreInteraction.h"
#import "MainMenu.h"
#import "fspanel.h"

/*****************************************************************************
 * VLCControlsBarCommon
 *
 *  Holds all outlets, actions and code common for controls bar in detached
 *  and in main window.
 *****************************************************************************/

@implementation VLCControlsBarCommon

@synthesize bottomBarView=o_bottombar_view;

- (void)awakeFromNib
{
    b_dark_interface = config_GetInt(VLCIntf, "macosx-interfacestyle");

    b_nativeFullscreenMode = NO;
#ifdef MAC_OS_X_VERSION_10_7
    if (!OSX_SNOW_LEOPARD)
        b_nativeFullscreenMode = var_InheritBool(VLCIntf, "macosx-nativefullscreenmode");
#endif

    [o_play_btn setToolTip: _NS("Play/Pause")];
    [[o_play_btn cell] accessibilitySetOverrideValue:_NS("Click to play or pause the current media.") forAttribute:NSAccessibilityDescriptionAttribute];
    [[o_play_btn cell] accessibilitySetOverrideValue:[o_play_btn toolTip] forAttribute:NSAccessibilityTitleAttribute];

    [o_bwd_btn setToolTip: _NS("Backward")];
    [[o_bwd_btn cell] accessibilitySetOverrideValue:_NS("Click to go to the previous playlist item. Hold to skip backward through the current media.") forAttribute:NSAccessibilityDescriptionAttribute];
    [[o_bwd_btn cell] accessibilitySetOverrideValue:[o_bwd_btn toolTip] forAttribute:NSAccessibilityTitleAttribute];

    [o_fwd_btn setToolTip: _NS("Forward")];
    [[o_fwd_btn cell] accessibilitySetOverrideValue:_NS("Click to go to the next playlist item. Hold to skip forward through the current media.") forAttribute:NSAccessibilityDescriptionAttribute];
    [[o_fwd_btn cell] accessibilitySetOverrideValue:[o_fwd_btn toolTip] forAttribute:NSAccessibilityTitleAttribute];

    [o_time_sld setToolTip: _NS("Position")];
    [[o_time_sld cell] accessibilitySetOverrideValue:_NS("Click and move the mouse while keeping the button pressed to use this slider to change current playback position.") forAttribute:NSAccessibilityDescriptionAttribute];
    [[o_time_sld cell] accessibilitySetOverrideValue:[o_time_sld toolTip] forAttribute:NSAccessibilityTitleAttribute];

    [o_fullscreen_btn setToolTip: _NS("Toggle Fullscreen mode")];
    [[o_fullscreen_btn cell] accessibilitySetOverrideValue:_NS("Click to enable fullscreen video playback.") forAttribute:NSAccessibilityDescriptionAttribute];
    [[o_fullscreen_btn cell] accessibilitySetOverrideValue:[o_fullscreen_btn toolTip] forAttribute:NSAccessibilityTitleAttribute];

    if (!b_dark_interface) {
        [o_bottombar_view setImagesLeft: [NSImage imageNamed:@"bottom-background"] middle: [NSImage imageNamed:@"bottom-background"] right: [NSImage imageNamed:@"bottom-background"]];

        [o_bwd_btn setImage: [NSImage imageNamed:@"backward-3btns"]];
        [o_bwd_btn setAlternateImage: [NSImage imageNamed:@"backward-3btns-pressed"]];
        o_play_img = [[NSImage imageNamed:@"play"] retain];
        o_play_pressed_img = [[NSImage imageNamed:@"play-pressed"] retain];
        o_pause_img = [[NSImage imageNamed:@"pause"] retain];
        o_pause_pressed_img = [[NSImage imageNamed:@"pause-pressed"] retain];
        [o_fwd_btn setImage: [NSImage imageNamed:@"forward-3btns"]];
        [o_fwd_btn setAlternateImage: [NSImage imageNamed:@"forward-3btns-pressed"]];

        [o_time_sld_background setImagesLeft: [NSImage imageNamed:@"progression-track-wrapper-left"] middle: [NSImage imageNamed:@"progression-track-wrapper-middle"] right: [NSImage imageNamed:@"progression-track-wrapper-right"]];
        [o_time_sld_fancygradient_view setImagesLeft:[NSImage imageNamed:@"progression-fill-left"] middle:[NSImage imageNamed:@"progression-fill-middle"] right:[NSImage imageNamed:@"progression-fill-right"]];

        [o_fullscreen_btn setImage: [NSImage imageNamed:@"fullscreen-one-button"]];
        [o_fullscreen_btn setAlternateImage: [NSImage imageNamed:@"fullscreen-one-button-pressed"]];
    } else {
        [o_bottombar_view setImagesLeft: [NSImage imageNamed:@"bottomdark-left"] middle: [NSImage imageNamed:@"bottom-background_dark"] right: [NSImage imageNamed:@"bottomdark-right"]];

        [o_bwd_btn setImage: [NSImage imageNamed:@"backward-3btns-dark"]];
        [o_bwd_btn setAlternateImage: [NSImage imageNamed:@"backward-3btns-dark-pressed"]];
        o_play_img = [[NSImage imageNamed:@"play_dark"] retain];
        o_play_pressed_img = [[NSImage imageNamed:@"play-pressed_dark"] retain];
        o_pause_img = [[NSImage imageNamed:@"pause_dark"] retain];
        o_pause_pressed_img = [[NSImage imageNamed:@"pause-pressed_dark"] retain];
        [o_fwd_btn setImage: [NSImage imageNamed:@"forward-3btns-dark"]];
        [o_fwd_btn setAlternateImage: [NSImage imageNamed:@"forward-3btns-dark-pressed"]];

        [o_time_sld_background setImagesLeft: [NSImage imageNamed:@"progression-track-wrapper-left_dark"] middle: [NSImage imageNamed:@"progression-track-wrapper-middle_dark"] right: [NSImage imageNamed:@"progression-track-wrapper-right_dark"]];
        [o_time_sld_fancygradient_view setImagesLeft:[NSImage imageNamed:@"progressbar-fill-left_dark"] middle:[NSImage imageNamed:@"progressbar-fill-middle_dark"] right:[NSImage imageNamed:@"progressbar-fill-right_dark"]];

        [o_fullscreen_btn setImage: [NSImage imageNamed:@"fullscreen-one-button-pressed_dark"]];
        [o_fullscreen_btn setAlternateImage: [NSImage imageNamed:@"fullscreen-one-button-pressed_dark"]];
    }

    [o_play_btn setImage: o_play_img];
    [o_play_btn setAlternateImage: o_play_pressed_img];

    NSColor *o_string_color;
    if (!var_InheritBool(VLCIntf, "macosx-interfacestyle"))
        o_string_color = [NSColor colorWithCalibratedRed:0.229 green:0.229 blue:0.229 alpha:100.0];
    else
        o_string_color = [NSColor colorWithCalibratedRed:0.64 green:0.64 blue:0.64 alpha:100.0];
    [o_time_fld setTextColor: o_string_color];
    [o_time_fld setFont:[NSFont titleBarFontOfSize:10.0]];
    [o_time_fld setAlignment: NSCenterTextAlignment];
    [o_time_fld setNeedsDisplay:YES];
    [o_time_fld setRemainingIdentifier:@"DisplayTimeAsTimeRemaining"];

    // prepare time slider fance gradient view
    if (!b_dark_interface) {
        NSRect frame;
        frame = [o_time_sld_fancygradient_view frame];
        frame.size.height = frame.size.height - 1;
        frame.origin.y = frame.origin.y + 1;
        [o_time_sld_fancygradient_view setFrame: frame];
    }

    NSRect frame;
    frame = [o_time_sld_fancygradient_view frame];
    frame.size.width = 0;
    [o_time_sld_fancygradient_view setFrame: frame];

    // hide resize view if necessary
    if (!OSX_SNOW_LEOPARD)
        [o_resize_view setImage: NULL];

    if ([[o_bottombar_view window] styleMask] & NSResizableWindowMask)
        [o_resize_view removeFromSuperviewWithoutNeedingDisplay];


    // remove fullscreen button for lion fullscreen
    if (b_nativeFullscreenMode) {
        float f_width = [o_fullscreen_btn frame].size.width;

        NSRect frame = [o_time_fld frame];
        frame.origin.x += f_width;
        [o_time_fld setFrame: frame];

        frame = [o_progress_view frame];
        frame.size.width = f_width + frame.size.width;
        [o_progress_view setFrame: frame];

        [o_fullscreen_btn removeFromSuperviewWithoutNeedingDisplay];
    }

    if (config_GetInt(VLCIntf, "macosx-show-playback-buttons"))
        [self toggleForwardBackwardMode: YES];

}

- (CGFloat)height
{
    return [o_bottombar_view frame].size.height;
}

- (void)toggleForwardBackwardMode:(BOOL)b_alt
{
    if (b_alt == YES) {
        /* change the accessibility help for the backward/forward buttons accordingly */
        [[o_bwd_btn cell] accessibilitySetOverrideValue:_NS("Click and hold to skip backward through the current media.") forAttribute:NSAccessibilityDescriptionAttribute];
        [[o_fwd_btn cell] accessibilitySetOverrideValue:_NS("Click and hold to skip forward through the current media.") forAttribute:NSAccessibilityDescriptionAttribute];

        [o_fwd_btn setAction:@selector(alternateForward:)];
        [o_bwd_btn setAction:@selector(alternateBackward:)];

    } else {
        /* change the accessibility help for the backward/forward buttons accordingly */
        [[o_bwd_btn cell] accessibilitySetOverrideValue:_NS("Click to go to the previous playlist item. Hold to skip backward through the current media.") forAttribute:NSAccessibilityDescriptionAttribute];
        [[o_fwd_btn cell] accessibilitySetOverrideValue:_NS("Click to go to the next playlist item. Hold to skip forward through the current media.") forAttribute:NSAccessibilityDescriptionAttribute];

        [o_fwd_btn setAction:@selector(fwd:)];
        [o_bwd_btn setAction:@selector(bwd:)];
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
    float f_updated;
    input_thread_t * p_input;

    switch([[NSApp currentEvent] type]) {
        case NSLeftMouseUp:
        case NSLeftMouseDown:
        case NSLeftMouseDragged:
            f_updated = [sender floatValue];
            break;

        default:
            return;
    }
    p_input = pl_CurrentInput(VLCIntf);
    if (p_input != NULL) {
        vlc_value_t pos;
        NSString * o_time;

        pos.f_float = f_updated / 10000.;
        var_Set(p_input, "position", pos);
        [o_time_sld setFloatValue: f_updated];

        o_time = [[VLCStringUtility sharedInstance] getCurrentTimeAsString: p_input negative:[o_time_fld timeRemaining]];
        [o_time_fld setStringValue: o_time];
        vlc_object_release(p_input);
    }
}

- (IBAction)fullscreen:(id)sender
{
    [[VLCCoreInteraction sharedInstance] toggleFullscreen];
}

#pragma mark -
#pragma mark Updaters

- (void)updateTimeSlider
{
    input_thread_t * p_input;
    p_input = pl_CurrentInput(VLCIntf);
    if (p_input) {
        NSString * o_time;
        vlc_value_t pos;
        float f_updated;

        var_Get(p_input, "position", &pos);
        f_updated = 10000. * pos.f_float;
        [o_time_sld setFloatValue: f_updated];

        o_time = [[VLCStringUtility sharedInstance] getCurrentTimeAsString: p_input negative:[o_time_fld timeRemaining]];

        mtime_t dur = input_item_GetDuration(input_GetItem(p_input));
        if (dur == -1) {
            [o_time_sld setHidden: YES];
            [o_time_sld_fancygradient_view setHidden: YES];
        } else {
            if ([o_time_sld isHidden] == YES) {
                bool b_buffering = false;
                input_state_e inputState = input_GetState(p_input);
                if (inputState == INIT_S || inputState == OPENING_S)
                    b_buffering = YES;

                [o_time_sld setHidden: b_buffering];
                [o_time_sld_fancygradient_view setHidden: b_buffering];
            }
        }
        [o_time_fld setStringValue: o_time];
        [o_time_fld setNeedsDisplay:YES];

        vlc_object_release(p_input);
    } else {
        [o_time_sld setFloatValue: 0.0];
        [o_time_fld setStringValue: @"00:00"];
        [o_time_sld setHidden: YES];
        [o_time_sld_fancygradient_view setHidden: YES];
    }
}

- (void)drawFancyGradientEffectForTimeSlider
{
    NSAutoreleasePool * o_pool = [[NSAutoreleasePool alloc] init];
    CGFloat f_value = [o_time_sld knobPosition];
    if (f_value > 7.5) {
        NSRect oldFrame = [o_time_sld_fancygradient_view frame];
        if (f_value != oldFrame.size.width) {
            if ([o_time_sld_fancygradient_view isHidden])
                [o_time_sld_fancygradient_view setHidden: NO];
            [o_time_sld_fancygradient_view setFrame: NSMakeRect(oldFrame.origin.x, oldFrame.origin.y, f_value, oldFrame.size.height)];
        }
    } else {
        NSRect frame;
        frame = [o_time_sld_fancygradient_view frame];
        if (frame.size.width > 0) {
            frame.size.width = 0;
            [o_time_sld_fancygradient_view setFrame: frame];
        }
        [o_time_sld_fancygradient_view setHidden: YES];
    }
    [o_pool release];
}

- (void)updateControls
{
    bool b_plmul = false;
    bool b_seekable = false;
    bool b_chapters = false;
    bool b_buffering = false;

    playlist_t * p_playlist = pl_Get(VLCIntf);

    PL_LOCK;
    b_plmul = playlist_CurrentSize(p_playlist) > 1;
    PL_UNLOCK;

    input_thread_t * p_input = playlist_CurrentInput(p_playlist);


    if (p_input) {
        input_state_e inputState = input_GetState(p_input);
        if (inputState == INIT_S || inputState == OPENING_S)
            b_buffering = YES;

        /* seekable streams */
        b_seekable = var_GetBool(p_input, "can-seek");

        /* chapters & titles */
        //FIXME! b_chapters = p_input->stream.i_area_nb > 1;

        vlc_object_release(p_input);
    }


    if (b_buffering) {
        [o_progress_bar startAnimation:self];
        [o_progress_bar setIndeterminate:YES];
        [o_progress_bar setHidden:NO];
    } else {
        [o_progress_bar stopAnimation:self];
        [o_progress_bar setHidden:YES];
    }

    [o_time_sld setEnabled: b_seekable];

    [o_fwd_btn setEnabled: (b_seekable || b_plmul || b_chapters)];
    [o_bwd_btn setEnabled: (b_seekable || b_plmul || b_chapters)];
}

- (void)setPause
{
    [o_play_btn setImage: o_pause_img];
    [o_play_btn setAlternateImage: o_pause_pressed_img];
    [o_play_btn setToolTip: _NS("Pause")];
}

- (void)setPlay
{
    [o_play_btn setImage: o_play_img];
    [o_play_btn setAlternateImage: o_play_pressed_img];
    [o_play_btn setToolTip: _NS("Play")];
}

- (void)setFullscreenState:(BOOL)b_fullscreen
{
    if (!b_nativeFullscreenMode)
        [o_fullscreen_btn setState:b_fullscreen];
}

@end


/*****************************************************************************
 * VLCMainWindowControlsBar
 *
 *  Holds all specific outlets, actions and code for the main window controls bar.
 *****************************************************************************/

@interface VLCMainWindowControlsBar (Internal)
- (void)addJumpButtons:(BOOL)b_fast;
- (void)removeJumpButtons:(BOOL)b_fast;
- (void)addPlaymodeButtons:(BOOL)b_fast;
- (void)removePlaymodeButtons:(BOOL)b_fast;
@end

@implementation VLCMainWindowControlsBar

- (void)awakeFromNib
{
    [super awakeFromNib];


    [o_stop_btn setToolTip: _NS("Stop")];
    [[o_stop_btn cell] accessibilitySetOverrideValue:_NS("Click to stop playback.") forAttribute:NSAccessibilityDescriptionAttribute];
    [[o_stop_btn cell] accessibilitySetOverrideValue:[o_stop_btn toolTip] forAttribute:NSAccessibilityTitleAttribute];

    [o_playlist_btn setToolTip: _NS("Show/Hide Playlist")];
    [[o_playlist_btn cell] accessibilitySetOverrideValue:_NS("Click to switch between video output and playlist. If no video is shown in the main window, this allows you to hide the playlist.") forAttribute:NSAccessibilityDescriptionAttribute];
    [[o_playlist_btn cell] accessibilitySetOverrideValue:[o_playlist_btn toolTip] forAttribute:NSAccessibilityTitleAttribute];

    [o_repeat_btn setToolTip: _NS("Repeat")];
    [[o_repeat_btn cell] accessibilitySetOverrideValue:_NS("Click to change repeat mode. There are 3 states: repeat one, repeat all and off.") forAttribute:NSAccessibilityDescriptionAttribute];
    [[o_repeat_btn cell] accessibilitySetOverrideValue:[o_repeat_btn toolTip] forAttribute:NSAccessibilityTitleAttribute];

    [o_shuffle_btn setToolTip: _NS("Shuffle")];
    [[o_shuffle_btn cell] accessibilitySetOverrideValue:[o_shuffle_btn toolTip] forAttribute:NSAccessibilityTitleAttribute];
    [[o_shuffle_btn cell] accessibilitySetOverrideValue:_NS("Click to enable or disable random playback.") forAttribute:NSAccessibilityDescriptionAttribute];

    [o_volume_sld setToolTip: _NS("Volume")];
    [[o_volume_sld cell] accessibilitySetOverrideValue:_NS("Click and move the mouse while keeping the button pressed to use this slider to change the volume.") forAttribute:NSAccessibilityDescriptionAttribute];
    [[o_volume_sld cell] accessibilitySetOverrideValue:[o_volume_sld toolTip] forAttribute:NSAccessibilityTitleAttribute];
    [o_volume_down_btn setToolTip: _NS("Mute")];
    [[o_volume_down_btn cell] accessibilitySetOverrideValue:_NS("Click to mute or unmute the audio.") forAttribute:NSAccessibilityDescriptionAttribute];
    [[o_volume_down_btn cell] accessibilitySetOverrideValue:[o_volume_down_btn toolTip] forAttribute:NSAccessibilityTitleAttribute];
    [o_volume_up_btn setToolTip: _NS("Full Volume")];
    [[o_volume_up_btn cell] accessibilitySetOverrideValue:_NS("Click to play the audio at maximum volume.") forAttribute:NSAccessibilityDescriptionAttribute];
    [[o_volume_up_btn cell] accessibilitySetOverrideValue:[o_volume_up_btn toolTip] forAttribute:NSAccessibilityTitleAttribute];

    [o_effects_btn setToolTip: _NS("Effects")];
    [[o_effects_btn cell] accessibilitySetOverrideValue:_NS("Click to show an Audio Effects panel featuring an equalizer and further filters.") forAttribute:NSAccessibilityDescriptionAttribute];
    [[o_effects_btn cell] accessibilitySetOverrideValue:[o_effects_btn toolTip] forAttribute:NSAccessibilityTitleAttribute];

    if (!b_dark_interface) {
        [o_stop_btn setImage: [NSImage imageNamed:@"stop"]];
        [o_stop_btn setAlternateImage: [NSImage imageNamed:@"stop-pressed"]];

        [o_playlist_btn setImage: [NSImage imageNamed:@"playlist-btn"]];
        [o_playlist_btn setAlternateImage: [NSImage imageNamed:@"playlist-btn-pressed"]];
        o_repeat_img = [[NSImage imageNamed:@"repeat"] retain];
        o_repeat_pressed_img = [[NSImage imageNamed:@"repeat-pressed"] retain];
        o_repeat_all_img  = [[NSImage imageNamed:@"repeat-all"] retain];
        o_repeat_all_pressed_img = [[NSImage imageNamed:@"repeat-all-pressed"] retain];
        o_repeat_one_img = [[NSImage imageNamed:@"repeat-one"] retain];
        o_repeat_one_pressed_img = [[NSImage imageNamed:@"repeat-one-pressed"] retain];
        o_shuffle_img = [[NSImage imageNamed:@"shuffle"] retain];
        o_shuffle_pressed_img = [[NSImage imageNamed:@"shuffle-pressed"] retain];
        o_shuffle_on_img = [[NSImage imageNamed:@"shuffle-blue"] retain];
        o_shuffle_on_pressed_img = [[NSImage imageNamed:@"shuffle-blue-pressed"] retain];

        [o_volume_down_btn setImage: [NSImage imageNamed:@"volume-low"]];
        [o_volume_track_view setImage: [NSImage imageNamed:@"volume-slider-track"]];
        [o_volume_up_btn setImage: [NSImage imageNamed:@"volume-high"]];
        [o_volume_sld setUsesBrightArtwork: YES];

        if (b_nativeFullscreenMode) {
            [o_effects_btn setImage: [NSImage imageNamed:@"effects-one-button"]];
            [o_effects_btn setAlternateImage: [NSImage imageNamed:@"effects-one-button-pressed"]];
        } else {
            [o_effects_btn setImage: [NSImage imageNamed:@"effects-double-buttons"]];
            [o_effects_btn setAlternateImage: [NSImage imageNamed:@"effects-double-buttons-pressed"]];
        }

        [o_fullscreen_btn setImage: [NSImage imageNamed:@"fullscreen-double-buttons"]];
        [o_fullscreen_btn setAlternateImage: [NSImage imageNamed:@"fullscreen-double-buttons-pressed"]];
    } else {
        [o_stop_btn setImage: [NSImage imageNamed:@"stop_dark"]];
        [o_stop_btn setAlternateImage: [NSImage imageNamed:@"stop-pressed_dark"]];

        [o_playlist_btn setImage: [NSImage imageNamed:@"playlist_dark"]];
        [o_playlist_btn setAlternateImage: [NSImage imageNamed:@"playlist-pressed_dark"]];
        o_repeat_img = [[NSImage imageNamed:@"repeat_dark"] retain];
        o_repeat_pressed_img = [[NSImage imageNamed:@"repeat-pressed_dark"] retain];
        o_repeat_all_img  = [[NSImage imageNamed:@"repeat-all-blue_dark"] retain];
        o_repeat_all_pressed_img = [[NSImage imageNamed:@"repeat-all-blue-pressed_dark"] retain];
        o_repeat_one_img = [[NSImage imageNamed:@"repeat-one-blue_dark"] retain];
        o_repeat_one_pressed_img = [[NSImage imageNamed:@"repeat-one-blue-pressed_dark"] retain];
        o_shuffle_img = [[NSImage imageNamed:@"shuffle_dark"] retain];
        o_shuffle_pressed_img = [[NSImage imageNamed:@"shuffle-pressed_dark"] retain];
        o_shuffle_on_img = [[NSImage imageNamed:@"shuffle-blue_dark"] retain];
        o_shuffle_on_pressed_img = [[NSImage imageNamed:@"shuffle-blue-pressed_dark"] retain];

        [o_volume_down_btn setImage: [NSImage imageNamed:@"volume-low_dark"]];
        [o_volume_track_view setImage: [NSImage imageNamed:@"volume-slider-track_dark"]];
        [o_volume_up_btn setImage: [NSImage imageNamed:@"volume-high_dark"]];
        [o_volume_sld setUsesBrightArtwork: NO];

        if (b_nativeFullscreenMode) {
            [o_effects_btn setImage: [NSImage imageNamed:@"effects-one-button_dark"]];
            [o_effects_btn setAlternateImage: [NSImage imageNamed:@"effects-one-button-pressed-dark"]];
        } else {
            [o_effects_btn setImage: [NSImage imageNamed:@"effects-double-buttons_dark"]];
            [o_effects_btn setAlternateImage: [NSImage imageNamed:@"effects-double-buttons-pressed_dark"]];
        }

        [o_fullscreen_btn setImage: [NSImage imageNamed:@"fullscreen-double-buttons_dark"]];
        [o_fullscreen_btn setAlternateImage: [NSImage imageNamed:@"fullscreen-double-buttons-pressed_dark"]];
    }
    [o_repeat_btn setImage: o_repeat_img];
    [o_repeat_btn setAlternateImage: o_repeat_pressed_img];
    [o_shuffle_btn setImage: o_shuffle_img];
    [o_shuffle_btn setAlternateImage: o_shuffle_pressed_img];

    BOOL b_mute = ![[VLCCoreInteraction sharedInstance] mute];
    [o_volume_sld setEnabled: b_mute];
    [o_volume_sld setMaxValue: [[VLCCoreInteraction sharedInstance] maxVolume]];
    [o_volume_up_btn setEnabled: b_mute];

    // remove fullscreen button for lion fullscreen
    if (b_nativeFullscreenMode) {
        NSRect frame;

        // == [o_fullscreen_btn frame].size.width;
        // button is already removed!
        float f_width = 29.;
#define moveItem(item) \
frame = [item frame]; \
frame.origin.x = f_width + frame.origin.x; \
[item setFrame: frame]

        moveItem(o_effects_btn);
        moveItem(o_volume_up_btn);
        moveItem(o_volume_sld);
        moveItem(o_volume_track_view);
        moveItem(o_volume_down_btn);
#undef moveItem

        // time field and progress bar are moved in super method!
    }


    b_show_jump_buttons = config_GetInt(VLCIntf, "macosx-show-playback-buttons");
    if (b_show_jump_buttons)
        [self addJumpButtons:YES];

    b_show_playmode_buttons = config_GetInt(VLCIntf, "macosx-show-playmode-buttons");
    if (!b_show_playmode_buttons)
        [self removePlaymodeButtons:YES];

    if (!config_GetInt(VLCIntf, "macosx-show-effects-button"))
        [self removeEffectsButton:YES];

    [[VLCMain sharedInstance] playbackModeUpdated];

}

#pragma mark -
#pragma mark interface customization


- (void)toggleEffectsButton
{
    if (config_GetInt(VLCIntf, "macosx-show-effects-button"))
        [self addEffectsButton:NO];
    else
        [self removeEffectsButton:NO];
}

- (void)addEffectsButton:(BOOL)b_fast
{
    if (!o_effects_btn)
        return;

    if (b_fast) {
        [o_effects_btn setHidden: NO];
    } else {
        [[o_effects_btn animator] setHidden: NO];
    }

#define moveItem(item) \
frame = [item frame]; \
frame.origin.x = frame.origin.x - f_space; \
if (b_fast) \
[item setFrame: frame]; \
else \
[[item animator] setFrame: frame]

    NSRect frame;
    float f_space = [o_effects_btn frame].size.width;
    // extra margin between button and volume up button
    if (b_nativeFullscreenMode)
        f_space += 2;


    moveItem(o_volume_up_btn);
    moveItem(o_volume_sld);
    moveItem(o_volume_track_view);
    moveItem(o_volume_down_btn);
    moveItem(o_time_fld);
#undef moveItem


    frame = [o_progress_view frame];
    frame.size.width = frame.size.width - f_space;
    if (b_fast)
        [o_progress_view setFrame: frame];
    else
        [[o_progress_view animator] setFrame: frame];

    if (!b_nativeFullscreenMode) {
        if (b_dark_interface) {
            [o_fullscreen_btn setImage: [NSImage imageNamed:@"fullscreen-double-buttons_dark"]];
            [o_fullscreen_btn setAlternateImage: [NSImage imageNamed:@"fullscreen-double-buttons-pressed_dark"]];
        } else {
            [o_fullscreen_btn setImage: [NSImage imageNamed:@"fullscreen-double-buttons"]];
            [o_fullscreen_btn setAlternateImage: [NSImage imageNamed:@"fullscreen-double-buttons-pressed"]];
        }
    }

    [o_bottombar_view setNeedsDisplay:YES];
}

- (void)removeEffectsButton:(BOOL)b_fast
{
    if (!o_effects_btn)
        return;

    [o_effects_btn setHidden: YES];

#define moveItem(item) \
frame = [item frame]; \
frame.origin.x = frame.origin.x + f_space; \
if (b_fast) \
[item setFrame: frame]; \
else \
[[item animator] setFrame: frame]

    NSRect frame;
    float f_space = [o_effects_btn frame].size.width;
    // extra margin between button and volume up button
    if (b_nativeFullscreenMode)
        f_space += 2;

    moveItem(o_volume_up_btn);
    moveItem(o_volume_sld);
    moveItem(o_volume_track_view);
    moveItem(o_volume_down_btn);
    moveItem(o_time_fld);
#undef moveItem


    frame = [o_progress_view frame];
    frame.size.width = frame.size.width + f_space;
    if (b_fast)
        [o_progress_view setFrame: frame];
    else
        [[o_progress_view animator] setFrame: frame];

    if (!b_nativeFullscreenMode) {
        if (b_dark_interface) {
            [[o_fullscreen_btn animator] setImage: [NSImage imageNamed:@"fullscreen-one-button_dark"]];
            [[o_fullscreen_btn animator] setAlternateImage: [NSImage imageNamed:@"fullscreen-one-button-pressed_dark"]];
        } else {
            [[o_fullscreen_btn animator] setImage: [NSImage imageNamed:@"fullscreen-one-button"]];
            [[o_fullscreen_btn animator] setAlternateImage: [NSImage imageNamed:@"fullscreen-one-button-pressed"]];
        }
    }

    [o_bottombar_view setNeedsDisplay:YES];
}

- (void)toggleJumpButtons
{
    b_show_jump_buttons = config_GetInt(VLCIntf, "macosx-show-playback-buttons");

    if (b_show_jump_buttons)
        [self addJumpButtons:NO];
    else
        [self removeJumpButtons:NO];
}

- (void)addJumpButtons:(BOOL)b_fast
{
    NSRect preliminaryFrame = [o_bwd_btn frame];
    BOOL b_enabled = [o_bwd_btn isEnabled];
    preliminaryFrame.size.width = 29.;
    o_prev_btn = [[NSButton alloc] initWithFrame:preliminaryFrame];
    [o_prev_btn setButtonType: NSMomentaryChangeButton];
    [o_prev_btn setBezelStyle:NSRegularSquareBezelStyle];
    [o_prev_btn setBordered:NO];
    [o_prev_btn setTarget:self];
    [o_prev_btn setAction:@selector(prev:)];
    [o_prev_btn setToolTip: _NS("Previous")];
    [[o_prev_btn cell] accessibilitySetOverrideValue:_NS("Previous") forAttribute:NSAccessibilityTitleAttribute];
    [[o_prev_btn cell] accessibilitySetOverrideValue:_NS("Click to go to the previous playlist item.") forAttribute:NSAccessibilityDescriptionAttribute];
    [o_prev_btn setEnabled: b_enabled];

    o_next_btn = [[NSButton alloc] initWithFrame:preliminaryFrame];
    [o_next_btn setButtonType: NSMomentaryChangeButton];
    [o_next_btn setBezelStyle:NSRegularSquareBezelStyle];
    [o_next_btn setBordered:NO];
    [o_next_btn setTarget:self];
    [o_next_btn setAction:@selector(next:)];
    [o_next_btn setToolTip: _NS("Next")];
    [[o_next_btn cell] accessibilitySetOverrideValue:_NS("Next") forAttribute:NSAccessibilityTitleAttribute];
    [[o_next_btn cell] accessibilitySetOverrideValue:_NS("Click to go to the next playlist item.") forAttribute:NSAccessibilityDescriptionAttribute];
    [o_next_btn setEnabled: b_enabled];

    if (b_dark_interface) {
        [o_prev_btn setImage: [NSImage imageNamed:@"previous-6btns-dark"]];
        [o_prev_btn setAlternateImage: [NSImage imageNamed:@"previous-6btns-dark-pressed"]];
        [o_next_btn setImage: [NSImage imageNamed:@"next-6btns-dark"]];
        [o_next_btn setAlternateImage: [NSImage imageNamed:@"next-6btns-dark-pressed"]];
    } else {
        [o_prev_btn setImage: [NSImage imageNamed:@"previous-6btns"]];
        [o_prev_btn setAlternateImage: [NSImage imageNamed:@"previous-6btns-pressed"]];
        [o_next_btn setImage: [NSImage imageNamed:@"next-6btns"]];
        [o_next_btn setAlternateImage: [NSImage imageNamed:@"next-6btns-pressed"]];
    }

    NSRect frame;
    frame = [o_bwd_btn frame];
    frame.size.width--;
    [o_bwd_btn setFrame:frame];
    frame = [o_fwd_btn frame];
    frame.size.width--;
    [o_fwd_btn setFrame:frame];

#define moveItem(item) \
frame = [item frame]; \
frame.origin.x = frame.origin.x + f_space; \
if (b_fast) \
    [item setFrame: frame]; \
else \
    [[item animator] setFrame: frame]

    float f_space = 29.;
    moveItem(o_bwd_btn);
    f_space = 28.;
    moveItem(o_play_btn);
    moveItem(o_fwd_btn);
    f_space = 28. * 2;
    moveItem(o_stop_btn);
    moveItem(o_playlist_btn);
    moveItem(o_repeat_btn);
    moveItem(o_shuffle_btn);
#undef moveItem

    frame = [o_progress_view frame];
    frame.size.width = frame.size.width - f_space;
    frame.origin.x = frame.origin.x + f_space;
    if (b_fast)
        [o_progress_view setFrame: frame];
    else
        [[o_progress_view animator] setFrame: frame];

    if (b_dark_interface) {
        [[o_fwd_btn animator] setImage:[NSImage imageNamed:@"forward-6btns-dark"]];
        [[o_fwd_btn animator] setAlternateImage:[NSImage imageNamed:@"forward-6btns-dark-pressed"]];
        [[o_bwd_btn animator] setImage:[NSImage imageNamed:@"backward-6btns-dark"]];
        [[o_bwd_btn animator] setAlternateImage:[NSImage imageNamed:@"backward-6btns-dark-pressed"]];
    } else {
        [[o_fwd_btn animator] setImage:[NSImage imageNamed:@"forward-6btns"]];
        [[o_fwd_btn animator] setAlternateImage:[NSImage imageNamed:@"forward-6btns-pressed"]];
        [[o_bwd_btn animator] setImage:[NSImage imageNamed:@"backward-6btns"]];
        [[o_bwd_btn animator] setAlternateImage:[NSImage imageNamed:@"backward-6btns-pressed"]];
    }

    preliminaryFrame.origin.x = [o_prev_btn frame].origin.x + [o_prev_btn frame].size.width + [o_bwd_btn frame].size.width + [o_play_btn frame].size.width + [o_fwd_btn frame].size.width;
    [o_next_btn setFrame: preliminaryFrame];

    // wait until the animation is done, if displayed
    if (b_fast) {
        [o_bottombar_view addSubview:o_prev_btn];
        [o_bottombar_view addSubview:o_next_btn];
    } else {
        [o_bottombar_view performSelector:@selector(addSubview:) withObject:o_prev_btn afterDelay:.2];
        [o_bottombar_view performSelector:@selector(addSubview:) withObject:o_next_btn afterDelay:.2];
    }

    [self toggleForwardBackwardMode: YES];
}

- (void)removeJumpButtons:(BOOL)b_fast
{
    if (!o_prev_btn || !o_next_btn)
        return;

    if (b_fast) {
        [o_prev_btn setHidden: YES];
        [o_next_btn setHidden: YES];
    } else {
        [[o_prev_btn animator] setHidden: YES];
        [[o_next_btn animator] setHidden: YES];
    }
    [o_prev_btn removeFromSuperviewWithoutNeedingDisplay];
    [o_next_btn removeFromSuperviewWithoutNeedingDisplay];
    [o_prev_btn release];
    o_prev_btn = NULL;
    [o_next_btn release];
    o_next_btn = NULL;

    NSRect frame;
    frame = [o_bwd_btn frame];
    frame.size.width++;
    [o_bwd_btn setFrame:frame];
    frame = [o_fwd_btn frame];
    frame.size.width++;
    [o_fwd_btn setFrame:frame];

#define moveItem(item) \
frame = [item frame]; \
frame.origin.x = frame.origin.x - f_space; \
if (b_fast) \
    [item setFrame: frame]; \
else \
    [[item animator] setFrame: frame]

    float f_space = 29.;
    moveItem(o_bwd_btn);
    f_space = 28.;
    moveItem(o_play_btn);
    moveItem(o_fwd_btn);
    f_space = 28. * 2;
    moveItem(o_stop_btn);
    moveItem(o_playlist_btn);
    moveItem(o_repeat_btn);
    moveItem(o_shuffle_btn);
#undef moveItem

    frame = [o_progress_view frame];
    frame.size.width = frame.size.width + f_space;
    frame.origin.x = frame.origin.x - f_space;
    if (b_fast)
        [o_progress_view setFrame: frame];
    else
        [[o_progress_view animator] setFrame: frame];

    if (b_dark_interface) {
        [[o_fwd_btn animator] setImage:[NSImage imageNamed:@"forward-3btns-dark"]];
        [[o_fwd_btn animator] setAlternateImage:[NSImage imageNamed:@"forward-3btns-dark-pressed"]];
        [[o_bwd_btn animator] setImage:[NSImage imageNamed:@"backward-3btns-dark"]];
        [[o_bwd_btn animator] setAlternateImage:[NSImage imageNamed:@"backward-3btns-dark-pressed"]];
    } else {
        [[o_fwd_btn animator] setImage:[NSImage imageNamed:@"forward-3btns"]];
        [[o_fwd_btn animator] setAlternateImage:[NSImage imageNamed:@"forward-3btns-pressed"]];
        [[o_bwd_btn animator] setImage:[NSImage imageNamed:@"backward-3btns"]];
        [[o_bwd_btn animator] setAlternateImage:[NSImage imageNamed:@"backward-3btns-pressed"]];
    }

    [self toggleForwardBackwardMode: NO];

    [o_bottombar_view setNeedsDisplay:YES];
}

- (void)togglePlaymodeButtons
{
    b_show_playmode_buttons = config_GetInt(VLCIntf, "macosx-show-playmode-buttons");

    if (b_show_playmode_buttons)
        [self addPlaymodeButtons:NO];
    else
        [self removePlaymodeButtons:NO];
}

- (void)addPlaymodeButtons:(BOOL)b_fast
{
    NSRect frame;
    float f_space = [o_repeat_btn frame].size.width + [o_shuffle_btn frame].size.width - 6.;

    if (b_dark_interface) {
        [[o_playlist_btn animator] setImage:[NSImage imageNamed:@"playlist_dark"]];
        [[o_playlist_btn animator] setAlternateImage:[NSImage imageNamed:@"playlist-pressed_dark"]];
    } else {
        [[o_playlist_btn animator] setImage:[NSImage imageNamed:@"playlist-btn"]];
        [[o_playlist_btn animator] setAlternateImage:[NSImage imageNamed:@"playlist-btn-pressed"]];
    }
    frame = [o_playlist_btn frame];
    frame.size.width--;
    [o_playlist_btn setFrame:frame];

    if (b_fast) {
        [o_repeat_btn setHidden: NO];
        [o_shuffle_btn setHidden: NO];
    } else {
        [[o_repeat_btn animator] setHidden: NO];
        [[o_shuffle_btn animator] setHidden: NO];
    }

    frame = [o_progress_view frame];
    frame.size.width = frame.size.width - f_space;
    frame.origin.x = frame.origin.x + f_space;
    if (b_fast)
        [o_progress_view setFrame: frame];
    else
        [[o_progress_view animator] setFrame: frame];
}

- (void)removePlaymodeButtons:(BOOL)b_fast
{
    NSRect frame;
    float f_space = [o_repeat_btn frame].size.width + [o_shuffle_btn frame].size.width - 6.;
    [o_repeat_btn setHidden: YES];
    [o_shuffle_btn setHidden: YES];

    if (b_dark_interface) {
        [[o_playlist_btn animator] setImage:[NSImage imageNamed:@"playlist-1btn-dark"]];
        [[o_playlist_btn animator] setAlternateImage:[NSImage imageNamed:@"playlist-1btn-dark-pressed"]];
    } else {
        [[o_playlist_btn animator] setImage:[NSImage imageNamed:@"playlist-1btn"]];
        [[o_playlist_btn animator] setAlternateImage:[NSImage imageNamed:@"playlist-1btn-pressed"]];
    }
    frame = [o_playlist_btn frame];
    frame.size.width++;
    [o_playlist_btn setFrame:frame];

    frame = [o_progress_view frame];
    frame.size.width = frame.size.width + f_space;
    frame.origin.x = frame.origin.x - f_space;
    if (b_fast)
        [o_progress_view setFrame: frame];
    else
        [[o_progress_view animator] setFrame: frame];
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
    [o_repeat_btn setImage: o_repeat_one_img];
    [o_repeat_btn setAlternateImage: o_repeat_one_pressed_img];
}

- (void)setRepeatAll
{
    [o_repeat_btn setImage: o_repeat_all_img];
    [o_repeat_btn setAlternateImage: o_repeat_all_pressed_img];
}

- (void)setRepeatOff
{
    [o_repeat_btn setImage: o_repeat_img];
    [o_repeat_btn setAlternateImage: o_repeat_pressed_img];
}

- (IBAction)repeat:(id)sender
{
    vlc_value_t looping,repeating;
    intf_thread_t * p_intf = VLCIntf;
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
    playlist_t *p_playlist = pl_Get(VLCIntf);
    b_value = var_GetBool(p_playlist, "random");

    if (b_value) {
        [o_shuffle_btn setImage: o_shuffle_on_img];
        [o_shuffle_btn setAlternateImage: o_shuffle_on_pressed_img];
    } else {
        [o_shuffle_btn setImage: o_shuffle_img];
        [o_shuffle_btn setAlternateImage: o_shuffle_pressed_img];
    }
}

- (IBAction)shuffle:(id)sender
{
    [[VLCCoreInteraction sharedInstance] shuffle];
    [self setShuffle];
}

- (IBAction)volumeAction:(id)sender
{
    if (sender == o_volume_sld)
        [[VLCCoreInteraction sharedInstance] setVolume: [sender intValue]];
    else if (sender == o_volume_down_btn)
        [[VLCCoreInteraction sharedInstance] toggleMute];
    else
        [[VLCCoreInteraction sharedInstance] setVolume: AOUT_VOLUME_MAX];
}

- (IBAction)effects:(id)sender
{
    [[VLCMainMenu sharedInstance] showAudioEffects: sender];
}

#pragma mark -
#pragma mark Extra updaters

- (void)updateVolumeSlider
{
    int i_volume = [[VLCCoreInteraction sharedInstance] volume];
    BOOL b_muted = [[VLCCoreInteraction sharedInstance] mute];

    if (!b_muted)
        [o_volume_sld setIntValue: i_volume];
    else
        [o_volume_sld setIntValue: 0];

    [o_volume_sld setEnabled: !b_muted];
    [o_volume_up_btn setEnabled: !b_muted];
}

- (void)updateControls
{
    [super updateControls];

    bool b_input = false;
    bool b_seekable = false;
    bool b_plmul = false;
    bool b_control = false;
    bool b_chapters = false;

    playlist_t * p_playlist = pl_Get(VLCIntf);

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

    [o_stop_btn setEnabled: b_input];

    if (b_show_jump_buttons) {
        [o_prev_btn setEnabled: (b_seekable || b_plmul || b_chapters)];
        [o_next_btn setEnabled: (b_seekable || b_plmul || b_chapters)];
    }

    [[VLCMainMenu sharedInstance] setRateControlsEnabled: b_control];
}

@end
