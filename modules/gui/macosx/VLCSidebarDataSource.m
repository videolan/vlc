/*****************************************************************************
* VLCSidebarDataSource.m: MacOS X interface module
*****************************************************************************
* Copyright (C) 2021 VLC authors and VideoLAN
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

#import "VLCSidebarDataSource.h"

#import <vlc_services_discovery.h>

#import "PXSourceList/PXSourceList.h"
#import "PXSourceList/PXSourceListDataSource.h"

#import "VLCMain.h"
#import "VLCPlaylist.h"
#import "VLCMainWindow.h"
#import "VLCSourceListTableCellView.h"
#import "VLCSourceListItem.h"

@interface VLCSidebarDataSource() <PXSourceListDataSource, PXSourceListDelegate>
{
    NSMutableArray *o_sidebaritems;
}

@end

@implementation VLCSidebarDataSource

- (void)reloadSidebar
{
    BOOL isAReload = NO;
    if (o_sidebaritems)
        isAReload = YES;

    BOOL darkMode = NO;
    if (@available(macOS 10.14, *)) {
        NSApplication *app = [NSApplication sharedApplication];
        if ([app.effectiveAppearance.name isEqualToString:NSAppearanceNameDarkAqua]) {
            darkMode = YES;
        }
    }

    o_sidebaritems = [[NSMutableArray alloc] init];
    VLCSourceListItem *libraryItem = [VLCSourceListItem itemWithTitle:_NS("LIBRARY") identifier:@"library"];
    VLCSourceListItem *playlistItem = [VLCSourceListItem itemWithTitle:_NS("Playlist") identifier:@"playlist"];
    [playlistItem setIcon: sidebarImageFromRes(@"sidebar-playlist", darkMode)];
    VLCSourceListItem *medialibraryItem = [VLCSourceListItem itemWithTitle:_NS("Media Library") identifier:@"medialibrary"];
    [medialibraryItem setIcon: sidebarImageFromRes(@"sidebar-playlist", darkMode)];
    VLCSourceListItem *mycompItem = [VLCSourceListItem itemWithTitle:_NS("MY COMPUTER") identifier:@"mycomputer"];
    VLCSourceListItem *devicesItem = [VLCSourceListItem itemWithTitle:_NS("DEVICES") identifier:@"devices"];
    VLCSourceListItem *lanItem = [VLCSourceListItem itemWithTitle:_NS("LOCAL NETWORK") identifier:@"localnetwork"];
    VLCSourceListItem *internetItem = [VLCSourceListItem itemWithTitle:_NS("INTERNET") identifier:@"internet"];

    /* SD subnodes, inspired by the Qt intf */
    char **ppsz_longnames = NULL;
    int *p_categories = NULL;
    char **ppsz_names = vlc_sd_GetNames(pl_Get(getIntf()), &ppsz_longnames, &p_categories);
    if (!ppsz_names)
        msg_Err(getIntf(), "no sd item found"); //TODO
    char **ppsz_name = ppsz_names, **ppsz_longname = ppsz_longnames;
    int *p_category = p_categories;
    NSMutableArray *internetItems = [[NSMutableArray alloc] init];
    NSMutableArray *devicesItems = [[NSMutableArray alloc] init];
    NSMutableArray *lanItems = [[NSMutableArray alloc] init];
    NSMutableArray *mycompItems = [[NSMutableArray alloc] init];
    NSString *o_identifier;
    for (; ppsz_name && *ppsz_name; ppsz_name++, ppsz_longname++, p_category++) {
        o_identifier = toNSStr(*ppsz_name);
        switch (*p_category) {
            case SD_CAT_INTERNET:
                [internetItems addObject: [VLCSourceListItem itemWithTitle: _NS(*ppsz_longname) identifier: o_identifier]];
                [[internetItems lastObject] setIcon: sidebarImageFromRes(@"sidebar-podcast", darkMode)];
                [[internetItems lastObject] setSdtype: SD_CAT_INTERNET];
                break;
            case SD_CAT_DEVICES:
                [devicesItems addObject: [VLCSourceListItem itemWithTitle: _NS(*ppsz_longname) identifier: o_identifier]];
                [[devicesItems lastObject] setIcon: sidebarImageFromRes(@"sidebar-local", darkMode)];
                [[devicesItems lastObject] setSdtype: SD_CAT_DEVICES];
                break;
            case SD_CAT_LAN:
                [lanItems addObject: [VLCSourceListItem itemWithTitle: _NS(*ppsz_longname) identifier: o_identifier]];
                [[lanItems lastObject] setIcon: sidebarImageFromRes(@"sidebar-local", darkMode)];
                [[lanItems lastObject] setSdtype: SD_CAT_LAN];
                break;
            case SD_CAT_MYCOMPUTER:
                [mycompItems addObject: [VLCSourceListItem itemWithTitle: _NS(*ppsz_longname) identifier: o_identifier]];
                if (!strncmp(*ppsz_name, "video_dir", 9))
                    [[mycompItems lastObject] setIcon: sidebarImageFromRes(@"sidebar-movie", darkMode)];
                else if (!strncmp(*ppsz_name, "audio_dir", 9))
                    [[mycompItems lastObject] setIcon: sidebarImageFromRes(@"sidebar-music", darkMode)];
                else if (!strncmp(*ppsz_name, "picture_dir", 11))
                    [[mycompItems lastObject] setIcon: sidebarImageFromRes(@"sidebar-pictures", darkMode)];
                else
                    [[mycompItems lastObject] setIcon: [NSImage imageNamed:@"NSApplicationIcon"]];
                [[mycompItems lastObject] setSdtype: SD_CAT_MYCOMPUTER];
                break;
            default:
                msg_Warn(getIntf(), "unknown SD type found, skipping (%s)", *ppsz_name);
                break;
        }

        free(*ppsz_name);
        free(*ppsz_longname);
    }
    [mycompItem setChildren: [NSArray arrayWithArray: mycompItems]];
    [devicesItem setChildren: [NSArray arrayWithArray: devicesItems]];
    [lanItem setChildren: [NSArray arrayWithArray: lanItems]];
    [internetItem setChildren: [NSArray arrayWithArray: internetItems]];
    free(ppsz_names);
    free(ppsz_longnames);
    free(p_categories);

    [libraryItem setChildren: [NSArray arrayWithObjects:playlistItem, medialibraryItem, nil]];
    [o_sidebaritems addObject: libraryItem];
    if ([mycompItem hasChildren])
        [o_sidebaritems addObject: mycompItem];
    if ([devicesItem hasChildren])
        [o_sidebaritems addObject: devicesItem];
    if ([lanItem hasChildren])
        [o_sidebaritems addObject: lanItem];
    if ([internetItem hasChildren])
        [o_sidebaritems addObject: internetItem];

    [_sidebarView reloadData];
    [_sidebarView setDropItem:playlistItem dropChildIndex:NSOutlineViewDropOnItemIndex];
    [_sidebarView registerForDraggedTypes:[NSArray arrayWithObjects:NSFilenamesPboardType, @"VLCPlaylistItemPboardType", nil]];

    [_sidebarView setAutosaveName:@"mainwindow-sidebar"];
    [_sidebarView setDataSource:self];
    [_sidebarView setDelegate:self];
    [_sidebarView setAutosaveExpandedItems:YES];

    [_sidebarView expandItem:libraryItem expandChildren:YES];

    if (isAReload) {
        [_sidebarView expandItem:nil expandChildren:YES];
    }
}

