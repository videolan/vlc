/*****************************************************************************
 * VLCPlaylist.m: MacOS X interface module
 *****************************************************************************
* Copyright (C) 2002-2015 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Derk-Jan Hartman <hartman at videola/n dot org>
 *          Benjamin Pracht <bigben at videolan dot org>
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
#import "VLCPlaylist.h"
#import "MainMenu.h"
#import "VLCPlaylistInfo.h"
#import "ResumeDialogController.h"

#include <vlc_keys.h>
#import <vlc_interface.h>
#include <vlc_url.h>

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

@interface VLCPlaylist ()
{
    NSImage *_descendingSortingImage;
    NSImage *_ascendingSortingImage;

    BOOL b_selected_item_met;
    BOOL b_isSortDescending;
    NSTableColumn *_sortTableColumn;

    BOOL b_playlistmenu_nib_loaded;
    BOOL b_view_setup;

    PLModel *_model;
}

- (void)saveTableColumns;
@end

@implementation VLCPlaylist

+ (void)initialize
{
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    NSMutableArray *columnArray = [[NSMutableArray alloc] init];
    [columnArray addObject: [NSArray arrayWithObjects:TITLE_COLUMN, [NSNumber numberWithFloat:190.], nil]];
    [columnArray addObject: [NSArray arrayWithObjects:ARTIST_COLUMN, [NSNumber numberWithFloat:95.], nil]];
    [columnArray addObject: [NSArray arrayWithObjects:DURATION_COLUMN, [NSNumber numberWithFloat:95.], nil]];

    NSDictionary *appDefaults = [NSDictionary dictionaryWithObjectsAndKeys:
                                 [NSArray arrayWithArray:columnArray], @"PlaylistColumnSelection",
                                 [NSArray array], @"recentlyPlayedMediaList",
                                 [NSDictionary dictionary], @"recentlyPlayedMedia", nil];

    [defaults registerDefaults:appDefaults];
}

- (PLModel *)model
{
    return _model;
}

- (void)reloadStyles
{
    NSFont *fontToUse;
    CGFloat rowHeight;
    if (var_InheritBool(VLCIntf, "macosx-large-text")) {
        fontToUse = [NSFont systemFontOfSize:13.];
        rowHeight = 21.;
    } else {
        fontToUse = [NSFont systemFontOfSize:11.];
        rowHeight = 16.;
    }

    NSArray *columns = [_outlineView tableColumns];
    NSUInteger count = columns.count;
    for (NSUInteger x = 0; x < count; x++)
        [[columns[x] dataCell] setFont:fontToUse];
    [_outlineView setRowHeight:rowHeight];
}

- (void)awakeFromNib
{
    if (b_view_setup)
        return;

    [self reloadStyles];
    [self initStrings];

    /* This uses a private API, but works fine on all current OSX releases.
     * Radar ID 11739459 request a public API for this. However, it is probably
     * easier and faster to recreate similar looking bitmaps ourselves. */
    _ascendingSortingImage = [[NSOutlineView class] _defaultTableHeaderSortImage];
    _descendingSortingImage = [[NSOutlineView class] _defaultTableHeaderReverseSortImage];

    _sortTableColumn = nil;

    [[NSNotificationCenter defaultCenter] addObserver: self selector: @selector(applicationWillTerminate:) name: NSApplicationWillTerminateNotification object: nil];

    b_view_setup = YES;
}

- (void)setOutlineView:(VLCPlaylistView * __nullable)outlineView
{
    _outlineView = outlineView;
    [_outlineView setDelegate:self];

    playlist_t * p_playlist = pl_Get(VLCIntf);

    _model = [[PLModel alloc] initWithOutlineView:_outlineView playlist:p_playlist rootItem:p_playlist->p_playing playlistObject:self];
    [_outlineView setDataSource:_model];
    [_outlineView reloadData];

    [_outlineView setTarget: self];
    [_outlineView setDoubleAction: @selector(playItem:)];

    [_outlineView setAllowsEmptySelection: NO];
    [_outlineView registerForDraggedTypes: [NSArray arrayWithObjects:NSFilenamesPboardType, @"VLCPlaylistItemPboardType", nil]];
    [_outlineView setIntercellSpacing: NSMakeSize (0.0, 1.0)];
}

