/*****************************************************************************
 * MainWindow.h: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2002-2013 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Felix Paul Kühne <fkuehne -at- videolan -dot- org>
 *          Jon Lech Johansen <jon-vl@nanocrew.net>
 *          Christophe Massiot <massiot@via.ecp.fr>
 *          Derk-Jan Hartman <hartman at videolan.org>
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
#import "PXSourceList.h"
#import "PXSourceListDataSource.h"

#import <vlc_input.h>
#import <vlc_vout_window.h>

#import "Windows.h"
#import "misc.h"
#import "fspanel.h"
#import "MainWindowTitle.h"

@class VLCDetachedVideoWindow;
@class VLCMainWindowControlsBar;
@class VLCVoutView;

@interface VLCMainWindow : VLCVideoWindowCommon <PXSourceListDataSource, PXSourceListDelegate, NSWindowDelegate, NSAnimationDelegate, NSSplitViewDelegate> {

    IBOutlet id o_search_fld;

    IBOutlet id o_playlist_table;
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

    NSUInteger i_lastSplitViewHeight;
    NSUInteger i_lastLeftSplitViewWidth;

    NSMutableArray *o_sidebaritems;

    /* this is only true, when we have NO video playing inside the main window */
    BOOL b_nonembedded;

    BOOL b_podcastView_displayed;

    VLCColorView * o_color_backdrop;

    NSRect frameBeforePlayback;
}
+ (VLCMainWindow *)sharedInstance;
@property (readonly) BOOL nativeFullscreenMode;
@property (readwrite) BOOL nonembedded;

@property (readonly) VLCFSPanel* fsPanel;

- (VLCMainWindowControlsBar *)controlsBar;

- (IBAction)togglePlaylist:(id)sender;

- (IBAction)dropzoneButtonAction:(id)sender;

- (IBAction)addPodcast:(id)sender;
- (IBAction)addPodcastWindowAction:(id)sender;
- (IBAction)removePodcast:(id)sender;
- (IBAction)removePodcastWindowAction:(id)sender;

- (void)windowResizedOrMoved:(NSNotification *)notification;

- (void)toggleLeftSubSplitView;
- (void)showDropZone;
- (void)hideDropZone;
- (void)updateTimeSlider;
- (void)updateWindow;
- (void)updateName;
- (void)setPause;
- (void)setPlay;
- (void)updateVolumeSlider;

- (void)showFullscreenController;

- (void)videoplayWillBeStarted;
- (void)setVideoplayEnabled;

@end

@interface VLCDetachedVideoWindow : VLCVideoWindowCommon
{
    VLCColorView * o_color_backdrop;
}

@end
