/*****************************************************************************
 * VLCLibraryFavoritesViewController.h: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2025 VLC authors and VideoLAN
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
#import "library/VLCLibraryDataTypes.h"
#import "library/VLCLibraryItemPresentingCapable.h"

@class VLCLibraryCollectionView;
@class VLCLibraryWindow;
@class VLCLibraryFavoritesDataSource;

NS_ASSUME_NONNULL_BEGIN

@interface VLCLibraryFavoritesViewController : VLCLibraryAbstractMediaLibrarySegmentViewController<VLCLibraryItemPresentingCapable>

@property (readonly, weak) NSView *favoritesLibraryView;
@property (readonly, weak) NSSplitView *favoritesLibrarySplitView;
@property (readonly, weak) NSScrollView *favoritesLibraryCollectionViewScrollView;
@property (readonly, weak) VLCLibraryCollectionView *favoritesLibraryCollectionView;
@property (readonly, weak) NSScrollView *favoritesLibraryGroupSelectionTableViewScrollView;
@property (readonly, weak) NSTableView *favoritesLibraryGroupSelectionTableView;
@property (readonly, weak) NSScrollView *favoritesLibraryGroupsTableViewScrollView;
@property (readonly, weak) NSTableView *favoritesLibraryGroupsTableView;

@property (readwrite, strong) VLCLibraryFavoritesDataSource *libraryFavoritesDataSource;

- (instancetype)initWithLibraryWindow:(VLCLibraryWindow *)libraryWindow;
- (void)presentFavoritesView;
- (void)presentLibraryItem:(id<VLCMediaLibraryItemProtocol>)libraryItem;

@end

NS_ASSUME_NONNULL_END
