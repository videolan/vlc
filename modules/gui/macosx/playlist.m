/*****************************************************************************
 * playlist.m: MacOS X interface module
 *****************************************************************************
* Copyright (C) 2002-2014 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Jon Lech Johansen <jon-vl@nanocrew.net>
 *          Derk-Jan Hartman <hartman at videola/n dot org>
 *          Benjamin Pracht <bigben at videolab dot org>
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

/* TODO
 * add 'icons' for different types of nodes? (http://www.cocoadev.com/index.pl?IconAndTextInTableCell)
 * reimplement enable/disable item
 */


/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>                                      /* malloc(), free() */
#include <sys/param.h>                                    /* for MAXPATHLEN */
#include <string.h>
#include <math.h>
#include <sys/mount.h>

#import "CompatibilityFixes.h"

#import "intf.h"
#import "wizard.h"
#import "bookmarks.h"
#import "playlistinfo.h"
#import "playlist.h"
#import "controls.h"
#import "misc.h"
#import "open.h"
#import "MainMenu.h"
#import "CoreInteraction.h"

#include <vlc_keys.h>
#import <vlc_interface.h>
#include <vlc_url.h>

/*****************************************************************************
 * VLCPlaylistView implementation
 *****************************************************************************/
@implementation VLCPlaylistView

- (NSMenu *)menuForEvent:(NSEvent *)o_event
{
    return([(VLCPlaylist *)[self delegate] menuForEvent: o_event]);
}

- (void)keyDown:(NSEvent *)o_event
{
    unichar key = 0;

    if ([[o_event characters] length])
        key = [[o_event characters] characterAtIndex: 0];

    switch(key) {
        case NSDeleteCharacter:
        case NSDeleteFunctionKey:
        case NSDeleteCharFunctionKey:
        case NSBackspaceCharacter:
            [(VLCPlaylist *)[self delegate] deleteItem:self];
            break;

        case NSEnterCharacter:
        case NSCarriageReturnCharacter:
            [(VLCPlaylist *)[[VLCMain sharedInstance] playlist] playItem:nil];
            break;

        default:
            [super keyDown: o_event];
            break;
    }
}

- (BOOL)validateMenuItem:(NSMenuItem *)item
{
    if (([self numberOfSelectedRows] >= 1 && [item action] == @selector(delete:)) || [item action] == @selector(selectAll:))
        return YES;

    return NO;
}

- (BOOL)acceptsFirstResponder
{
    return YES;
}

- (BOOL)becomeFirstResponder
{
    [self setNeedsDisplay:YES];
    return YES;
}

- (BOOL)resignFirstResponder
{
    [self setNeedsDisplay:YES];
    return YES;
}

- (IBAction)delete:(id)sender
{
    [[[VLCMain sharedInstance] playlist] deleteItem: sender];
}

@end

/*****************************************************************************
 * VLCPlaylistWizard implementation
 *****************************************************************************/
@implementation VLCPlaylistWizard

- (IBAction)reloadOutlineView
{
    /* Only reload the outlineview if the wizard window is open since this can
       be quite long on big playlists */


    // to be removed
}

@end

/*****************************************************************************
 * An extension to NSOutlineView's interface to fix compilation warnings
 * and let us access these 2 functions properly.
 * This uses a private API, but works fine on all current OSX releases.
 * Radar ID 11739459 request a public API for this. However, it is probably
 * easier and faster to recreate similar looking bitmaps ourselves.
 *****************************************************************************/

@interface NSOutlineView (UndocumentedSortImages)
+ (NSImage *)_defaultTableHeaderSortImage;
+ (NSImage *)_defaultTableHeaderReverseSortImage;
@end


/*****************************************************************************
 * VLCPlaylist implementation
 *****************************************************************************/
@interface VLCPlaylist ()
{
    NSImage *o_descendingSortingImage;
    NSImage *o_ascendingSortingImage;

    BOOL b_selected_item_met;
    BOOL b_isSortDescending;
    id o_tc_sortColumn;
    NSUInteger retainedRowSelection;

    BOOL b_playlistmenu_nib_loaded;
    BOOL b_view_setup;
}

- (void)saveTableColumns;
@end

@implementation VLCPlaylist

+ (void)initialize
{
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    NSMutableArray *o_columnArray = [[NSMutableArray alloc] init];
    [o_columnArray addObject: [NSArray arrayWithObjects:TITLE_COLUMN, [NSNumber numberWithFloat:190.], nil]];
    [o_columnArray addObject: [NSArray arrayWithObjects:ARTIST_COLUMN, [NSNumber numberWithFloat:95.], nil]];
    [o_columnArray addObject: [NSArray arrayWithObjects:DURATION_COLUMN, [NSNumber numberWithFloat:95.], nil]];

    NSDictionary *appDefaults = [NSDictionary dictionaryWithObjectsAndKeys:
                                 [NSArray arrayWithArray:o_columnArray], @"PlaylistColumnSelection",
                                 [NSArray array], @"recentlyPlayedMediaList",
                                 [NSDictionary dictionary], @"recentlyPlayedMedia", nil];

    [defaults registerDefaults:appDefaults];
    [o_columnArray release];
}

