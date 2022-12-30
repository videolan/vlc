/*****************************************************************************
 * VLCLibraryVideoViewController.h: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2022 VLC authors and VideoLAN
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

@class VLCLibraryWindow;
@class VLCLibraryVideoTableViewDataSource;
@class VLCLibraryVideoCollectionViewsStackViewController;

NS_ASSUME_NONNULL_BEGIN

@interface VLCLibraryVideoViewController : NSObject

@property (readonly) VLCLibraryWindow *libraryWindow;
@property (readonly) NSView *libraryTargetView;
@property (readonly) NSView *videoLibraryView;
@property (readonly) NSSplitView *videoLibrarySplitView;
@property (readonly) NSScrollView *videoLibraryCollectionViewsStackViewScrollView;
@property (readonly) NSStackView *videoLibraryCollectionViewsStackView;
@property (readonly) NSScrollView *videoLibraryGroupSelectionTableViewScrollView;
@property (readonly) NSTableView *videoLibraryGroupSelectionTableView;
@property (readonly) NSScrollView *videoLibraryGroupsTableViewScrollView;
@property (readonly) NSTableView *videoLibraryGroupsTableView;
@property (readonly) NSSegmentedControl *gridVsListSegmentedControl;
@property (readonly) NSButton *librarySortButton;
@property (readonly) NSSearchField *librarySearchField;
@property (readonly) NSVisualEffectView *optionBarView;
@property (readonly) NSImageView *placeholderImageView;
@property (readonly) NSTextField *placeholderLabel;
@property (readonly) NSView *emptyLibraryView;

@property (readonly) VLCLibraryVideoTableViewDataSource *libraryVideoTableViewDataSource;
@property (readonly) VLCLibraryVideoCollectionViewsStackViewController *libraryVideoCollectionViewsStackViewController;

@property (readonly) NSArray<NSLayoutConstraint *> *videoPlaceholderImageViewSizeConstraints;

- (instancetype)initWithLibraryWindow:(VLCLibraryWindow *)libraryWindow;
- (void)presentVideoLibraryView;


@end

NS_ASSUME_NONNULL_END
