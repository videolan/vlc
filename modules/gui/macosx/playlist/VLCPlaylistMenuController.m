/*****************************************************************************
 * VLCPlaylistMenuController.m: MacOS X interface module
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

#import "VLCPlaylistMenuController.h"

#import "extensions/NSString+Helpers.h"
#import "extensions/NSMenu+VLCAdditions.h"
#import "main/VLCMain.h"
#import "playlist/VLCPlaylistController.h"
#import "playlist/VLCPlaylistModel.h"
#import "playlist/VLCPlaylistItem.h"
#import "playlist/VLCPlaylistSortingMenuController.h"
#import "windows/VLCOpenWindowController.h"
#import "panels/VLCInformationWindowController.h"

@interface VLCPlaylistMenuController ()
{
    VLCPlaylistController *_playlistController;
    VLCPlaylistSortingMenuController *_playlistSortingMenuController;
    VLCInformationWindowController *_informationWindowController;

    NSMenuItem *_playMenuItem;
    NSMenuItem *_removeMenuItem;
    NSMenuItem *_informationMenuItem;
    NSMenuItem *_revealInFinderMenuItem;
    NSMenuItem *_addFilesToPlaylistMenuItem;
    NSMenuItem *_clearPlaylistMenuItem;
    NSMenuItem *_sortMenuItem;
}
@end

@implementation VLCPlaylistMenuController

- (instancetype)init
{
    self = [super init];
    if (self) {
        [self createMenu];
        _playlistController = [[VLCMain sharedInstance] playlistController];
    }
    return self;
}

- (void)createMenu
{
    _playMenuItem = [[NSMenuItem alloc] initWithTitle:_NS("Play") action:@selector(play:) keyEquivalent:@""];
    _playMenuItem.target = self;

    _removeMenuItem = [[NSMenuItem alloc] initWithTitle:_NS("Delete") action:@selector(remove:) keyEquivalent:@""];
    _removeMenuItem.target = self;

    _revealInFinderMenuItem = [[NSMenuItem alloc] initWithTitle:_NS("Reveal in Finder") action:@selector(revealInFinder:) keyEquivalent:@""];
    _revealInFinderMenuItem.target = self;

    _informationMenuItem = [[NSMenuItem alloc] initWithTitle:_NS("Information...") action:@selector(showInformationPanel:) keyEquivalent:@""];
    _informationMenuItem.target = self;

    _addFilesToPlaylistMenuItem = [[NSMenuItem alloc] initWithTitle:_NS("Add File...") action:@selector(addFilesToPlaylist:) keyEquivalent:@""];
    _addFilesToPlaylistMenuItem.target = self;

    _clearPlaylistMenuItem = [[NSMenuItem alloc] initWithTitle:_NS("Clear the playlist") action:@selector(clearPlaylist:) keyEquivalent:@""];
    _clearPlaylistMenuItem.target = self;

    _playlistSortingMenuController = [[VLCPlaylistSortingMenuController alloc] init];
    _sortMenuItem = [[NSMenuItem alloc] initWithTitle:_NS("Sort") action:nil keyEquivalent:@""];
    [_sortMenuItem setSubmenu:_playlistSortingMenuController.playlistSortingMenu];

    _playlistMenu = [[NSMenu alloc] init];
    [_playlistMenu addMenuItemsFromArray:@[_playMenuItem, _removeMenuItem, _revealInFinderMenuItem, _informationMenuItem, [NSMenuItem separatorItem], _addFilesToPlaylistMenuItem, _clearPlaylistMenuItem, _sortMenuItem]];
}

- (void)play:(id)sender
{
    NSInteger selectedRow = self.playlistTableView.selectedRow;

    if (selectedRow != -1) {
        [_playlistController playItemAtIndex:selectedRow];
    } else {
        [_playlistController startPlaylist];
    }
}

- (void)remove:(id)sender
{
    if (self.playlistTableView.selectedRow == -1)
        return;

    [_playlistController removeItemsAtIndexes:self.playlistTableView.selectedRowIndexes];
}

- (void)showInformationPanel:(id)sender
{
    if (!_informationWindowController) {
        _informationWindowController = [[VLCInformationWindowController alloc] init];
    }

    NSInteger selectedRow = self.playlistTableView.selectedRow;

    if (selectedRow == -1)
        return;

    VLCPlaylistItem *playlistItem = [_playlistController.playlistModel playlistItemAtIndex:selectedRow];
    if (playlistItem == nil)
        return;

    _informationWindowController.representedInputItem = playlistItem.inputItem;

    [_informationWindowController toggleWindow:sender];
}

- (void)revealInFinder:(id)sender
{
    NSInteger selectedRow = self.playlistTableView.selectedRow;

    if (selectedRow == -1)
        return;

    VLCPlaylistItem *item = [_playlistController.playlistModel playlistItemAtIndex:selectedRow];
    if (item == nil)
        return;

    NSString *path = item.path;
    [[NSWorkspace sharedWorkspace] selectFile:path inFileViewerRootedAtPath:path];
}

- (void)addFilesToPlaylist:(id)sender
{
    NSInteger selectedRow = self.playlistTableView.selectedRow;

    [[[VLCMain sharedInstance] open] openFileWithAction:^(NSArray *files) {
        [self->_playlistController addPlaylistItems:files
                                         atPosition:selectedRow
                                      startPlayback:NO];
    }];
}

- (void)clearPlaylist:(id)sender
{
    [_playlistController clearPlaylist];
}

- (BOOL)validateMenuItem:(NSMenuItem *)menuItem
{
    if (menuItem == _addFilesToPlaylistMenuItem) {
        return YES;

    } else if (menuItem == _clearPlaylistMenuItem) {
        return (self.playlistTableView.numberOfRows > 0);

    } else if (menuItem == _removeMenuItem ||
               menuItem == _playMenuItem ||
               menuItem == _informationMenuItem) {
        return (self.playlistTableView.numberOfSelectedRows > 0);

    } else if (menuItem == _revealInFinderMenuItem) {
        return (self.playlistTableView.numberOfSelectedRows == 1);
    }

    return NO;
}

@end
