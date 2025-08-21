/*****************************************************************************
 * VLCPlayQueueMenuController.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2002-2019 VLC authors and VideoLAN
 *
 * Authors: Benjamin Pracht <bigben at videolan dot org>
 *          Felix Paul Kühne <fkuehne at videolan dot org>
 *          David Fuhrmann <dfuhrmann # videolan.org>
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

#import "VLCPlayQueueMenuController.h"

#import "extensions/NSString+Helpers.h"
#import "extensions/NSMenu+VLCAdditions.h"
#import "main/VLCMain.h"
#import "playqueue/VLCPlayQueueController.h"
#import "playqueue/VLCPlayQueueModel.h"
#import "playqueue/VLCPlayQueueItem.h"
#import "playqueue/VLCPlayQueueSortingMenuController.h"
#import "windows/VLCOpenWindowController.h"
#import "panels/VLCInformationWindowController.h"

@interface VLCPlayQueueMenuController ()
{
    VLCPlayQueueController *_playQueueController;
    VLCPlayQueueSortingMenuController *_playQueueSortingMenuController;
    VLCInformationWindowController *_informationWindowController;

    NSMenuItem *_playMenuItem;
    NSMenuItem *_removeMenuItem;
    NSMenuItem *_informationMenuItem;
    NSMenuItem *_revealInFinderMenuItem;
    NSMenuItem *_addFilesToPlayQueueMenuItem;
    NSMenuItem *_clearPlayQueueMenuItem;
    NSMenuItem *_sortMenuItem;
    NSMenuItem *_createPlaylistMenuItem;
}

@property (readwrite, atomic) NSArray<NSMenuItem *> *items;
@property (readwrite, atomic) NSArray<NSMenuItem *> *multipleSelectionItems;

@end

@implementation VLCPlayQueueMenuController

- (instancetype)init
{
    self = [super init];
    if (self) {
        [self createMenu];
        _playQueueController = VLCMain.sharedInstance.playQueueController;
    }
    return self;
}

- (void)createMenu
{
    _playMenuItem = [[NSMenuItem alloc] initWithTitle:_NS("Play") action:@selector(play:) keyEquivalent:@""];
    _playMenuItem.target = self;

    _removeMenuItem = [[NSMenuItem alloc] initWithTitle:_NS("Remove from Play Queue") action:@selector(remove:) keyEquivalent:@""];
    _removeMenuItem.target = self;

    _revealInFinderMenuItem = [[NSMenuItem alloc] initWithTitle:_NS("Reveal in Finder") action:@selector(revealInFinder:) keyEquivalent:@""];
    _revealInFinderMenuItem.target = self;

    _informationMenuItem = [[NSMenuItem alloc] initWithTitle:_NS("Information...") action:@selector(showInformationPanel:) keyEquivalent:@""];
    _informationMenuItem.target = self;

    _addFilesToPlayQueueMenuItem = [[NSMenuItem alloc] initWithTitle:_NS("Add File...") action:@selector(addFilesToPlayQueue:) keyEquivalent:@""];
    _addFilesToPlayQueueMenuItem.target = self;

    _clearPlayQueueMenuItem = [[NSMenuItem alloc] initWithTitle:_NS("Clear Play Queue") action:@selector(clearPlayQueue:) keyEquivalent:@""];
    _clearPlayQueueMenuItem.target = self;

    _createPlaylistMenuItem = [[NSMenuItem alloc] initWithTitle:_NS("Create Playlist from Queue") action:@selector(createPlaylistFromQueue:) keyEquivalent:@""];
    _createPlaylistMenuItem.target = self;

    _playQueueSortingMenuController = [[VLCPlayQueueSortingMenuController alloc] init];
    _sortMenuItem = [[NSMenuItem alloc] initWithTitle:_NS("Sort Play Queue") action:nil keyEquivalent:@""];
    [_sortMenuItem setSubmenu:_playQueueSortingMenuController.playQueueSortingMenu];

    self.items = @[
        _playMenuItem,
        _removeMenuItem,
        _revealInFinderMenuItem,
        _informationMenuItem,
        NSMenuItem.separatorItem,
        _addFilesToPlayQueueMenuItem,
        _clearPlayQueueMenuItem,
        _createPlaylistMenuItem,
        _sortMenuItem
    ];

    self.multipleSelectionItems = @[
        _removeMenuItem,
        NSMenuItem.separatorItem,
        _addFilesToPlayQueueMenuItem,
        _clearPlayQueueMenuItem,
        _createPlaylistMenuItem,
        _sortMenuItem
    ];

    _playQueueMenu = [[NSMenu alloc] init];
    _playQueueMenu.itemArray = self.items;
}

- (void)setPlayQueueTableView:(NSTableView *)playQueueTableView
{
    NSNotificationCenter * const notificationCenter = NSNotificationCenter.defaultCenter;
    if (self.playQueueTableView != nil) {
        [notificationCenter removeObserver:self
                                      name:NSTableViewSelectionDidChangeNotification
                                    object:self.playQueueTableView];
    }

    _playQueueTableView = playQueueTableView;
    [notificationCenter addObserver:self
                           selector:@selector(tableViewSelectionDidChange:)
                               name:NSTableViewSelectionDidChangeNotification
                             object:self.playQueueTableView];

}

- (void)play:(id)sender
{
    NSInteger selectedRow = self.playQueueTableView.selectedRow;

    if (selectedRow != -1) {
        [_playQueueController playItemAtIndex:selectedRow];
    } else {
        [_playQueueController startPlayQueue];
    }
}

- (void)remove:(id)sender
{
    [_playQueueController removeItemsAtIndexes:self.playQueueTableView.selectedRowIndexes];
}

- (void)showInformationPanel:(id)sender
{
    if (!_informationWindowController) {
        _informationWindowController = [[VLCInformationWindowController alloc] init];
    }

    NSMutableArray * const inputItems = NSMutableArray.array;
    NSIndexSet * const selectedIndices = self.playQueueTableView.selectedRowIndexes;

    [selectedIndices enumerateIndexesUsingBlock:^(const NSUInteger idx, BOOL * const stop) {
        VLCPlayQueueItem * const item =
            [self->_playQueueController.playQueueModel playQueueItemAtIndex:idx];
        if (item == nil) {
            return;
        }
        [inputItems addObject:item.inputItem];
    }];

    _informationWindowController.representedInputItems = inputItems.copy;
    [_informationWindowController toggleWindow:sender];
}

- (void)revealInFinder:(id)sender
{
    NSInteger selectedRow = self.playQueueTableView.selectedRow;

    if (selectedRow == -1)
        return;

    VLCPlayQueueItem *item = [_playQueueController.playQueueModel playQueueItemAtIndex:selectedRow];
    if (item == nil)
        return;

    NSString *path = item.path;
    [NSWorkspace.sharedWorkspace selectFile:path inFileViewerRootedAtPath:path];
}

- (void)addFilesToPlayQueue:(id)sender
{
    NSInteger selectedRow = self.playQueueTableView.selectedRow;

    [[VLCMain.sharedInstance open] openFileWithAction:^(NSArray *files) {
        [self->_playQueueController addPlayQueueItems:files
                                           atPosition:selectedRow
                                        startPlayback:NO];
    }];
}

- (void)clearPlayQueue:(id)sender
{
    [_playQueueController clearPlayQueue];
}

- (void)createPlaylistFromQueue:(id)sender
{
    NSIndexSet * const selectedIndexes = self.playQueueTableView.selectedRowIndexes;
    
    // Show an input dialog to get the playlist name
    NSAlert * const alert = [[NSAlert alloc] init];
    alert.messageText = selectedIndexes.count > 0 ? _NS("Create Playlist from Selected Items") : _NS("Create Playlist from Queue");
    alert.informativeText = _NS("Enter a name for the new playlist:");
    [alert addButtonWithTitle:_NS("Create")];
    [alert addButtonWithTitle:_NS("Cancel")];
    
    NSTextField * const input = [[NSTextField alloc] initWithFrame:NSMakeRect(0, 0, 200, 24)];
    input.stringValue = _NS("New Playlist");
    alert.accessoryView = input;
    
    const NSInteger response = [alert runModal];
    if (response != NSAlertFirstButtonReturn) {
        return;
    }
    
    NSString * const playlistName = input.stringValue;
    if (playlistName.length == 0) {
        return;
    }
    
    const BOOL success = selectedIndexes.count > 0
        ? [_playQueueController createPlaylistFromPlayQueueWithName:playlistName itemIndexes:selectedIndexes] 
        : [_playQueueController createPlaylistFromPlayQueueWithName:playlistName];

    if (!success) {
        NSAlert * const errorAlert = [[NSAlert alloc] init];
        errorAlert.messageText = _NS("Failed to Create Playlist");
        errorAlert.informativeText = _NS("The playlist could not be created. Please check that the media library is available.");
        errorAlert.alertStyle = NSAlertStyleWarning;
        [errorAlert runModal];
    }
}

- (BOOL)validateMenuItem:(NSMenuItem *)menuItem
{
    if (menuItem == _addFilesToPlayQueueMenuItem) {
        return YES;

    } else if (menuItem == _clearPlayQueueMenuItem) {
        return (self.playQueueTableView.numberOfRows > 0);

    } else if (menuItem == _createPlaylistMenuItem) {
        return (self.playQueueTableView.numberOfRows > 0);

    } else if (menuItem == _removeMenuItem ||
               menuItem == _playMenuItem ||
               menuItem == _informationMenuItem) {
        return (self.playQueueTableView.numberOfSelectedRows > 0);

    } else if (menuItem == _revealInFinderMenuItem) {
        return (self.playQueueTableView.numberOfSelectedRows == 1);
    }

    return NO;
}

- (void)tableViewSelectionDidChange:(NSNotification *)notification
{
    NSTableView * const tableView = notification.object;
    if (tableView != self.playQueueTableView) {
        return;
    }

    const BOOL multipleSelection = tableView.selectedRowIndexes.count > 1;
    if (multipleSelection) {
        self.playQueueMenu.itemArray = self.multipleSelectionItems;
    } else {
        self.playQueueMenu.itemArray = self.items;
    }
}

@end