- (PLModel *)model
{
    return o_model;
}

- (void)reloadStyles
{
    NSFont *fontToUse;
    CGFloat rowHeight;
    if (config_GetInt(VLCIntf, "macosx-large-text")) {
        fontToUse = [NSFont systemFontOfSize:13.];
        rowHeight = 21.;
    } else {
        fontToUse = [NSFont systemFontOfSize:11.];
        rowHeight = 16.;
    }

    NSArray *columns = [o_outline_view tableColumns];
    NSUInteger count = columns.count;
    for (NSUInteger x = 0; x < count; x++)
        [[[columns objectAtIndex:x] dataCell] setFont:fontToUse];
    [o_outline_view setRowHeight:rowHeight];
}

- (void)dealloc
{
    [super dealloc];
}

- (void)awakeFromNib
{
    if (b_view_setup)
        return;

    playlist_t * p_playlist = pl_Get(VLCIntf);

    [self reloadStyles];
    [self initStrings];

    o_model = [[PLModel alloc] initWithOutlineView:o_outline_view playlist:p_playlist rootItem:p_playlist->p_playing playlistObject:self];
    [o_outline_view setDataSource:o_model];
    [o_outline_view reloadData];

    [o_outline_view setTarget: self];
    [o_outline_view setDoubleAction: @selector(playItem:)];

    [o_outline_view setAllowsEmptySelection: NO];
    [o_outline_view registerForDraggedTypes: [NSArray arrayWithObjects:NSFilenamesPboardType, @"VLCPlaylistItemPboardType", nil]];
    [o_outline_view setIntercellSpacing: NSMakeSize (0.0, 1.0)];

    /* This uses a private API, but works fine on all current OSX releases.
     * Radar ID 11739459 request a public API for this. However, it is probably
     * easier and faster to recreate similar looking bitmaps ourselves. */
    o_ascendingSortingImage = [[NSOutlineView class] _defaultTableHeaderSortImage];
    o_descendingSortingImage = [[NSOutlineView class] _defaultTableHeaderReverseSortImage];

    o_tc_sortColumn = nil;

    NSArray * o_columnArray = [[NSUserDefaults standardUserDefaults] arrayForKey:@"PlaylistColumnSelection"];
    NSUInteger count = [o_columnArray count];

    id o_menu = [[VLCMain sharedInstance] mainMenu];
    NSString * o_column;

    NSMenu *o_context_menu = [o_menu setupPlaylistTableColumnsMenu];
    [o_playlist_header setMenu: o_context_menu];

    for (NSUInteger i = 0; i < count; i++) {
        o_column = [[o_columnArray objectAtIndex:i] objectAtIndex:0];
        if ([o_column isEqualToString:@"status"])
            continue;

        if(![o_menu setPlaylistColumnTableState: NSOnState forColumn: o_column])
            continue;

        [[o_outline_view tableColumnWithIdentifier: o_column] setWidth: [[[o_columnArray objectAtIndex:i] objectAtIndex:1] floatValue]];
    }

    [[NSNotificationCenter defaultCenter] addObserver: self selector: @selector(applicationWillTerminate:) name: NSApplicationWillTerminateNotification object: nil];

    b_view_setup = YES;
}

- (void)applicationWillTerminate:(NSNotification *)notification
{
    /* let's make sure we save the correct widths and positions, since this likely changed since the last time the user played with the column selection */
    [self saveTableColumns];
}

- (void)initStrings
{
    [o_mi_play setTitle: _NS("Play")];
    [o_mi_delete setTitle: _NS("Delete")];
    [o_mi_recursive_expand setTitle: _NS("Expand Node")];
    [o_mi_selectall setTitle: _NS("Select All")];
    [o_mi_info setTitle: _NS("Media Information...")];
    [o_mi_dl_cover_art setTitle: _NS("Download Cover Art")];
    [o_mi_preparse setTitle: _NS("Fetch Meta Data")];
    [o_mi_revealInFinder setTitle: _NS("Reveal in Finder")];
    [o_mi_sort_name setTitle: _NS("Sort Node by Name")];
    [o_mi_sort_author setTitle: _NS("Sort Node by Author")];

    [o_search_field setToolTip: _NS("Search in Playlist")];
}

- (void)playlistUpdated
{
    [o_outline_view reloadData];
}

