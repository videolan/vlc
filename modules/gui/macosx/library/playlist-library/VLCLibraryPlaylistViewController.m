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

#import "views/VLCLoadingOverlayView.h"

#import "windows/video/VLCMainVideoViewController.h"

@interface VLCLibraryPlaylistViewController ()
{
    NSArray<NSLayoutConstraint *> *_internalPlaceholderImageViewSizeConstraints;
}
@end

@implementation VLCLibraryPlaylistViewController

- (instancetype)initWithLibraryWindow:(VLCLibraryWindow *)libraryWindow
{
    self = [super initWithLibraryWindow:libraryWindow];

    if (self) {
        _dataSource = [[VLCLibraryPlaylistDataSource alloc] init];

        [self setupPlaylistCollectionView];
        [self setupPlaylistTableView];
        [self setupPlaylistPlaceholderView];

        NSNotificationCenter * const notificationCenter = NSNotificationCenter.defaultCenter;
        [notificationCenter addObserver:self
                               selector:@selector(libraryModelUpdated:)
                                   name:VLCLibraryModelPlaylistAdded
                                 object:nil];
        [notificationCenter addObserver:self
                               selector:@selector(libraryModelUpdated:)
                                   name:VLCLibraryModelPlaylistDeleted
                                 object:nil];

        NSString * const playlistListResetLongLoadStartNotification = [VLCLibraryModelPlaylistAdded stringByAppendingString:VLCLongNotificationNameStartSuffix];
        NSString * const playlistListResetLongLoadFinishNotification = [VLCLibraryModelPlaylistAdded stringByAppendingString:VLCLongNotificationNameFinishSuffix];
        NSString * const playlistDeletedLongLoadStartNotification = [VLCLibraryModelPlaylistDeleted stringByAppendingString:VLCLongNotificationNameStartSuffix];
        NSString * const playlistDeletedLongLoadFinishNotification = [VLCLibraryModelPlaylistDeleted stringByAppendingString:VLCLongNotificationNameFinishSuffix];

        [notificationCenter addObserver:self
                               selector:@selector(libraryModelLongLoadStarted:)
                                   name:playlistListResetLongLoadStartNotification
                                 object:nil];
        [notificationCenter addObserver:self
                               selector:@selector(libraryModelLongLoadFinished:)
                                   name:playlistListResetLongLoadFinishNotification
                                 object:nil];
        [notificationCenter addObserver:self
                               selector:@selector(libraryModelLongLoadStarted:)
                                   name:playlistDeletedLongLoadStartNotification
                                 object:nil];
        [notificationCenter addObserver:self
                               selector:@selector(libraryModelLongLoadFinished:)
                                   name:playlistDeletedLongLoadFinishNotification
                                 object:nil];
    }

    return self;
}

- (void)setupPlaylistCollectionView
{
    _collectionViewScrollView = 
        [[NSScrollView alloc] initWithFrame:self.libraryWindow.libraryTargetView.frame];
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
    _internalPlaceholderImageViewSizeConstraints = @[
        [NSLayoutConstraint constraintWithItem:self.libraryWindow.placeholderImageView
                                     attribute:NSLayoutAttributeWidth
                                     relatedBy:NSLayoutRelationEqual
                                        toItem:nil
                                     attribute:NSLayoutAttributeNotAnAttribute
                                    multiplier:0.f
                                      constant:149.f],
        [NSLayoutConstraint constraintWithItem:self.libraryWindow.placeholderImageView
                                     attribute:NSLayoutAttributeHeight
                                     relatedBy:NSLayoutRelationEqual
                                        toItem:nil
                                     attribute:NSLayoutAttributeNotAnAttribute
                                    multiplier:0.f
                                      constant:149.f],
    ];
}