- (void)setPlaylistHeaderView:(NSTableHeaderView * __nullable)playlistHeaderView
{
    VLCMainMenu *mainMenu = [[VLCMain sharedInstance] mainMenu];
    _playlistHeaderView = playlistHeaderView;
    NSMenu *contextMenu = [mainMenu setupPlaylistTableColumnsMenu];
    [_playlistHeaderView setMenu: contextMenu];

    NSArray * columnArray = [[NSUserDefaults standardUserDefaults] arrayForKey:@"PlaylistColumnSelection"];
    NSUInteger columnCount = [columnArray count];
    NSString * column;

    for (NSUInteger i = 0; i < columnCount; i++) {
        column = [columnArray[i] firstObject];
        if ([column isEqualToString:@"status"])
            continue;

        if(![mainMenu setPlaylistColumnTableState: NSOnState forColumn:column])
            continue;

        [[_outlineView tableColumnWithIdentifier: column] setWidth: [columnArray[i][1] floatValue]];
    }
}

- (void)applicationWillTerminate:(NSNotification *)notification
{
    /* let's make sure we save the correct widths and positions, since this likely changed since the last time the user played with the column selection */
    [self saveTableColumns];
}

- (void)initStrings
{
    [_playPlaylistMenuItem setTitle: _NS("Play")];
    [_deletePlaylistMenuItem setTitle: _NS("Delete")];
    [_recursiveExpandPlaylistMenuItem setTitle: _NS("Expand Node")];
    [_selectAllPlaylistMenuItem setTitle: _NS("Select All")];
    [_infoPlaylistMenuItem setTitle: _NS("Media Information...")];
    [_downloadCoverArtPlaylistMenuItem setTitle: _NS("Download Cover Art")];
    [_preparsePlaylistMenuItem setTitle: _NS("Fetch Meta Data")];
    [_revealInFinderPlaylistMenuItem setTitle: _NS("Reveal in Finder")];
    [_sortNamePlaylistMenuItem setTitle: _NS("Sort Node by Name")];
    [_sortAuthorPlaylistMenuItem setTitle: _NS("Sort Node by Author")];
}

- (void)playlistUpdated
{
    [_outlineView reloadData];
}

- (void)playbackModeUpdated
{
    [_model playbackModeUpdated];
}

- (void)updateTogglePlaylistState
{
    [self outlineViewSelectionDidChange: NULL];
}

- (void)outlineViewSelectionDidChange:(NSNotification *)notification
{
//    // FIXME: unsafe
//    playlist_item_t * p_item = [[_outlineView itemAtRow:[_outlineView selectedRow]] pointerValue];
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
//        [[VLCInfo sharedInstance] updatePanelWithItem:p_item->p_input];
//    }
}

- (BOOL)isSelectionEmpty
{
    return [_outlineView selectedRow] == -1;
}

- (void)currentlyPlayingItemChanged
{
    PLItem *item = [[self model] currentlyPlayingItem];
    if (!item)
        return;

    // select item
    NSInteger itemIndex = [_outlineView rowForItem:item];
    if (itemIndex < 0) {
        // expand if needed
        while (item != nil) {
            PLItem *parent = [item parent];

            if (![_outlineView isExpandable: parent])
                break;
            if (![_outlineView isItemExpanded: parent])
                [_outlineView expandItem: parent];
            item = parent;
        }

        // search for row again
        itemIndex = [_outlineView rowForItem:item];
        if (itemIndex < 0) {
            return;
        }
    }

    [_outlineView selectRowIndexes: [NSIndexSet indexSetWithIndex: itemIndex] byExtendingSelection: NO];
}