- (void)outlineViewSelectionDidChange:(NSNotification *)notification
{
//    // FIXME: unsafe
//    playlist_item_t * p_item = [[o_outline_view itemAtRow:[o_outline_view selectedRow]] pointerValue];
//
//    if (p_item) {
//        /* update the state of our Reveal-in-Finder menu items */
//        NSMutableString *o_mrl;
//        char *psz_uri = input_item_GetURI(p_item->p_input);
//
//        [o_mi_revealInFinder setEnabled: NO];
//        [o_mm_mi_revealInFinder setEnabled: NO];
//        if (psz_uri) {
//            o_mrl = [NSMutableString stringWithUTF8String: psz_uri];
//
//            /* perform some checks whether it is a file and if it is local at all... */
//            NSRange prefix_range = [o_mrl rangeOfString: @"file:"];
//            if (prefix_range.location != NSNotFound)
//                [o_mrl deleteCharactersInRange: prefix_range];
//
//            if ([o_mrl characterAtIndex:0] == '/') {
//                [o_mi_revealInFinder setEnabled: YES];
//                [o_mm_mi_revealInFinder setEnabled: YES];
//            }
//            free(psz_uri);
//        }
//
//        /* update our info-panel to reflect the new item */
//        [[[VLCMain sharedInstance] info] updatePanelWithItem:p_item->p_input];
//    }
}

- (BOOL)isSelectionEmpty
{
    return [o_outline_view selectedRow] == -1;
}

- (void)currentlyPlayingItemChanged
{
    PLItem *item = [[self model] currentlyPlayingItem];
    if (!item)
        return;

    // select item
    NSInteger itemIndex = [o_outline_view rowForItem:item];
    if (itemIndex < 0) {
        // expand if needed
        while (item != nil) {
            PLItem *parent = [item parent];

            if (![o_outline_view isExpandable: parent])
                break;
            if (![o_outline_view isItemExpanded: parent])
                [o_outline_view expandItem: parent];
            item = parent;
        }

        // search for row again
        itemIndex = [o_outline_view rowForItem:item];
        if (itemIndex < 0) {
            return;
        }
    }

    [o_outline_view selectRowIndexes: [NSIndexSet indexSetWithIndex: itemIndex] byExtendingSelection: NO];
}

- (IBAction)savePlaylist:(id)sender
{
    playlist_t * p_playlist = pl_Get(VLCIntf);

    NSSavePanel *o_save_panel = [NSSavePanel savePanel];
    NSString * o_name = [NSString stringWithFormat: @"%@", _NS("Untitled")];

    [NSBundle loadNibNamed:@"PlaylistAccessoryView" owner:self];

    [o_save_accessory_text setStringValue: _NS("File Format:")];
    [[o_save_accessory_popup itemAtIndex:0] setTitle: _NS("Extended M3U")];
    [[o_save_accessory_popup itemAtIndex:1] setTitle: _NS("XML Shareable Playlist Format (XSPF)")];
    [[o_save_accessory_popup itemAtIndex:2] setTitle: _NS("HTML playlist")];

    [o_save_panel setTitle: _NS("Save Playlist")];
    [o_save_panel setPrompt: _NS("Save")];
    [o_save_panel setAccessoryView: o_save_accessory_view];
    [o_save_panel setNameFieldStringValue: o_name];

    if ([o_save_panel runModal] == NSFileHandlingPanelOKButton) {
        NSString *o_filename = [[o_save_panel URL] path];

        if ([o_save_accessory_popup indexOfSelectedItem] == 0) {
            NSString * o_real_filename;
            NSRange range;
            range.location = [o_filename length] - [@".m3u" length];
            range.length = [@".m3u" length];

            if ([o_filename compare:@".m3u" options: NSCaseInsensitiveSearch range: range] != NSOrderedSame)
                o_real_filename = [NSString stringWithFormat: @"%@.m3u", o_filename];
            else
                o_real_filename = o_filename;

            playlist_Export(p_playlist,
                [o_real_filename fileSystemRepresentation],
                p_playlist->p_local_category, "export-m3u");
        } else if ([o_save_accessory_popup indexOfSelectedItem] == 1) {
            NSString * o_real_filename;
            NSRange range;
            range.location = [o_filename length] - [@".xspf" length];
            range.length = [@".xspf" length];

            if ([o_filename compare:@".xspf" options: NSCaseInsensitiveSearch range: range] != NSOrderedSame)
                o_real_filename = [NSString stringWithFormat: @"%@.xspf", o_filename];
            else
                o_real_filename = o_filename;

            playlist_Export(p_playlist,
                [o_real_filename fileSystemRepresentation],
                p_playlist->p_local_category, "export-xspf");
        } else {
            NSString * o_real_filename;
            NSRange range;
            range.location = [o_filename length] - [@".html" length];
            range.length = [@".html" length];

            if ([o_filename compare:@".html" options: NSCaseInsensitiveSearch range: range] != NSOrderedSame)
                o_real_filename = [NSString stringWithFormat: @"%@.html", o_filename];
            else
                o_real_filename = o_filename;

            playlist_Export(p_playlist,
                [o_real_filename fileSystemRepresentation],
                p_playlist->p_local_category, "export-html");
        }
    }
}

/* When called retrieves the selected outlineview row and plays that node or item */
- (IBAction)playItem:(id)sender
{
    playlist_t *p_playlist = pl_Get(VLCIntf);

    // ignore clicks on column header when handling double action
    if (sender == o_outline_view && [o_outline_view clickedRow] == -1)
        return;

    PLItem *o_item = [o_outline_view itemAtRow:[o_outline_view selectedRow]];
    if (!o_item)
        return;

    PL_LOCK;
    playlist_item_t *p_item = playlist_ItemGetById(p_playlist, [o_item plItemId]);
    playlist_item_t *p_node = playlist_ItemGetById(p_playlist, [[[self model] rootItem] plItemId]);

    if (p_item && p_node) {
        playlist_Control(p_playlist, PLAYLIST_VIEWPLAY, pl_Locked, p_node, p_item);
    }
    PL_UNLOCK;
}

