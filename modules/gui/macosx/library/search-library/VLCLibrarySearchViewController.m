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
#import "extensions/NSString+Helpers.h"

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

static const NSTimeInterval VLCLibrarySearchDebounceInterval = 0.3;
static const NSTimeInterval VLCLibrarySearchSpinnerFade = 0.25;

@interface VLCLibrarySearchViewController () <NSSearchFieldDelegate>

@property (readwrite) VLCLibraryCollectionViewDelegate *collectionViewDelegate;
@property (readwrite) VLCLibraryCollectionViewFlowLayout *collectionViewLayout;
@property (readwrite) VLCLibrarySectionedTableViewDelegate *tableViewDelegate;
@property (readwrite) NSTimer *searchDebounceTimer;
@property (readwrite) BOOL spinnerAnimating;
@property (readwrite) NSArray<NSLayoutConstraint *> *internalPlaceholderImageViewSizeConstraints;

@end

@implementation VLCLibrarySearchViewController

- (instancetype)initWithLibraryWindow:(VLCLibraryWindow *)libraryWindow
{
    self = [super initWithLibraryWindow:libraryWindow];
    if (self) {
        _dataSource = [[VLCLibrarySearchDataSource alloc] init];
        [self setupSearchField];
        [self setupStatusLabel];
        [self setupCollectionView];
        [self setupTableView];
        [self setupPlaceholderView];

        [NSNotificationCenter.defaultCenter addObserver:self
                                               selector:@selector(searchProviderResultsUpdated:)
                                                   name:VLCLibrarySearchProviderResultsUpdated
                                                 object:nil];
    }
    return self;
}

#pragma mark - Setup

- (void)setupSearchField
{
    _searchField = [[NSSearchField alloc] init];
    self.searchField.translatesAutoresizingMaskIntoConstraints = NO;
    self.searchField.placeholderString = _NS("Search the library");
    self.searchField.delegate = self;
    self.searchField.sendsSearchStringImmediately = NO;
    self.searchField.sendsWholeSearchString = NO;
}

- (void)setupStatusLabel
{
    _statusLabel = [NSTextField labelWithString:_NS("Search your library")];
    self.statusLabel.translatesAutoresizingMaskIntoConstraints = NO;
    self.statusLabel.font = NSFont.VLClibrarySectionHeaderFont;
    self.statusLabel.alignment = NSTextAlignmentCenter;

    _spinner = [[NSProgressIndicator alloc] init];
    self.spinner.translatesAutoresizingMaskIntoConstraints = NO;
    self.spinner.style = NSProgressIndicatorStyleSpinning;
    self.spinner.displayedWhenStopped = NO;
    self.spinner.wantsLayer = YES;
    self.spinner.alphaValue = 0.0;
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
                     withIdentifier:VLCLibraryCollectionViewAudioGroupSupplementaryDetailViewIdentifier];

    const CGFloat searchFieldAreaHeight =
        self.searchField.intrinsicContentSize.height + VLCLibraryUIUnits.largeSpacing * 2;

    _collectionViewScrollView = [[NSScrollView alloc] init];
    self.collectionViewScrollView.translatesAutoresizingMaskIntoConstraints = NO;
    self.collectionViewScrollView.hasHorizontalScroller = NO;
    self.collectionViewScrollView.borderType = NSNoBorder;
    self.collectionViewScrollView.documentView = self.collectionView;
    self.collectionViewScrollView.automaticallyAdjustsContentInsets = NO;

    NSEdgeInsets collectionInsets = VLCLibraryUIUnits.libraryViewScrollViewContentInsets;
    collectionInsets.top += searchFieldAreaHeight;
    self.collectionViewScrollView.contentInsets = collectionInsets;

    NSEdgeInsets collectionScrollerInsets = VLCLibraryUIUnits.libraryViewScrollViewScrollerInsets;
    collectionScrollerInsets.top -= searchFieldAreaHeight;
    self.collectionViewScrollView.scrollerInsets = collectionScrollerInsets;
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

    _tableViewScrollView = [[NSScrollView alloc] init];
    self.tableViewScrollView.translatesAutoresizingMaskIntoConstraints = NO;
    self.tableViewScrollView.hasHorizontalScroller = NO;
    self.tableViewScrollView.borderType = NSNoBorder;
    self.tableViewScrollView.documentView = self.tableView;
    self.tableViewScrollView.automaticallyAdjustsContentInsets = NO;

    const CGFloat searchFieldAreaHeight =
        self.searchField.intrinsicContentSize.height + VLCLibraryUIUnits.largeSpacing * 2;

    NSEdgeInsets tableInsets = VLCLibraryUIUnits.libraryViewScrollViewContentInsets;
    tableInsets.top += searchFieldAreaHeight;
    self.tableViewScrollView.contentInsets = tableInsets;

    NSEdgeInsets tableScrollerInsets = VLCLibraryUIUnits.libraryViewScrollViewScrollerInsets;
    tableScrollerInsets.top -= searchFieldAreaHeight;
    self.tableViewScrollView.scrollerInsets = tableScrollerInsets;
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

