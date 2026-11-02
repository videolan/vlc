/*****************************************************************************
 * VLCLibraryHistoryViewController.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2026 VLC authors and VideoLAN
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

#import "VLCLibraryHistoryViewController.h"

#import "extensions/NSImage+VLCAdditions.h"
#import "extensions/NSScrollView+VLCAdditions.h"
#import "extensions/NSString+Helpers.h"
#import "library/VLCLibraryCollectionView.h"
#import "library/VLCLibraryCollectionViewDelegate.h"
#import "library/VLCLibraryCollectionViewFlowLayout.h"
#import "library/VLCLibraryCollectionViewItem.h"
#import "library/VLCLibraryCollectionViewMediaItemSupplementaryDetailView.h"
#import "library/VLCLibraryCollectionViewSupplementaryElementView.h"
#import "library/VLCLibraryController.h"
#import "library/VLCLibraryModel.h"
#import "library/VLCLibrarySegment.h"
#import "library/VLCLibrarySectionedTableViewDelegate.h"
#import "library/VLCLibraryTableCellView.h"
#import "library/VLCLibraryTableView.h"
#import "library/VLCLibraryWindow.h"
#import "library/VLCLibraryWindowPersistentPreferences.h"
#import "library/history-library/VLCLibraryHistoryDataSource.h"
#import "main/VLCMain.h"

@interface VLCLibraryHistoryViewController ()
{
    VLCLibraryCollectionView *_collectionView;
    NSScrollView *_scrollView;
    VLCLibraryCollectionViewDelegate *_collectionViewDelegate;
    VLCLibraryTableView *_tableView;
    NSScrollView *_tableScrollView;
    VLCLibrarySectionedTableViewDelegate *_tableViewDelegate;
    VLCLibraryHistoryDataSource *_dataSource;
    NSArray<NSLayoutConstraint *> *_placeholderConstraints;
}
@end

@implementation VLCLibraryHistoryViewController

- (instancetype)initWithLibraryWindow:(VLCLibraryWindow *)libraryWindow
{
    self = [super initWithLibraryWindow:libraryWindow];
    if (self) {
        _collectionView = [[VLCLibraryCollectionView alloc] init];
        _collectionView.translatesAutoresizingMaskIntoConstraints = NO;
        _collectionView.selectable = YES;
        _collectionView.allowsEmptySelection = YES;
        _collectionView.allowsMultipleSelection = YES;

        VLCLibraryCollectionViewFlowLayout * const layout = VLCLibraryCollectionViewFlowLayout.standardLayout;
        layout.headerReferenceSize = VLCLibraryCollectionViewSupplementaryElementView.defaultHeaderSize;
        _collectionView.collectionViewLayout = layout;
        _collectionViewDelegate = [[VLCLibraryCollectionViewDelegate alloc] init];
        _collectionView.delegate = _collectionViewDelegate;

        [_collectionView registerClass:VLCLibraryCollectionViewItem.class
                 forItemWithIdentifier:VLCLibraryCollectionViewItemIdentifier];
        [_collectionView registerClass:VLCLibraryCollectionViewSupplementaryElementView.class
            forSupplementaryViewOfKind:NSCollectionElementKindSectionHeader
                        withIdentifier:VLCLibrarySupplementaryElementViewIdentifier];
        NSNib * const detailNib = [[NSNib alloc]
            initWithNibNamed:NSStringFromClass(VLCLibraryCollectionViewMediaItemSupplementaryDetailView.class)
                      bundle:nil];
        [_collectionView registerNib:detailNib
          forSupplementaryViewOfKind:VLCLibraryCollectionViewMediaItemSupplementaryDetailViewKind
                      withIdentifier:VLCLibraryCollectionViewMediaItemSupplementaryDetailViewIdentifier];
        _scrollView = [NSScrollView libraryScrollViewWithDocumentView:_collectionView];

        _tableView = [[VLCLibraryTableView alloc] init];
        _tableView.headerView = nil;
        _tableView.allowsEmptySelection = YES;
        _tableView.allowsMultipleSelection = YES;
        _tableView.columnAutoresizingStyle = NSTableViewLastColumnOnlyAutoresizingStyle;
        NSTableColumn * const column = [[NSTableColumn alloc] initWithIdentifier:@"history"];
        [_tableView addTableColumn:column];
        NSNib * const tableCellNib = [[NSNib alloc]
            initWithNibNamed:NSStringFromClass(VLCLibraryTableCellView.class)
                      bundle:nil];
        [_tableView registerNib:tableCellNib forIdentifier:VLCLibraryTableCellViewIdentifier];
        _tableViewDelegate = [[VLCLibrarySectionedTableViewDelegate alloc] init];
        _tableViewDelegate.cellViewIdentifier = VLCLibraryTableCellViewIdentifier;
        _tableView.delegate = _tableViewDelegate;
        _tableScrollView = [NSScrollView libraryDetailScrollViewWithDocumentView:_tableView];

        _placeholderConstraints = @[
            [self.placeholderImageView.widthAnchor constraintEqualToConstant:182.],
            [self.placeholderImageView.heightAnchor constraintEqualToConstant:114.]
        ];

        _dataSource = [[VLCLibraryHistoryDataSource alloc] init];
        _dataSource.libraryModel = VLCMain.sharedInstance.libraryController.libraryModel;
        _dataSource.collectionView = _collectionView;
        _dataSource.tableView = _tableView;
        _collectionView.dataSource = _dataSource;
        _tableView.dataSource = _dataSource;
        _tableView.target = _dataSource;
        _tableView.doubleAction = @selector(doubleClickAction:);
        _dataSource.tableViewActive = VLCLibraryWindowPersistentPreferences.sharedInstance.historyLibraryViewMode ==
                                     VLCLibraryListViewModeSegment;
        __weak typeof(self) weakSelf = self;
        _dataSource.dataChangedHandler = ^{
            [weakSelf updatePresentedView];
        };

        NSNotificationCenter * const center = NSNotificationCenter.defaultCenter;
        for (NSNotificationName name in @[
            NSCalendarDayChangedNotification,
            NSSystemTimeZoneDidChangeNotification,
            NSCurrentLocaleDidChangeNotification,
            NSApplicationDidBecomeActiveNotification
        ]) {
            [center addObserver:self selector:@selector(calendarChanged:) name:name object:nil];
        }
        [self connect];
    }
    return self;
}

- (id<VLCLibraryDataSource>)currentDataSource
{
    return _dataSource;
}

- (NSArray<NSLayoutConstraint *> *)placeholderImageViewSizeConstraints
{
    return _placeholderConstraints;
}

- (void)presentHistoryView
{
    const BOOL tableViewActive = VLCLibraryWindowPersistentPreferences.sharedInstance.historyLibraryViewMode ==
                                 VLCLibraryListViewModeSegment;
    const BOOL modeChanged = _dataSource.tableViewActive != tableViewActive;
    _dataSource.tableViewActive = tableViewActive;
    [_dataSource reloadData];
    [self updatePresentedView];
    if (modeChanged) {
        if (tableViewActive && _tableView.selectedRow >= 0) {
            [_tableView scrollRowToVisible:_tableView.selectedRow];
        } else if (!tableViewActive && _collectionView.selectionIndexPaths.count > 0) {
            NSIndexPath * const firstSelection =
                [_collectionView.selectionIndexPaths.allObjects sortedArrayUsingSelector:@selector(compare:)].firstObject;
            [_collectionView scrollToItemsAtIndexPaths:[NSSet setWithObject:firstSelection]
                                        scrollPosition:NSCollectionViewScrollPositionTop];
        }
    }
}

- (void)connect
{
    [super connect];
    [self updatePresentedView];
}

- (void)updatePresentedView
{
    if (!self.connected || self.libraryWindow.librarySegmentType != VLCLibraryHistorySegmentType) {
        return;
    }
    if (_dataSource.sections.count > 0) {
        NSScrollView * const selectedView = _dataSource.tableViewActive ? _tableScrollView : _scrollView;
        if (selectedView.superview != self.libraryTargetView) {
            [self.libraryWindow displayLibraryView:selectedView];
        }
    } else if (_dataSource.libraryModel.filterString.length > 0) {
        [self.libraryWindow displayNoResultsMessage];
    } else {
        [self.libraryWindow displayLibraryPlaceholderViewWithImage:NSImage.VLCPlaceholderVideoImage
                                                  usingConstraints:self.placeholderImageViewSizeConstraints
                                                 displayingMessage:_NS("Your playback history will appear here.")];
    }
}

- (void)calendarChanged:(NSNotification *)notification
{
    if (self.connected && self.libraryWindow.librarySegmentType == VLCLibraryHistorySegmentType) {
        [_dataSource reloadData];
    }
}

@end
