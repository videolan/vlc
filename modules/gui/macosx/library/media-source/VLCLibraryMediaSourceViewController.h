/*****************************************************************************
 * VLCLibraryMediaSourceViewController.h: MacOS X interface module
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

#import "library/VLCLibraryAbstractSegmentViewController.h"

NS_ASSUME_NONNULL_BEGIN

@class VLCInputNodePathControl;
@class VLCLibraryCollectionView;
@class VLCLibraryMediaSourceViewNavigationStack;
@class VLCLibraryWindow;
@class VLCMediaSourceBaseDataSource;

@interface VLCLibraryMediaSourceViewController : VLCLibraryAbstractSegmentViewController

@property (readonly, weak) NSView *mediaSourceView;
@property (readonly, weak) NSTableView *mediaSourceTableView;
@property (readonly, weak) VLCLibraryCollectionView *collectionView;
@property (readonly, weak) NSScrollView *collectionViewScrollView;
@property (readonly, weak) NSTableView *tableView;
@property (readonly, weak) NSScrollView *tableViewScrollView;
@property (readonly, weak) NSButton *homeButton;
@property (readonly, weak) VLCInputNodePathControl *pathControl;
@property (readonly, weak) NSVisualEffectView *pathControlVisualEffectView;
@property (readonly, weak) NSSegmentedControl *gridVsListSegmentedControl;
@property (readonly) NSTextField *browsePlaceholderLabel; // Use library window's placeholder views?
@property (readonly, weak) NSLayoutConstraint *pathControlViewTopConstraintToSuperview;

@property (readonly) VLCMediaSourceBaseDataSource *baseDataSource;
@property (readonly) VLCLibraryMediaSourceViewNavigationStack *navigationStack;
@property (readonly) NSView *pathControlContainerView;

- (instancetype)initWithLibraryWindow:(VLCLibraryWindow *)libraryWindow;

- (void)presentBrowseView;
- (void)presentStreamsView;
- (void)presentLocalFolderMrl:(NSString *)mrl;

@end

NS_ASSUME_NONNULL_END
