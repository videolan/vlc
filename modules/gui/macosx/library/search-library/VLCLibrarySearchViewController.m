/*****************************************************************************
 * VLCLibrarySearchViewController.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2025 VLC authors and VideoLAN
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

#import "VLCLibrarySearchViewController.h"

#import "VLCLibrarySearchDataSource.h"
#import "VLCLibrarySearchProvider.h"

#import "extensions/NSFont+VLCAdditions.h"
#import "extensions/NSImage+VLCAdditions.h"
#import "extensions/NSScrollView+VLCAdditions.h"
#import "extensions/NSString+Helpers.h"
#import "extensions/NSView+VLCAdditions.h"

#import "library/VLCLibraryCollectionView.h"
#import "library/VLCLibraryCollectionViewDelegate.h"
#import "library/VLCLibraryCollectionViewFlowLayout.h"
#import "library/VLCLibraryCollectionViewItem.h"
#import "library/VLCLibraryCollectionViewMediaItemListSupplementaryDetailView.h"
#import "library/VLCLibraryCollectionViewMediaItemSupplementaryDetailView.h"
#import "library/VLCLibraryCollectionViewSupplementaryElementView.h"
#import "library/VLCLibraryController.h"
#import "library/VLCLibraryModel.h"
#import "library/VLCLibrarySectionedTableViewDelegate.h"
#import "library/VLCLibraryTableCellView.h"
#import "library/VLCLibraryTableView.h"
#import "library/VLCLibraryUIUnits.h"
#import "library/VLCLibraryWindow.h"
#import "library/VLCLibraryWindowPersistentPreferences.h"

#import "library/audio-library/VLCLibraryCollectionViewAudioGroupSupplementaryDetailView.h"

#import "main/VLCMain.h"

@interface VLCLibrarySearchViewController ()

@property (readwrite) VLCLibraryCollectionViewDelegate *collectionViewDelegate;
@property (readwrite) VLCLibraryCollectionViewFlowLayout *collectionViewLayout;
@property (readwrite) VLCLibrarySectionedTableViewDelegate *tableViewDelegate;
@property (readwrite) NSArray<NSLayoutConstraint *> *internalPlaceholderImageViewSizeConstraints;
@property (readwrite, nullable) NSView *presentationContainer;
@property (readwrite, copy, nullable) NSString *currentQuery;
@property (readwrite) NSView *backgroundView;
@property (readwrite, nullable) NSTimer *searchDebounceTimer;

@end

@implementation VLCLibrarySearchViewController

- (instancetype)initWithLibraryWindow:(VLCLibraryWindow *)libraryWindow
{
    self = [super initWithLibraryWindow:libraryWindow];
    if (self) {
        _dataSource = [[VLCLibrarySearchDataSource alloc] init];
        self.dataSource.viewMode = VLCLibrarySmallestSentinelViewModeSegment;

        [self setupBackgroundView];
        [self setupStatusLabel];
        [self setupCollectionView];
        [self setupTableView];
        [self setupPlaceholderView];

        [NSNotificationCenter.defaultCenter addObserver:self
                                               selector:@selector(dataSourceDidReload:)
                                                   name:VLCLibrarySearchDataSourceDidReloadNotification
                                                 object:self.dataSource];
    }
    return self;
}

- (void)dealloc
{
    [NSNotificationCenter.defaultCenter removeObserver:self];
}

- (void)dataSourceDidReload:(NSNotification *)notification
{
    [self updatePresentationForQuery:self.currentQuery ?: @""];
}

#pragma mark - Setup

- (void)setupBackgroundView
{
    _backgroundView = [[NSView alloc] init];
    self.backgroundView.translatesAutoresizingMaskIntoConstraints = NO;
    self.backgroundView.wantsLayer = YES;
    self.backgroundView.layer.backgroundColor = NSColor.windowBackgroundColor.CGColor;
}

- (void)setupStatusLabel
{
    _statusLabel = [NSTextField labelWithString:_NS("Search your library")];
    self.statusLabel.translatesAutoresizingMaskIntoConstraints = NO;
    self.statusLabel.font = NSFont.VLClibrarySectionHeaderFont;
    self.statusLabel.alignment = NSTextAlignmentCenter;
}

- (void)setupCollectionView
{
    _collectionViewLayout = [[VLCLibraryCollectionViewFlowLayout alloc] init];
    const CGFloat collectionItemSpacing = VLCLibraryUIUnits.collectionViewItemSpacing;
    const NSEdgeInsets collectionViewSectionInset = VLCLibraryUIUnits.collectionViewSectionInsets;
    self.collectionViewLayout.headerReferenceSize = VLCLibraryCollectionViewSupplementaryElementView.defaultHeaderSize;
    self.collectionViewLayout.minimumLineSpacing = collectionItemSpacing;
    self.collectionViewLayout.minimumInteritemSpacing = collectionItemSpacing;
    self.collectionViewLayout.sectionInset = collectionViewSectionInset;

    _collectionView = [[VLCLibraryCollectionView alloc] initWithFrame:NSZeroRect];
    self.collectionView.collectionViewLayout = self.collectionViewLayout;
    self.collectionView.selectable = YES;
    self.collectionView.allowsEmptySelection = YES;
    self.collectionView.allowsMultipleSelection = YES;

    _collectionViewDelegate = [[VLCLibraryCollectionViewDelegate alloc] init];
    self.collectionViewDelegate.itemsAspectRatio = VLCLibraryCollectionViewItemAspectRatioDefaultItem;
    self.collectionViewDelegate.staticItemSize = VLCLibraryUIUnits.defaultMediaItemCollectionViewItemSize;
    self.collectionView.delegate = self.collectionViewDelegate;

    [self.collectionView registerClass:VLCLibraryCollectionViewItem.class
                 forItemWithIdentifier:VLCLibraryCollectionViewItemIdentifier];

    [self.collectionView registerClass:VLCLibraryCollectionViewSupplementaryElementView.class
            forSupplementaryViewOfKind:NSCollectionElementKindSectionHeader
                        withIdentifier:VLCLibrarySupplementaryElementViewIdentifier];

    NSString * const mediaItemDetailViewString =
        NSStringFromClass(VLCLibraryCollectionViewMediaItemSupplementaryDetailView.class);
    NSNib * const mediaItemDetailViewNib =
        [[NSNib alloc] initWithNibNamed:mediaItemDetailViewString bundle:nil];
    [self.collectionView registerNib:mediaItemDetailViewNib
         forSupplementaryViewOfKind:VLCLibraryCollectionViewMediaItemSupplementaryDetailViewKind
                     withIdentifier:VLCLibraryCollectionViewMediaItemSupplementaryDetailViewIdentifier];

    NSString * const albumDetailViewString =
        NSStringFromClass(VLCLibraryCollectionViewMediaItemListSupplementaryDetailView.class);
    NSNib * const albumDetailViewNib =
        [[NSNib alloc] initWithNibNamed:albumDetailViewString bundle:nil];
    [self.collectionView registerNib:albumDetailViewNib
         forSupplementaryViewOfKind:VLCLibraryCollectionViewMediaItemListSupplementaryDetailViewKind
                     withIdentifier:VLCLibraryCollectionViewMediaItemListSupplementaryDetailViewKind];

    NSString * const audioGroupDetailViewString =
        NSStringFromClass(VLCLibraryCollectionViewAudioGroupSupplementaryDetailView.class);
    NSNib * const audioGroupDetailViewNib =
        [[NSNib alloc] initWithNibNamed:audioGroupDetailViewString bundle:nil];
    [self.collectionView registerNib:audioGroupDetailViewNib
         forSupplementaryViewOfKind:VLCLibraryCollectionViewAudioGroupSupplementaryDetailViewKind
                     withIdentifier:VLCLibraryCollectionViewAudioGroupSupplementaryDetailViewKind];

    _collectionViewScrollView = [NSScrollView libraryScrollViewWithDocumentView:self.collectionView
                                                                  contentInsets:VLCLibraryUIUnits.libraryViewScrollViewContentInsets];
    
    self.dataSource.collectionView = self.collectionView;
    self.collectionView.dataSource = self.dataSource;
}

- (void)setupTableView
{
    _tableView = [[VLCLibraryTableView alloc] init];
    self.tableView.headerView = nil;
    self.tableView.rowHeight = VLCLibraryUIUnits.mediumTableViewRowHeight;
    self.tableView.allowsMultipleSelection = YES;
    self.tableView.floatsGroupRows = NO;

    NSTableColumn * const column =
        [[NSTableColumn alloc] initWithIdentifier:@"VLCSearchLibraryTableViewColumnIdentifier"];
    column.resizingMask = NSTableColumnAutoresizingMask;
    [self.tableView addTableColumn:column];

    NSNib * const tableCellViewNib =
        [[NSNib alloc] initWithNibNamed:NSStringFromClass(VLCLibraryTableCellView.class) bundle:nil];
    [self.tableView registerNib:tableCellViewNib
                  forIdentifier:VLCLibraryTableCellViewIdentifier];

    _tableViewDelegate = [[VLCLibrarySectionedTableViewDelegate alloc] init];
    self.tableView.delegate = self.tableViewDelegate;

    _tableViewScrollView = [NSScrollView libraryScrollViewWithDocumentView:self.tableView
                                                             contentInsets:VLCLibraryUIUnits.libraryViewScrollViewContentInsets];

    self.tableView.dataSource = self.dataSource;
    self.tableView.delegate = self.tableViewDelegate;
    self.dataSource.tableView = self.tableView;
}

- (void)setupPlaceholderView
{
    _internalPlaceholderImageViewSizeConstraints = @[
        [NSLayoutConstraint constraintWithItem:self.placeholderImageView
                                     attribute:NSLayoutAttributeWidth
                                     relatedBy:NSLayoutRelationEqual
                                        toItem:nil
                                     attribute:NSLayoutAttributeNotAnAttribute
                                    multiplier:0.f
                                      constant:182.f],
        [NSLayoutConstraint constraintWithItem:self.placeholderImageView
                                     attribute:NSLayoutAttributeHeight
                                     relatedBy:NSLayoutRelationEqual
                                        toItem:nil
                                     attribute:NSLayoutAttributeNotAnAttribute
                                    multiplier:0.f
                                      constant:182.f],
    ];
}

#pragma mark - Abstract overrides

- (NSArray<NSLayoutConstraint *> *)placeholderImageViewSizeConstraints
{
    return self.internalPlaceholderImageViewSizeConstraints;
}

- (id<VLCLibraryDataSource>)currentDataSource
{
    return self.dataSource;
}

#pragma mark - Presentation

- (void)presentInContainer:(NSView *)container
{
    NSParameterAssert(container);
    const VLCLibraryViewModeSegment viewMode =
        VLCLibraryWindowPersistentPreferences.sharedInstance.searchLibraryViewMode;
    if (self.presentationContainer == container && self.dataSource.viewMode == viewMode) {
        return;
    }

    if (self.presentationContainer != container) {
        [self.backgroundView removeFromSuperview];
        [self.collectionViewScrollView removeFromSuperview];
        [self.tableViewScrollView removeFromSuperview];
        [self.statusLabel removeFromSuperview];

        [container addSubview:self.backgroundView];
        [self.backgroundView applyConstraintsToFillSuperview];

        [container addSubview:self.statusLabel];
        [NSLayoutConstraint activateConstraints:@[
            [self.statusLabel.centerXAnchor constraintEqualToAnchor:container.centerXAnchor],
            [self.statusLabel.centerYAnchor constraintEqualToAnchor:container.centerYAnchor],
        ]];
        self.statusLabel.hidden = YES;
    }

    if (self.dataSource.viewMode != viewMode || self.presentationContainer != container) {
        if (viewMode == VLCLibraryGridViewModeSegment) {
            [self.tableViewScrollView removeFromSuperview];
            [container addSubview:self.collectionViewScrollView positioned:NSWindowBelow relativeTo:self.statusLabel];
            [self.collectionViewScrollView applyConstraintsToFillSuperview];
        } else if (viewMode == VLCLibraryListViewModeSegment) {
            [self.collectionViewScrollView removeFromSuperview];
            [container addSubview:self.tableViewScrollView positioned:NSWindowBelow relativeTo:self.statusLabel];
            [self.tableViewScrollView applyConstraintsToFillSuperview];
        }
        self.dataSource.viewMode = viewMode;
        [self updatePresentationForQuery:self.currentQuery ?: @""];
    }

    self.presentationContainer = container;
}

- (void)dismissFromContainer
{
    if (self.presentationContainer == nil) {
        return;
    }
    [self.backgroundView removeFromSuperview];
    [self.collectionViewScrollView removeFromSuperview];
    [self.tableViewScrollView removeFromSuperview];
    [self.statusLabel removeFromSuperview];
    [self.searchDebounceTimer invalidate];
    self.searchDebounceTimer = nil;
    self.presentationContainer = nil;
}

- (void)updatePresentationForQuery:(NSString *)query
{
    NSParameterAssert(query);

    if (self.presentationContainer == nil) {
        return;
    }

    const BOOL hasSearchText = query.length > 0;
    const BOOL hasResults = hasSearchText && [self hasAnyResults];

    self.statusLabel.hidden = hasResults;
    if (!self.statusLabel.hidden) {
        self.statusLabel.stringValue = hasSearchText
            ? _NS("No results")
            : _NS("Search your library");
    }
}

- (BOOL)hasAnyResults
{
    const NSInteger sectionCount =
        [self.dataSource numberOfSectionsInCollectionView:self.collectionView];
    for (NSInteger i = 0; i < sectionCount; i++) {
        if ([self.dataSource collectionView:self.collectionView numberOfItemsInSection:i] > 0) {
            return YES;
        }
    }
    return NO;
}

#pragma mark - Search driving

- (void)searchForString:(NSString *)searchString
{
    NSParameterAssert(searchString);
    if (searchString.length == 0) {
        [self clearSearch];
        return;
    }

    self.currentQuery = searchString;
    [self updatePresentationForQuery:searchString];

    [self.searchDebounceTimer invalidate];
    self.searchDebounceTimer = [NSTimer scheduledTimerWithTimeInterval:0.15
                                                               target:self
                                                             selector:@selector(performPendingSearch)
                                                             userInfo:nil
                                                              repeats:NO];
}

- (void)performPendingSearch
{
    self.searchDebounceTimer = nil;
    if (self.currentQuery.length == 0) {
        return;
    }
    [self.dataSource searchForString:self.currentQuery];
}

- (void)clearSearch
{
    [self.searchDebounceTimer invalidate];
    self.searchDebounceTimer = nil;
    self.currentQuery = nil;
    [self.dataSource clearSearch];
    [self updatePresentationForQuery:@""];
}

@end
