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

#import "VLCMain.h"
#import "VLCPlaylist.h"
#import "VLCMainMenu.h"
#import "VLCPlaylistInfo.h"
#import "VLCResumeDialogController.h"
#import "VLCOpenWindowController.h"

#include <vlc_actions.h>
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

    VLCPLModel *_model;

    // information for playlist table columns menu

    NSDictionary *_translationsForPlaylistTableColumns;
    NSArray *_menuOrderOfPlaylistTableColumns;
}

- (void)saveTableColumns;
@end

@implementation VLCPlaylist

- (id)init
{
    self = [super init];
    if (self) {
        /* This uses a private API, but works fine on all current OSX releases.
         * Radar ID 11739459 request a public API for this. However, it is probably
         * easier and faster to recreate similar looking bitmaps ourselves. */
        _ascendingSortingImage = [[NSOutlineView class] _defaultTableHeaderSortImage];
        _descendingSortingImage = [[NSOutlineView class] _defaultTableHeaderReverseSortImage];

        [[NSNotificationCenter defaultCenter] addObserver: self selector: @selector(applicationWillTerminate:) name: NSApplicationWillTerminateNotification object: nil];


        _translationsForPlaylistTableColumns = [[NSDictionary alloc] initWithObjectsAndKeys:
                                                _NS("Track Number"),  TRACKNUM_COLUMN,
                                                _NS("Title"),         TITLE_COLUMN,
                                                _NS("Author"),        ARTIST_COLUMN,
                                                _NS("Duration"),      DURATION_COLUMN,
                                                _NS("Genre"),         GENRE_COLUMN,
                                                _NS("Album"),         ALBUM_COLUMN,
                                                _NS("Description"),   DESCRIPTION_COLUMN,
                                                _NS("Date"),          DATE_COLUMN,
                                                _NS("Language"),      LANGUAGE_COLUMN,
                                                _NS("URI"),           URI_COLUMN,
                                                _NS("File Size"),     FILESIZE_COLUMN,
                                                nil];
        // this array also assigns tags (index) to type of menu item
        _menuOrderOfPlaylistTableColumns = [[NSArray alloc] initWithObjects: TRACKNUM_COLUMN, TITLE_COLUMN,
                                            ARTIST_COLUMN, DURATION_COLUMN, GENRE_COLUMN, ALBUM_COLUMN,
                                            DESCRIPTION_COLUMN, DATE_COLUMN, LANGUAGE_COLUMN, URI_COLUMN,
                                            FILESIZE_COLUMN,nil];

    }
    return self;
}

+ (void)initialize
{
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    NSMutableArray *columnArray = [[NSMutableArray alloc] init];
    [columnArray addObject: [NSArray arrayWithObjects:TITLE_COLUMN, [NSNumber numberWithFloat:190.], nil]];
    [columnArray addObject: [NSArray arrayWithObjects:ARTIST_COLUMN, [NSNumber numberWithFloat:95.], nil]];
    [columnArray addObject: [NSArray arrayWithObjects:DURATION_COLUMN, [NSNumber numberWithFloat:95.], nil]];

    NSDictionary *appDefaults = [NSDictionary dictionaryWithObjectsAndKeys:
                                 [NSArray arrayWithArray:columnArray], @"PlaylistColumnSelection", nil];

    [defaults registerDefaults:appDefaults];
}

- (VLCPLModel *)model
{
    return _model;
}

- (void)reloadStyles
{
    NSFont *fontToUse;
    CGFloat rowHeight;
    if (var_InheritBool(getIntf(), "macosx-large-text")) {
        fontToUse = [NSFont systemFontOfSize:13.];
        rowHeight = 21.;
    } else {
        fontToUse = [NSFont systemFontOfSize:11.];
        rowHeight = 16.;
    }

    NSArray *columns = [_outlineView tableColumns];
    NSUInteger count = columns.count;
    for (NSUInteger x = 0; x < count; x++)
        [[[columns objectAtIndex:x] dataCell] setFont:fontToUse];
    [_outlineView setRowHeight:rowHeight];
}

- (void)awakeFromNib
{
    // This is only called for the playlist popup menu
    [self initStrings];
}

