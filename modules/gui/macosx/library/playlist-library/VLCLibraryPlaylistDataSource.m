/*****************************************************************************
 * VLCLibraryPlaylistDataSource.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2023 VLC authors and VideoLAN
 *
 * Authors: Claudio Cambra <developer@claudiocambra.com>
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

#import "VLCLibraryPlaylistDataSource.h"

#import "library/VLCLibraryController.h"
#import "library/VLCLibraryDataTypes.h"
#import "library/VLCLibraryModel.h"

#import "main/VLCMain.h"

typedef NS_ENUM(NSInteger, VLCLibraryDataSourceCacheAction) {
    VLCLibraryDataSourceCacheUpdateAction,
    VLCLibraryDataSourceCacheDeleteAction,
};

@interface VLCLibraryPlaylistDataSource ()

@property (readwrite, atomic) NSArray<VLCMediaLibraryPlaylist *> *playlists;

@end

@implementation VLCLibraryPlaylistDataSource

- (instancetype)init
{
    self = [super init];
    if (self) {
        [self setup];
    }
    return self;
}

- (void)setup
{
    _libraryModel = VLCMain.sharedInstance.libraryController.libraryModel;
    [self reloadPlaylists];

    NSNotificationCenter * const notificationCenter = NSNotificationCenter.defaultCenter;
    [notificationCenter addObserver:self
                           selector:@selector(playlistsReset:)
                               name:VLCLibraryModelPlaylistListReset
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(playlistUpdated:)
                               name:VLCLibraryModelPlaylistUpdated
                             object:nil];
}

- (void)playlistsReset:(NSNotification *)notification
{
    NSParameterAssert(notification);
    [self reloadPlaylists];
}

- (void)playlistUpdated:(NSNotification *)notification
{
    NSParameterAssert(notification);
    VLCMediaLibraryPlaylist * const playlist = (VLCMediaLibraryPlaylist *)notification.object;
    [self cacheAction:VLCLibraryDataSourceCacheUpdateAction onPlaylist:playlist];
}

- (void)reloadPlaylists
{
    self.playlists = _libraryModel.listOfPlaylists;
    [self reloadViews];
}

- (void)reloadViews
{
    for (NSCollectionView * const collectionView in self.collectionViews) {
        [collectionView reloadData];
    }
}

- (NSUInteger)indexForPlaylistWithId:(const int64_t)itemId
{
    return [self.playlists indexOfObjectPassingTest:^BOOL(const VLCMediaLibraryPlaylist *playlist, const NSUInteger idx, BOOL * const stop) {
        NSAssert(playlist != nil, @"Cache list should not contain nil playlists");
        return playlist.libraryID == itemId;
    }];
}

- (void)cacheAction:(VLCLibraryDataSourceCacheAction)action
         onPlaylist:(VLCMediaLibraryPlaylist * const)playlist
{
    NSParameterAssert(playlist != nil);

    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INTERACTIVE, 0), ^{
        const NSUInteger idx = [self indexForPlaylistWithId:playlist.libraryID];
        if (idx == NSNotFound) {
            return;
        }

        NSMutableArray * const mutablePlaylists = self.playlists.mutableCopy;

        switch (action) {
            case VLCLibraryDataSourceCacheUpdateAction:
                [mutablePlaylists replaceObjectAtIndex:idx withObject:playlist];
                break;
            case VLCLibraryDataSourceCacheDeleteAction:
                [mutablePlaylists removeObjectAtIndex:idx];
                break;
            default:
                return;
        }

        dispatch_async(dispatch_get_main_queue(), ^{
            self.playlists = mutablePlaylists.copy;
        });
    });
}

@end
