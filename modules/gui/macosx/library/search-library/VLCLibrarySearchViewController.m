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

#import "library/audio-library/VLCLibraryCollectionViewAudioGroupSupplementaryDetailView.h"
#import "library/VLCLibraryController.h"
#import "library/VLCLibraryModel.h"
#import "library/VLCLibraryTableCellView.h"
#import "library/VLCLibraryTableView.h"
#import "library/VLCLibraryUIUnits.h"

#import "library/VLCLibrarySectionedTableViewDelegate.h"
#import "library/VLCLibraryWindow.h"
#import "library/VLCLibraryWindowPersistentPreferences.h"

#import "main/VLCMain.h"

@interface VLCLibrarySearchViewController ()

@property (readwrite) VLCLibraryCollectionViewDelegate *collectionViewDelegate;
@property (readwrite) VLCLibraryCollectionViewFlowLayout *collectionViewLayout;
@property (readwrite) VLCLibrarySectionedTableViewDelegate *tableViewDelegate;
@property (readwrite) NSArray<NSLayoutConstraint *> *internalPlaceholderImageViewSizeConstraints;
@property (readwrite, nullable) NSView *presentationContainer;
@property (readwrite, copy, nullable) NSString *currentQuery;
@property (readwrite) NSView *backgroundView;

@end

@implementation VLCLibrarySearchViewController

- (instancetype)initWithLibraryWindow:(VLCLibraryWindow *)libraryWindow
{
    self = [super initWithLibraryWindow:libraryWindow];
    if (self) {
        _dataSource = [[VLCLibrarySearchDataSource alloc] init];
        [self setupBackgroundView];
        [self setupStatusLabel];
        [self setupCollectionView];
        [self setupTableView];
        [self setupPlaceholderView];
    }
    return self;
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
    self.collectionViewDelegate.staticItemSize = VLCLibraryCollectionViewItem.defaultSize;
    self.collectionView.delegate = self.collectionViewDelegate;

    [self.collectionView registerClass:VLCLibraryCollectionViewItem.class
                 forItemWithIdentifier:VLCLibraryCellIdentifier];

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
    if (self.presentationContainer == container) {
        return;
    }

    if (self.presentationContainer != nil) {
        [self.backgroundView removeFromSuperview];
        [self.collectionViewScrollView removeFromSuperview];
        [self.tableViewScrollView removeFromSuperview];
        [self.statusLabel removeFromSuperview];
    }

    self.dataSource.collectionView = self.collectionView;
    self.dataSource.tableView = self.tableView;
    self.collectionView.dataSource = self.dataSource;
    self.tableView.dataSource = self.dataSource;
    self.tableView.delegate = self.tableViewDelegate;

    self.presentationContainer = container;
    [container addSubview:self.backgroundView];
    [container addSubview:self.collectionViewScrollView];
    [container addSubview:self.tableViewScrollView];
    [container addSubview:self.statusLabel];

    [self.backgroundView applyConstraintsToFillSuperview];
    [self.collectionViewScrollView applyConstraintsToFillSuperview];
    [self.tableViewScrollView applyConstraintsToFillSuperview];

    [NSLayoutConstraint activateConstraints:@[
        [self.statusLabel.centerXAnchor constraintEqualToAnchor:container.centerXAnchor],
        [self.statusLabel.centerYAnchor constraintEqualToAnchor:container.centerYAnchor],
    ]];

    // Cover the container immediately so the home view (or whatever is below) is
    // not visible. The caller is expected to follow up with `searchForString:` or
    // `updatePresentationForQuery:` to refine the visibility (results vs. status
    // label text) based on the current query and data source state.
    self.collectionViewScrollView.hidden = YES;
    self.tableViewScrollView.hidden = YES;
    self.statusLabel.hidden = YES;
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
    self.presentationContainer = nil;
}

- (void)updatePresentationForQuery:(NSString *)query
{
    NSParameterAssert(query);

    if (self.presentationContainer == nil) {
        return;
    }

    VLCLibraryModel * const libraryModel = VLCMain.sharedInstance.libraryController.libraryModel;
    const BOOL emptyLibrary =
        libraryModel.numberOfAudioMedia == 0 && libraryModel.numberOfVideoMedia == 0;

    if (emptyLibrary) {
        self.collectionViewScrollView.hidden = YES;
        self.tableViewScrollView.hidden = YES;
        self.statusLabel.hidden = NO;
        self.statusLabel.stringValue = _NS("Your library is empty.\nAdd media to start searching.");
        return;
    }

    const VLCLibraryViewModeSegment viewMode =
        VLCLibraryWindowPersistentPreferences.sharedInstance.searchLibraryViewMode;
    const BOOL gridMode = (viewMode == VLCLibraryGridViewModeSegment);

    const BOOL hasSearchText = query.length > 0;
    const BOOL hasResults = hasSearchText && [self hasAnyResults];

    // Show results as soon as any provider has reported matches, even if more are
    // still on the way. The status label is shown whenever there are no results to
    // display, with the wording depending on whether the user has typed anything.
    const BOOL showResults = hasResults;

    self.collectionViewScrollView.hidden = !(showResults && gridMode);
    self.tableViewScrollView.hidden = !(showResults && !gridMode);
    self.statusLabel.hidden = showResults;
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
    [self.dataSource searchForString:searchString];
    [self updatePresentationForQuery:searchString];
}

- (void)clearSearch
{
    self.currentQuery = nil;
    [self.dataSource clearSearch];
    [self updatePresentationForQuery:@""];
}

@end