- (void)setOutlineView:(VLCPlaylistView * __nullable)outlineView
{
    _outlineView = outlineView;
    [_outlineView setDelegate:self];

    playlist_t * p_playlist = pl_Get(getIntf());

    _model = [[VLCPLModel alloc] initWithOutlineView:_outlineView playlist:p_playlist rootItem:p_playlist->p_playing];
    [_outlineView setDataSource:_model];
    [_outlineView reloadData];

    [_outlineView setTarget: self];
    [_outlineView setDoubleAction: @selector(playItem:)];

    [_outlineView setAllowsEmptySelection: NO];
    [_outlineView registerForDraggedTypes: [NSArray arrayWithObjects:NSFilenamesPboardType, @"VLCPlaylistItemPboardType", nil]];
    [_outlineView setIntercellSpacing: NSMakeSize (0.0, 1.0)];

    [self reloadStyles];
}

- (void)setPlaylistHeaderView:(NSTableHeaderView * __nullable)playlistHeaderView
{
    _playlistHeaderView = playlistHeaderView;

    // Setup playlist table column selection for both context and main menu
    NSMenu *contextMenu = [[NSMenu alloc] init];
    [self setupPlaylistTableColumnsForMenu:contextMenu];
    [_playlistHeaderView setMenu: contextMenu];
    [self setupPlaylistTableColumnsForMenu:[[[VLCMain sharedInstance] mainMenu] playlistTableColumnsMenu]];

    NSArray * columnArray = [[NSUserDefaults standardUserDefaults] arrayForKey:@"PlaylistColumnSelection"];

    for (NSArray *column in columnArray) {
        NSString *columnName = column[0];
        NSNumber *columnWidth = column[1];

        if ([columnName isEqualToString:@"status"])
            continue;

        if(![self setPlaylistColumnTableState: NSOnState forColumn:columnName])
            continue;

        [[_outlineView tableColumnWithIdentifier: columnName] setWidth: [columnWidth floatValue]];
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
    [_recursiveExpandPlaylistMenuItem setTitle: _NS("Expand All")];
    [_recursiveCollapsePlaylistMenuItem setTitle: _NS("Collapse All")];
    [_selectAllPlaylistMenuItem setTitle: _NS("Select All")];
    [_infoPlaylistMenuItem setTitle: _NS("Media Information...")];
    [_revealInFinderPlaylistMenuItem setTitle: _NS("Reveal in Finder")];
    [_addFilesToPlaylistMenuItem setTitle: _NS("Add File...")];
}

- (void)playlistUpdated
{
    [_outlineView reloadData];
}

- (void)playbackModeUpdated
{
    [_model playbackModeUpdated];
}


- (BOOL)isSelectionEmpty
{
    return [_outlineView selectedRow] == -1;
}

- (void)currentlyPlayingItemChanged
{
    VLCPLItem *item = [[self model] currentlyPlayingItem];
    if (!item)
        return;

    // Search for item row for selection
    NSInteger itemIndex = [_outlineView rowForItem:item];
    if (itemIndex < 0) {
        // Expand if needed. This must be done from root to child
        // item in order to work
        NSMutableArray *itemsToExpand = [NSMutableArray array];
        VLCPLItem *tmpItem = [item parent];
        while (tmpItem != nil) {
            [itemsToExpand addObject:tmpItem];
            tmpItem = [tmpItem parent];
        }

        for(int i = (int)itemsToExpand.count - 1; i >= 0; i--) {
            VLCPLItem *currentItem = [itemsToExpand objectAtIndex:i];
            [_outlineView expandItem: currentItem];
        }
    }

    // Update highlight for currently playing item
    [_outlineView reloadData];

    // Search for row again
    itemIndex = [_outlineView rowForItem:item];
    if (itemIndex < 0) {
        return;
    }

    [_outlineView selectRowIndexes: [NSIndexSet indexSetWithIndex: itemIndex] byExtendingSelection: NO];
    [_outlineView scrollRowToVisible: itemIndex];
}

#pragma mark -
#pragma mark Playlist actions

/* When called retrieves the selected outlineview row and plays that node or item */
- (IBAction)playItem:(id)sender
{
    playlist_t *p_playlist = pl_Get(getIntf());

    // ignore clicks on column header when handling double action
    if (sender == _outlineView && [_outlineView clickedRow] == -1)
        return;

    VLCPLItem *o_item = [_outlineView itemAtRow:[_outlineView selectedRow]];
    if (!o_item)
        return;

    PL_LOCK;
    playlist_item_t *p_item = playlist_ItemGetById(p_playlist, [o_item plItemId]);
    playlist_item_t *p_node = playlist_ItemGetById(p_playlist, [[[self model] rootItem] plItemId]);

    if (p_item && p_node) {
        playlist_ViewPlay(p_playlist, p_node, p_item);
    }
    PL_UNLOCK;
}

- (IBAction)revealItemInFinder:(id)sender
{
    NSIndexSet *selectedRows = [_outlineView selectedRowIndexes];
    if (selectedRows.count < 1)
        return;

    VLCPLItem *o_item = [_outlineView itemAtRow:selectedRows.firstIndex];

    char *psz_url = input_item_GetURI([o_item input]);
    if (!psz_url)
        return;
    char *psz_path = vlc_uri2path(psz_url);
    NSString *path = toNSStr(psz_path);
    free(psz_url);
    free(psz_path);

    msg_Dbg(getIntf(), "Reveal url %s in finder", [path UTF8String]);
    [[NSWorkspace sharedWorkspace] selectFile: path inFileViewerRootedAtPath: path];
}

- (IBAction)selectAll:(id)sender
{
    [_outlineView selectAll: nil];
}

- (IBAction)showInfoPanel:(id)sender
{
    [[[VLCMain sharedInstance] currentMediaInfoPanel] toggleWindow:sender];
}

- (IBAction)addFilesToPlaylist:(id)sender
{
    NSIndexSet *selectedRows = [_outlineView selectedRowIndexes];

    int position = -1;
    VLCPLItem *parentItem = [[self model] rootItem];

    if (selectedRows.count >= 1) {
        position = (int)selectedRows.firstIndex + 1;
        parentItem = [_outlineView itemAtRow:selectedRows.firstIndex];
        if ([parentItem parent] != nil)
            parentItem = [parentItem parent];
    }

    [[[VLCMain sharedInstance] open] openFileWithAction:^(NSArray *files) {
        [self addPlaylistItems:files
              withParentItemId:[parentItem plItemId]
                         atPos:position
                 startPlayback:NO];
    }];
}

- (IBAction)deleteItem:(id)sender
{
    [_model deleteSelectedItem];
}

// Actions for playlist column selections


- (void)togglePlaylistColumnTable:(id)sender
{
    NSInteger i_new_state = ![sender state];
    NSInteger i_tag = [sender tag];

    NSString *column = [_menuOrderOfPlaylistTableColumns objectAtIndex:i_tag];

    [self setPlaylistColumnTableState:i_new_state forColumn:column];
}

- (BOOL)setPlaylistColumnTableState:(NSInteger)i_state forColumn:(NSString *)columnId
{
    NSUInteger i_tag = [_menuOrderOfPlaylistTableColumns indexOfObject: columnId];
    // prevent setting unknown columns
    if(i_tag == NSNotFound)
        return NO;

    // update state of menu items
    [[[_playlistHeaderView menu] itemWithTag: i_tag] setState: i_state];
    [[[[[VLCMain sharedInstance] mainMenu] playlistTableColumnsMenu] itemWithTag: i_tag] setState: i_state];

    // Change outline view
    if (i_state == NSOnState) {
        NSString *title = [_translationsForPlaylistTableColumns objectForKey:columnId];
        if (!title)
            return NO;

        NSTableColumn *tableColumn = [[NSTableColumn alloc] initWithIdentifier:columnId];
        [tableColumn setEditable:NO];
        [[tableColumn dataCell] setFont:[NSFont controlContentFontOfSize:11.]];

        [[tableColumn headerCell] setStringValue:[_translationsForPlaylistTableColumns objectForKey:columnId]];

        if ([columnId isEqualToString: TRACKNUM_COLUMN]) {
            [tableColumn setMinWidth:20.];
            [tableColumn setMaxWidth:70.];
            [[tableColumn headerCell] setStringValue:@"#"];

        } else {
            [tableColumn setMinWidth:42.];
        }

        [_outlineView addTableColumn:tableColumn];
        [_outlineView reloadData];
        [_outlineView setNeedsDisplay: YES];
    }
    else
        [_outlineView removeTableColumn: [_outlineView tableColumnWithIdentifier:columnId]];

    [_outlineView setOutlineTableColumn: [_outlineView tableColumnWithIdentifier:TITLE_COLUMN]];

    return YES;
}

- (BOOL)validateMenuItem:(NSMenuItem *)item
{
    if ([item action] == @selector(revealItemInFinder:)) {
        NSIndexSet *selectedRows = [_outlineView selectedRowIndexes];
        if (selectedRows.count != 1)
            return NO;

        VLCPLItem *o_item = [_outlineView itemAtRow:selectedRows.firstIndex];

        // Check if item exists in file system
        char *psz_url = input_item_GetURI([o_item input]);
        NSURL *url = [NSURL URLWithString:toNSStr(psz_url)];
        free(psz_url);
        if (![url isFileURL])
            return NO;
        if (![[NSFileManager defaultManager] fileExistsAtPath:[url path]])
            return NO;

    } else if ([item action] == @selector(deleteItem:)) {
        return [_outlineView numberOfSelectedRows] > 0 && _model.editAllowed;
    } else if ([item action] == @selector(selectAll:)) {
        return [_outlineView numberOfRows] > 0;
    } else if ([item action] == @selector(playItem:)) {
        return [_outlineView numberOfSelectedRows] > 0;
    } else if ([item action] == @selector(recursiveExpandOrCollapseNode:)) {
        return [_outlineView numberOfSelectedRows] > 0;
    } else if ([item action] == @selector(showInfoPanel:)) {
        return [_outlineView numberOfSelectedRows] > 0;
    }

    return YES;
}

#pragma mark -
#pragma mark Helper for playlist table columns

- (void)setupPlaylistTableColumnsForMenu:(NSMenu *)menu
{
    NSMenuItem *menuItem;
    NSUInteger count = [_menuOrderOfPlaylistTableColumns count];
    for (NSUInteger i = 0; i < count; i++) {
        NSString *columnId = [_menuOrderOfPlaylistTableColumns objectAtIndex:i];
        NSString *title = [_translationsForPlaylistTableColumns objectForKey:columnId];
        menuItem = [menu addItemWithTitle:title
                                   action:@selector(togglePlaylistColumnTable:)
                            keyEquivalent:@""];
        [menuItem setTarget:self];
        [menuItem setTag:i];

        /* don't set a valid action for the title column selector, since we want it to be disabled */
        if ([columnId isEqualToString: TITLE_COLUMN])
            [menuItem setAction:nil];

    }
}

- (void)saveTableColumns
{
    NSMutableArray *arrayToSave = [[NSMutableArray alloc] init];
    NSArray *columns = [[NSArray alloc] initWithArray:[_outlineView tableColumns]];
    NSUInteger columnCount = [columns count];
    NSTableColumn *currentColumn;
    for (NSUInteger i = 0; i < columnCount; i++) {
        currentColumn = [columns objectAtIndex:i];
        [arrayToSave addObject:[NSArray arrayWithObjects:[currentColumn identifier], [NSNumber numberWithFloat:[currentColumn width]], nil]];
    }
    [[NSUserDefaults standardUserDefaults] setObject:arrayToSave forKey:@"PlaylistColumnSelection"];
    [[NSUserDefaults standardUserDefaults] synchronize];
}

#pragma mark -
#pragma mark Item helpers

- (input_item_t *)createItem:(NSDictionary *)itemToCreateDict
{
    intf_thread_t *p_intf = getIntf();

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
            msg_Warn(getIntf(), "unknown disk type, treating %s as regular input", [path UTF8String]);

        p_input = input_item_New([uri UTF8String], [[[NSFileManager defaultManager] displayNameAtPath:path] UTF8String]);
    }
    else
        p_input = input_item_New([uri fileSystemRepresentation], name ? [name UTF8String] : NULL);

    if (!p_input)
        return NULL;

    if (optionsArray) {
        NSUInteger count = [optionsArray count];
        for (NSUInteger i = 0; i < count; i++)
            input_item_AddOption(p_input, [[optionsArray objectAtIndex:i] UTF8String], VLC_INPUT_OPTION_TRUSTED);
    }

    /* Recent documents menu */
    if (url != nil && var_InheritBool(getIntf(), "macosx-recentitems"))
        [[NSDocumentController sharedDocumentController] noteNewRecentDocumentURL:url];

    return p_input;
}

