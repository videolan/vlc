/*****************************************************************************
 * MainWindow.h: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2002-2012 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Felix Paul Kühne <fkuehne -at- videolan -dot- org>
 *          Jon Lech Johansen <jon-vl@nanocrew.net>
 *          Christophe Massiot <massiot@via.ecp.fr>
 *          Derk-Jan Hartman <hartman at videolan.org>
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
#import "PXSourceList.h"
#import "PXSourceListDataSource.h"
#import <vlc_input.h>
#import "Windows.h"
#import "misc.h"
#import "fspanel.h"
#import "MainWindowTitle.h"

@interface VLCMainWindow : VLCVideoWindowCommon <PXSourceListDataSource, PXSourceListDelegate, NSWindowDelegate, NSAnimationDelegate, NSSplitViewDelegate> {
    IBOutlet id o_play_btn;
    IBOutlet id o_bwd_btn;
    IBOutlet id o_fwd_btn;
    IBOutlet id o_stop_btn;
    IBOutlet id o_playlist_btn;
    IBOutlet id o_repeat_btn;
    IBOutlet id o_shuffle_btn;
    IBOutlet id o_effects_btn;
    IBOutlet id o_fullscreen_btn;
    IBOutlet id o_search_fld;
    IBOutlet id o_topbar_view;
    IBOutlet id o_volume_sld;
    IBOutlet id o_volume_track_view;
    IBOutlet id o_volume_down_btn;
    IBOutlet id o_volume_up_btn;
    IBOutlet id o_progress_view;
    IBOutlet id o_time_sld;
    IBOutlet id o_time_sld_fancygradient_view;
    IBOutlet id o_time_fld;
    IBOutlet id o_progress_bar;
    IBOutlet id o_bottombar_view;
    IBOutlet id o_time_sld_background;
    IBOutlet id o_playlist_table;
    IBOutlet id o_video_view;
    IBOutlet id o_split_view;
    IBOutlet id o_left_split_view;
    IBOutlet id o_right_split_view;
    IBOutlet id o_sidebar_view;
    IBOutlet id o_sidebar_scrollview;
    IBOutlet id o_chosen_category_lbl;

    IBOutlet id o_dropzone_view;
    IBOutlet id o_dropzone_btn;
    IBOutlet id o_dropzone_lbl;
    IBOutlet id o_dropzone_box;

    IBOutlet VLCFSPanel *o_fspanel;
    IBOutlet id o_resize_view;
    IBOutlet id o_detached_resize_view;

    IBOutlet id o_detached_play_btn;
    IBOutlet id o_detached_fwd_btn;
    IBOutlet id o_detached_bwd_btn;
    IBOutlet id o_detached_fullscreen_btn;
    IBOutlet id o_detached_time_fld;
    IBOutlet id o_detached_time_sld;
    IBOutlet id o_detached_time_sld_background;
    IBOutlet id o_detached_progress_bar;
    IBOutlet id o_detached_time_sld_fancygradient_view;
    IBOutlet id o_detached_bottombar_view;
    IBOutlet id o_detached_video_window;

    IBOutlet id o_podcast_view;
    IBOutlet id o_podcast_add_btn;
    IBOutlet id o_podcast_remove_btn;
    IBOutlet id o_podcast_subscribe_window;
    IBOutlet id o_podcast_subscribe_title_lbl;
    IBOutlet id o_podcast_subscribe_subtitle_lbl;
    IBOutlet id o_podcast_subscribe_url_fld;
    IBOutlet id o_podcast_subscribe_cancel_btn;
    IBOutlet id o_podcast_subscribe_ok_btn;
    IBOutlet id o_podcast_unsubscribe_window;
    IBOutlet id o_podcast_unsubscribe_title_lbl;
    IBOutlet id o_podcast_unsubscribe_subtitle_lbl;
    IBOutlet id o_podcast_unsubscribe_pop;
    IBOutlet id o_podcast_unsubscribe_ok_btn;
    IBOutlet id o_podcast_unsubscribe_cancel_btn;

    BOOL b_nativeFullscreenMode;
    BOOL b_video_playback_enabled;
    BOOL b_dropzone_active;
    BOOL b_splitview_removed;
    BOOL b_minimized_view;
    BOOL b_show_jump_buttons;
    BOOL b_show_playmode_buttons;
    int i_lastSplitViewHeight;
    input_state_e cachedInputState;

    NSImage * o_pause_img;
    NSImage * o_pause_pressed_img;
    NSImage * o_play_img;
    NSImage * o_play_pressed_img;
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

    NSTimeInterval last_fwd_event;
    NSTimeInterval last_bwd_event;
    BOOL just_triggered_next;
    BOOL just_triggered_previous;
    NSButton * o_prev_btn;
    NSButton * o_next_btn;

    NSMutableArray *o_sidebaritems;

    BOOL              b_nonembedded;
    BOOL              b_podcastView_displayed;

    VLCWindow       * o_fullscreen_window;
    NSViewAnimation * o_fullscreen_anim1;
    NSViewAnimation * o_fullscreen_anim2;
    NSViewAnimation * o_makekey_anim;
    NSView          * o_temp_view;
    /* set to yes if we are fullscreen and all animations are over */
    BOOL              b_fullscreen;
    BOOL              b_window_is_invisible;
    NSRecursiveLock * o_animation_lock;
    NSSize nativeVideoSize;

    NSTimer *t_hide_mouse_timer;

    VLCColorView * o_color_backdrop;
    NSInteger i_originalLevel;

    VLCWindow *o_extra_video_window;
    id o_current_video_window;

    NSRect frameBeforePlayback;
}
+ (VLCMainWindow *)sharedInstance;
@property (readonly) BOOL fullscreen;

