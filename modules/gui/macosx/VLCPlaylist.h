/*****************************************************************************
 * VLCPlaylist.h: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2002-2015 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Benjamin Pracht <bigben at videolab dot org>
 *          Felix Paul Kühne <fkuehne at videolan dot org>
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

#import "VLCPLModel.h"
#import "VLCPlaylistView.h"

@interface VLCPlaylist : NSResponder<NSOutlineViewDelegate>

@property (readwrite, weak) IBOutlet NSMenu *playlistMenu;
@property (readwrite, weak) IBOutlet NSMenuItem *playPlaylistMenuItem;
@property (readwrite, weak) IBOutlet NSMenuItem *deletePlaylistMenuItem;
@property (readwrite, weak) IBOutlet NSMenuItem *infoPlaylistMenuItem;
@property (readwrite, weak) IBOutlet NSMenuItem *revealInFinderPlaylistMenuItem;
@property (readwrite, weak) IBOutlet NSMenuItem *selectAllPlaylistMenuItem;
@property (readwrite, weak) IBOutlet NSMenuItem *recursiveExpandPlaylistMenuItem;
@property (readwrite, weak) IBOutlet NSMenuItem *recursiveCollapsePlaylistMenuItem;
@property (readwrite, weak) IBOutlet NSMenuItem *addFilesToPlaylistMenuItem;

@property (nonatomic, readwrite, weak) VLCPlaylistView *outlineView;
@property (nonatomic, readwrite, weak) NSTableHeaderView *playlistHeaderView;

- (VLCPLModel *)model;

- (void)reloadStyles;

- (NSMenu *)menuForEvent:(NSEvent *)o_event;

- (void)playlistUpdated;
- (void)playbackModeUpdated;

- (void)currentlyPlayingItemChanged;

- (BOOL)isSelectionEmpty;

- (IBAction)playItem:(id)sender;
- (IBAction)revealItemInFinder:(id)sender;
- (IBAction)deleteItem:(id)sender;
- (IBAction)selectAll:(id)sender;
- (IBAction)recursiveExpandOrCollapseNode:(id)sender;
- (IBAction)showInfoPanel:(id)sender;
- (IBAction)addFilesToPlaylist:(id)sender;

- (NSArray *)draggedItems;

/**
 * Prepares an array of playlist items for all suitable pasteboard types.
 *
 * This function checks external pasteboard objects (like files). If suitable,
 * an array of all objects is prepared.
 */
- (NSArray *)createItemsFromExternalPasteboard:(NSPasteboard *)pasteboard;

/**
 * Simplified version to add new items at the end of the current playlist
 * @param array array of items. Each item is a Dictionary with meta info.
 */
- (void)addPlaylistItems:(NSArray*)array;

/**
 * Add new items to playlist, with the possibility to check if an item can be added
 * to the currently playing media as subtitle.
 *
 * @param array array of items. Each item is a Dictionary with meta info.
 * @param isSubtitle if YES, method tries to add the item as a subtitle
 */
- (void)addPlaylistItems:(NSArray*)array tryAsSubtitle:(BOOL)isSubtitle;

/**
 * Adds new items to the playlist, at specified parent node and index.
 * @param o_array array of items. Each item is a Dictionary with meta info.
 * @param i_plItemId parent playlist node id, -1 for default playlist
 * @param i_position index for new items, -1 for appending at end
 * @param b_start starts playback of first item if true
 */
- (void)addPlaylistItems:(NSArray*)o_array withParentItemId:(int)i_plItemId atPos:(int)i_position startPlayback:(BOOL)b_start;

@end
