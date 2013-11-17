/*****************************************************************************
 * ControlsBar.h: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2012 VLC authors and VideoLAN
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

#import <Cocoa/Cocoa.h>
#import "CompatibilityFixes.h"
#import "misc.h"

@class VLCFSPanel;

/*****************************************************************************
 * VLCControlsBarCommon
 *
 *  Holds all outlets, actions and code common for controls bar in detached
 *  and in main window.
 *****************************************************************************/

@interface VLCControlsBarCommon : NSObject
{
    IBOutlet id o_bottombar_view;

    IBOutlet id o_play_btn;
    IBOutlet id o_bwd_btn;
    IBOutlet id o_fwd_btn;

    IBOutlet id o_progress_view;
    IBOutlet id o_time_sld;
    IBOutlet id o_time_sld_fancygradient_view;
    IBOutlet id o_time_sld_background;
    IBOutlet id o_progress_bar;

    IBOutlet id o_time_fld;
    IBOutlet id o_fullscreen_btn;
    IBOutlet id o_resize_view;

    NSImage * o_pause_img;
    NSImage * o_pause_pressed_img;
    NSImage * o_play_img;
    NSImage * o_play_pressed_img;

    NSTimeInterval last_fwd_event;
    NSTimeInterval last_bwd_event;
    BOOL just_triggered_next;
    BOOL just_triggered_previous;

    BOOL b_nativeFullscreenMode;
    BOOL b_dark_interface;

}

@property (readonly) id bottomBarView;

- (CGFloat)height;
- (void)toggleForwardBackwardMode:(BOOL)b_alt;

- (IBAction)play:(id)sender;
- (IBAction)bwd:(id)sender;
- (IBAction)fwd:(id)sender;

- (IBAction)timeSliderAction:(id)sender;
- (IBAction)fullscreen:(id)sender;


- (void)updateTimeSlider;
- (void)drawFancyGradientEffectForTimeSlider;
- (void)updateControls;
- (void)setPause;
- (void)setPlay;
- (void)setFullscreenState:(BOOL)b_fullscreen;

@end


/*****************************************************************************
 * VLCMainWindowControlsBar
 *
 *  Holds all specific outlets, actions and code for the main window controls bar.
 *****************************************************************************/

@interface VLCMainWindowControlsBar : VLCControlsBarCommon
{
    IBOutlet id o_stop_btn;

    IBOutlet id o_playlist_btn;
    IBOutlet id o_repeat_btn;
    IBOutlet id o_shuffle_btn;

    IBOutlet VLCVolumeSliderCommon * o_volume_sld;
    IBOutlet id o_volume_track_view;
    IBOutlet id o_volume_down_btn;
    IBOutlet id o_volume_up_btn;

    IBOutlet id o_effects_btn;

    NSImage * o_repeat_img;
    NSImage * o_repeat_pressed_img;
    NSImage * o_repeat_all_img;
    NSImage * o_repeat_all_pressed_img;
    NSImage * o_repeat_one_img;
    NSImage * o_repeat_one_pressed_img;
    NSImage * o_shuffle_img;
    NSImage * o_shuffle_pressed_img;
    NSImage * o_shuffle_on_img;
    NSImage * o_shuffle_on_pressed_img;

    NSButton * o_prev_btn;
    NSButton * o_next_btn;

    BOOL b_show_jump_buttons;
    BOOL b_show_playmode_buttons;
}

- (IBAction)stop:(id)sender;

- (IBAction)shuffle:(id)sender;
- (IBAction)volumeAction:(id)sender;
- (IBAction)effects:(id)sender;


- (void)setRepeatOne;
- (void)setRepeatAll;
- (void)setRepeatOff;
- (IBAction)repeat:(id)sender;

- (void)setShuffle;
- (IBAction)shuffle:(id)sender;

- (void)toggleEffectsButton;
- (void)toggleJumpButtons;
- (void)togglePlaymodeButtons;

- (void)updateVolumeSlider;
- (void)updateControls;

@end

