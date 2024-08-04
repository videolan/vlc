/*****************************************************************************
 * VLCLibraryGroupsViewController.h: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2024 VLC authors and VideoLAN
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

NS_ASSUME_NONNULL_BEGIN

@class VLCLibraryCollectionViewDelegate;
@class VLCLibraryGroupsDataSource;
@class VLCLibraryMasterDetailViewTableViewDelegate;
@class VLCLibraryTableView;
@class VLCLibraryWindow;
@class VLCMediaLibraryGroup;

@interface VLCLibraryGroupsViewController : NSObject<NSSplitViewDelegate>

@property (readonly) VLCLibraryWindow *libraryWindow;
@property (readonly) NSView *libraryTargetView;
@property (readonly) NSScrollView *collectionViewScrollView;
@property (readonly) NSCollectionView *collectionView;
@property (readonly) NSSplitView *listViewSplitView;
@property (readonly) NSScrollView *groupsTableViewScrollView;
@property (readonly) VLCLibraryTableView *groupsTableView;
@property (readonly) NSScrollView *selectedGroupTableViewScrollView;
@property (readonly) VLCLibraryTableView *selectedGroupTableView;
@property (readonly) NSView *emptyLibraryView;
@property (readonly) NSImageView *placeholderImageView;
@property (readonly) NSTextField *placeholderLabel;

@property (readonly) VLCLibraryCollectionViewDelegate *collectionViewDelegate;
@property (readonly) VLCLibraryMasterDetailViewTableViewDelegate *tableViewDelegate;
@property (readonly) VLCLibraryGroupsDataSource *dataSource;

- (instancetype)initWithLibraryWindow:(VLCLibraryWindow *)libraryWindow;

- (void)presentGroupsView;
- (void)presentGroup:(VLCMediaLibraryGroup *)group;

@end

NS_ASSUME_NONNULL_END
