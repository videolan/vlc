/*****************************************************************************
 * VLCLibraryHistoryDataSource.h: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2026 VLC authors and VideoLAN
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

#import "library/VLCLibraryCollectionViewDataSource.h"
#import "library/VLCLibraryConnectableDataSource.h"
#import "library/VLCLibrarySectionedTableViewDataSource.h"
#import "VLCLibraryHistorySection.h"

NS_ASSUME_NONNULL_BEGIN

@class VLCLibraryModel;

@interface VLCLibraryHistoryDataSource : NSObject<VLCLibraryCollectionViewDataSource, VLCLibrarySectionedTableViewDataSource, VLCLibraryConnectableDataSource>

@property (readwrite, strong, nonatomic) VLCLibraryModel *libraryModel;
@property (readwrite, weak) NSCollectionView *collectionView;
@property (readwrite, weak) NSTableView *tableView;
@property (readwrite, nonatomic) BOOL tableViewActive;
@property (readonly, copy) NSArray<VLCLibraryHistorySection *> *sections;
@property (readwrite, copy, nullable) void (^dataChangedHandler)(void);

- (void)connect;
- (void)disconnect;
- (void)reloadData;
- (void)doubleClickAction:(id)sender;

@end

NS_ASSUME_NONNULL_END
