/*****************************************************************************
 * VLCLibraryVideoViewController.m: MacOS X interface module
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

#import "VLCLibraryVideoViewController.h"

#import "extensions/NSString+Helpers.h"

#import "library/VLCLibraryCollectionViewDelegate.h"
#import "library/VLCLibraryCollectionViewFlowLayout.h"
#import "library/VLCLibraryCollectionViewItem.h"
#import "library/VLCLibraryCollectionViewMediaItemSupplementaryDetailView.h"
#import "library/VLCLibraryCollectionViewSupplementaryElementView.h"
#import "library/VLCLibraryController.h"
#import "library/VLCLibraryModel.h"
#import "library/VLCLibrarySegment.h"
#import "library/VLCLibraryTableCellView.h"
#import "library/VLCLibraryTwoPaneSplitViewDelegate.h"
#import "library/VLCLibraryUIUnits.h"
#import "library/VLCLibraryWindow.h"
#import "library/VLCLibraryWindowPersistentPreferences.h"

#import "library/audio-library/VLCLibraryAudioViewController.h"

#import "library/home-library/VLCLibraryHomeViewVideoContainerViewDataSource.h"

#import "library/playlist-library/VLCLibraryPlaylistViewController.h"

#import "library/video-library/VLCLibraryShowsDataSource.h"
#import "library/video-library/VLCLibraryVideoDataSource.h"
#import "library/video-library/VLCLibraryVideoTableViewDelegate.h"

#import "main/VLCMain.h"

#import "views/VLCLoadingOverlayView.h"
#import "views/VLCNoResultsLabel.h"

#import "windows/video/VLCVoutView.h"
#import "windows/video/VLCMainVideoViewController.h"

@interface VLCLibraryVideoViewController ()
{
    VLCLibraryVideoTableViewDelegate *_videoLibraryTableViewDelegate;
    VLCLibraryTwoPaneSplitViewDelegate *_splitViewDelegate;
    VLCLibraryCollectionViewDelegate *_collectionViewDelegate;
    VLCLibraryCollectionViewFlowLayout *_collectionViewLayout;

    id<VLCMediaLibraryItemProtocol> _awaitingPresentingLibraryItem;

    NSArray<NSLayoutConstraint *> *_loadingOverlayViewConstraints;

    VLCNoResultsLabel *_noResultsLabel;
}
@end

@implementation VLCLibraryVideoViewController

- (instancetype)initWithLibraryWindow:(VLCLibraryWindow *)libraryWindow
{
    self = [super init];

    if(self) {
        _videoLibraryTableViewDelegate = [[VLCLibraryVideoTableViewDelegate alloc] init];
        _splitViewDelegate = [[VLCLibraryTwoPaneSplitViewDelegate alloc] init];

        [self setupPropertiesFromLibraryWindow:libraryWindow];
        [self setupDataSources];
        [self setupCollectionView];
        [self setupVideoPlaceholderView];
        [self setupVideoLibraryViews];
        [self setupLoadingOverlayView];

        NSNotificationCenter *notificationCenter = NSNotificationCenter.defaultCenter;
        [notificationCenter addObserver:self
                               selector:@selector(libraryModelUpdated:)
                                   name:VLCLibraryModelVideoMediaListReset
                                 object:nil];
        [notificationCenter addObserver:self
                               selector:@selector(libraryModelUpdated:)
                                   name:VLCLibraryModelVideoMediaItemDeleted
                                 object:nil];

        NSString * const videoMediaResetLongLoadStartNotification = [VLCLibraryModelVideoMediaListReset stringByAppendingString:VLCLongNotificationNameStartSuffix];
        NSString * const videoMediaResetLongLoadFinishNotification = [VLCLibraryModelVideoMediaListReset stringByAppendingString:VLCLongNotificationNameFinishSuffix];
        NSString * const videoMediaDeletedLongLoadStartNotification = [VLCLibraryModelVideoMediaItemDeleted stringByAppendingString:VLCLongNotificationNameStartSuffix];
        NSString * const videoMediaDeletedLongLoadFinishNotification = [VLCLibraryModelVideoMediaItemDeleted stringByAppendingString:VLCLongNotificationNameFinishSuffix];

        [notificationCenter addObserver:self
                               selector:@selector(libraryModelLongLoadStarted:)
                                   name:videoMediaResetLongLoadStartNotification
                                 object:nil];
        [notificationCenter addObserver:self
                               selector:@selector(libraryModelLongLoadFinished:)
                                   name:videoMediaResetLongLoadFinishNotification
                                 object:nil];
        [notificationCenter addObserver:self
                               selector:@selector(libraryModelLongLoadStarted:)
                                   name:videoMediaDeletedLongLoadStartNotification
                                 object:nil];
        [notificationCenter addObserver:self
                               selector:@selector(libraryModelLongLoadFinished:)
                                   name:videoMediaDeletedLongLoadFinishNotification
                                 object:nil];
    }

    return self;
}

- (void)setupPropertiesFromLibraryWindow:(VLCLibraryWindow *)libraryWindow
{
    NSParameterAssert(libraryWindow);
    _libraryWindow = libraryWindow;
    _libraryTargetView = libraryWindow.libraryTargetView;
    _videoLibraryView = libraryWindow.videoLibraryView;
    _videoLibrarySplitView = libraryWindow.videoLibrarySplitView;
    _videoLibraryCollectionViewScrollView = libraryWindow.videoLibraryCollectionViewScrollView;
    _videoLibraryCollectionView = libraryWindow.videoLibraryCollectionView;
    _videoLibraryGroupSelectionTableViewScrollView = libraryWindow.videoLibraryGroupSelectionTableViewScrollView;
    _videoLibraryGroupSelectionTableView = libraryWindow.videoLibraryGroupSelectionTableView;
    _videoLibraryGroupsTableViewScrollView = libraryWindow.videoLibraryGroupsTableViewScrollView;
    _videoLibraryGroupsTableView = libraryWindow.videoLibraryGroupsTableView;

    _placeholderImageView = libraryWindow.placeholderImageView;
    _placeholderLabel = libraryWindow.placeholderLabel;
    _emptyLibraryView = libraryWindow.emptyLibraryView;
}

- (void)setupDataSources
{
    _videoLibrarySplitView.delegate = _splitViewDelegate;
    [_splitViewDelegate resetDefaultSplitForSplitView:self.videoLibrarySplitView];

    _libraryVideoDataSource = [[VLCLibraryVideoDataSource alloc] init];
    _libraryVideoDataSource.libraryModel = VLCMain.sharedInstance.libraryController.libraryModel;
    _libraryVideoDataSource.masterTableView = _videoLibraryGroupsTableView;
    _libraryVideoDataSource.detailTableView = _videoLibraryGroupSelectionTableView;
    _libraryVideoDataSource.collectionView = _videoLibraryCollectionView;

    _libraryShowsDataSource = [[VLCLibraryShowsDataSource alloc] init];
    self.libraryShowsDataSource.libraryModel =
        VLCMain.sharedInstance.libraryController.libraryModel;
    self.libraryShowsDataSource.collectionView = self.videoLibraryCollectionView;
    self.libraryShowsDataSource.masterTableView = self.videoLibraryGroupsTableView;
    self.libraryShowsDataSource.detailTableView = self.videoLibraryGroupSelectionTableView;

    NSNib * const tableCellViewNib = [[NSNib alloc] initWithNibNamed:NSStringFromClass(VLCLibraryTableCellView.class) bundle:nil];
    [_videoLibraryGroupsTableView registerNib:tableCellViewNib forIdentifier:@"VLCVideoLibraryTableViewCellIdentifier"];
    [_videoLibraryGroupSelectionTableView registerNib:tableCellViewNib forIdentifier:@"VLCVideoLibraryTableViewCellIdentifier"];
}

- (void)setupCollectionView
{
    _collectionViewLayout = [[VLCLibraryCollectionViewFlowLayout alloc] init];

    const CGFloat collectionItemSpacing = VLCLibraryUIUnits.collectionViewItemSpacing;
    const NSEdgeInsets collectionViewSectionInset = VLCLibraryUIUnits.collectionViewSectionInsets;
    _collectionViewLayout.headerReferenceSize = VLCLibraryCollectionViewSupplementaryElementView.defaultHeaderSize;
    _collectionViewLayout.minimumLineSpacing = collectionItemSpacing;
    _collectionViewLayout.minimumInteritemSpacing = collectionItemSpacing;
    _collectionViewLayout.sectionInset = collectionViewSectionInset;

    NSCollectionView * const collectionView = self.videoLibraryCollectionView;
    collectionView.collectionViewLayout = _collectionViewLayout;

    _collectionViewDelegate = [[VLCLibraryCollectionViewDelegate alloc] init];
    _collectionViewDelegate.itemsAspectRatio = VLCLibraryCollectionViewItemAspectRatioVideoItem;
    _collectionViewDelegate.staticItemSize = VLCLibraryCollectionViewItem.defaultVideoItemSize;
    collectionView.delegate = _collectionViewDelegate;

    [collectionView registerClass:VLCLibraryCollectionViewItem.class
            forItemWithIdentifier:VLCLibraryCellIdentifier];

    [collectionView registerClass:VLCLibraryCollectionViewSupplementaryElementView.class
       forSupplementaryViewOfKind:NSCollectionElementKindSectionHeader
                   withIdentifier:VLCLibrarySupplementaryElementViewIdentifier];

    NSString * const mediaItemSupplementaryDetailViewString =
        NSStringFromClass(VLCLibraryCollectionViewMediaItemSupplementaryDetailView.class);
    NSNib * const mediaItemSupplementaryDetailViewNib =
        [[NSNib alloc] initWithNibNamed:mediaItemSupplementaryDetailViewString bundle:nil];
    
    [collectionView registerNib:mediaItemSupplementaryDetailViewNib
     forSupplementaryViewOfKind:VLCLibraryCollectionViewMediaItemSupplementaryDetailViewKind
                 withIdentifier:VLCLibraryCollectionViewMediaItemSupplementaryDetailViewIdentifier];
}

- (void)setupVideoPlaceholderView
{
    _videoPlaceholderImageViewSizeConstraints = @[
        [NSLayoutConstraint constraintWithItem:_placeholderImageView
                                     attribute:NSLayoutAttributeWidth
                                     relatedBy:NSLayoutRelationEqual
                                        toItem:nil
                                     attribute:NSLayoutAttributeNotAnAttribute
                                    multiplier:0.f
                                      constant:182.f],
        [NSLayoutConstraint constraintWithItem:_placeholderImageView
                                     attribute:NSLayoutAttributeHeight
                                     relatedBy:NSLayoutRelationEqual
                                        toItem:nil
                                     attribute:NSLayoutAttributeNotAnAttribute
                                    multiplier:0.f
                                      constant:114.f],
    ];
}

- (void)setupVideoLibraryViews
{
    _videoLibraryGroupsTableView.rowHeight = VLCLibraryUIUnits.mediumTableViewRowHeight;
    _videoLibraryGroupSelectionTableView.rowHeight = VLCLibraryUIUnits.mediumTableViewRowHeight;

    const NSEdgeInsets defaultInsets = VLCLibraryUIUnits.libraryViewScrollViewContentInsets;
    const NSEdgeInsets scrollerInsets = VLCLibraryUIUnits.libraryViewScrollViewScrollerInsets;

    _videoLibraryCollectionViewScrollView.automaticallyAdjustsContentInsets = NO;
    _videoLibraryCollectionViewScrollView.contentInsets = defaultInsets;
    _videoLibraryCollectionViewScrollView.scrollerInsets = scrollerInsets;

    _videoLibraryGroupsTableViewScrollView.automaticallyAdjustsContentInsets = NO;
    _videoLibraryGroupsTableViewScrollView.contentInsets = defaultInsets;
    _videoLibraryGroupsTableViewScrollView.scrollerInsets = scrollerInsets;
    _videoLibraryGroupSelectionTableViewScrollView.automaticallyAdjustsContentInsets = NO;
    _videoLibraryGroupSelectionTableViewScrollView.contentInsets = defaultInsets;
    _videoLibraryGroupSelectionTableViewScrollView.scrollerInsets = scrollerInsets;
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

#pragma mark - Show the video library view

- (void)updatePresentedVideoLibraryView
{
    self.videoLibraryCollectionView.dataSource = self.libraryVideoDataSource;

    self.videoLibraryGroupsTableView.dataSource = self.libraryVideoDataSource;
    self.videoLibraryGroupsTableView.target = self.libraryVideoDataSource;
    self.videoLibraryGroupsTableView.delegate = _videoLibraryTableViewDelegate;

    self.videoLibraryGroupSelectionTableView.dataSource = self.libraryVideoDataSource;
    self.videoLibraryGroupSelectionTableView.target = self.libraryVideoDataSource;
    self.videoLibraryGroupSelectionTableView.delegate = _videoLibraryTableViewDelegate;

    [self.libraryVideoDataSource reloadData];

    const BOOL anyVideoMedia = self.libraryVideoDataSource.libraryModel.numberOfVideoMedia > 0;
    if (anyVideoMedia) {
        const VLCLibraryViewModeSegment viewModeSegment = VLCLibraryWindowPersistentPreferences.sharedInstance.videoLibraryViewMode;
        [self presentVideoLibraryView:viewModeSegment];
    } else if (self.libraryVideoDataSource.libraryModel.filterString.length > 0) {
        [self presentNoResultsView];
    } else {
        [self presentPlaceholderVideoLibraryView];
    }
}

- (void)updatePresentedShowsLibraryView
{
    self.videoLibraryCollectionView.dataSource = self.libraryShowsDataSource;

    self.videoLibraryGroupsTableView.dataSource = self.libraryShowsDataSource;
    self.videoLibraryGroupsTableView.target = self.libraryShowsDataSource;
    self.videoLibraryGroupsTableView.delegate = _videoLibraryTableViewDelegate;

    self.videoLibraryGroupSelectionTableView.dataSource = self.libraryShowsDataSource;
    self.videoLibraryGroupSelectionTableView.target = self.libraryShowsDataSource;
    self.videoLibraryGroupSelectionTableView.delegate = _videoLibraryTableViewDelegate;

    [self.libraryShowsDataSource reloadData];

    const BOOL anyShows = self.libraryShowsDataSource.libraryModel.listOfShows.count > 0;
    if (anyShows) {
        const VLCLibraryViewModeSegment viewModeSegment = VLCLibraryWindowPersistentPreferences.sharedInstance.showsLibraryViewMode;
        [self presentVideoLibraryView:viewModeSegment];
    } else if (self.libraryShowsDataSource.libraryModel.filterString.length > 0) {
        [self presentNoResultsView];
    } else {
        [self presentPlaceholderVideoLibraryView];
    }
}

- (void)presentVideoView
{
    _libraryTargetView.subviews = @[];
    [self updatePresentedVideoLibraryView];
}

- (void)presentShowsView
{
    self.libraryTargetView.subviews = @[];
    [self updatePresentedShowsLibraryView];
}

- (void)presentPlaceholderVideoLibraryView
{
    for (NSLayoutConstraint * const constraint in _libraryWindow.libraryAudioViewController.audioPlaceholderImageViewSizeConstraints) {
        constraint.active = NO;
    }
    for (NSLayoutConstraint * const constraint in _libraryWindow.libraryPlaylistViewController.placeholderImageViewConstraints) {
        constraint.active = NO;
    }
    for (NSLayoutConstraint * const constraint in _videoPlaceholderImageViewSizeConstraints) {
        constraint.active = YES;
    }

    _emptyLibraryView.translatesAutoresizingMaskIntoConstraints = NO;
    if ([self.libraryTargetView.subviews containsObject:self.loadingOverlayView]) {
        self.libraryTargetView.subviews = @[self.emptyLibraryView, self.loadingOverlayView];
    } else {
        self.libraryTargetView.subviews = @[self.emptyLibraryView];
    }
    NSDictionary *dict = NSDictionaryOfVariableBindings(_emptyLibraryView);
    [_libraryTargetView addConstraints:[NSLayoutConstraint constraintsWithVisualFormat:@"H:|[_emptyLibraryView(>=572.)]|" options:0 metrics:0 views:dict]];
    [_libraryTargetView addConstraints:[NSLayoutConstraint constraintsWithVisualFormat:@"V:|[_emptyLibraryView(>=444.)]|" options:0 metrics:0 views:dict]];

    _placeholderImageView.image = [NSImage imageNamed:@"placeholder-video"];
    _placeholderLabel.stringValue = _NS("Your favorite videos will appear here.\nGo to the Browse section to add videos you love.");
}

- (void)presentNoResultsView
{
    if (_noResultsLabel == nil) {
        _noResultsLabel = [[VLCNoResultsLabel alloc] init];
        _noResultsLabel.translatesAutoresizingMaskIntoConstraints = NO;
    }

    if ([self.libraryTargetView.subviews containsObject:self.loadingOverlayView]) {
        self.libraryTargetView.subviews = @[_noResultsLabel, self.loadingOverlayView];
    } else {
        self.libraryTargetView.subviews = @[_noResultsLabel];
    }

    [NSLayoutConstraint activateConstraints:@[
        [_noResultsLabel.centerXAnchor constraintEqualToAnchor:self.libraryTargetView.centerXAnchor],
        [_noResultsLabel.centerYAnchor constraintEqualToAnchor:self.libraryTargetView.centerYAnchor]
    ]];
}

- (void)presentVideoLibraryView:(VLCLibraryViewModeSegment)viewModeSegment
{
    _videoLibraryView.translatesAutoresizingMaskIntoConstraints = NO;
    if ([self.libraryTargetView.subviews containsObject:self.loadingOverlayView]) {
        self.libraryTargetView.subviews = @[self.videoLibraryView, self.loadingOverlayView];
    } else {
        self.libraryTargetView.subviews = @[self.videoLibraryView];
    }

    NSDictionary *dict = NSDictionaryOfVariableBindings(_videoLibraryView);
    [_libraryTargetView addConstraints:[NSLayoutConstraint constraintsWithVisualFormat:@"H:|[_videoLibraryView(>=572.)]|" options:0 metrics:0 views:dict]];
    [_libraryTargetView addConstraints:[NSLayoutConstraint constraintsWithVisualFormat:@"V:|[_videoLibraryView(>=444.)]|" options:0 metrics:0 views:dict]];

    if (viewModeSegment == VLCLibraryGridViewModeSegment) {
        _videoLibrarySplitView.hidden = YES;
        _videoLibraryCollectionViewScrollView.hidden = NO;
    } else if (viewModeSegment == VLCLibraryListViewModeSegment) {
        _videoLibrarySplitView.hidden = NO;
        _videoLibraryCollectionViewScrollView.hidden = YES;
    } else {
        NSAssert(false, @"View mode must be grid or list mode");
    }
}

- (void)libraryModelUpdated:(NSNotification *)aNotification
{
    NSParameterAssert(aNotification);
    VLCLibraryModel *model = VLCMain.sharedInstance.libraryController.libraryModel;
    const NSUInteger videoCount = model.numberOfVideoMedia;
    const NSUInteger showsCount = model.numberOfShows;

    if (_libraryWindow.librarySegmentType == VLCLibraryVideoSegment &&
        ((videoCount == 0 && ![_libraryTargetView.subviews containsObject:_emptyLibraryView]) ||
         (videoCount > 0 && ![_libraryTargetView.subviews containsObject:_videoLibraryView])) &&
        _libraryWindow.videoViewController.view.hidden) {

        [self updatePresentedVideoLibraryView];
    } else if (_libraryWindow.librarySegmentType == VLCLibraryShowsVideoSubSegment &&
         ((showsCount == 0 && ![_libraryTargetView.subviews containsObject:_emptyLibraryView]) ||
          (showsCount > 0 && ![_libraryTargetView.subviews containsObject:_videoLibraryView])) &&
         _libraryWindow.videoViewController.view.hidden) {

         [self updatePresentedShowsLibraryView];
     }
}

- (void)presentLibraryItemWaitForCollectionViewDataSourceFinished:(NSNotification *)notification
{
    [NSNotificationCenter.defaultCenter removeObserver:self
                                                  name:VLCLibraryVideoCollectionViewDataSourceDisplayedCollectionChangedNotification
                                                object:self.libraryVideoDataSource];

    // TODO: Present for collection view
    _awaitingPresentingLibraryItem = nil;
}

- (void)presentLibraryItemWaitForTableViewDataSourceFinished:(NSNotification *)notification
{
    [NSNotificationCenter.defaultCenter removeObserver:self
                                                  name:VLCLibraryVideoDataSourceDisplayedCollectionChangedNotification
                                             object:self.libraryVideoDataSource];

    const NSInteger rowForLibraryItem = [self.libraryVideoDataSource rowForLibraryItem:_awaitingPresentingLibraryItem];
    if (rowForLibraryItem != NSNotFound) {
        NSIndexSet * const indexSet = [NSIndexSet indexSetWithIndex:rowForLibraryItem];
        [self.videoLibraryGroupsTableView selectRowIndexes:indexSet byExtendingSelection:NO];
        [self.videoLibraryGroupsTableView scrollRowToVisible:rowForLibraryItem];
    }

    _awaitingPresentingLibraryItem = nil;
}

- (void)presentLibraryItem:(id<VLCMediaLibraryItemProtocol>)libraryItem
{
    if (libraryItem == nil) {
        return;
    }

    _awaitingPresentingLibraryItem = libraryItem;

     const VLCLibraryViewModeSegment viewModeSegment = VLCLibraryWindowPersistentPreferences.sharedInstance.videoLibraryViewMode;

    if (viewModeSegment == VLCLibraryGridViewModeSegment) {
        [NSNotificationCenter.defaultCenter addObserver:self
                                           selector:@selector(presentLibraryItemWaitForTableViewDataSourceFinished:)
                                               name:VLCLibraryVideoDataSourceDisplayedCollectionChangedNotification
                                             object:self.libraryVideoDataSource];

    } else if (viewModeSegment == VLCLibraryListViewModeSegment) {
        [NSNotificationCenter.defaultCenter addObserver:self
                                           selector:@selector(presentLibraryItemWaitForTableViewDataSourceFinished:)
                                               name:VLCLibraryVideoCollectionViewDataSourceDisplayedCollectionChangedNotification
                                             object:self.libraryVideoDataSource];

    } else {
        NSAssert(false, @"View mode must be grid or list mode");
    }
}

- (void)libraryModelLongLoadStarted:(NSNotification *)notification
{
    if ([self.libraryTargetView.subviews containsObject:self.loadingOverlayView]) {
        return;
    }

    [self.libraryVideoDataSource disconnect];

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

    [self.libraryVideoDataSource connect];

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

@end
