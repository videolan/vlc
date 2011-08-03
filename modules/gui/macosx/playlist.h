/*****************************************************************************
 * playlist.h: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2002-2006 the VideoLAN team
 * $Id$
 *
 * Authors: Jon Lech Johansen <jon-vl@nanocrew.net>
 *          Derk-Jan Hartman <hartman at videolan dot org>
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
{
}

@end

/*****************************************************************************
 * VLCPlaylistCommon interface
 *****************************************************************************/
@interface VLCPlaylistCommon : NSObject <NSOutlineViewDataSource, NSOutlineViewDelegate>
{
    IBOutlet id o_tc_name;
    IBOutlet id o_tc_author;
    IBOutlet id o_tc_duration;
    IBOutlet VLCPlaylistView* o_outline_view;

    IBOutlet id o_tc_name_other;
    IBOutlet id o_tc_author_other;
    IBOutlet id o_tc_duration_other;
    IBOutlet VLCPlaylistView* o_outline_view_other;

    NSMutableDictionary *o_outline_dict;
}

- (void)initStrings;
- (playlist_item_t *)selectedPlaylistItem;
- (NSOutlineView *)outlineView;
- (void)swapPlaylists:(id)newList;
@end

/*****************************************************************************
 * VLCPlaylistWizard interface
 *****************************************************************************/
@interface VLCPlaylistWizard : VLCPlaylistCommon
{
}

- (IBAction)reloadOutlineView;

@end

/*****************************************************************************
 * VLCPlaylist interface
 *****************************************************************************/
@interface VLCPlaylist : VLCPlaylistCommon
{
    IBOutlet id o_controller;
    IBOutlet id o_playlist_wizard;

    IBOutlet id o_btn_playlist;
    IBOutlet id o_playlist_view;
    IBOutlet id o_search_field;
    IBOutlet id o_search_field_other;
    IBOutlet id o_mi_save_playlist;
    IBOutlet id o_ctx_menu;

    IBOutlet id o_mi_play;
    IBOutlet id o_mi_delete;
    IBOutlet id o_mi_info;
    IBOutlet id o_mi_preparse;
    IBOutlet id o_mi_revealInFinder;
    IBOutlet id o_mm_mi_revealInFinder;
    IBOutlet id o_mi_dl_cover_art;
    IBOutlet id o_mi_selectall;
    IBOutlet id o_mi_sort_name;
    IBOutlet id o_mi_sort_author;
    IBOutlet id o_mi_recursive_expand;

    /* "services discovery" menu in the playlist menu */
    IBOutlet id o_mi_services;
    IBOutlet id o_mu_services;

    /* "services discovery" menu in the main menu */
    IBOutlet id o_mm_mi_services;
    IBOutlet id o_mm_mu_services;

    IBOutlet id o_save_accessory_view;
    IBOutlet id o_save_accessory_popup;
    IBOutlet id o_save_accessory_text;


    NSImage *o_descendingSortingImage;
    NSImage *o_ascendingSortingImage;

    NSMutableArray *o_nodes_array;
    NSMutableArray *o_items_array;

    BOOL b_selected_item_met;
    BOOL b_isSortDescending;
    id o_tc_sortColumn;
}

- (void)searchfieldChanged:(NSNotification *)o_notification;
- (NSMenu *)menuForEvent:(NSEvent *)o_event;

- (IBAction)searchItem:(id)sender;

- (void)playlistUpdated;
- (void)outlineViewSelectionDidChange:(NSNotification *)notification;
- (void)sortNode:(int)i_mode;
- (void)updateRowSelection;

- (BOOL)isSelectionEmpty;

- (IBAction)servicesChange:(id)sender;
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

- (id)playingItem;

- (void)appendArray:(NSArray*)o_array atPos:(int)i_position enqueue:(BOOL)b_enqueue;
- (void)appendNodeArray:(NSArray*)o_array inNode:(playlist_item_t *)p_node atPos:(int)i_position enqueue:(BOOL)b_enqueue;

@end
