/*****************************************************************************
 * playlist.h: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2002-2012 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Jon Lech Johansen <jon-vl@nanocrew.net>
 *          Derk-Jan Hartman <hartman at videolan dot org>
 *          Felix Paul Kühne <fkuehne at videolan dot org>
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

#import "PXSourceList.h"

/*****************************************************************************
 * VLCPlaylistView interface
 *****************************************************************************/
@interface VLCPlaylistView : NSOutlineView

@end

/*****************************************************************************
 * VLCPlaylistWizard interface
 *****************************************************************************/
@interface VLCPlaylistWizard : NSObject
- (IBAction)reloadOutlineView;

@end

#import "PLModel.h"

/*****************************************************************************
 * VLCPlaylist interface
 *****************************************************************************/
@interface VLCPlaylist : NSObject<NSOutlineViewDataSource, NSOutlineViewDelegate>
{
    IBOutlet VLCPlaylistView* o_outline_view;

    IBOutlet id o_controller;
    IBOutlet id o_playlist_wizard;

    IBOutlet id o_btn_playlist;
    IBOutlet id o_playlist_view;
    IBOutlet id o_search_field;
    IBOutlet id o_mi_save_playlist;
    IBOutlet id o_ctx_menu;

    IBOutlet id o_mi_play;
    IBOutlet id o_mi_delete;
    IBOutlet id o_mi_info;
    IBOutlet id o_mi_preparse;
    IBOutlet id o_mi_revealInFinder;
    IBOutlet id o_mi_dl_cover_art;
    IBOutlet id o_mi_selectall;
    IBOutlet id o_mi_sort_name;
    IBOutlet id o_mi_sort_author;
    IBOutlet id o_mi_recursive_expand;

    IBOutlet id o_save_accessory_view;
    IBOutlet id o_save_accessory_popup;
    IBOutlet id o_save_accessory_text;

    IBOutlet id o_playlist_header;

    int currentResumeTimeout;

    PLModel *o_model;
}

- (PLModel *)model;

- (void)reloadStyles;

- (NSMenu *)menuForEvent:(NSEvent *)o_event;

- (IBAction)searchItem:(id)sender;

- (void)playlistUpdated;
- (void)outlineViewSelectionDidChange:(NSNotification *)notification;
- (void)sortNode:(int)i_mode;

- (void)currentlyPlayingItemChanged;

- (BOOL)isSelectionEmpty;

- (void)deletionCompleted;


- (IBAction)playItem:(id)sender;
- (IBAction)revealItemInFinder:(id)sender;
- (IBAction)preparseItem:(id)sender;
- (IBAction)downloadCoverArt:(id)sender;
- (IBAction)savePlaylist:(id)sender;
- (IBAction)deleteItem:(id)sender;
- (IBAction)selectAll:(id)sender;
- (IBAction)sortNodeByName:(id)sender;
- (IBAction)sortNodeByAuthor:(id)sender;
- (IBAction)recursiveExpandNode:(id)sender;
- (IBAction)showInfoPanel:(id)sender;

- (NSArray *)draggedItems;

/**
 * Simplified version to add new items at the end of the current playlist
 */
- (void)addPlaylistItems:(NSArray*)o_array;

/**
 * Adds new items to the playlist, at specified parent node and index.
 * @param o_array array of items. Each item is a Dictionary with meta info.
 * @param i_plItemId parent playlist node id, -1 for default playlist
 * @param i_position index for new items, -1 for appending at end
 * @param b_start starts playback of first item if true
 */
- (void)addPlaylistItems:(NSArray*)o_array withParentItemId:(int)i_plItemId atPos:(int)i_position startPlayback:(BOOL)b_start;


- (void)setColumn: (NSString *)o_column state: (NSInteger)i_state translationDict:(NSDictionary *)o_dict;
- (void)continuePlaybackWhereYouLeftOff:(input_thread_t *)p_input_thread;
- (void)storePlaybackPositionForItem:(input_thread_t *)p_input_thread;

@end