- (IBAction)play:(id)sender;
- (IBAction)prev:(id)sender;
- (IBAction)backward:(id)sender;
- (IBAction)bwd:(id)sender;
- (IBAction)next:(id)sender;
- (IBAction)forward:(id)sender;
- (IBAction)fwd:(id)sender;
- (IBAction)stop:(id)sender;
- (IBAction)togglePlaylist:(id)sender;
- (IBAction)repeat:(id)sender;
- (IBAction)shuffle:(id)sender;
- (IBAction)timeSliderAction:(id)sender;
- (IBAction)volumeAction:(id)sender;
- (IBAction)effects:(id)sender;
- (IBAction)fullscreen:(id)sender;
- (IBAction)dropzoneButtonAction:(id)sender;

- (IBAction)addPodcast:(id)sender;
- (IBAction)addPodcastWindowAction:(id)sender;
- (IBAction)removePodcast:(id)sender;
- (IBAction)removePodcastWindowAction:(id)sender;

- (void)windowResizedOrMoved:(NSNotification *)notification;

- (void)showDropZone;
- (void)hideDropZone;
- (void)showSplitView;
- (void)hideSplitView;
- (void)updateTimeSlider;
- (void)updateVolumeSlider;
- (void)updateWindow;
- (void)updateName;
- (void)setPause;
- (void)setPlay;
- (void)setRepeatOne;
- (void)setRepeatAll;
- (void)setRepeatOff;
- (void)setShuffle;
- (void)toggleJumpButtons;
- (void)togglePlaymodeButtons;

- (void)drawFancyGradientEffectForTimeSlider;

- (id)videoView;
- (void)setupVideoView;
- (void)setVideoplayEnabled;
- (void)resizeWindow;
- (void)setNativeVideoSize:(NSSize)size;

- (void)hideMouseCursor:(NSTimer *)timer;
- (void)recreateHideMouseTimer;

/* fullscreen handling */
- (void)showFullscreenController;
- (void)lockFullscreenAnimation;
- (void)unlockFullscreenAnimation;
- (void)enterFullscreen;
- (void)leaveFullscreen;
- (void)leaveFullscreenAndFadeOut: (BOOL)fadeout;
- (void)hasEndedFullscreen;
- (void)hasBecomeFullscreen;

/* lion's native fullscreen handling */
- (void)windowWillEnterFullScreen:(NSNotification *)notification;
- (void)windowDidEnterFullScreen:(NSNotification *)notification;
- (void)windowWillExitFullScreen:(NSNotification *)notification;

@end

@interface VLCDetachedVideoWindow : VLCVideoWindowCommon

@end