- (void)setupLoadingOverlayView
{
    _loadingOverlayView = [[VLCLoadingOverlayView alloc] init];
    self.loadingOverlayView.translatesAutoresizingMaskIntoConstraints = NO;
    _loadingOverlayViewConstraints = @[
        [NSLayoutConstraint constraintWithItem:self.loadingOverlayView
                                     attribute:NSLayoutAttributeTop
                                     relatedBy:NSLayoutRelationEqual
                                        toItem:self.libraryTargetView
                                     attribute:NSLayoutAttributeTop
                                    multiplier:1
                                      constant:0],
        [NSLayoutConstraint constraintWithItem:self.loadingOverlayView
                                     attribute:NSLayoutAttributeRight
                                     relatedBy:NSLayoutRelationEqual
                                        toItem:self.libraryTargetView
                                     attribute:NSLayoutAttributeRight
                                    multiplier:1
                                      constant:0],
        [NSLayoutConstraint constraintWithItem:self.loadingOverlayView
                                     attribute:NSLayoutAttributeBottom
                                     relatedBy:NSLayoutRelationEqual
                                        toItem:self.libraryTargetView
                                     attribute:NSLayoutAttributeBottom
                                    multiplier:1
                                      constant:0],
        [NSLayoutConstraint constraintWithItem:self.loadingOverlayView
                                     attribute:NSLayoutAttributeLeft
                                     relatedBy:NSLayoutRelationEqual
                                        toItem:self.libraryTargetView
                                     attribute:NSLayoutAttributeLeft
                                    multiplier:1
                                      constant:0]
    ];
}

- (NSArray<NSLayoutConstraint *> *)placeholderImageViewSizeConstraints
{
    return _internalPlaceholderImageViewSizeConstraints;
}

// TODO: This is duplicated almost verbatim across all the library view
// controllers. Ideally we should have the placeholder view handle this
// itself, or move this into a common superclass
- (void)presentPlaceholderPlaylistLibraryView
{
    for (NSLayoutConstraint * const constraint in self.libraryWindow.libraryAudioViewController.placeholderImageViewSizeConstraints) {
        constraint.active = NO;
    }
    for (NSLayoutConstraint * const constraint in self.libraryWindow.libraryVideoViewController.placeholderImageViewSizeConstraints) {
        constraint.active = NO;
    }
    for (NSLayoutConstraint * const constraint in self.placeholderImageViewSizeConstraints) {
        constraint.active = YES;
    }

    self.libraryWindow.emptyLibraryView.translatesAutoresizingMaskIntoConstraints = NO;
    self.libraryWindow.libraryTargetView.subviews = @[self.libraryWindow.emptyLibraryView];
    NSDictionary * const dict = @{@"emptyLibraryView": self.libraryWindow.emptyLibraryView};
    [self.libraryWindow.libraryTargetView addConstraints:[NSLayoutConstraint constraintsWithVisualFormat:@"H:|[emptyLibraryView(>=572.)]|" options:0 metrics:0 views:dict]];
    [self.libraryWindow.libraryTargetView addConstraints:[NSLayoutConstraint constraintsWithVisualFormat:@"V:|[emptyLibraryView(>=444.)]|" options:0 metrics:0 views:dict]];

    const vlc_ml_playlist_type_t playlistType = self.dataSource.playlistType;
    NSString *placeholderPlaylistsString = nil;
    switch (playlistType) {
        case VLC_ML_PLAYLIST_TYPE_ALL:
            placeholderPlaylistsString =
                _NS("Your favorite playlists will appear here.\n"
                    "Go to the Browse section to add playlists you love.");
            break;
        case VLC_ML_PLAYLIST_TYPE_AUDIO:
        case VLC_ML_PLAYLIST_TYPE_AUDIO_ONLY:
            placeholderPlaylistsString =
                _NS("Your favorite music playlists will appear here.\n"
                    "Go to the Browse section to add playlists you love.");
            break;
        case VLC_ML_PLAYLIST_TYPE_VIDEO:
        case VLC_ML_PLAYLIST_TYPE_VIDEO_ONLY:
            placeholderPlaylistsString =
                _NS("Your favorite video playlists will appear here.\n"
                    "Go to the Browse section to add playlists you love.");
            break;
    }

    self.libraryWindow.placeholderImageView.image = [NSImage imageNamed:@"placeholder-group2"];
    self.libraryWindow.placeholderLabel.stringValue = placeholderPlaylistsString;
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
    const vlc_ml_playlist_type_t playlistType = self.dataSource.playlistType;
    VLCLibraryModel * const libraryModel = VLCMain.sharedInstance.libraryController.libraryModel;
    if ([libraryModel numberOfPlaylistsOfType:playlistType] <= 0) {
        [self presentPlaceholderPlaylistLibraryView];
    } else {
        [self presentPlaylistLibraryView];
    }
}