- (IBAction)revealItemInFinder:(id)sender
{
    NSIndexSet *selectedRows = [o_outline_view selectedRowIndexes];
    [selectedRows enumerateIndexesUsingBlock:^(NSUInteger idx, BOOL *stop) {

        PLItem *o_item = [o_outline_view itemAtRow:idx];

        /* perform some checks whether it is a file and if it is local at all... */
        char *psz_url = input_item_GetURI([o_item input]);
        NSURL *url = [NSURL URLWithString:toNSStr(psz_url)];
        free(psz_url);
        if (![url isFileURL])
            return;
        if (![[NSFileManager defaultManager] fileExistsAtPath:[url path]])
            return;

        msg_Dbg(VLCIntf, "Reveal url %s in finder", [[url path] UTF8String]);
        [[NSWorkspace sharedWorkspace] selectFile: [url path] inFileViewerRootedAtPath: [url path]];
    }];

}

/* When called retrieves the selected outlineview row and plays that node or item */
- (IBAction)preparseItem:(id)sender
{
    int i_count;
    NSIndexSet *o_selected_indexes;
    intf_thread_t * p_intf = VLCIntf;
    playlist_t * p_playlist = pl_Get(p_intf);
    playlist_item_t *p_item = NULL;

    o_selected_indexes = [o_outline_view selectedRowIndexes];
    i_count = [o_selected_indexes count];

    NSUInteger indexes[i_count];
    [o_selected_indexes getIndexes:indexes maxCount:i_count inIndexRange:nil];
    for (int i = 0; i < i_count; i++) {
        PLItem *o_item = [o_outline_view itemAtRow:indexes[i]];
        [o_outline_view deselectRow: indexes[i]];

        if (![o_item isLeaf]) {
            msg_Dbg(p_intf, "preparsing nodes not implemented");
            continue;
        }

        libvlc_MetaRequest(p_intf->p_libvlc, [o_item input], META_REQUEST_OPTION_NONE);

    }
    [self playlistUpdated];
}

- (IBAction)downloadCoverArt:(id)sender
{
    int i_count;
    NSIndexSet *o_selected_indexes;
    intf_thread_t * p_intf = VLCIntf;
    playlist_t * p_playlist = pl_Get(p_intf);
    playlist_item_t *p_item = NULL;

    o_selected_indexes = [o_outline_view selectedRowIndexes];
    i_count = [o_selected_indexes count];

    NSUInteger indexes[i_count];
    [o_selected_indexes getIndexes:indexes maxCount:i_count inIndexRange:nil];
    for (int i = 0; i < i_count; i++) {
        PLItem *o_item = [o_outline_view itemAtRow: indexes[i]];

        if (![o_item isLeaf])
            continue;

        libvlc_ArtRequest(p_intf->p_libvlc, [o_item input], META_REQUEST_OPTION_NONE);
    }
    [self playlistUpdated];
}

- (IBAction)selectAll:(id)sender
{
    [o_outline_view selectAll: nil];
}

- (IBAction)showInfoPanel:(id)sender
{
    [[[VLCMain sharedInstance] info] initPanel];
}

- (void)deletionCompleted
{
    // retain selection before deletion
    [o_outline_view selectRowIndexes:[NSIndexSet indexSetWithIndex:retainedRowSelection] byExtendingSelection:NO];
}

- (IBAction)deleteItem:(id)sender
{
    playlist_t * p_playlist = pl_Get(VLCIntf);

    // check if deletion is allowed
    if (![[self model] editAllowed])
        return;

    NSIndexSet *o_selected_indexes = [o_outline_view selectedRowIndexes];
    retainedRowSelection = [o_selected_indexes firstIndex];
    if (retainedRowSelection == NSNotFound)
        retainedRowSelection = 0;

    [o_selected_indexes enumerateIndexesUsingBlock:^(NSUInteger idx, BOOL *stop) {
        PLItem *o_item = [o_outline_view itemAtRow: idx];
        if (!o_item)
            return;

        // model deletion is done via callback
        playlist_DeleteFromInput(p_playlist, [o_item input], pl_Unlocked);
    }];
}

- (IBAction)sortNodeByName:(id)sender
{
    [self sortNode: SORT_TITLE];
}

- (IBAction)sortNodeByAuthor:(id)sender
{
    [self sortNode: SORT_ARTIST];
}

