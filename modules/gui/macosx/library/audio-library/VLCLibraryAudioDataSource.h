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

#import "library/VLCLibraryCollectionViewDataSource.h"
#import "library/VLCLibraryTableViewDataSource.h"
#import "library/audio-library/VLCLibraryAudioDataSourceHeaderDelegate.h"

#include "views/iCarousel/iCarousel.h"

NS_ASSUME_NONNULL_BEGIN

@class VLCLibraryModel;
@class VLCLibraryAudioGroupDataSource;
@class VLCMediaLibraryAlbum;

typedef NS_ENUM(NSInteger, VLCAudioLibrarySegment) {
    VLCAudioLibraryUnknownSegment = -1,
    VLCAudioLibraryArtistsSegment,
    VLCAudioLibraryAlbumsSegment,
    VLCAudioLibrarySongsSegment,
    VLCAudioLibraryGenresSegment,
    VLCAudioLibraryRecentsSegment
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

extern NSString * const VLCLibraryAudioDataSourceDisplayedCollectionChangedNotification;

@class VLCLibraryRepresentedItem;

@interface VLCLibraryAudioDataSource : NSObject <VLCLibraryTableViewDataSource, VLCLibraryCollectionViewDataSource, iCarouselDataSource>

@property (readwrite, weak) VLCLibraryModel *libraryModel;
@property (readwrite, weak) NSTableView *collectionSelectionTableView;
@property (readwrite, weak) NSTableView *songsTableView;
@property (readwrite, weak) NSCollectionView *collectionView;
@property (readwrite, weak) iCarousel *carouselView;
@property (readwrite, weak) NSTableView *gridModeListTableView;
@property (readwrite, weak, nullable) id<VLCLibraryAudioDataSourceHeaderDelegate> headerDelegate;

@property (nonatomic, readwrite, assign) VLCAudioLibrarySegment audioLibrarySegment;
@property (readwrite, strong) VLCLibraryAudioGroupDataSource *audioGroupDataSource;

@property (readonly) size_t collectionToDisplayCount;
@property (readonly) NSInteger displayedCollectionCount;
@property (readonly) BOOL displayedCollectionUpdating;

+ (void)setupCollectionView:(NSCollectionView *)collectionView;
- (void)setup;
- (void)reloadData;
- (void)tableView:(NSTableView * const)tableView selectRowIndices:(NSIndexSet * const)indices;
- (void)applySelectionForTableView:(NSTableView *)tableView;

@end

NS_ASSUME_NONNULL_END
