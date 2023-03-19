/*****************************************************************************
 * VLCLibraryAudioDataSource.h: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2019 VLC authors and VideoLAN
 *
 * Authors: Felix Paul Kühne <fkuehne # videolan -dot- org>
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

#import <Cocoa/Cocoa.h>

#import "library/VLCLibraryTableView.h"
#import "library/VLCLibraryCollectionViewDataSource.h"

NS_ASSUME_NONNULL_BEGIN

@class VLCLibraryModel;
@class VLCLibraryAudioGroupDataSource;
@class VLCMediaLibraryAlbum;

typedef NS_ENUM(NSUInteger, VLCAudioLibrarySegment) {
    VLCAudioLibraryArtistsSegment = 0,
    VLCAudioLibraryAlbumsSegment,
    VLCAudioLibrarySongsSegment,
    VLCAudioLibraryGenresSegment
};

extern NSString * const VLCLibrarySongsTableViewSongPlayingColumnIdentifier;
extern NSString * const VLCLibrarySongsTableViewTitleColumnIdentifier;
extern NSString * const VLCLibrarySongsTableViewDurationColumnIdentifier;
extern NSString * const VLCLibrarySongsTableViewArtistColumnIdentifier;
extern NSString * const VLCLibrarySongsTableViewAlbumColumnIdentifier;
extern NSString * const VLCLibrarySongsTableViewGenreColumnIdentifier;
extern NSString * const VLCLibrarySongsTableViewPlayCountColumnIdentifier;
extern NSString * const VLCLibrarySongsTableViewYearColumnIdentifier;

extern NSString * const VLCLibraryTitleSortDescriptorKey;
extern NSString * const VLCLibraryDurationSortDescriptorKey;
extern NSString * const VLCLibraryArtistSortDescriptorKey;
extern NSString * const VLCLibraryAlbumSortDescriptorKey;
extern NSString * const VLCLibraryPlayCountSortDescriptorKey;
extern NSString * const VLCLibraryYearSortDescriptorKey;

@interface VLCLibraryAudioDataSource : NSObject <VLCLibraryTableViewDataSource, VLCLibraryCollectionViewDataSource>

@property (readwrite, assign) VLCLibraryModel *libraryModel;
@property (readwrite, assign) VLCLibraryAudioGroupDataSource *audioGroupDataSource;
@property (readwrite, assign) NSTableView *collectionSelectionTableView;
@property (readwrite, assign) NSTableView *groupSelectionTableView;
@property (readwrite, assign) NSTableView *songsTableView;
@property (readwrite, assign) NSCollectionView *collectionView;
@property (readwrite, assign) NSTableView *gridModeListTableView;
@property (readwrite, assign) NSCollectionView *gridModeListSelectionCollectionView;

@property (nonatomic, readwrite, assign) VLCAudioLibrarySegment audioLibrarySegment;

- (void)setup;
- (void)setupCollectionView:(NSCollectionView *)collectionView;

- (void)reloadData;
- (void)tableView:(NSTableView * const)tableView selectRow:(NSInteger)row;


@end

NS_ASSUME_NONNULL_END