- (NSArray *)createItemsFromExternalPasteboard:(NSPasteboard *)pasteboard
{
    NSArray *o_array = [NSArray array];
    if (![[pasteboard types] containsObject: NSFilenamesPboardType])
        return o_array;

    NSArray *o_values = [[pasteboard propertyListForType: NSFilenamesPboardType] sortedArrayUsingSelector:@selector(caseInsensitiveCompare:)];
    NSUInteger count = [o_values count];

    for (NSUInteger i = 0; i < count; i++) {
        NSDictionary *o_dic;
        char *psz_uri = vlc_path2uri([[o_values objectAtIndex:i] UTF8String], NULL);
        if (!psz_uri)
            continue;

        o_dic = [NSDictionary dictionaryWithObject:toNSStr(psz_uri) forKey:@"ITEM_URL"];
        free(psz_uri);

        o_array = [o_array arrayByAddingObject: o_dic];
    }

    return o_array;
}

- (void)addPlaylistItems:(NSArray*)array
{

    int i_plItemId = -1;

    BOOL b_autoplay = var_InheritBool(getIntf(), "macosx-autoplay");

    [self addPlaylistItems:array withParentItemId:i_plItemId atPos:-1 startPlayback:b_autoplay];
}

- (void)addPlaylistItems:(NSArray*)array tryAsSubtitle:(BOOL)isSubtitle
{
    input_thread_t *p_input = pl_CurrentInput(getIntf());
    if (isSubtitle && array.count == 1 && p_input) {
        int i_result = input_AddSlave(p_input, SLAVE_TYPE_SPU,
                    [[[array firstObject] objectForKey:@"ITEM_URL"] UTF8String],
                    true, true, true);
        if (i_result == VLC_SUCCESS) {
            vlc_object_release(p_input);
            return;
        }
    }

    if (p_input)
        vlc_object_release(p_input);

    [self addPlaylistItems:array];
}