- (void)sortNode:(int)i_mode
{
    playlist_t * p_playlist = pl_Get(VLCIntf);
    playlist_item_t * p_item;

    // TODO why do we need this kind of sort? It looks crap and confusing...

//    if ([o_outline_view selectedRow] > -1) {
//        p_item = [[o_outline_view itemAtRow: [o_outline_view selectedRow]] pointerValue];
//        if (!p_item)
//            return;
//    } else
//        p_item = [self currentPlaylistRoot]; // If no item is selected, sort the whole playlist
//
//    PL_LOCK;
//    if (p_item->i_children > -1) // the item is a node
//        playlist_RecursiveNodeSort(p_playlist, p_item, i_mode, ORDER_NORMAL);
//    else
//        playlist_RecursiveNodeSort(p_playlist, p_item->p_parent, i_mode, ORDER_NORMAL);
//
//    PL_UNLOCK;
//    [self playlistUpdated];
}

- (input_item_t *)createItem:(NSDictionary *)o_one_item
{
    intf_thread_t * p_intf = VLCIntf;
    playlist_t * p_playlist = pl_Get(p_intf);

    input_item_t *p_input;
    BOOL b_rem = FALSE, b_dir = FALSE, b_writable = FALSE;
    NSString *o_uri, *o_name, *o_path;
    NSURL * o_nsurl;
    NSArray *o_options;
    NSURL *o_true_file;

    /* Get the item */
    o_uri = (NSString *)[o_one_item objectForKey: @"ITEM_URL"];
    o_nsurl = [NSURL URLWithString: o_uri];
    o_path = [o_nsurl path];
    o_name = (NSString *)[o_one_item objectForKey: @"ITEM_NAME"];
    o_options = (NSArray *)[o_one_item objectForKey: @"ITEM_OPTIONS"];

    if ([[NSFileManager defaultManager] fileExistsAtPath:o_path isDirectory:&b_dir] && b_dir &&
        [[NSWorkspace sharedWorkspace] getFileSystemInfoForPath:o_path isRemovable: &b_rem
                                                     isWritable:&b_writable isUnmountable:NULL description:NULL type:NULL] && b_rem && !b_writable && [o_nsurl isFileURL]) {

        NSString *diskType = [VLCOpen getVolumeTypeFromMountPath: o_path];
        msg_Dbg(p_intf, "detected optical media of type %s in the file input", [diskType UTF8String]);

        if ([diskType isEqualToString: kVLCMediaDVD])
            o_uri = [NSString stringWithFormat: @"dvdnav://%@", [VLCOpen getBSDNodeFromMountPath: o_path]];
        else if ([diskType isEqualToString: kVLCMediaVideoTSFolder])
            o_uri = [NSString stringWithFormat: @"dvdnav://%@", o_path];
        else if ([diskType isEqualToString: kVLCMediaAudioCD])
            o_uri = [NSString stringWithFormat: @"cdda://%@", [VLCOpen getBSDNodeFromMountPath: o_path]];
        else if ([diskType isEqualToString: kVLCMediaVCD])
            o_uri = [NSString stringWithFormat: @"vcd://%@#0:0", [VLCOpen getBSDNodeFromMountPath: o_path]];
        else if ([diskType isEqualToString: kVLCMediaSVCD])
            o_uri = [NSString stringWithFormat: @"vcd://%@@0:0", [VLCOpen getBSDNodeFromMountPath: o_path]];
        else if ([diskType isEqualToString: kVLCMediaBD] || [diskType isEqualToString: kVLCMediaBDMVFolder])
            o_uri = [NSString stringWithFormat: @"bluray://%@", o_path];
        else
            msg_Warn(VLCIntf, "unknown disk type, treating %s as regular input", [o_path UTF8String]);

        p_input = input_item_New([o_uri UTF8String], [[[NSFileManager defaultManager] displayNameAtPath: o_path] UTF8String]);
    }
    else
        p_input = input_item_New([o_uri fileSystemRepresentation], o_name ? [o_name UTF8String] : NULL);

    if (!p_input)
        return NULL;

    if (o_options) {
        NSUInteger count = [o_options count];
        for (NSUInteger i = 0; i < count; i++)
            input_item_AddOption(p_input, [[o_options objectAtIndex:i] UTF8String], VLC_INPUT_OPTION_TRUSTED);
    }

    /* Recent documents menu */
    if (o_nsurl != nil && (BOOL)config_GetInt(p_playlist, "macosx-recentitems") == YES)
        [[NSDocumentController sharedDocumentController] noteNewRecentDocumentURL: o_nsurl];

    return p_input;
}

- (void)addPlaylistItems:(NSArray*)o_array
{

    int i_plItemId = -1;

    // add items directly to media library if this is the current root
    if ([[self model] currentRootType] == ROOT_TYPE_MEDIALIBRARY)
        i_plItemId = [[[self model] rootItem] plItemId];

    BOOL b_autoplay = var_InheritBool(VLCIntf, "macosx-autoplay");

    [self addPlaylistItems:o_array withParentItemId:i_plItemId atPos:-1 startPlayback:b_autoplay];
}

