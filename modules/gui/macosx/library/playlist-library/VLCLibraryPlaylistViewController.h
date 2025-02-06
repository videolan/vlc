/*****************************************************************************
 * VLCLibraryPlaylistViewController.h: MacOS X interface module
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

#import <Cocoa/Cocoa.h>

#import "library/VLCLibraryAbstractMediaLibrarySegmentViewController.h"

#include <vlc_media_library.h>

NS_ASSUME_NONNULL_BEGIN

@class VLCLibraryCollectionView;
@class VLCLibraryCollectionViewDelegate;
@class VLCLibraryMasterDetailViewTableViewDelegate;
@class VLCLibraryPlaylistDataSource;
@class VLCLibraryTableView;
@class VLCLibraryWindow;

@interface VLCLibraryPlaylistViewController : VLCLibraryAbstractMediaLibrarySegmentViewController<NSSplitViewDelegate>

@property (readonly) NSSplitView *listViewSplitView;
@property (readonly) NSScrollView *masterTableViewScrollView;
@property (readonly) VLCLibraryTableView *masterTableView;
@property (readonly) NSScrollView *detailTableViewScrollView;
@property (readonly) VLCLibraryTableView *detailTableView;
@property (readonly) NSScrollView *collectionViewScrollView;
@property (readonly) VLCLibraryCollectionView *collectionView;

@property (readonly) VLCLibraryPlaylistDataSource *dataSource;
@property (readonly) VLCLibraryCollectionViewDelegate *collectionViewDelegate;
@property (readonly) VLCLibraryMasterDetailViewTableViewDelegate *tableViewDelegate;

- (instancetype)initWithLibraryWindow:(VLCLibraryWindow *)libraryWindow;

- (void)presentPlaylistsView;
- (void)presentPlaylistsViewForPlaylistType:(enum vlc_ml_playlist_type_t)playlistType;

@end

NS_ASSUME_NONNULL_END
