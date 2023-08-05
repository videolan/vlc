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
#import "VLCMediaSourceDataSource.h"

#import "extensions/NSFont+VLCAdditions.h"
#import "extensions/NSString+Helpers.h"
#import "extensions/NSWindow+VLCAdditions.h"

#import "library/VLCLibraryCollectionViewFlowLayout.h"
#import "library/VLCLibraryCollectionViewItem.h"
#import "library/VLCLibraryController.h"
#import "library/VLCLibrarySegment.h"
#import "library/VLCLibraryUIUnits.h"
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
        [self setupMediaSourceLibraryViews];
        [self setupPlaceholderLabel];
        [self setupPathControlView];

        NSNotificationCenter * const defaultCenter = NSNotificationCenter.defaultCenter;
        [defaultCenter addObserver:self 
                          selector:@selector(updatePlaceholderLabel:) 
                              name:VLCMediaSourceBaseDataSourceNodeChanged 
                            object:nil];
        [defaultCenter addObserver:self 
                          selector:@selector(updatePlaceholderLabel:) 
                              name:VLCMediaSourceDataSourceNodeChanged 
                            object:nil];
    }
    return self;
}

- (void)setupPropertiesFromLibraryWindow:(VLCLibraryWindow *)libraryWindow
{
    NSParameterAssert(libraryWindow);
    _libraryWindow = libraryWindow;
    _libraryTargetView = libraryWindow.libraryTargetView;
    _mediaSourceView = libraryWindow.mediaSourceView;
    _mediaSourceTableView = libraryWindow.mediaSourceTableView;
    _collectionView = libraryWindow.mediaSourceCollectionView;
    _collectionViewScrollView = libraryWindow.mediaSourceCollectionViewScrollView;
    _tableView = libraryWindow.mediaSourceTableView;
    _tableViewScrollView = libraryWindow.mediaSourceTableViewScrollView;
    _homeButton = libraryWindow.mediaSourceHomeButton;
    _pathControl = libraryWindow.mediaSourcePathControl;
    _pathControlVisualEffectView = libraryWindow.mediaSourcePathControlVisualEffectView;
    _gridVsListSegmentedControl = libraryWindow.gridVsListSegmentedControl;
}

- (void)setupBaseDataSource
{
    _baseDataSource = [[VLCMediaSourceBaseDataSource alloc] init];
    _baseDataSource.collectionView = _collectionView;
    _baseDataSource.collectionViewScrollView = _collectionViewScrollView;
    _baseDataSource.homeButton = _homeButton;
    _baseDataSource.pathControl = _pathControl;
    _baseDataSource.pathControlVisualEffectView = _pathControlVisualEffectView;
    _baseDataSource.tableView = _tableView;
    _baseDataSource.tableViewScrollView = _tableViewScrollView;
    [_baseDataSource setupViews];
}

- (void)setupCollectionView
{
    VLCLibraryCollectionViewFlowLayout * const mediaSourceCollectionViewLayout = VLCLibraryCollectionViewFlowLayout.standardLayout;
    _collectionView.collectionViewLayout = mediaSourceCollectionViewLayout;
    mediaSourceCollectionViewLayout.itemSize = VLCLibraryCollectionViewItem.defaultSize;
}

- (void)setupMediaSourceLibraryViews
{
    _mediaSourceTableView.rowHeight = VLCLibraryUIUnits.mediumTableViewRowHeight;

    const NSEdgeInsets defaultInsets = VLCLibraryUIUnits.libraryViewScrollViewContentInsets;
    const NSEdgeInsets scrollerInsets = VLCLibraryUIUnits.libraryViewScrollViewScrollerInsets;

    _collectionViewScrollView.automaticallyAdjustsContentInsets = NO;
    _collectionViewScrollView.contentInsets = defaultInsets;
    _collectionViewScrollView.scrollerInsets = scrollerInsets;

    _tableViewScrollView.automaticallyAdjustsContentInsets = NO;
    _tableViewScrollView.contentInsets = defaultInsets;
    _tableViewScrollView.scrollerInsets = scrollerInsets;
}

- (void)setupPlaceholderLabel
{
    if (@available(macOS 10.12, *)) {
        _placeholderLabel = [NSTextField labelWithString:_NS("No files")];
    } else {
        _placeholderLabel = [[NSTextField alloc] init];
        self.placeholderLabel.stringValue = _NS("No files");
        self.placeholderLabel.editable = NO;
    }
    self.placeholderLabel.font = NSFont.VLClibrarySectionHeaderFont;
    self.placeholderLabel.textColor = NSColor.secondaryLabelColor;
    self.placeholderLabel.alignment = NSTextAlignmentCenter;
    self.placeholderLabel.backgroundColor = NSColor.clearColor;
    self.placeholderLabel.bezeled = NO;
    self.placeholderLabel.translatesAutoresizingMaskIntoConstraints = NO;
    [self.mediaSourceView addSubview:self.placeholderLabel];
    [self.mediaSourceView addConstraints:@[
        [self.placeholderLabel.centerXAnchor constraintEqualToAnchor:self.mediaSourceView.centerXAnchor],
        [self.placeholderLabel.centerYAnchor constraintEqualToAnchor:self.mediaSourceView.centerYAnchor],
    ]];
    [self updatePlaceholderLabel:nil];
}

- (void)setupPathControlView
{
    _pathControlViewTopConstraintToSuperview = [NSLayoutConstraint constraintWithItem:self.pathControlVisualEffectView
                                                                                    attribute:NSLayoutAttributeTop
                                                                                    relatedBy:NSLayoutRelationEqual
                                                                                       toItem:self.mediaSourceView
                                                                                    attribute:NSLayoutAttributeTop
                                                                                   multiplier:1.
                                                                                     constant:self.libraryWindow.titlebarHeight];
    [self.mediaSourceView addConstraint:_pathControlViewTopConstraintToSuperview];
    _pathControlViewTopConstraintToSuperview.active = YES;
}

- (void)updatePlaceholderLabel:(NSNotification *)notification
{
    self.placeholderLabel.hidden = self.mediaSourceTableView.numberOfRows > 0;
}

- (void)presentBrowseView
{
    [self presentMediaSourceView:VLCLibraryBrowseSegment];
}

- (void)presentStreamsView
{
    [self presentMediaSourceView:VLCLibraryStreamsSegment];
}

- (void)presentMediaSourceView:(VLCLibrarySegmentType)viewSegment
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

- (void)presentLocalFolderMrl:(NSString *)mrl
{
    [self presentBrowseView];
    [self.baseDataSource presentLocalFolderMrl:mrl];
}

@end
