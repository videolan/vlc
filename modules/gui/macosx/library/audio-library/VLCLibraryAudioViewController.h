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

@class VLCLibraryAudioDataSource;
@class VLCLibraryAudioGroupDataSource;
@class VLCLibraryWindow;

NS_ASSUME_NONNULL_BEGIN

@interface VLCLibraryAudioViewController : NSObject

@property (readonly) VLCLibraryWindow *libraryWindow;
@property (readonly) NSView *libraryTargetView;
@property (readonly) NSView *audioLibraryView;
@property (readonly) NSSplitView *audioLibrarySplitView;
@property (readonly) NSScrollView *audioCollectionSelectionTableViewScrollView;
@property (readonly) NSTableView *audioCollectionSelectionTableView;
@property (readonly) NSScrollView *audioGroupSelectionTableViewScrollView;
@property (readonly) NSTableView *audioGroupSelectionTableView;
@property (readonly) NSScrollView *audioSongTableViewScrollView;
@property (readonly) NSTableView *audioSongTableView;
@property (readonly) NSScrollView *audioCollectionViewScrollView;
@property (readonly) NSCollectionView *audioLibraryCollectionView;
@property (readonly) NSSplitView *audioLibraryGridModeSplitView;
@property (readonly) NSScrollView *audioLibraryGridModeSplitViewListTableViewScrollView;
@property (readonly) NSTableView *audioLibraryGridModeSplitViewListTableView;
@property (readonly) NSScrollView *audioLibraryGridModeSplitViewListSelectionCollectionViewScrollView;
@property (readonly) NSCollectionView *audioLibraryGridModeSplitViewListSelectionCollectionView;
@property (readonly) NSSegmentedControl *audioSegmentedControl;
@property (readonly) NSSegmentedControl *gridVsListSegmentedControl;
@property (readonly) NSButton *librarySortButton;
@property (readonly) NSSearchField *librarySearchField;
@property (readonly) NSVisualEffectView *optionBarView;
@property (readonly) NSImageView *placeholderImageView;
@property (readonly) NSTextField *placeholderLabel;
@property (readonly) NSView *emptyLibraryView;

@property (readonly) VLCLibraryAudioDataSource *audioDataSource;
@property (readonly) VLCLibraryAudioGroupDataSource *audioGroupDataSource;

@property (readonly) NSArray<NSLayoutConstraint *> *audioPlaceholderImageViewSizeConstraints;

- (instancetype)initWithLibraryWindow:(VLCLibraryWindow *)libraryWindow;
- (IBAction)segmentedControlAction:(id)sender;
- (void)presentAudioView;
- (void)reloadData;

@end

NS_ASSUME_NONNULL_END
