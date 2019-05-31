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
#import "main/VLCMain.h"
#import "playlist/VLCPlaylistController.h"
#import "playlist/VLCPlaylistModel.h"
#import "playlist/VLCPlaylistItem.h"
#import "windows/VLCOpenWindowController.h"

@interface VLCPlaylistMenuController ()
{
    VLCPlaylistController *_playlistController;

    NSMenuItem *_playMenuItem;
    NSMenuItem *_revealInFinderMenuItem;
    NSMenuItem *_addFilesToPlaylistMenuItem;
    NSMenuItem *_removeMenuItem;
    NSMenuItem *_clearPlaylistMenuItem;
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
    _playlistMenu = [[NSMenu alloc] init];

    _playMenuItem = [[NSMenuItem alloc] initWithTitle:_NS("Play") action:@selector(play:) keyEquivalent:@""];
    _playMenuItem.target = self;
    [_playlistMenu addItem:_playMenuItem];

    _removeMenuItem = [[NSMenuItem alloc] initWithTitle:_NS("Delete") action:@selector(remove:) keyEquivalent:@""];
    _removeMenuItem.target = self;
    [_playlistMenu addItem:_removeMenuItem];

    _revealInFinderMenuItem = [[NSMenuItem alloc] initWithTitle:_NS("Reveal in Finder") action:@selector(revealInFinder:) keyEquivalent:@""];
    _revealInFinderMenuItem.target = self;
    [_playlistMenu addItem:_revealInFinderMenuItem];

    [_playlistMenu addItem:[NSMenuItem separatorItem]];

    _addFilesToPlaylistMenuItem = [[NSMenuItem alloc] initWithTitle:_NS("Add File...") action:@selector(addFilesToPlaylist:) keyEquivalent:@""];
    _addFilesToPlaylistMenuItem.target = self;
    [_playlistMenu addItem:_addFilesToPlaylistMenuItem];

    _clearPlaylistMenuItem = [[NSMenuItem alloc] initWithTitle:_NS("Clear the playlist") action:@selector(clearPlaylist:) keyEquivalent:@""];
    _clearPlaylistMenuItem.target = self;
    [_playlistMenu addItem:_clearPlaylistMenuItem];
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
               menuItem == _playMenuItem) {
        return (self.playlistTableView.numberOfSelectedRows > 0);

    } else if (menuItem == _revealInFinderMenuItem) {
        return (self.playlistTableView.numberOfSelectedRows == 1);
    }

    return NO;
}

@end