#pragma mark -
#pragma mark Side Bar Data handling
/* taken under BSD-new from the PXSourceList sample project, adapted for VLC */
- (NSUInteger)sourceList:(PXSourceList*)sourceList numberOfChildrenOfItem:(id)item
{
    //Works the same way as the NSOutlineView data source: `nil` means a parent item
    if (item==nil)
        return [o_sidebaritems count];
    else
        return [[item children] count];
}


- (id)sourceList:(PXSourceList*)aSourceList child:(NSUInteger)index ofItem:(id)item
{
    //Works the same way as the NSOutlineView data source: `nil` means a parent item
    if (item==nil)
        return [o_sidebaritems objectAtIndex:index];
    else
        return [[item children] objectAtIndex:index];
}

- (BOOL)sourceList:(PXSourceList*)aSourceList isItemExpandable:(id)item
{
    return [item hasChildren];
}

- (BOOL)sourceList:(PXSourceList*)aSourceList itemHasIcon:(id)item
{
    return ([item icon] != nil);
}


- (NSImage*)sourceList:(PXSourceList*)aSourceList iconForItem:(id)item
{
    return [item icon];
}

- (NSMenu*)sourceList:(PXSourceList*)aSourceList menuForEvent:(NSEvent*)theEvent item:(id)item
{
    if ([theEvent type] == NSRightMouseDown || ([theEvent type] == NSLeftMouseDown && ([theEvent modifierFlags] & NSControlKeyMask) == NSControlKeyMask)) {
        if (item != nil) {
            if ([item sdtype] > 0)
            {
                NSMenu *m = [[NSMenu alloc] init];
                playlist_t * p_playlist = pl_Get(getIntf());
                BOOL sd_loaded = playlist_IsServicesDiscoveryLoaded(p_playlist, [[item identifier] UTF8String]);
                if (!sd_loaded)
                    [m addItemWithTitle:_NS("Enable") action:@selector(sdmenuhandler:) keyEquivalent:@""];
                else
                    [m addItemWithTitle:_NS("Disable") action:@selector(sdmenuhandler:) keyEquivalent:@""];
                [[m itemAtIndex:0] setRepresentedObject: [item identifier]];
                return m;
            }
        }
    }

    return nil;
}