- (void)presentPlaylistsView
{
    self.libraryWindow.libraryTargetView.subviews = @[];
    [self updatePresentedView];
}

- (void)presentPlaylistsViewForPlaylistType:(enum vlc_ml_playlist_type_t)playlistType
{
    self.dataSource.playlistType = playlistType;
    [self presentPlaylistsView];
}

- (void)libraryModelUpdated:(NSNotification *)notification
{
    NSParameterAssert(notification);
    VLCLibraryModel * const model = (VLCLibraryModel *)notification.object;
    NSAssert(model, @"Notification object should be a VLCLibraryModel");
    const vlc_ml_playlist_type_t playlistType = self.dataSource.playlistType;
    const size_t numberOfPlaylists = [model numberOfPlaylistsOfType:playlistType];

    if (self.libraryWindow.librarySegmentType == VLCLibraryPlaylistsSegment &&
        ((numberOfPlaylists == 0 && ![self.libraryWindow.libraryTargetView.subviews containsObject:self.libraryWindow.emptyLibraryView]) ||
         (numberOfPlaylists > 0 && ![self.libraryWindow.libraryTargetView.subviews containsObject:_collectionViewScrollView])) &&
        self.libraryWindow.videoViewController.view.hidden) {

        [self updatePresentedView];
    }
}

- (void)libraryModelLongLoadStarted:(NSNotification *)notification
{
    if ([self.libraryTargetView.subviews containsObject:self.loadingOverlayView]) {
        return;
    }

    [self.dataSource disconnect];

    self.loadingOverlayView.wantsLayer = YES;
    self.loadingOverlayView.alphaValue = 0.0;

    NSArray * const views = [self.libraryTargetView.subviews arrayByAddingObject:self.loadingOverlayView];
    self.libraryTargetView.subviews = views;
    [self.libraryTargetView addConstraints:_loadingOverlayViewConstraints];

    [NSAnimationContext runAnimationGroup:^(NSAnimationContext * const context) {
        context.duration = 0.5;
        self.loadingOverlayView.animator.alphaValue = 1.0;
    } completionHandler:nil];
    [self.loadingOverlayView.indicator startAnimation:self];
}

- (void)libraryModelLongLoadFinished:(NSNotification *)notification
{
    if (![self.libraryTargetView.subviews containsObject:self.loadingOverlayView]) {
        return;
    }

    [self.dataSource connect];

    self.loadingOverlayView.wantsLayer = YES;
    self.loadingOverlayView.alphaValue = 1.0;

    [NSAnimationContext runAnimationGroup:^(NSAnimationContext * const context) {
        context.duration = 1.0;
        self.loadingOverlayView.animator.alphaValue = 0.0;
    } completionHandler:^{
        [self.libraryTargetView removeConstraints:_loadingOverlayViewConstraints];
        NSMutableArray * const views = self.libraryTargetView.subviews.mutableCopy;
        [views removeObject:self.loadingOverlayView];
        self.libraryTargetView.subviews = views.copy;
        [self.loadingOverlayView.indicator stopAnimation:self];
    }];
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
