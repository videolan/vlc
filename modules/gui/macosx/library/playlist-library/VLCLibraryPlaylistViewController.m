/*****************************************************************************
 * VLCLibraryPlaylistViewController.m: MacOS X interface module
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

#import "VLCLibraryPlaylistViewController.h"

#import "extensions/NSString+Helpers.h"

#import "library/VLCLibraryCollectionViewDelegate.h"
#import "library/VLCLibraryCollectionViewFlowLayout.h"
#import "library/VLCLibraryController.h"
#import "library/VLCLibraryMasterDetailViewTableViewDelegate.h"
#import "library/VLCLibraryModel.h"
#import "library/VLCLibraryTableCellView.h"
#import "library/VLCLibraryTableView.h"
#import "library/VLCLibraryUIUnits.h"
#import "library/VLCLibraryWindow.h"
#import "library/VLCLibraryWindowPersistentPreferences.h"

#import "library/audio-library/VLCLibraryAudioViewController.h"

#import "library/playlist-library/VLCLibraryPlaylistDataSource.h"

#import "library/video-library/VLCLibraryVideoViewController.h"

#import "main/VLCMain.h"

#import "windows/video/VLCMainVideoViewController.h"

@implementation VLCLibraryPlaylistViewController

- (instancetype)initWithLibraryWindow:(VLCLibraryWindow *)libraryWindow
{
    self = [super init];

    if (self) {
        [self setupPropertiesFromLibraryWindow:libraryWindow];

        _dataSource = [[VLCLibraryPlaylistDataSource alloc] init];

        [self setupPlaylistCollectionView];
        [self setupPlaylistTableView];
        [self setupPlaylistPlaceholderView];

        NSNotificationCenter * const notificationCenter = NSNotificationCenter.defaultCenter;
        [notificationCenter addObserver:self
                               selector:@selector(libraryModelUpdated:)
                                   name:VLCLibraryModelPlaylistListReset
                                 object:nil];
        [notificationCenter addObserver:self
                               selector:@selector(libraryModelUpdated:)
                                   name:VLCLibraryModelPlaylistDeleted
                                 object:nil];
    }

    return self;
}

- (void)setupPropertiesFromLibraryWindow:(VLCLibraryWindow*)libraryWindow
{
    NSParameterAssert(libraryWindow);
    _libraryWindow = libraryWindow;
    _libraryTargetView = libraryWindow.libraryTargetView;
}

- (void)setupPlaylistCollectionView
{
    _collectionViewScrollView = [[NSScrollView alloc] initWithFrame:_libraryWindow.libraryTargetView.frame];
    _collectionViewDelegate = [[VLCLibraryCollectionViewDelegate alloc] init];
    _collectionView = [[NSCollectionView alloc] init];

    _collectionViewScrollView.documentView = _collectionView;
    _collectionViewScrollView.translatesAutoresizingMaskIntoConstraints = NO;
    _collectionViewScrollView.automaticallyAdjustsContentInsets = NO;
    _collectionViewScrollView.contentInsets = VLCLibraryUIUnits.libraryViewScrollViewContentInsets;
    _collectionViewScrollView.scrollerInsets = VLCLibraryUIUnits.libraryViewScrollViewScrollerInsets;

    _collectionView.delegate = _collectionViewDelegate;
    _collectionView.collectionViewLayout = VLCLibraryCollectionViewFlowLayout.standardLayout;
    _collectionView.selectable = YES;
    _collectionView.allowsMultipleSelection = NO;
    _collectionView.allowsEmptySelection = YES;

    self.dataSource.collectionViews = @[self.collectionView];
}

- (void)setupPlaylistTableView
{
    _masterTableViewScrollView = [[NSScrollView alloc] init];
    _detailTableViewScrollView = [[NSScrollView alloc] init];
    _tableViewDelegate = [[VLCLibraryMasterDetailViewTableViewDelegate alloc] init];
    _masterTableView = [[VLCLibraryTableView alloc] init];
    _detailTableView = [[VLCLibraryTableView alloc] init];
    _listViewSplitView = [[NSSplitView alloc] init];

    self.masterTableViewScrollView.translatesAutoresizingMaskIntoConstraints = NO;
    self.detailTableViewScrollView.translatesAutoresizingMaskIntoConstraints = NO;
    self.listViewSplitView.translatesAutoresizingMaskIntoConstraints = NO;
    self.masterTableView.translatesAutoresizingMaskIntoConstraints = NO;
    self.detailTableView.translatesAutoresizingMaskIntoConstraints = NO;

    const NSEdgeInsets defaultInsets = VLCLibraryUIUnits.libraryViewScrollViewContentInsets;
    const NSEdgeInsets scrollerInsets = VLCLibraryUIUnits.libraryViewScrollViewScrollerInsets;

    self.masterTableViewScrollView.hasHorizontalScroller = NO;
    self.masterTableViewScrollView.borderType = NSNoBorder;
    self.masterTableViewScrollView.automaticallyAdjustsContentInsets = NO;
    self.masterTableViewScrollView.contentInsets = defaultInsets;
    self.masterTableViewScrollView.scrollerInsets = scrollerInsets;

    self.detailTableViewScrollView.hasHorizontalScroller = NO;
    self.detailTableViewScrollView.borderType = NSNoBorder;
    self.detailTableViewScrollView.automaticallyAdjustsContentInsets = NO;
    self.detailTableViewScrollView.contentInsets = defaultInsets;
    self.detailTableViewScrollView.scrollerInsets = scrollerInsets;

    self.masterTableViewScrollView.documentView = self.masterTableView;
    self.detailTableViewScrollView.documentView = self.detailTableView;

    self.listViewSplitView.vertical = YES;
    self.listViewSplitView.dividerStyle = NSSplitViewDividerStyleThin;
    self.listViewSplitView.delegate = self;
    [self.listViewSplitView addArrangedSubview:self.masterTableViewScrollView];
    [self.listViewSplitView addArrangedSubview:self.detailTableViewScrollView];

    NSTableColumn * const masterColumn = [[NSTableColumn alloc] initWithIdentifier:@"playlists"];
    NSTableColumn * const detailColumn =
        [[NSTableColumn alloc] initWithIdentifier:@"selectedPlaylist"];

    [self.masterTableView addTableColumn:masterColumn];
    [self.detailTableView addTableColumn:detailColumn];

    NSNib * const tableCellViewNib =
        [[NSNib alloc] initWithNibNamed:NSStringFromClass(VLCLibraryTableCellView.class)
                                 bundle:nil];
    [self.masterTableView registerNib:tableCellViewNib
                        forIdentifier:@"VLCLibraryTableViewCellIdentifier"];
    [self.detailTableView registerNib:tableCellViewNib
                        forIdentifier:@"VLCLibraryTableViewCellIdentifier"];

    self.masterTableView.headerView = nil;
    self.detailTableView.headerView = nil;

    self.masterTableView.rowHeight = VLCLibraryUIUnits.mediumTableViewRowHeight;
    self.detailTableView.rowHeight = VLCLibraryUIUnits.mediumTableViewRowHeight;

    self.masterTableView.delegate = self.tableViewDelegate;
    self.detailTableView.delegate = self.tableViewDelegate;

    self.dataSource.masterTableView = self.masterTableView;
    self.dataSource.detailTableView = self.detailTableView;

    self.masterTableView.dataSource = self.dataSource;
    self.detailTableView.dataSource = self.dataSource;
}

- (void)setupPlaylistPlaceholderView
{
    _placeholderImageViewConstraints = @[
        [NSLayoutConstraint constraintWithItem:_libraryWindow.placeholderImageView
                                     attribute:NSLayoutAttributeWidth
                                     relatedBy:NSLayoutRelationEqual
                                        toItem:nil
                                     attribute:NSLayoutAttributeNotAnAttribute
                                    multiplier:0.f
                                      constant:149.f],
        [NSLayoutConstraint constraintWithItem:_libraryWindow.placeholderImageView
                                     attribute:NSLayoutAttributeHeight
                                     relatedBy:NSLayoutRelationEqual
                                        toItem:nil
                                     attribute:NSLayoutAttributeNotAnAttribute
                                    multiplier:0.f
                                      constant:149.f],
    ];
}

// TODO: This is duplicated almost verbatim across all the library view
// controllers. Ideally we should have the placeholder view handle this
// itself, or move this into a common superclass
- (void)presentPlaceholderPlaylistLibraryView
{
    for (NSLayoutConstraint * const constraint in _libraryWindow.libraryAudioViewController.audioPlaceholderImageViewSizeConstraints) {
        constraint.active = NO;
    }
    for (NSLayoutConstraint * const constraint in _libraryWindow.libraryVideoViewController.videoPlaceholderImageViewSizeConstraints) {
        constraint.active = NO;
    }
    for (NSLayoutConstraint * const constraint in _placeholderImageViewConstraints) {
        constraint.active = YES;
    }

    _libraryWindow.emptyLibraryView.translatesAutoresizingMaskIntoConstraints = NO;
    _libraryWindow.libraryTargetView.subviews = @[_libraryWindow.emptyLibraryView];
    NSDictionary * const dict = @{@"emptyLibraryView": _libraryWindow.emptyLibraryView};
    [_libraryWindow.libraryTargetView addConstraints:[NSLayoutConstraint constraintsWithVisualFormat:@"H:|[emptyLibraryView(>=572.)]|" options:0 metrics:0 views:dict]];
    [_libraryWindow.libraryTargetView addConstraints:[NSLayoutConstraint constraintsWithVisualFormat:@"V:|[emptyLibraryView(>=444.)]|" options:0 metrics:0 views:dict]];

    _libraryWindow.placeholderImageView.image = [NSImage imageNamed:@"placeholder-group2"];
    _libraryWindow.placeholderLabel.stringValue = _NS("Your favorite playlists will appear here.\nGo to the Browse section to add playlists you love.");
}

- (void)presentPlaylistLibraryView
{
    const VLCLibraryViewModeSegment viewModeSegment =
        VLCLibraryWindowPersistentPreferences.sharedInstance.playlistLibraryViewMode;
    NSView *viewToPresent = nil;

    if (viewModeSegment == VLCLibraryGridViewModeSegment) {
        viewToPresent = self.collectionViewScrollView;
    } else {
        viewToPresent = self.listViewSplitView;
    }
    NSParameterAssert(viewToPresent != nil);

    self.libraryTargetView.subviews = @[viewToPresent];
    [NSLayoutConstraint activateConstraints:@[
        [self.libraryTargetView.topAnchor constraintEqualToAnchor:viewToPresent.topAnchor],
        [self.libraryTargetView.bottomAnchor constraintEqualToAnchor:viewToPresent.bottomAnchor],
        [self.libraryTargetView.leadingAnchor constraintEqualToAnchor:viewToPresent.leadingAnchor],
        [self.libraryTargetView.trailingAnchor constraintEqualToAnchor:viewToPresent.trailingAnchor]
    ]];
}

- (void)updatePresentedView
{
    if (VLCMain.sharedInstance.libraryController.libraryModel.numberOfPlaylists <= 0) {
        [self presentPlaceholderPlaylistLibraryView];
    } else {
        [self presentPlaylistLibraryView];
    }
}

- (void)presentPlaylistsView
{
    _libraryWindow.libraryTargetView.subviews = @[];
    [self updatePresentedView];
}

- (void)libraryModelUpdated:(NSNotification *)notification
{
    NSParameterAssert(notification);
    VLCLibraryModel * const model = (VLCLibraryModel *)notification.object;
    NSAssert(model, @"Notification object should be a VLCLibraryModel");

    if (_libraryWindow.librarySegmentType == VLCLibraryPlaylistsSegment &&
        ((model.numberOfPlaylists == 0 && ![_libraryWindow.libraryTargetView.subviews containsObject:_libraryWindow.emptyLibraryView]) ||
         (model.numberOfPlaylists > 0 && ![_libraryWindow.libraryTargetView.subviews containsObject:_collectionViewScrollView])) &&
        _libraryWindow.videoViewController.view.hidden) {

        [self updatePresentedView];
    }
}

#pragma mark - NSSplitViewDelegate

- (CGFloat)splitView:(NSSplitView *)splitView 
constrainMinCoordinate:(CGFloat)proposedMinimumPosition
         ofSubviewAt:(NSInteger)dividerIndex
{
    if (dividerIndex == 0) {
        return VLCLibraryUIUnits.librarySplitViewSelectionViewDefaultWidth;
    } else {
        return VLCLibraryUIUnits.librarySplitViewMainViewMinimumWidth;
    }
}

@end
