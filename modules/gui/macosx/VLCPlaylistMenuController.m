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
#import "VLCMain.h"
#import "VLCPlaylistController.h"
#import "VLCPlaylistModel.h"
#import "VLCPlaylistItem.h"
#import "VLCOpenWindowController.h"

@interface VLCPlaylistMenuController ()
{
    VLCPlaylistController *_playlistController;
}
@end

@implementation VLCPlaylistMenuController

- (instancetype)init
{
    self = [super init];
    if (self) {
        _playlistController = [[VLCMain sharedInstance] playlistController];
        [[NSBundle mainBundle] loadNibNamed:@"VLCPlaylistMenu" owner:self topLevelObjects:nil];
    }
    return self;
}

- (void)awakeFromNib
{
    [_playMenuItem setTitle:_NS("Play")];
    [_revealInFinderMenuItem setTitle:_NS("Reveal in Finder")];
    [_addFilesToPlaylistMenuItem setTitle:_NS("Add File...")];
    [_removeMenuItem setTitle:_NS("Delete")];
    [_clearPlaylistMenuItem setTitle:_NS("Clear the playlist")];
    [_sortPlaylistMenuItem setTitle:_NS("Sort by")];
}

- (IBAction)play:(id)sender
{
    NSInteger selectedRow = self.playlistTableView.selectedRow;

    if (selectedRow != -1) {
        [_playlistController playItemAtIndex:selectedRow];
    } else {
        [_playlistController startPlaylist];
    }
}

- (IBAction)remove:(id)sender
{
    NSInteger selectedRow = self.playlistTableView.selectedRow;

    if (selectedRow != -1) {
        [_playlistController removeItemAtIndex:selectedRow];
    }
}

- (IBAction)revealInFinder:(id)sender
{
    NSInteger selectedRow = self.playlistTableView.selectedRow;

    if (selectedRow == -1)
        return;

    VLCPlaylistItem *item = [_playlistController.playlistModel playlistItemAtIndex:selectedRow];
    if (item == nil) {
        return;
    }

    NSString *path = item.path;
    [[NSWorkspace sharedWorkspace] selectFile:path inFileViewerRootedAtPath:path];
}

- (IBAction)addFilesToPlaylist:(id)sender
{
    NSInteger selectedRow = self.playlistTableView.selectedRow;

    [[[VLCMain sharedInstance] open] openFileWithAction:^(NSArray *files) {
        [self->_playlistController addPlaylistItems:files
                                         atPosition:selectedRow
                                      startPlayback:NO];
    }];
}

- (IBAction)clearPlaylist:(id)sender
{
    [_playlistController clearPlaylist];
}

- (BOOL)validateMenuItem:(NSMenuItem *)menuItem
{
    if (menuItem == self.addFilesToPlaylistMenuItem) {
        return YES;
    }

    if (_playlistController.playlistModel.numberOfPlaylistItems > 0) {
        return YES;
    }

    return NO;
}

@end
