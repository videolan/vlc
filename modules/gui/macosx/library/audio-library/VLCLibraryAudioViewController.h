/*****************************************************************************
 * VLCLibraryAudioViewController.h: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2022 VLC authors and VideoLAN
 *
 * Authors: Claudio Cambra <claudio.cambra@gmail.com>
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

#import "library/VLCLibrarySegment.h"
#import "library/VLCLibraryAbstractMediaLibrarySegmentViewController.h"
#import "library/VLCLibraryItemPresentingCapable.h"

@class VLCLibraryAudioDataSource;
@class VLCLibraryAudioGroupDataSource;
@class VLCLibraryCollectionView;
@class VLCLibraryWindow;

@protocol VLCMediaLibraryItemProtocol;

NS_ASSUME_NONNULL_BEGIN

@interface VLCLibraryAudioViewController : VLCLibraryAbstractMediaLibrarySegmentViewController<VLCLibraryItemPresentingCapable>

@property (readonly, weak) NSView *audioLibraryView;
@property (readonly, weak) NSSplitView *audioLibrarySplitView;
@property (readonly, weak) NSScrollView *audioCollectionSelectionTableViewScrollView;
@property (readonly, weak) NSTableView *audioCollectionSelectionTableView;
@property (readonly, weak) NSScrollView *audioGroupSelectionTableViewScrollView;
@property (readonly, weak) NSTableView *audioGroupSelectionTableView;
@property (readonly, weak) NSScrollView *audioSongTableViewScrollView;
@property (readonly, weak) NSTableView *audioSongTableView;
@property (readonly, weak) NSScrollView *audioCollectionViewScrollView;
@property (readonly, weak) VLCLibraryCollectionView *audioLibraryCollectionView;
@property (readonly, weak) NSSplitView *audioLibraryGridModeSplitView;
@property (readonly, weak) NSScrollView *audioLibraryGridModeSplitViewListTableViewScrollView;
@property (readonly, weak) NSTableView *audioLibraryGridModeSplitViewListTableView;
@property (readonly, weak) NSScrollView *audioLibraryGridModeSplitViewListSelectionCollectionViewScrollView;
@property (readonly, weak) VLCLibraryCollectionView *audioLibraryGridModeSplitViewListSelectionCollectionView;

@property (readonly) VLCLibraryAudioDataSource *audioDataSource;
@property (readonly) VLCLibraryAudioGroupDataSource *audioGroupDataSource;

- (instancetype)initWithLibraryWindow:(VLCLibraryWindow *)libraryWindow;

- (void)presentAudioView;
- (void)reloadData;

@end

NS_ASSUME_NONNULL_END