- (void)addPlaylistItems:(NSArray*)o_array withParentItemId:(int)i_plItemId atPos:(int)i_position startPlayback:(BOOL)b_start
{
    playlist_t * p_playlist = pl_Get(VLCIntf);
    PL_LOCK;

    playlist_item_t *p_parent = NULL;
    if (i_plItemId >= 0)
        p_parent = playlist_ItemGetById(p_playlist, i_plItemId);
    else
        p_parent = p_playlist->p_playing;

    if (!p_parent) {
        PL_UNLOCK;
        return;
    }

    NSUInteger count = [o_array count];
    int i_current_offset = 0;
    for (NSUInteger i = 0; i < count; ++i) {

        NSDictionary *o_current_item = [o_array objectAtIndex:i];
        input_item_t *p_input = [self createItem: o_current_item];
        if (!p_input)
            continue;

        int i_pos = (i_position == -1) ? PLAYLIST_END : i_position + i_current_offset++;
        playlist_item_t *p_item = playlist_NodeAddInput(p_playlist, p_input, p_parent,
                                                        PLAYLIST_INSERT, i_pos, pl_Locked);
        if (!p_item)
            continue;

        if (i == 0 && b_start) {
            playlist_Control(p_playlist, PLAYLIST_VIEWPLAY, pl_Locked, p_parent, p_item);
        }
        input_item_Release(p_input);
    }
    PL_UNLOCK;
}


- (IBAction)searchItem:(id)sender
{
    [[self model] searchUpdate:[o_search_field stringValue]];
}

- (IBAction)recursiveExpandNode:(id)sender
{
    NSIndexSet * selectedRows = [o_outline_view selectedRowIndexes];
    NSUInteger count = [selectedRows count];
    NSUInteger indexes[count];
    [selectedRows getIndexes:indexes maxCount:count inIndexRange:nil];

    id o_item;
    playlist_item_t *p_item;
    for (NSUInteger i = 0; i < count; i++) {
        o_item = [o_outline_view itemAtRow: indexes[i]];

        /* We need to collapse the node first, since OSX refuses to recursively
         expand an already expanded node, even if children nodes are collapsed. */
        if ([o_outline_view isExpandable:o_item]) {
            [o_outline_view collapseItem: o_item collapseChildren: YES];
            [o_outline_view expandItem: o_item expandChildren: YES];
        }

        selectedRows = [o_outline_view selectedRowIndexes];
        [selectedRows getIndexes:indexes maxCount:count inIndexRange:nil];
    }
}

- (NSMenu *)menuForEvent:(NSEvent *)o_event
{
    if (!b_playlistmenu_nib_loaded)
        b_playlistmenu_nib_loaded = [NSBundle loadNibNamed:@"PlaylistMenu" owner:self];

    NSPoint pt;
    bool b_rows;
    bool b_item_sel;

    pt = [o_outline_view convertPoint: [o_event locationInWindow] fromView: nil];
    int row = [o_outline_view rowAtPoint:pt];
    if (row != -1 && ![[o_outline_view selectedRowIndexes] containsIndex: row])
        [o_outline_view selectRowIndexes:[NSIndexSet indexSetWithIndex:row] byExtendingSelection:NO];

    b_item_sel = (row != -1 && [o_outline_view selectedRow] != -1);
    b_rows = [o_outline_view numberOfRows] != 0;

    playlist_t *p_playlist = pl_Get(VLCIntf);
    bool b_del_allowed = [[self model] editAllowed];

    [o_mi_play setEnabled: b_item_sel];
    [o_mi_delete setEnabled: b_item_sel && b_del_allowed];
    [o_mi_selectall setEnabled: b_rows];
    [o_mi_info setEnabled: b_item_sel];
    [o_mi_preparse setEnabled: b_item_sel];
    [o_mi_recursive_expand setEnabled: b_item_sel];
    [o_mi_sort_name setEnabled: b_item_sel];
    [o_mi_sort_author setEnabled: b_item_sel];
    [o_mi_dl_cover_art setEnabled: b_item_sel];

    return o_ctx_menu;
}

- (void)outlineView: (NSOutlineView *)o_tv didClickTableColumn:(NSTableColumn *)o_tc
{
    int i_mode, i_type = 0;
    intf_thread_t *p_intf = VLCIntf;
    NSString * o_identifier = [o_tc identifier];

    playlist_t *p_playlist = pl_Get(p_intf);

    if (o_tc_sortColumn == o_tc)
        b_isSortDescending = !b_isSortDescending;
    else
        b_isSortDescending = false;

    if (b_isSortDescending)
        i_type = ORDER_REVERSE;
    else
        i_type = ORDER_NORMAL;

    [[self model] sortForColumn:o_identifier withMode:i_type];

    // TODO rework, why do we need a full call here?
//    [self playlistUpdated];

    /* Clear indications of any existing column sorting */
    NSUInteger count = [[o_outline_view tableColumns] count];
    for (NSUInteger i = 0 ; i < count ; i++)
        [o_outline_view setIndicatorImage:nil inTableColumn: [[o_outline_view tableColumns] objectAtIndex:i]];

    [o_outline_view setHighlightedTableColumn:nil];
    o_tc_sortColumn = nil;


    o_tc_sortColumn = o_tc;
    [o_outline_view setHighlightedTableColumn:o_tc];

    if (b_isSortDescending)
        [o_outline_view setIndicatorImage:o_descendingSortingImage inTableColumn:o_tc];
    else
        [o_outline_view setIndicatorImage:o_ascendingSortingImage inTableColumn:o_tc];
}