- (void)presentSearchView
{
    VLCLibraryModel * const libraryModel = VLCMain.sharedInstance.libraryController.libraryModel;
    const BOOL emptyLibrary =
        libraryModel.numberOfAudioMedia == 0 && libraryModel.numberOfVideoMedia == 0;

    if (emptyLibrary) {
        [self.libraryWindow displayLibraryPlaceholderViewWithImage:NSImage.VLCGenericImage
                                                  usingConstraints:self.placeholderImageViewSizeConstraints
                                                 displayingMessage:_NS("Your library is empty.\nAdd media to start searching.")];
        return;
    }

    self.dataSource.collectionView = self.collectionView;
    self.dataSource.tableView = self.tableView;

    self.collectionView.dataSource = self.dataSource;
    self.tableView.dataSource = self.dataSource;
    self.tableView.delegate = self.tableViewDelegate;

    const VLCLibraryViewModeSegment viewMode =
        VLCLibraryWindowPersistentPreferences.sharedInstance.searchLibraryViewMode;
    const BOOL gridMode = (viewMode == VLCLibraryGridViewModeSegment);

    const BOOL hasSearchText = self.searchField.stringValue.length > 0;
    const BOOL isSearching = self.dataSource.searching;
    const BOOL hasResults = hasSearchText && [self hasAnyResults];
    const BOOL showResults = hasSearchText && hasResults && !isSearching;

    // Add all views once, then toggle visibility
    if (self.searchField.superview != self.libraryTargetView) {
        const CGFloat spacing = VLCLibraryUIUnits.largeSpacing;
        NSLayoutAnchor *topAnchor = self.libraryTargetView.topAnchor;
        if (@available(macOS 11.0, *)) {
            topAnchor = self.libraryTargetView.safeAreaLayoutGuide.topAnchor;
        }

        self.libraryTargetView.subviews = @[];
        [self.libraryTargetView addSubview:self.collectionViewScrollView];
        [self.libraryTargetView addSubview:self.tableViewScrollView];
        [self.libraryTargetView addSubview:self.spinner];
        [self.libraryTargetView addSubview:self.statusLabel];
        [self.libraryTargetView addSubview:self.searchField]; // On top

        [NSLayoutConstraint activateConstraints:@[
            [self.searchField.topAnchor constraintEqualToAnchor:topAnchor
                                                      constant:spacing],
            [self.searchField.leadingAnchor constraintEqualToAnchor:self.libraryTargetView.leadingAnchor
                                                          constant:spacing],
            [self.searchField.trailingAnchor constraintEqualToAnchor:self.libraryTargetView.trailingAnchor
                                                           constant:-spacing],

            [self.collectionViewScrollView.topAnchor constraintEqualToAnchor:self.libraryTargetView.topAnchor],
            [self.collectionViewScrollView.leadingAnchor constraintEqualToAnchor:self.libraryTargetView.leadingAnchor],
            [self.collectionViewScrollView.trailingAnchor constraintEqualToAnchor:self.libraryTargetView.trailingAnchor],
            [self.collectionViewScrollView.bottomAnchor constraintEqualToAnchor:self.libraryTargetView.bottomAnchor],

            [self.tableViewScrollView.topAnchor constraintEqualToAnchor:self.libraryTargetView.topAnchor],
            [self.tableViewScrollView.leadingAnchor constraintEqualToAnchor:self.libraryTargetView.leadingAnchor],
            [self.tableViewScrollView.trailingAnchor constraintEqualToAnchor:self.libraryTargetView.trailingAnchor],
            [self.tableViewScrollView.bottomAnchor constraintEqualToAnchor:self.libraryTargetView.bottomAnchor],

            [self.spinner.centerXAnchor constraintEqualToAnchor:self.libraryTargetView.centerXAnchor],
            [self.spinner.centerYAnchor constraintEqualToAnchor:self.libraryTargetView.centerYAnchor],

            [self.statusLabel.centerXAnchor constraintEqualToAnchor:self.libraryTargetView.centerXAnchor],
            [self.statusLabel.centerYAnchor constraintEqualToAnchor:self.libraryTargetView.centerYAnchor],
        ]];
    }

    if (self.spinnerAnimating) {
        return;
    }

    self.collectionViewScrollView.hidden = !(showResults && gridMode);
    self.tableViewScrollView.hidden = !(showResults && !gridMode);
    self.statusLabel.hidden = showResults || isSearching;
    if (!self.statusLabel.hidden) {
        self.statusLabel.stringValue = hasSearchText
            ? _NS("No results")
            : _NS("Search your library");
    }

    if (isSearching && self.spinner.alphaValue == 0) {
        self.spinnerAnimating = YES;
        [self.spinner startAnimation:nil];
        [NSAnimationContext runAnimationGroup:^(NSAnimationContext * const context) {
            context.duration = VLCLibrarySearchSpinnerFade;
            self.spinner.animator.alphaValue = 1.0;
        } completionHandler:^{
            self.spinnerAnimating = NO;
            [self presentSearchView];
        }];
    } else if (!isSearching && self.spinner.alphaValue > 0) {
        self.spinnerAnimating = YES;
        [NSAnimationContext runAnimationGroup:^(NSAnimationContext * const context) {
            context.duration = VLCLibrarySearchSpinnerFade;
            self.spinner.animator.alphaValue = 0.0;
        } completionHandler:^{
            [self.spinner stopAnimation:nil];
            self.spinnerAnimating = NO;
        }];
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

#pragma mark - NSSearchFieldDelegate

- (void)controlTextDidChange:(NSNotification *)notification
{
    [self.searchDebounceTimer invalidate];
    self.searchDebounceTimer =
        [NSTimer scheduledTimerWithTimeInterval:VLCLibrarySearchDebounceInterval
                                        target:self
                                      selector:@selector(performSearch)
                                      userInfo:nil
                                       repeats:NO];
}

- (void)searchProviderResultsUpdated:(NSNotification *)notification
{
    [self.dataSource reloadData];
    [self presentSearchView];
}

- (void)performSearch
{
    NSString * const searchString = self.searchField.stringValue;
    if (searchString.length == 0) {
        [self.dataSource clearSearch];
        return;
    }

    [self.dataSource searchForString:searchString];
    [self presentSearchView];
}

@end
