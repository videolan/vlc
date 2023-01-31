/*****************************************************************************
 * VLCLibraryMediaSourceViewController.m: MacOS X interface module
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

#import "VLCLibraryMediaSourceViewController.h"

#import "VLCMediaSourceBaseDataSource.h"

#import "library/VLCLibraryCollectionViewFlowLayout.h"
#import "library/VLCLibraryController.h"
#import "library/VLCLibraryWindow.h"

#import "main/VLCMain.h"

@implementation VLCLibraryMediaSourceViewController

- (instancetype)initWithLibraryWindow:(VLCLibraryWindow *)libraryWindow
{
    self = [super init];
    if (self) {
        [self setupPropertiesFromLibraryWindow:libraryWindow];
        [self setupBaseDataSource];
        [self setupCollectionView];
    }
    return self;
}

- (void)setupPropertiesFromLibraryWindow:(VLCLibraryWindow *)libraryWindow
{
    NSParameterAssert(libraryWindow);
    _libraryTargetView = libraryWindow.libraryTargetView;
    _mediaSourceView = libraryWindow.mediaSourceView;
    _collectionView = libraryWindow.mediaSourceCollectionView;
    _collectionViewScrollView = libraryWindow.mediaSourceCollectionViewScrollView;
    _tableView = libraryWindow.mediaSourceTableView;
    _tableViewScrollView = libraryWindow.mediaSourceTableViewScrollView;
    _homeButton = libraryWindow.mediaSourceHomeButton;
    _pathControl = libraryWindow.mediaSourcePathControl;
    _gridVsListSegmentedControl = libraryWindow.gridVsListSegmentedControl;
}

- (void)setupBaseDataSource
{
    _baseDataSource = [[VLCMediaSourceBaseDataSource alloc] init];
    _baseDataSource.collectionView = _collectionView;
    _baseDataSource.collectionViewScrollView = _collectionViewScrollView;
    _baseDataSource.homeButton = _homeButton;
    _baseDataSource.pathControl = _pathControl;
    _baseDataSource.gridVsListSegmentedControl = _gridVsListSegmentedControl;
    _baseDataSource.tableView = _tableView;
    [_baseDataSource setupViews];
}

- (void)setupCollectionView
{
    _collectionView.collectionViewLayout = [[VLCLibraryCollectionViewFlowLayout alloc] init];
}

- (void)presentBrowseView
{
    [self presentMediaSourceView:VLCLibraryBrowseSegment];
}

- (void)presentStreamsView
{
    [self presentMediaSourceView:VLCLibraryStreamsSegment];
}

- (void)presentMediaSourceView:(VLCLibrarySegment)viewSegment
{
    _libraryTargetView.subviews = @[];

    if (_mediaSourceView.superview == nil) {
        _mediaSourceView.translatesAutoresizingMaskIntoConstraints = NO;
        _libraryTargetView.subviews = @[_mediaSourceView];
        NSDictionary *dict = NSDictionaryOfVariableBindings(_mediaSourceView);
        [_libraryTargetView addConstraints:[NSLayoutConstraint constraintsWithVisualFormat:@"H:|[_mediaSourceView(>=572.)]|" options:0 metrics:0 views:dict]];
        [_libraryTargetView addConstraints:[NSLayoutConstraint constraintsWithVisualFormat:@"V:|[_mediaSourceView(>=444.)]|" options:0 metrics:0 views:dict]];
    }

    _baseDataSource.mediaSourceMode = viewSegment == VLCLibraryBrowseSegment ? VLCMediaSourceModeLAN : VLCMediaSourceModeInternet;
    [_baseDataSource reloadViews];
}

@end