- (void)addPlaylistItems:(NSArray*)array withParentItemId:(int)i_plItemId atPos:(int)i_position startPlayback:(BOOL)b_start
{
    playlist_t * p_playlist = pl_Get(getIntf());
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

        NSDictionary *o_current_item = [array objectAtIndex:i];
        input_item_t *p_input = [self createItem: o_current_item];
        if (!p_input)
            continue;

        int i_pos = (i_position == -1) ? PLAYLIST_END : i_position + i_current_offset++;
        playlist_item_t *p_item = playlist_NodeAddInput(p_playlist, p_input,
                                                        p_parent, i_pos);
        if (!p_item)
            continue;

        if (i == 0 && b_start) {
            playlist_ViewPlay(p_playlist, p_parent, p_item);
        }
        input_item_Release(p_input);
    }
    PL_UNLOCK;
}

- (IBAction)recursiveExpandOrCollapseNode:(id)sender
{
    bool expand = (sender == _recursiveExpandPlaylistMenuItem);

    NSIndexSet * selectedRows = [_outlineView selectedRowIndexes];
    NSUInteger count = [selectedRows count];
    NSUInteger indexes[count];
    [selectedRows getIndexes:indexes maxCount:count inIndexRange:nil];

    id item;
    for (NSUInteger i = 0; i < count; i++) {
        item = [_outlineView itemAtRow: indexes[i]];

        /* We need to collapse the node first, since OSX refuses to recursively
         expand an already expanded node, even if children nodes are collapsed. */
        if ([_outlineView isExpandable:item]) {
            [_outlineView collapseItem: item collapseChildren: YES];

            if (expand)
                [_outlineView expandItem: item expandChildren: YES];
        }

        selectedRows = [_outlineView selectedRowIndexes];
        [selectedRows getIndexes:indexes maxCount:count inIndexRange:nil];
    }
}

