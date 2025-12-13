/*****************************************************************************
 * VLCLibraryAudioViewController.m: MacOS X interface module
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

#import "VLCLibraryAudioViewController.h"

#import "extensions/NSString+Helpers.h"
#import "extensions/NSWindow+VLCAdditions.h"

#import "library/VLCLibraryCollectionView.h"
#import "library/VLCLibraryCollectionViewDelegate.h"
#import "library/VLCLibraryCollectionViewFlowLayout.h"
#import "library/VLCLibraryController.h"
#import "library/VLCLibraryDataTypes.h"
#import "library/VLCLibraryModel.h"
#import "library/VLCLibrarySegment.h"
#import "library/VLCLibraryWindowNavigationSidebarViewController.h"
#import "library/VLCLibraryWindowSplitViewController.h"
#import "library/VLCLibraryTwoPaneSplitViewDelegate.h"
#import "library/VLCLibraryWindow.h"
#import "library/VLCLibraryWindowPersistentPreferences.h"

#import "library/audio-library/VLCLibraryAlbumTableCellView.h"
#import "library/audio-library/VLCLibraryAudioDataSource.h"
#import "library/audio-library/VLCLibraryAudioGroupDataSource.h"
#import "library/audio-library/VLCLibraryAudioGroupHeaderView.h"
#import "library/audio-library/VLCLibraryAudioGroupTableHeaderView.h"
#import "library/audio-library/VLCLibraryAudioGroupTableViewDelegate.h"
#import "library/audio-library/VLCLibraryAudioGroupTableHeaderCell.h"
#import "library/audio-library/VLCLibraryAudioTableViewDelegate.h"

#import "library/playlist-library/VLCLibraryPlaylistViewController.h"

#import "library/video-library/VLCLibraryVideoViewController.h"

#import "main/VLCMain.h"

#import "windows/video/VLCVoutView.h"
#import "windows/video/VLCMainVideoViewController.h"

NSString *VLCLibraryPlaceholderAudioViewIdentifier = @"VLCLibraryPlaceholderAudioViewIdentifier";

@interface VLCLibraryAudioViewController()
{
    id<VLCMediaLibraryItemProtocol> _awaitingPresentingLibraryItem;

    NSArray<NSLayoutConstraint *> *_internalPlaceholderImageViewSizeConstraints;
    NSArray<NSString *> *_placeholderImageNames;
    NSArray<NSString *> *_placeholderLabelStrings;

    VLCLibraryCollectionViewDelegate *_audioLibraryCollectionViewDelegate;
    VLCLibraryAudioTableViewDelegate *_audioLibraryTableViewDelegate;
    VLCLibraryAudioGroupTableViewDelegate *_audioGroupLibraryTableViewDelegate;
    VLCLibraryTwoPaneSplitViewDelegate *_splitViewDelegate;
}
@end

@implementation VLCLibraryAudioViewController

#pragma mark - Set up the view controller

- (instancetype)initWithLibraryWindow:(VLCLibraryWindow *)libraryWindow
{
    self = [super initWithLibraryWindow:libraryWindow];

    if (self) {
        [self setupPropertiesFromLibraryWindow:libraryWindow];
        [self setupAudioDataSource];

        _audioLibraryCollectionViewDelegate = [[VLCLibraryCollectionViewDelegate alloc] init];
        _audioLibraryTableViewDelegate = [[VLCLibraryAudioTableViewDelegate alloc] init];
        _audioGroupLibraryTableViewDelegate = [[VLCLibraryAudioGroupTableViewDelegate alloc] init];
        _splitViewDelegate = [[VLCLibraryTwoPaneSplitViewDelegate alloc] init];

        [self setupAudioPlaceholderView];
        [self setupAudioCollectionView];
        [self setupGridModeSplitView];
        [self setupAudioTableViews];
        [self setupAudioLibraryViews];

        NSNotificationCenter *notificationCenter = NSNotificationCenter.defaultCenter;
        [notificationCenter addObserver:self
                               selector:@selector(libraryModelUpdated:)
                                   name:VLCLibraryModelAudioMediaListReset
                                 object:nil];
        [notificationCenter addObserver:self
                               selector:@selector(libraryModelUpdated:)
                                   name:VLCLibraryModelAudioMediaItemDeleted
                                 object:nil];

        NSString * const audioMediaResetLongLoadStartNotification = [VLCLibraryModelAudioMediaListReset stringByAppendingString:VLCLongNotificationNameStartSuffix];
        NSString * const audioMediaResetLongLoadFinishNotification = [VLCLibraryModelAudioMediaListReset stringByAppendingString:VLCLongNotificationNameFinishSuffix];
        NSString * const audioMediaDeletedLongLoadStartNotification = [VLCLibraryModelAudioMediaItemDeleted stringByAppendingString:VLCLongNotificationNameStartSuffix];
        NSString * const audioMediaDeletedLongLoadFinishNotification = [VLCLibraryModelAudioMediaItemDeleted stringByAppendingString:VLCLongNotificationNameFinishSuffix];

        [notificationCenter addObserver:self
                               selector:@selector(libraryModelLongLoadStarted:)
                                   name:audioMediaResetLongLoadStartNotification
                                 object:nil];
        [notificationCenter addObserver:self
                               selector:@selector(libraryModelLongLoadFinished:)
                                   name:audioMediaResetLongLoadFinishNotification
                                 object:nil];
        [notificationCenter addObserver:self
                               selector:@selector(libraryModelLongLoadStarted:)
                                   name:audioMediaDeletedLongLoadStartNotification
                                 object:nil];
        [notificationCenter addObserver:self
                               selector:@selector(libraryModelLongLoadFinished:)
                                   name:audioMediaDeletedLongLoadFinishNotification
                                 object:nil];
    }

    return self;
}

- (void)setupPropertiesFromLibraryWindow:(VLCLibraryWindow*)libraryWindow
{
    NSParameterAssert(libraryWindow);

    _audioLibraryView = libraryWindow.audioLibraryView;
    _audioLibrarySplitView = libraryWindow.audioLibrarySplitView;
    _audioCollectionSelectionTableViewScrollView = libraryWindow.audioCollectionSelectionTableViewScrollView;
    _audioCollectionSelectionTableView = libraryWindow.audioCollectionSelectionTableView;
    _audioGroupSelectionTableViewScrollView = libraryWindow.audioGroupSelectionTableViewScrollView;
    _audioGroupSelectionTableView = libraryWindow.audioGroupSelectionTableView;
    _audioSongTableViewScrollView = libraryWindow.audioLibrarySongsTableViewScrollView;
    _audioSongTableView = libraryWindow.audioLibrarySongsTableView;
    _audioCollectionViewScrollView = libraryWindow.audioCollectionViewScrollView;
    _audioLibraryCollectionView = libraryWindow.audioLibraryCollectionView;
    _audioLibraryGridModeSplitView = libraryWindow.audioLibraryGridModeSplitView;
    _audioLibraryGridModeSplitViewListTableViewScrollView = libraryWindow.audioLibraryGridModeSplitViewListTableViewScrollView;
    _audioLibraryGridModeSplitViewListTableView = libraryWindow.audioLibraryGridModeSplitViewListTableView;
    _audioLibraryGridModeSplitViewListSelectionCollectionViewScrollView = libraryWindow.audioLibraryGridModeSplitViewListSelectionCollectionViewScrollView;
    _audioLibraryGridModeSplitViewListSelectionCollectionView = libraryWindow.audioLibraryGridModeSplitViewListSelectionCollectionView;
}

- (void)setupAudioDataSource
{
    _audioDataSource = [[VLCLibraryAudioDataSource alloc] init];
    _audioDataSource.libraryModel = VLCMain.sharedInstance.libraryController.libraryModel;
    _audioDataSource.collectionSelectionTableView = _audioCollectionSelectionTableView;
    _audioDataSource.songsTableView = _audioSongTableView;
    _audioDataSource.collectionView = _audioLibraryCollectionView;
    _audioDataSource.gridModeListTableView = _audioLibraryGridModeSplitViewListTableView;
    _audioDataSource.headerDelegate = self;
    [_audioDataSource setup];

    _audioGroupDataSource = [[VLCLibraryAudioGroupDataSource alloc] init];
    _audioGroupDataSource.tableViews = @[_audioGroupSelectionTableView];
    _audioGroupDataSource.collectionViews = @[_audioLibraryGridModeSplitViewListSelectionCollectionView];
    _audioDataSource.audioGroupDataSource = _audioGroupDataSource;
}

- (void)setupAudioCollectionView
{
    _audioLibraryCollectionView.dataSource = _audioDataSource;
    _audioLibraryCollectionView.delegate = _audioLibraryCollectionViewDelegate;

    _audioLibraryCollectionView.selectable = YES;
    _audioLibraryCollectionView.allowsMultipleSelection = NO;
    _audioLibraryCollectionView.allowsEmptySelection = YES;
    _audioLibraryCollectionView.collectionViewLayout = VLCLibraryCollectionViewFlowLayout.standardLayout;
}

- (void)setupAudioTableViews
{
    _audioLibrarySplitView.delegate = _splitViewDelegate;

    _audioCollectionSelectionTableView.dataSource = _audioDataSource;
    _audioCollectionSelectionTableView.delegate = _audioLibraryTableViewDelegate;

    CGFloat headerHeight = VLCLibraryAudioGroupTableHeaderViewHeight;
    if (@available(macOS 26.0, *)) {
        headerHeight += VLCLibraryUIUnits.largeSpacing * 2;
    }

    const NSRect headerFrame = NSMakeRect(0.f,
                                          0.f,
                                          _audioGroupSelectionTableView.bounds.size.width,
                                          headerHeight);
    _audioCollectionHeaderView = [[VLCLibraryAudioGroupTableHeaderView alloc] initWithFrame:headerFrame];
    _audioCollectionHeaderView.autoresizingMask = NSViewWidthSizable;

    _audioGroupSelectionTableView.headerView = self.audioCollectionHeaderView;
    _audioGroupSelectionTableView.tableColumns.firstObject.headerCell = [VLCLibraryAudioGroupTableHeaderCell new];

    _audioGroupSelectionTableView.dataSource = _audioGroupDataSource;
    _audioGroupSelectionTableView.delegate = _audioGroupLibraryTableViewDelegate;

    if(@available(macOS 11.0, *)) {
        _audioGroupSelectionTableView.style = NSTableViewStyleFullWidth;
    }

    _audioSongTableView.dataSource = _audioDataSource;
    _audioSongTableView.delegate = _audioLibraryTableViewDelegate;

    [_audioDataSource applySelectionForTableView:_audioCollectionSelectionTableView];
}

- (void)setupGridModeSplitView
{
    _audioLibraryGridModeSplitView.delegate = _splitViewDelegate;

    _audioLibraryGridModeSplitViewListTableView.dataSource = _audioDataSource;
    _audioLibraryGridModeSplitViewListTableView.delegate = _audioLibraryTableViewDelegate;

    _audioLibraryGridModeSplitViewListSelectionCollectionView.dataSource = _audioGroupDataSource;
    _audioLibraryGridModeSplitViewListSelectionCollectionView.delegate = _audioLibraryCollectionViewDelegate;

    _audioLibraryGridModeSplitViewListSelectionCollectionView.selectable = YES;
    _audioLibraryGridModeSplitViewListSelectionCollectionView.allowsMultipleSelection = NO;
    _audioLibraryGridModeSplitViewListSelectionCollectionView.allowsEmptySelection = YES;

    VLCLibraryCollectionViewFlowLayout * const audioLibraryGridModeListSelectionCollectionViewLayout =
        VLCLibraryCollectionViewFlowLayout.standardLayout;
    _audioLibraryGridModeSplitViewListSelectionCollectionView.collectionViewLayout = audioLibraryGridModeListSelectionCollectionViewLayout;
    audioLibraryGridModeListSelectionCollectionViewLayout.headerReferenceSize = VLCLibraryAudioGroupHeaderView.defaultHeaderSize;

    if (@available(macOS 10.12, *)) {
        audioLibraryGridModeListSelectionCollectionViewLayout.sectionHeadersPinToVisibleBounds = YES;
    }

    [VLCLibraryAudioDataSource setupCollectionView:_audioLibraryGridModeSplitViewListSelectionCollectionView];
    [VLCLibraryAudioGroupDataSource setupCollectionView:_audioLibraryGridModeSplitViewListSelectionCollectionView];
}

- (void)setupAudioPlaceholderView
{
    _internalPlaceholderImageViewSizeConstraints = @[
        [NSLayoutConstraint constraintWithItem:self.placeholderImageView
                                     attribute:NSLayoutAttributeWidth
                                     relatedBy:NSLayoutRelationEqual
                                        toItem:nil
                                     attribute:NSLayoutAttributeNotAnAttribute
                                    multiplier:0.f
                                      constant:149.f],
        [NSLayoutConstraint constraintWithItem:self.placeholderImageView
                                     attribute:NSLayoutAttributeHeight
                                     relatedBy:NSLayoutRelationEqual
                                        toItem:nil
                                     attribute:NSLayoutAttributeNotAnAttribute
                                    multiplier:0.f
                                      constant:149.f],
    ];

    _placeholderImageNames = @[@"placeholder-group2", @"placeholder-music", @"placeholder-music", @"placeholder-music"];
    _placeholderLabelStrings = @[
        _NS("Your music artists will appear here.\nGo to the Browse section to add artists you love."),
        _NS("Your music albums will appear here.\nGo to the Browse section to add albums you love."),
        _NS("Your music tracks will appear here.\nGo to the Browse section to add tracks you love."),
        _NS("Your music genres will appear here.\nGo to the Browse section to add genres you love."),
    ];
}

- (void)setupAudioLibraryViews
{
    _audioCollectionSelectionTableView.rowHeight = VLCLibraryUIUnits.mediumTableViewRowHeight;
    _audioLibraryGridModeSplitViewListTableView.rowHeight = VLCLibraryUIUnits.mediumTableViewRowHeight;
    _audioGroupSelectionTableView.rowHeight = VLCLibraryAlbumTableCellView.defaultHeight;

    const NSEdgeInsets audioScrollViewContentInsets = VLCLibraryUIUnits.libraryViewScrollViewContentInsets;
    const NSEdgeInsets audioScrollViewScrollerInsets = VLCLibraryUIUnits.libraryViewScrollViewScrollerInsets;

    _audioCollectionViewScrollView.automaticallyAdjustsContentInsets = NO;
    _audioCollectionViewScrollView.contentInsets = audioScrollViewContentInsets;
    _audioCollectionViewScrollView.scrollerInsets = audioScrollViewScrollerInsets;

    _audioCollectionSelectionTableViewScrollView.automaticallyAdjustsContentInsets = NO;
    _audioCollectionSelectionTableViewScrollView.contentInsets = audioScrollViewContentInsets;
    _audioCollectionSelectionTableViewScrollView.scrollerInsets = audioScrollViewScrollerInsets;
    _audioGroupSelectionTableViewScrollView.automaticallyAdjustsContentInsets = NO;
    NSEdgeInsets adjustedInsets = audioScrollViewContentInsets;
    adjustedInsets.top -= VLCLibraryUIUnits.largeSpacing;
    _audioGroupSelectionTableViewScrollView.contentInsets = adjustedInsets;
    _audioGroupSelectionTableViewScrollView.scrollerInsets = audioScrollViewScrollerInsets;

    _audioLibraryGridModeSplitViewListTableViewScrollView.automaticallyAdjustsContentInsets = NO;
    _audioLibraryGridModeSplitViewListTableViewScrollView.contentInsets = audioScrollViewContentInsets;
    _audioLibraryGridModeSplitViewListTableViewScrollView.scrollerInsets = audioScrollViewScrollerInsets;
    _audioLibraryGridModeSplitViewListSelectionCollectionViewScrollView.automaticallyAdjustsContentInsets = NO;
    _audioLibraryGridModeSplitViewListSelectionCollectionViewScrollView.contentInsets = audioScrollViewContentInsets;
    _audioLibraryGridModeSplitViewListSelectionCollectionViewScrollView.scrollerInsets = audioScrollViewScrollerInsets;

    // Songs table view needs bottom padding for controls bar
    const CGFloat controlsBarHeight = VLCLibraryUIUnits.libraryWindowControlsBarHeight;
    const CGFloat controlsBarPadding = VLCLibraryUIUnits.largeSpacing * 2;
    NSClipView * const clipView = _audioSongTableViewScrollView.contentView;
    const CGFloat topInset = self.libraryWindow.titlebarHeight + self.audioSongTableView.headerView.frame.size.height;
    clipView.automaticallyAdjustsContentInsets = NO;
    clipView.contentInsets = NSEdgeInsetsMake(topInset, 0, controlsBarHeight + controlsBarPadding, 0);
}

#pragma mark - Superclass property overrides

- (NSArray<NSLayoutConstraint *> *)placeholderImageViewSizeConstraints
{
    return _internalPlaceholderImageViewSizeConstraints;
}

- (id<VLCLibraryDataSource>)currentDataSource
{
    return self.audioDataSource;
}

#pragma mark - Show the audio view

- (void)presentAudioView
{
    [self updatePresentedView];
}

- (void)presentPlaceholderAudioView
{
    const NSInteger selectedLibrarySegment = self.audioDataSource.audioLibrarySegment;
    NSAssert(selectedLibrarySegment != VLCAudioLibraryRecentsSegment &&
             selectedLibrarySegment != VLCAudioLibraryUnknownSegment,
             @"Received invalid audio library segment from audio data source!");
    [self.libraryWindow displayLibraryPlaceholderViewWithImage:[NSImage imageNamed:_placeholderImageNames[selectedLibrarySegment]]
                                              usingConstraints:self.placeholderImageViewSizeConstraints
                                             displayingMessage:_placeholderLabelStrings[selectedLibrarySegment]];
}

- (void)hideAllViews
{
    _audioLibrarySplitView.hidden = YES;
    _audioLibraryGridModeSplitView.hidden = YES;
    _audioSongTableViewScrollView.hidden = YES;
    _audioCollectionViewScrollView.hidden = YES;
}

- (void)presentAudioGridModeView
{
    const VLCLibrarySegmentType selectedSegment = self.libraryWindow.librarySegmentType;
    if (selectedSegment == VLCLibrarySongsMusicSubSegmentType ||
        selectedSegment == VLCLibraryAlbumsMusicSubSegmentType) {

        [_audioLibraryCollectionView deselectAll:self];
        [(VLCLibraryCollectionViewFlowLayout *)_audioLibraryCollectionView.collectionViewLayout resetLayout];

        _audioCollectionViewScrollView.hidden = NO;
    } else {
        _audioLibraryGridModeSplitView.hidden = NO;
        [_splitViewDelegate resetDefaultSplitForSplitView:self.audioLibraryGridModeSplitView];
    }
}

- (void)presentAudioTableView
{
    if (self.libraryWindow.librarySegmentType == VLCLibrarySongsMusicSubSegmentType) {
        _audioSongTableViewScrollView.hidden = NO;
    } else {
        _audioLibrarySplitView.hidden = NO;
        [_splitViewDelegate resetDefaultSplitForSplitView:self.audioLibrarySplitView];
    }
}

- (VLCLibraryViewModeSegment)viewModeSegmentForCurrentLibrarySegment
{
    VLCLibraryWindowPersistentPreferences * const libraryWindowPrefs = VLCLibraryWindowPersistentPreferences.sharedInstance;

    switch (self.libraryWindow.librarySegmentType) {
        case VLCLibraryArtistsMusicSubSegmentType:
            return libraryWindowPrefs.artistLibraryViewMode;
        case VLCLibraryGenresMusicSubSegmentType:
            return libraryWindowPrefs.genreLibraryViewMode;
        case VLCLibrarySongsMusicSubSegmentType:
            return libraryWindowPrefs.songsLibraryViewMode;
        case VLCLibraryAlbumsMusicSubSegmentType:
            return libraryWindowPrefs.albumLibraryViewMode;
        default:
            return VLCLibraryGridViewModeSegment;
    }
}

- (VLCAudioLibrarySegment)currentLibrarySegmentToAudioLibrarySegment
{
    switch (self.libraryWindow.librarySegmentType) {
        case VLCLibraryMusicSegmentType:
        case VLCLibraryArtistsMusicSubSegmentType:
            return VLCAudioLibraryArtistsSegment;
        case VLCLibraryAlbumsMusicSubSegmentType:
            return VLCAudioLibraryAlbumsSegment;
        case VLCLibrarySongsMusicSubSegmentType:
            return VLCAudioLibrarySongsSegment;
        case VLCLibraryGenresMusicSubSegmentType:
            return VLCAudioLibraryGenresSegment;
        default:
            NSAssert(false, @"Non-audio or unknown library subsegment received");
            return VLCAudioLibraryUnknownSegment;
    }
}

- (void)updatePresentedView
{
    self.audioDataSource.audioLibrarySegment = [self currentLibrarySegmentToAudioLibrarySegment];
    const BOOL anyAudioMedia = self.audioDataSource.libraryModel.numberOfAudioMedia > 0;

    if (anyAudioMedia) {
        [self.libraryWindow displayLibraryView:self.audioLibraryView];
        [self hideAllViews];

        const VLCLibraryViewModeSegment viewModeSegment = [self viewModeSegmentForCurrentLibrarySegment];

        if (viewModeSegment == VLCLibraryListViewModeSegment) {
            [self presentAudioTableView];
        } else if (viewModeSegment == VLCLibraryGridViewModeSegment) {
            [self presentAudioGridModeView];
        } else {
            NSAssert(false, @"View mode must be grid or list mode");
        }

        [VLCMain.sharedInstance.libraryWindow updateGridVsListViewModeSegmentedControl];
    } else if (self.audioDataSource.libraryModel.filterString.length > 0) {
        [self.libraryWindow displayNoResultsMessage];
    } else {
        [self presentPlaceholderAudioView];
    }
}

- (void)reloadData
{
    [_audioDataSource reloadData];
}

- (void)presentLibraryItemInTableView:(id<VLCMediaLibraryItemProtocol>)libraryItem
{
    if (libraryItem == nil) {
        return;
    }

    NSTableView *targetMainTableView;
    if ([libraryItem isKindOfClass:VLCMediaLibraryMediaItem.class]) {
        targetMainTableView = self.audioSongTableView;
    } else if ([self viewModeSegmentForCurrentLibrarySegment] == VLCLibraryGridViewModeSegment) {
        targetMainTableView = self.audioLibraryGridModeSplitViewListTableView;
    } else {
        targetMainTableView = self.audioCollectionSelectionTableView;
    }
    NSAssert(targetMainTableView != nil, @"Target tableview for presenting audio library view is nil");
    NSAssert(targetMainTableView.dataSource == self.audioDataSource, @"Target tableview data source is unexpected");

    const NSInteger rowForLibraryItem = [self.audioDataSource rowForLibraryItem:libraryItem];
    if (rowForLibraryItem != NSNotFound) {
        NSIndexSet * const indexSet = [NSIndexSet indexSetWithIndex:rowForLibraryItem];
        [self.audioDataSource tableView:targetMainTableView selectRowIndices:indexSet];
    }
}

- (void)presentLibraryItemInCollectionView:(id<VLCMediaLibraryItemProtocol>)libraryItem
{
    if (libraryItem == nil) {
        return;
    }

    // If we are handling a library item that is shown in one of the split view modes,
    // with a table view on the left and the collection view on the right, defer back
    // to presentLibraryItemInTabelView
    if ([libraryItem isKindOfClass:VLCMediaLibraryGenre.class] ||
        [libraryItem isKindOfClass:VLCMediaLibraryArtist.class]) {
        [self presentLibraryItemInTableView:libraryItem];
    }

    NSIndexPath * const indexPathForLibraryItem = [self.audioDataSource indexPathForLibraryItem:libraryItem];
    if (indexPathForLibraryItem) {
        NSSet<NSIndexPath *> * const indexPathSet = [NSSet setWithObject:indexPathForLibraryItem];
        NSCollectionView * const collectionView = self.audioLibraryCollectionView;
        VLCLibraryCollectionViewFlowLayout * const expandableFlowLayout = (VLCLibraryCollectionViewFlowLayout *)collectionView.collectionViewLayout;

        [collectionView selectItemsAtIndexPaths:indexPathSet
                                 scrollPosition:NSCollectionViewScrollPositionTop];
        [expandableFlowLayout expandDetailSectionAtIndex:indexPathForLibraryItem];
    }
}

- (void)presentLibraryItemWaitForDataSourceFinished:(nullable NSNotification *)aNotification
{
    if ((NSUInteger)self.audioDataSource.displayedCollectionCount < self.audioDataSource.collectionToDisplayCount) {
        return;
    }
    
    [NSNotificationCenter.defaultCenter removeObserver:self
                                                  name:VLCLibraryAudioDataSourceDisplayedCollectionChangedNotification
                                                object:self.audioDataSource];

    const VLCLibraryViewModeSegment viewModeSegment = [self viewModeSegmentForCurrentLibrarySegment];
    if (viewModeSegment == VLCLibraryListViewModeSegment) {
        [self presentLibraryItemInTableView:_awaitingPresentingLibraryItem];
    } else if (viewModeSegment == VLCLibraryGridViewModeSegment) {
        [self presentLibraryItemInCollectionView:_awaitingPresentingLibraryItem];
    } else {
        NSAssert(NO, @"No valid view mode segment acquired, cannot present item!");
    }

    _awaitingPresentingLibraryItem = nil;
}

- (void)presentLibraryItem:(id<VLCMediaLibraryItemProtocol>)libraryItem
{
    if (libraryItem == nil) {
        return;
    }

    _awaitingPresentingLibraryItem = libraryItem;

    // If the library item is a media item, we need to select the corresponding segment
    // in the segmented control. We then need to update the presented view.
    VLCLibrarySegmentType segmentType;
    if ([libraryItem isKindOfClass:VLCMediaLibraryAlbum.class]) {
        segmentType = VLCLibraryAlbumsMusicSubSegmentType;
    } else if ([libraryItem isKindOfClass:VLCMediaLibraryArtist.class]) {
        segmentType = VLCLibraryArtistsMusicSubSegmentType;
    } else if ([libraryItem isKindOfClass:VLCMediaLibraryGenre.class]) {
        segmentType = VLCLibraryGenresMusicSubSegmentType;
    } else {
        segmentType = VLCLibrarySongsMusicSubSegmentType;
    }

    VLCLibraryWindow * const libraryWindow = self.libraryWindow;
    if (segmentType == libraryWindow.librarySegmentType) {
        [self presentLibraryItemWaitForDataSourceFinished:nil];
        return;
    }

    [NSNotificationCenter.defaultCenter addObserver:self
                                           selector:@selector(presentLibraryItemWaitForDataSourceFinished:)
                                               name:VLCLibraryAudioDataSourceDisplayedCollectionChangedNotification
                                             object:self.audioDataSource];

    libraryWindow.librarySegmentType = segmentType;
    [libraryWindow.splitViewController.navSidebarViewController selectSegment:segmentType];
}

- (void)libraryModelUpdated:(NSNotification *)aNotification
{
    VLCLibraryModel * const model = VLCMain.sharedInstance.libraryController.libraryModel;
    const NSUInteger audioCount = model.numberOfAudioMedia;

    if ((self.libraryWindow.librarySegmentType == VLCLibraryMusicSegmentType ||
         self.libraryWindow.librarySegmentType == VLCLibrarySongsMusicSubSegmentType ||
         self.libraryWindow.librarySegmentType == VLCLibraryArtistsMusicSubSegmentType ||
         self.libraryWindow.librarySegmentType == VLCLibraryAlbumsMusicSubSegmentType ||
         self.libraryWindow.librarySegmentType == VLCLibraryGenresMusicSubSegmentType) &&
        ((audioCount == 0 && ![self.libraryTargetView.subviews containsObject:self.emptyLibraryView]) ||
         (audioCount > 0 && ![self.libraryTargetView.subviews containsObject:_audioLibraryView])) &&
        !self.libraryWindow.embeddedVideoPlaybackActive) {

        [self updatePresentedView];
    }
}

- (void)libraryModelLongLoadStarted:(NSNotification *)notification
{
    if (self.connected) {
        [self.audioDataSource disconnect];
        [self.audioGroupDataSource disconnect];
    }

    [self.libraryWindow showLoadingOverlay];
}

- (void)libraryModelLongLoadFinished:(NSNotification *)notification
{
    if (self.connected) {
        [self.audioDataSource connect];
        [self.audioGroupDataSource connect];
    }

    [self.libraryWindow hideLoadingOverlay];
}

- (void)audioDataSource:(VLCLibraryAudioDataSource *)dataSource
updateHeaderForTableView:(NSTableView *)tableView
    withRepresentedItem:(VLCLibraryRepresentedItem *)representedItem
          fallbackTitle:(NSString *)fallbackTitle
         fallbackDetail:(NSString *)fallbackDetail
{
    if (tableView != self.audioCollectionSelectionTableView &&
        tableView != self.audioGroupSelectionTableView &&
        ![self.audioGroupDataSource.tableViews containsObject:tableView])
        return;

    if (representedItem != nil) {
        self.audioCollectionHeaderView.representedItem = representedItem;
    } else {
        [self.audioCollectionHeaderView updateWithRepresentedItem:nil
                                                    fallbackTitle:fallbackTitle
                                                   fallbackDetail:fallbackDetail];
    }
}

@end