#pragma mark -
#pragma mark Side Bar Delegate Methods
/* taken under BSD-new from the PXSourceList sample project, adapted for VLC */
- (BOOL)sourceList:(PXSourceList*)aSourceList isGroupAlwaysExpanded:(id)group
{
    if ([[group identifier] isEqualToString:@"library"])
        return YES;

    return NO;
}

- (void)sourceListSelectionDidChange:(NSNotification *)notification
{
    [(VLCMainWindow *)[_sidebarView window] sourceListSelectionDidChange:notification];
}

- (NSView *)sourceList:(PXSourceList *)aSourceList viewForItem:(id)item
{
    PXSourceListItem *sourceListItem = item;

    if ([aSourceList levelForItem:item] == 0) {
        PXSourceListTableCellView *cellView = [aSourceList makeViewWithIdentifier:@"HeaderCell" owner:nil];

        cellView.textField.editable = NO;
        cellView.textField.selectable = NO;
        cellView.textField.stringValue = sourceListItem.title ? sourceListItem.title : @"";

        return cellView;
    }

    VLCSourceListTableCellView * cellView = [aSourceList makeViewWithIdentifier:@"DataCell" owner:nil];

    cellView.textField.editable = NO;
    cellView.textField.selectable = NO;
    cellView.textField.stringValue = sourceListItem.title ? sourceListItem.title : @"";
    cellView.imageView.image = [sourceListItem icon];

    // Badge count
    {
        playlist_t *p_playlist = pl_Get(getIntf());
        playlist_item_t *p_pl_item = NULL;
        NSInteger i_playlist_size = 0;

        if ([[sourceListItem identifier] isEqualToString: @"playlist"]) {
            p_pl_item = p_playlist->p_playing;
        } else if ([[sourceListItem identifier] isEqualToString: @"medialibrary"]) {
            p_pl_item = p_playlist->p_media_library;
        }

        PL_LOCK;
        if (p_pl_item)
            i_playlist_size = p_pl_item->i_children;
        PL_UNLOCK;

        if (p_pl_item) {
            cellView.badgeView.integerValue = i_playlist_size;
        } else {
            cellView.badgeView.integerValue = sourceListItem.badgeValue.integerValue;
        }
    }

    return cellView;
}

- (NSDragOperation)sourceList:(PXSourceList *)aSourceList validateDrop:(id <NSDraggingInfo>)info proposedItem:(id)item proposedChildIndex:(NSInteger)index
{
    if ([[item identifier] isEqualToString:@"playlist"] || [[item identifier] isEqualToString:@"medialibrary"]) {
        NSPasteboard *o_pasteboard = [info draggingPasteboard];
        if ([[o_pasteboard types] containsObject: VLCPLItemPasteboadType] || [[o_pasteboard types] containsObject: NSFilenamesPboardType])
            return NSDragOperationGeneric;
    }
    return NSDragOperationNone;
}

- (BOOL)sourceList:(PXSourceList *)aSourceList acceptDrop:(id <NSDraggingInfo>)info item:(id)item childIndex:(NSInteger)index
{
    NSPasteboard *o_pasteboard = [info draggingPasteboard];

    playlist_t * p_playlist = pl_Get(getIntf());
    playlist_item_t *p_node;

    if ([[item identifier] isEqualToString:@"playlist"])
        p_node = p_playlist->p_playing;
    else
        p_node = p_playlist->p_media_library;

    if ([[o_pasteboard types] containsObject: @"VLCPlaylistItemPboardType"]) {
        NSArray * array = [[[VLCMain sharedInstance] playlist] draggedItems];

        NSUInteger count = [array count];

        PL_LOCK;
        for(NSUInteger i = 0; i < count; i++) {
            playlist_item_t *p_item = playlist_ItemGetById(p_playlist, [[array objectAtIndex:i] plItemId]);
            if (!p_item) continue;
            playlist_NodeAddCopy(p_playlist, p_item, p_node, PLAYLIST_END);
        }
        PL_UNLOCK;

        return YES;
    }

    // check if dropped item is a file
    NSArray *items = [[[VLCMain sharedInstance] playlist] createItemsFromExternalPasteboard:o_pasteboard];
    if (items.count == 0)
        return NO;

    [[[VLCMain sharedInstance] playlist] addPlaylistItems:items
                                         withParentItemId:p_node->i_id
                                                    atPos:-1
                                            startPlayback:NO];
    return YES;
}

- (id)sourceList:(PXSourceList *)aSourceList persistentObjectForItem:(id)item
{
    return [item identifier];
}

- (id)sourceList:(PXSourceList *)aSourceList itemForPersistentObject:(id)object
{
    /* the following code assumes for sakes of simplicity that only the top level
     * items are allowed to have children */

    NSArray * array = [NSArray arrayWithArray: o_sidebaritems]; // read-only arrays are noticebly faster
    NSUInteger count = [array count];
    if (count < 1)
        return nil;

    for (NSUInteger x = 0; x < count; x++) {
        id item = [array objectAtIndex:x]; // save one objc selector call
        if ([[item identifier] isEqualToString:object])
            return item;
    }

    return nil;
}

@end