- (NSMenu *)menuForEvent:(NSEvent *)o_event
{
    if (!b_playlistmenu_nib_loaded)
        b_playlistmenu_nib_loaded = [[NSBundle mainBundle] loadNibNamed:@"PlaylistMenu" owner:self topLevelObjects:nil];

    NSPoint pt = [_outlineView convertPoint: [o_event locationInWindow] fromView: nil];
    NSInteger row = [_outlineView rowAtPoint:pt];
    if (row != -1 && ![[_outlineView selectedRowIndexes] containsIndex: row])
        [_outlineView selectRowIndexes:[NSIndexSet indexSetWithIndex:row] byExtendingSelection:NO];

    // TODO Reenable once per-item info panel is supported again
    _infoPlaylistMenuItem.hidden = YES;

    return _playlistMenu;
}

- (void)outlineView:(NSOutlineView *)outlineView didClickTableColumn:(NSTableColumn *)aTableColumn
{
    int type = 0;
    NSString * identifier = [aTableColumn identifier];

    if (_sortTableColumn == aTableColumn)
        b_isSortDescending = !b_isSortDescending;
    else
        b_isSortDescending = false;

    if (b_isSortDescending)
        type = ORDER_REVERSE;
    else
        type = ORDER_NORMAL;

    [[self model] sortForColumn:identifier withMode:type];

    /* Clear indications of any existing column sorting */
    NSUInteger count = [[_outlineView tableColumns] count];
    for (NSUInteger i = 0 ; i < count ; i++)
        [_outlineView setIndicatorImage:nil inTableColumn: [[_outlineView tableColumns] objectAtIndex:i]];

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
    intf_thread_t * p_intf = getIntf();
    if (!p_intf)
        return;
    playlist_t *p_playlist = pl_Get(p_intf);

    NSFont *fontToUse;
    if (var_InheritBool(getIntf(), "macosx-large-text"))
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

@end
