/*****************************************************************************
 * VLCLibraryAddToPlaylistMenuController.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2026 VLC authors and VideoLAN
 *
 * Authors: Serhii Bykov <esphynox@gmail.com>
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

#import "VLCLibraryAddToPlaylistMenuController.h"

#import "extensions/NSString+Helpers.h"

#import "library/VLCLibraryController.h"
#import "library/VLCLibraryDataTypes.h"
#import "library/VLCLibraryModel.h"
#import "library/VLCLibraryRepresentedItem.h"

#import "main/VLCMain.h"

#import <vlc_media_library.h>

@interface VLCLibraryAddToPlaylistMenuController () <NSMenuDelegate>
@end

@implementation VLCLibraryAddToPlaylistMenuController

- (instancetype)init
{
    self = [super init];
    if (self) {
        _addToPlaylistMenu = [[NSMenu alloc] initWithTitle:@""];
        _addToPlaylistMenu.delegate = self;
    }
    return self;
}

#pragma mark - Helpers

- (nullable NSArray<VLCMediaLibraryPlaylist *> *)writablePlaylists
{
    VLCLibraryModel * const libraryModel = VLCMain.sharedInstance.libraryController.libraryModel;
    NSArray<VLCMediaLibraryPlaylist *> * const allPlaylists =
        [libraryModel listOfPlaylistsOfType:VLC_ML_PLAYLIST_TYPE_ALL];
    if (allPlaylists.count == 0) {
        return nil;
    }

    NSMutableArray<VLCMediaLibraryPlaylist *> * const writable =
        [NSMutableArray arrayWithCapacity:allPlaylists.count];
    for (VLCMediaLibraryPlaylist * const playlist in allPlaylists) {
        if (!playlist.readOnly) {
            [writable addObject:playlist];
        }
    }

    [writable sortUsingComparator:^NSComparisonResult(VLCMediaLibraryPlaylist * _Nonnull lhs,
                                                      VLCMediaLibraryPlaylist * _Nonnull rhs) {
        return [lhs.displayString caseInsensitiveCompare:rhs.displayString];
    }];

    return writable;
}

- (NSArray<VLCMediaLibraryMediaItem *> *)selectedMediaItems
{
    NSMutableArray<VLCMediaLibraryMediaItem *> * const mediaItems =
        [NSMutableArray arrayWithCapacity:self.representedItems.count];
    for (VLCLibraryRepresentedItem * const representedItem in self.representedItems) {
        [mediaItems addObjectsFromArray:representedItem.item.mediaItems];
    }
    return mediaItems;
}

#pragma mark - NSMenuDelegate

// Rebuilt on demand so newly created playlists appear without cache invalidation.
- (void)menuNeedsUpdate:(NSMenu *)menu
{
    [menu removeAllItems];

    NSMenuItem * const newPlaylistItem =
        [[NSMenuItem alloc] initWithTitle:_NS("New Playlist...")
                                   action:@selector(createNewPlaylist:)
                            keyEquivalent:@""];
    newPlaylistItem.target = self;
    [menu addItem:newPlaylistItem];

    NSArray<VLCMediaLibraryPlaylist *> * const playlists = [self writablePlaylists];
    if (playlists.count == 0) {
        return;
    }

    [menu addItem:NSMenuItem.separatorItem];

    for (VLCMediaLibraryPlaylist * const playlist in playlists) {
        NSMenuItem * const item = [[NSMenuItem alloc] initWithTitle:playlist.displayString
                                                             action:@selector(addToSelectedPlaylist:)
                                                      keyEquivalent:@""];
        item.target = self;
        item.representedObject = playlist;
        [menu addItem:item];
    }
}

#pragma mark - Actions

- (void)createNewPlaylist:(NSMenuItem *)sender
{
    NSArray<VLCMediaLibraryMediaItem *> * const mediaItems = [self selectedMediaItems];
    if (mediaItems.count == 0) {
        return;
    }

    [VLCMain.sharedInstance.libraryController showCreatePlaylistDialogForMediaItems:mediaItems];
}

- (void)addToSelectedPlaylist:(NSMenuItem *)sender
{
    VLCMediaLibraryPlaylist * const playlist = sender.representedObject;
    if (![playlist isKindOfClass:VLCMediaLibraryPlaylist.class]) {
        return;
    }

    [playlist appendMediaItems:[self selectedMediaItems]];
}

@end