- (void)outlineView:(NSOutlineView *)outlineView
    willDisplayCell:(id)cell
     forTableColumn:(NSTableColumn *)tableColumn
               item:(id)item
{
    /* this method can be called when VLC is already dead, hence the extra checks */
    intf_thread_t * p_intf = VLCIntf;
    if (!p_intf)
        return;
    playlist_t *p_playlist = pl_Get(p_intf);

    NSFont *fontToUse;
    if (config_GetInt(VLCIntf, "macosx-large-text"))
        fontToUse = [NSFont systemFontOfSize:13.];
    else
        fontToUse = [NSFont systemFontOfSize:11.];

    BOOL b_is_playing = NO;
    PL_LOCK;
    playlist_item_t *p_current_item = playlist_CurrentPlayingItem(p_playlist);
    if (p_current_item) {
        b_is_playing = p_current_item->i_id == [item plItemId];
    }
    PL_UNLOCK;

    /*
     TODO: repaint all items bold:
     [self isItem: [o_playing_item pointerValue] inNode: [item pointerValue] checkItemExistence:YES locked:NO]
     || [o_playing_item isEqual: item]
     */

    if (b_is_playing)
        [cell setFont: [[NSFontManager sharedFontManager] convertFont:fontToUse toHaveTrait:NSBoldFontMask]];
    else
        [cell setFont: [[NSFontManager sharedFontManager] convertFont:fontToUse toNotHaveTrait:NSBoldFontMask]];
}

// TODO remove method
- (NSArray *)draggedItems
{
    return [[self model] draggedItems];
}

- (void)setColumn: (NSString *)o_column state: (NSInteger)i_state translationDict:(NSDictionary *)o_dict
{
    NSTableColumn * o_work_tc;

    if (i_state == NSOnState) {
        NSString *o_title = [o_dict objectForKey:o_column];
        if (!o_title)
            return;

        o_work_tc = [[NSTableColumn alloc] initWithIdentifier: o_column];
        [o_work_tc setEditable: NO];
        [[o_work_tc dataCell] setFont: [NSFont controlContentFontOfSize:11.]];

        [[o_work_tc headerCell] setStringValue: [o_dict objectForKey:o_column]];

        if ([o_column isEqualToString: TRACKNUM_COLUMN]) {
            [o_work_tc setWidth: 20.];
            [o_work_tc setResizingMask: NSTableColumnNoResizing];
            [[o_work_tc headerCell] setStringValue: @"#"];
        }

        [o_outline_view addTableColumn: o_work_tc];
        [o_work_tc release];
        [o_outline_view reloadData];
        [o_outline_view setNeedsDisplay: YES];
    }
    else
        [o_outline_view removeTableColumn: [o_outline_view tableColumnWithIdentifier: o_column]];

    [o_outline_view setOutlineTableColumn: [o_outline_view tableColumnWithIdentifier:TITLE_COLUMN]];
}

- (void)saveTableColumns
{
    NSMutableArray * o_arrayToSave = [[NSMutableArray alloc] init];
    NSArray * o_columns = [[NSArray alloc] initWithArray:[o_outline_view tableColumns]];
    NSUInteger count = [o_columns count];
    NSTableColumn * o_currentColumn;
    for (NSUInteger i = 0; i < count; i++) {
        o_currentColumn = [o_columns objectAtIndex:i];
        [o_arrayToSave addObject:[NSArray arrayWithObjects:[o_currentColumn identifier], [NSNumber numberWithFloat:[o_currentColumn width]], nil]];
    }
    [[NSUserDefaults standardUserDefaults] setObject: o_arrayToSave forKey:@"PlaylistColumnSelection"];
    [[NSUserDefaults standardUserDefaults] synchronize];
    [o_columns release];
    [o_arrayToSave release];
}

- (BOOL)isValidResumeItem:(input_item_t *)p_item
{
    char *psz_url = input_item_GetURI(p_item);
    NSString *o_url_string = toNSStr(psz_url);
    free(psz_url);

    if ([o_url_string isEqualToString:@""])
        return NO;

    NSURL *o_url = [NSURL URLWithString:o_url_string];

    if (![o_url isFileURL])
        return NO;

    BOOL isDir = false;
    if (![[NSFileManager defaultManager] fileExistsAtPath:[o_url path] isDirectory:&isDir])
        return NO;

    if (isDir)
        return NO;

    return YES;
}

- (void)updateAlertWindow:(NSTimer *)timer
{
    NSAlert *alert = [timer userInfo];

    --currentResumeTimeout;
    if (currentResumeTimeout <= 0) {
        [[alert window] close];
        [NSApp abortModal];
    }

    NSString *buttonLabel = _NS("Restart playback");
    buttonLabel = [buttonLabel stringByAppendingFormat:@" (%d)", currentResumeTimeout];

    [[[alert buttons] objectAtIndex:2] setTitle:buttonLabel];
}

