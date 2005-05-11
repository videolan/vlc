/*****************************************************************************
 * playlist.h: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2002-2005 VideoLAN
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * VLCPlaylistView interface 
 *****************************************************************************/
@interface VLCPlaylistView : NSOutlineView
{
}

@end

/*****************************************************************************
 * VLCPlaylist interface 
 *****************************************************************************/
@interface VLCPlaylist : NSObject
{
    IBOutlet id o_controller;

    IBOutlet id o_btn_playlist;
    IBOutlet id o_playlist_view;
    IBOutlet id o_outline_view;
    IBOutlet id o_tc_name;
    IBOutlet id o_tc_author;
    IBOutlet id o_tc_duration;
    IBOutlet id o_status_field;
    IBOutlet id o_search_field;
    IBOutlet id o_random_ckb;
    IBOutlet id o_loop_popup;
    IBOutlet id o_mi_save_playlist;
    IBOutlet id o_ctx_menu;

    IBOutlet id o_mi_play;
    IBOutlet id o_mi_delete;
    IBOutlet id o_mi_info;
    IBOutlet id o_mi_selectall;
    IBOutlet id o_mi_sort_name;
    IBOutlet id o_mi_sort_author;

    /* "services discovery" menu in the playlist menu */
    IBOutlet id o_mi_services;
    IBOutlet id o_mu_services;

    /* "services discovery" menu in the main menu */
    IBOutlet id o_mm_mi_services;
    IBOutlet id o_mm_mu_services;

    NSImage *o_descendingSortingImage;
    NSImage *o_ascendingSortingImage;

    NSMutableDictionary *o_outline_dict;

    BOOL b_selected_item_met;
    BOOL b_isSortDescending;
    int i_current_view;
    id o_tc_sortColumn;
}

- (void)initStrings;
- (void)searchfieldChanged:(NSNotification *)o_notification;
- (NSMenu *)menuForEvent:(NSEvent *)o_event;
- (NSOutlineView *)outlineView;

- (IBAction)handlePopUp:(id)sender;
- (IBAction)searchItem:(id)sender;

- (void)playlistUpdated;
- (void)playModeUpdated;
- (void)sortNode:(int)i_mode;
- (void)updateRowSelection;

- (IBAction)servicesChange:(id)sender;
- (IBAction)playItem:(id)sender;
- (IBAction)deleteItem:(id)sender;
- (IBAction)selectAll:(id)sender;
- (IBAction)sortNodeByName:(id)sender;
- (IBAction)sortNodeByAuthor:(id)sender;

- (void)appendArray:(NSArray*)o_array atPos:(int)i_position enqueue:(BOOL)b_enqueue;
- (void)appendNodeArray:(NSArray*)o_array inNode:(playlist_item_t *)p_node atPos:(int)i_position inView:(int)i_view enqueue:(BOOL)b_enqueue;

- (playlist_item_t *)selectedPlaylistItem;

@end