/* When called retrieves the selected outlineview row and plays that node or item */
- (IBAction)playItem:(id)sender
{
    playlist_t *p_playlist = pl_Get(VLCIntf);

    // ignore clicks on column header when handling double action
    if (sender == _outlineView && [_outlineView clickedRow] == -1)
        return;

    PLItem *o_item = [_outlineView itemAtRow:[_outlineView selectedRow]];
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
    NSIndexSet *selectedRows = [_outlineView selectedRowIndexes];
    [selectedRows enumerateIndexesUsingBlock:^(NSUInteger idx, BOOL *stop) {

        PLItem *o_item = [_outlineView itemAtRow:idx];

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

    o_selected_indexes = [_outlineView selectedRowIndexes];
    i_count = [o_selected_indexes count];

    NSUInteger indexes[i_count];
    [o_selected_indexes getIndexes:indexes maxCount:i_count inIndexRange:nil];
    for (int i = 0; i < i_count; i++) {
        PLItem *o_item = [_outlineView itemAtRow:indexes[i]];
        [_outlineView deselectRow: indexes[i]];

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

    o_selected_indexes = [_outlineView selectedRowIndexes];
    i_count = [o_selected_indexes count];

    NSUInteger indexes[i_count];
    [o_selected_indexes getIndexes:indexes maxCount:i_count inIndexRange:nil];
    for (int i = 0; i < i_count; i++) {
        PLItem *o_item = [_outlineView itemAtRow: indexes[i]];

        if (![o_item isLeaf])
            continue;

        libvlc_ArtRequest(p_intf->p_libvlc, [o_item input], META_REQUEST_OPTION_NONE);
    }
    [self playlistUpdated];
}

- (IBAction)selectAll:(id)sender
{
    [_outlineView selectAll: nil];
}

- (IBAction)showInfoPanel:(id)sender
{
    [[VLCInfo sharedInstance] initPanel];
}

- (IBAction)deleteItem:(id)sender
{
    [_model deleteSelectedItem];
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

//    if ([_outlineView selectedRow] > -1) {
//        p_item = [[_outlineView itemAtRow: [_outlineView selectedRow]] pointerValue];
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

- (input_item_t *)createItem:(NSDictionary *)itemToCreateDict
{
    intf_thread_t *p_intf = VLCIntf;
    playlist_t *p_playlist = pl_Get(p_intf);

    input_item_t *p_input;
    BOOL b_rem = FALSE, b_dir = FALSE, b_writable = FALSE;
    NSString *uri, *name, *path;
    NSURL * url;
    NSArray *optionsArray;

    /* Get the item */
    uri = (NSString *)[itemToCreateDict objectForKey: @"ITEM_URL"];
    url = [NSURL URLWithString: uri];
    path = [url path];
    name = (NSString *)[itemToCreateDict objectForKey: @"ITEM_NAME"];
    optionsArray = (NSArray *)[itemToCreateDict objectForKey: @"ITEM_OPTIONS"];

    if ([[NSFileManager defaultManager] fileExistsAtPath:path isDirectory:&b_dir] && b_dir &&
        [[NSWorkspace sharedWorkspace] getFileSystemInfoForPath:path isRemovable: &b_rem
                                                     isWritable:&b_writable isUnmountable:NULL description:NULL type:NULL] && b_rem && !b_writable && [url isFileURL]) {

        NSString *diskType = [[VLCStringUtility sharedInstance] getVolumeTypeFromMountPath: path];
        msg_Dbg(p_intf, "detected optical media of type %s in the file input", [diskType UTF8String]);

        if ([diskType isEqualToString: kVLCMediaDVD])
            uri = [NSString stringWithFormat: @"dvdnav://%@", [[VLCStringUtility sharedInstance] getBSDNodeFromMountPath: path]];
        else if ([diskType isEqualToString: kVLCMediaVideoTSFolder])
            uri = [NSString stringWithFormat: @"dvdnav://%@", path];
        else if ([diskType isEqualToString: kVLCMediaAudioCD])
            uri = [NSString stringWithFormat: @"cdda://%@", [[VLCStringUtility sharedInstance] getBSDNodeFromMountPath: path]];
        else if ([diskType isEqualToString: kVLCMediaVCD])
            uri = [NSString stringWithFormat: @"vcd://%@#0:0", [[VLCStringUtility sharedInstance] getBSDNodeFromMountPath: path]];
        else if ([diskType isEqualToString: kVLCMediaSVCD])
            uri = [NSString stringWithFormat: @"vcd://%@@0:0", [[VLCStringUtility sharedInstance] getBSDNodeFromMountPath: path]];
        else if ([diskType isEqualToString: kVLCMediaBD] || [diskType isEqualToString: kVLCMediaBDMVFolder])
            uri = [NSString stringWithFormat: @"bluray://%@", path];
        else
            msg_Warn(VLCIntf, "unknown disk type, treating %s as regular input", [path UTF8String]);

        p_input = input_item_New([uri UTF8String], [[[NSFileManager defaultManager] displayNameAtPath:path] UTF8String]);
    }
    else
        p_input = input_item_New([uri fileSystemRepresentation], name ? [name UTF8String] : NULL);

    if (!p_input)
        return NULL;

    if (optionsArray) {
        NSUInteger count = [optionsArray count];
        for (NSUInteger i = 0; i < count; i++)
            input_item_AddOption(p_input, [optionsArray[i] UTF8String], VLC_INPUT_OPTION_TRUSTED);
    }

    /* Recent documents menu */
    if (url != nil && (BOOL)config_GetInt(p_playlist, "macosx-recentitems") == YES)
        [[NSDocumentController sharedDocumentController] noteNewRecentDocumentURL:url];

    return p_input;
}

- (void)addPlaylistItems:(NSArray*)array
{

    int i_plItemId = -1;

    // add items directly to media library if this is the current root
    if ([[self model] currentRootType] == ROOT_TYPE_MEDIALIBRARY)
        i_plItemId = [[[self model] rootItem] plItemId];

    BOOL b_autoplay = var_InheritBool(VLCIntf, "macosx-autoplay");

    [self addPlaylistItems:array withParentItemId:i_plItemId atPos:-1 startPlayback:b_autoplay];
}

- (void)addPlaylistItems:(NSArray*)array withParentItemId:(int)i_plItemId atPos:(int)i_position startPlayback:(BOOL)b_start
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

    NSUInteger count = [array count];
    int i_current_offset = 0;
    for (NSUInteger i = 0; i < count; ++i) {

        NSDictionary *o_current_item = array[i];
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

- (IBAction)recursiveExpandNode:(id)sender
{
    NSIndexSet * selectedRows = [_outlineView selectedRowIndexes];
    NSUInteger count = [selectedRows count];
    NSUInteger indexes[count];
    [selectedRows getIndexes:indexes maxCount:count inIndexRange:nil];

    id item;
    playlist_item_t *p_item;
    for (NSUInteger i = 0; i < count; i++) {
        item = [_outlineView itemAtRow: indexes[i]];

        /* We need to collapse the node first, since OSX refuses to recursively
         expand an already expanded node, even if children nodes are collapsed. */
        if ([_outlineView isExpandable:item]) {
            [_outlineView collapseItem: item collapseChildren: YES];
            [_outlineView expandItem: item expandChildren: YES];
        }

        selectedRows = [_outlineView selectedRowIndexes];
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

    pt = [_outlineView convertPoint: [o_event locationInWindow] fromView: nil];
    int row = [_outlineView rowAtPoint:pt];
    if (row != -1 && ![[_outlineView selectedRowIndexes] containsIndex: row])
        [_outlineView selectRowIndexes:[NSIndexSet indexSetWithIndex:row] byExtendingSelection:NO];

    b_item_sel = (row != -1 && [_outlineView selectedRow] != -1);
    b_rows = [_outlineView numberOfRows] != 0;

    playlist_t *p_playlist = pl_Get(VLCIntf);
    bool b_del_allowed = [[self model] editAllowed];

    [_playPlaylistMenuItem setEnabled: b_item_sel];
    [_deletePlaylistMenuItem setEnabled: b_item_sel && b_del_allowed];
    [_selectAllPlaylistMenuItem setEnabled: b_rows];
    [_infoPlaylistMenuItem setEnabled: b_item_sel];
    [_preparsePlaylistMenuItem setEnabled: b_item_sel];
    [_recursiveExpandPlaylistMenuItem setEnabled: b_item_sel];
    [_sortNamePlaylistMenuItem setEnabled: b_item_sel];
    [_sortAuthorPlaylistMenuItem setEnabled: b_item_sel];
    [_downloadCoverArtPlaylistMenuItem setEnabled: b_item_sel];

    return _playlistMenu;
}

- (void)outlineView:(NSOutlineView *)outlineView didClickTableColumn:(NSTableColumn *)aTableColumn
{
    int type = 0;
    intf_thread_t *p_intf = VLCIntf;
    NSString * identifier = [aTableColumn identifier];

    playlist_t *p_playlist = pl_Get(p_intf);

    if (_sortTableColumn == aTableColumn)
        b_isSortDescending = !b_isSortDescending;
    else
        b_isSortDescending = false;

    if (b_isSortDescending)
        type = ORDER_REVERSE;
    else
        type = ORDER_NORMAL;

    [[self model] sortForColumn:identifier withMode:type];

    // TODO rework, why do we need a full call here?
//    [self playlistUpdated];

    /* Clear indications of any existing column sorting */
    NSUInteger count = [[_outlineView tableColumns] count];
    for (NSUInteger i = 0 ; i < count ; i++)
        [_outlineView setIndicatorImage:nil inTableColumn: [_outlineView tableColumns][i]];

    [_outlineView setHighlightedTableColumn:nil];
    _sortTableColumn = aTableColumn;
    [_outlineView setHighlightedTableColumn:aTableColumn];

    if (b_isSortDescending)
        [_outlineView setIndicatorImage:_descendingSortingImage inTableColumn:aTableColumn];
    else
        [_outlineView setIndicatorImage:_ascendingSortingImage inTableColumn:aTableColumn];
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
    if (var_InheritBool(VLCIntf, "macosx-large-text"))
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

- (void)setColumn:(NSString *)columnIdentifier state:(NSInteger)i_state translationDict:(NSDictionary *)translationDict
{
    if (i_state == NSOnState) {
        NSString *title = [translationDict objectForKey:columnIdentifier];
        if (!title)
            return;

        NSTableColumn *tableColumn = [[NSTableColumn alloc] initWithIdentifier:columnIdentifier];
        [tableColumn setEditable:NO];
        [[tableColumn dataCell] setFont:[NSFont controlContentFontOfSize:11.]];

        [[tableColumn headerCell] setStringValue:[translationDict objectForKey:columnIdentifier]];

        if ([columnIdentifier isEqualToString: TRACKNUM_COLUMN]) {
            [tableColumn setWidth:20.];
            [tableColumn setResizingMask:NSTableColumnNoResizing];
            [[tableColumn headerCell] setStringValue:@"#"];
        }

        [_outlineView addTableColumn:tableColumn];
        [_outlineView reloadData];
        [_outlineView setNeedsDisplay: YES];
    }
    else
        [_outlineView removeTableColumn: [_outlineView tableColumnWithIdentifier:columnIdentifier]];

    [_outlineView setOutlineTableColumn: [_outlineView tableColumnWithIdentifier:TITLE_COLUMN]];
}

- (void)saveTableColumns
{
    NSMutableArray *arrayToSave = [[NSMutableArray alloc] init];
    NSArray *columns = [[NSArray alloc] initWithArray:[_outlineView tableColumns]];
    NSUInteger columnCount = [columns count];
    NSTableColumn *currentColumn;
    for (NSUInteger i = 0; i < columnCount; i++) {
        currentColumn = columns[i];
        [arrayToSave addObject:[NSArray arrayWithObjects:[currentColumn identifier], [NSNumber numberWithFloat:[currentColumn width]], nil]];
    }
    [[NSUserDefaults standardUserDefaults] setObject:arrayToSave forKey:@"PlaylistColumnSelection"];
    [[NSUserDefaults standardUserDefaults] synchronize];
}

- (BOOL)isValidResumeItem:(input_item_t *)p_item
{
    char *psz_url = input_item_GetURI(p_item);
    NSString *urlString = toNSStr(psz_url);
    free(psz_url);

    if ([urlString isEqualToString:@""])
        return NO;

    NSURL *url = [NSURL URLWithString:urlString];

    if (![url isFileURL])
        return NO;

    BOOL isDir = false;
    if (![[NSFileManager defaultManager] fileExistsAtPath:[url path] isDirectory:&isDir])
        return NO;

    if (isDir)
        return NO;

    return YES;
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

    CompletionBlock completionBlock = ^(enum ResumeResult result) {

        if (result == RESUME_RESTART)
            return;

        mtime_t lastPos = (mtime_t)lastPosition.intValue * 1000000;
        msg_Dbg(VLCIntf, "continuing playback at %lld", lastPos);
        var_SetInteger(p_input_thread, "time", lastPos);

        if (result == RESUME_ALWAYS)
            config_PutInt(VLCIntf, "macosx-continue-playback", 1);
    };

    if (settingValue == 1) { // always
        completionBlock(RESUME_NOW);
        return;
    }

    [[[VLCMain sharedInstance] resumeDialog] showWindowWithItem:p_item
                                               withLastPosition:lastPosition.intValue
                                                completionBlock:completionBlock];

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
                [mutDict removeObjectForKey:[mediaList firstObject]];
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
}

@end