- (void)continuePlaybackWhereYouLeftOff:(input_thread_t *)p_input_thread
{
    NSDictionary *recentlyPlayedFiles = [[NSUserDefaults standardUserDefaults] objectForKey:@"recentlyPlayedMedia"];
    if (!recentlyPlayedFiles)
        return;

    input_item_t *p_item = input_GetItem(p_input_thread);
    if (!p_item)
        return;

    /* allow the user to over-write the start/stop/run-time */
    if (var_GetFloat(p_input_thread, "run-time") > 0 ||
        var_GetFloat(p_input_thread, "start-time") > 0 ||
        var_GetFloat(p_input_thread, "stop-time") != 0) {
        return;
    }

    /* check for file existance before resuming */
    if (![self isValidResumeItem:p_item])
        return;

    char *psz_url = decode_URI(input_item_GetURI(p_item));
    if (!psz_url)
        return;
    NSString *url = toNSStr(psz_url);
    free(psz_url);

    NSNumber *lastPosition = [recentlyPlayedFiles objectForKey:url];
    if (!lastPosition || lastPosition.intValue <= 0)
        return;

    int settingValue = config_GetInt(VLCIntf, "macosx-continue-playback");
    if (settingValue == 2) // never resume
        return;

    NSInteger returnValue = NSAlertErrorReturn;
    if (settingValue == 0) { // ask

        char *psz_title_name = input_item_GetTitleFbName(p_item);
        NSString *o_title = toNSStr(psz_title_name);
        free(psz_title_name);

        currentResumeTimeout = 6;
        NSString *o_restartButtonLabel = _NS("Restart playback");
        o_restartButtonLabel = [o_restartButtonLabel stringByAppendingFormat:@" (%d)", currentResumeTimeout];
        NSAlert *theAlert = [NSAlert alertWithMessageText:_NS("Continue playback?") defaultButton:_NS("Continue") alternateButton:o_restartButtonLabel otherButton:_NS("Always continue") informativeTextWithFormat:_NS("Playback of \"%@\" will continue at %@"), o_title, [[VLCStringUtility sharedInstance] stringForTime:lastPosition.intValue]];

        NSTimer *timer = [NSTimer timerWithTimeInterval:1
                                                 target:self
                                               selector:@selector(updateAlertWindow:)
                                               userInfo:theAlert
                                                repeats:YES];

        [[NSRunLoop currentRunLoop] addTimer:timer forMode:NSModalPanelRunLoopMode];

        returnValue = [theAlert runModal];
        [timer invalidate];

        // restart button was pressed or timeout happened
        if (returnValue == NSAlertAlternateReturn ||
            returnValue == NSRunAbortedResponse)
            return;
    }

    mtime_t lastPos = (mtime_t)lastPosition.intValue * 1000000;
    msg_Dbg(VLCIntf, "continuing playback at %lld", lastPos);
    var_SetInteger(p_input_thread, "time", lastPos);

    if (returnValue == NSAlertOtherReturn)
        config_PutInt(VLCIntf, "macosx-continue-playback", 1);
}

- (void)storePlaybackPositionForItem:(input_thread_t *)p_input_thread
{
    if (!var_InheritBool(VLCIntf, "macosx-recentitems"))
        return;

    input_item_t *p_item = input_GetItem(p_input_thread);
    if (!p_item)
        return;

    if (![self isValidResumeItem:p_item])
        return;

    char *psz_url = decode_URI(input_item_GetURI(p_item));
    if (!psz_url)
        return;
    NSString *url = toNSStr(psz_url);
    free(psz_url);

    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    NSMutableDictionary *mutDict = [[NSMutableDictionary alloc] initWithDictionary:[defaults objectForKey:@"recentlyPlayedMedia"]];

    float relativePos = var_GetFloat(p_input_thread, "position");
    mtime_t pos = var_GetInteger(p_input_thread, "time") / CLOCK_FREQ;
    mtime_t dur = input_item_GetDuration(p_item) / 1000000;

    NSMutableArray *mediaList = [[defaults objectForKey:@"recentlyPlayedMediaList"] mutableCopy];

    if (relativePos > .05 && relativePos < .95 && dur > 180) {
        [mutDict setObject:[NSNumber numberWithInt:pos] forKey:url];

        [mediaList removeObject:url];
        [mediaList addObject:url];
        NSUInteger mediaListCount = mediaList.count;
        if (mediaListCount > 30) {
            for (NSUInteger x = 0; x < mediaListCount - 30; x++) {
                [mutDict removeObjectForKey:[mediaList objectAtIndex:0]];
                [mediaList removeObjectAtIndex:0];
            }
        }
    } else {
        [mutDict removeObjectForKey:url];
        [mediaList removeObject:url];
    }
    [defaults setObject:mutDict forKey:@"recentlyPlayedMedia"];
    [defaults setObject:mediaList forKey:@"recentlyPlayedMediaList"];
    [defaults synchronize];

    [mutDict release];
    [mediaList release];
}

@end
