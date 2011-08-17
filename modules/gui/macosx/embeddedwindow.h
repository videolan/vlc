/*****************************************************************************
 * embeddedwindow.h: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2005-2007 the VideoLAN team
 * $Id$
 *
 * Authors: Benjamin Pracht <bigben at videolan dot org>
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

/*****************************************************************************
 * VLCEmbeddedWindow interface
 *****************************************************************************/

#import "CompatibilityFixes.h"
#import "misc.h"

@interface VLCEmbeddedWindow : NSWindow <NSWindowDelegate, NSAnimationDelegate>
{
    IBOutlet id o_btn_backward;
    IBOutlet id o_btn_forward;
    IBOutlet id o_btn_fullscreen;
    IBOutlet id o_btn_equalizer;
    IBOutlet id o_btn_playlist;
    IBOutlet id o_btn_play;
    IBOutlet id o_btn_prev;
    IBOutlet id o_btn_stop;
    IBOutlet id o_btn_next;
    IBOutlet id o_btn_volume_down;
    IBOutlet id o_volumeslider;
    IBOutlet id o_btn_volume_up;
    IBOutlet id o_backgroundimg_right;
    IBOutlet id o_backgroundimg_middle;
    IBOutlet id o_timeslider;
    IBOutlet id o_main_pgbar;
    IBOutlet id o_time;
    IBOutlet id o_scrollfield;
    IBOutlet id o_horizontal_split;
    IBOutlet id o_vertical_split;
    IBOutlet id o_videosubview;
    IBOutlet id o_view;
    IBOutlet id o_background_view;
	IBOutlet id o_searchfield;
	IBOutlet id o_status;
	IBOutlet id o_playlist;
	IBOutlet id o_playlist_view;
	IBOutlet id o_playlist_table;
	IBOutlet id o_vlc_main;
    IBOutlet id o_video_view;

    NSImage * o_img_play;
    NSImage * o_img_play_pressed;
    NSImage * o_img_pause;
    NSImage * o_img_pause_pressed;

    VLCWindow       * o_fullscreen_window;
    NSViewAnimation * o_fullscreen_anim1;
    NSViewAnimation * o_fullscreen_anim2;
    NSViewAnimation * o_makekey_anim;
    NSView          * o_temp_view;
    /* set to yes if we are fullscreen and all animations are over */
    BOOL              b_fullscreen;
    NSRecursiveLock * o_animation_lock;

    BOOL              b_window_is_invisible;

    NSSize videoRatio;
    NSInteger originalLevel;
}

- (void)controlTintChanged;

- (id)videoView;

- (void)setTime: (NSString *)o_arg_ime position: (float)f_position;
- (id)getPgbar;
- (void)playStatusUpdated: (int)i_status;
- (void)setSeekable: (BOOL)b_seekable;
- (void)setStop:(BOOL)b_input;
- (void)setPrev:(BOOL)b_input;
- (void)setNext:(BOOL)b_input;
- (void)setVolumeEnabled:(BOOL)b_input;

- (void)setScrollString:(NSString *)o_string;

- (void)setVolumeSlider:(float)f_level;

- (void)setVideoRatio:(NSSize)ratio;

- (NSView *)mainView;

- (IBAction)togglePlaylist:(id)sender;

- (BOOL)isFullscreen;

- (void)lockFullscreenAnimation;
- (void)unlockFullscreenAnimation;

- (void)enterFullscreen;
- (void)leaveFullscreen;
/* Allows leaving fullscreen by simply fading out the display */
- (void)leaveFullscreenAndFadeOut: (BOOL)fadeout;

/* private */
- (void)hasEndedFullscreen;
- (void)hasBecomeFullscreen;

- (void)setFrameOnMainThread:(NSData*)packedargs;
@end

/*****************************************************************************
 * embeddedbackground
 *****************************************************************************/


@interface embeddedbackground : NSView
{
    IBOutlet id o_window;
    IBOutlet id o_timeslider;
    IBOutlet id o_main_pgbar;
    IBOutlet id o_time;
    IBOutlet id o_scrollfield;
    IBOutlet id o_searchfield;
    IBOutlet id o_btn_backward;
    IBOutlet id o_btn_forward;
    IBOutlet id o_btn_fullscreen;
    IBOutlet id o_btn_equalizer;
    IBOutlet id o_btn_playlist;
    IBOutlet id o_btn_play;
    IBOutlet id o_btn_prev;
    IBOutlet id o_btn_stop;
    IBOutlet id o_btn_next;
    IBOutlet id o_btn_volume_down;
    IBOutlet id o_volumeslider;
    IBOutlet id o_btn_volume_up;
    
    NSPoint dragStart;
}

@end

/*****************************************************************************
 * statusbar
 *****************************************************************************/


@interface statusbar : NSView
{
    IBOutlet id o_text;
	
	BOOL mainwindow;
}

@end
