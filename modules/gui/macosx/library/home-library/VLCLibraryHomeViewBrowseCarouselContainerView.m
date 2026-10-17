/*****************************************************************************
 * VLCLibraryHomeViewBrowseCarouselContainerView.m: MacOS X interface module
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

#import "VLCLibraryHomeViewBrowseCarouselContainerView.h"

#import "extensions/NSString+Helpers.h"

#import "library/VLCLibrarySegment.h"
#import "library/VLCLibraryWindow.h"
#import "library/home-library/VLCLibraryHomeBrowseCarouselView.h"
#import "library/home-library/VLCLibraryHomeBrowseCollectionViewItem.h"
#import "library/home-library/VLCLibraryHomeViewBrowseCarouselItemView.h"

#import "main/VLCMain.h"

#import "views/VLCUIUnits.h"

static NSString * const VLCLibraryHomeBrowseItemIdentifier = @"VLCLibraryHomeBrowseItem";

@interface VLCLibraryHomeViewBrowseCarouselContainerView () <NSCollectionViewDataSource, NSCollectionViewDelegateFlowLayout>

@property (copy) NSArray<NSString *> *bookmarkedLocations;
@property BOOL observingBookmarks;

@end

@implementation VLCLibraryHomeViewBrowseCarouselContainerView

- (void)setup
{
    [super setup];
    self.headerView.title = _NS("Browse");
    self.itemHeight = VLCUIUnits.libraryCompactCardSize.height;
    [self.collectionView registerClass:VLCLibraryHomeBrowseCollectionViewItem.class
                forItemWithIdentifier:VLCLibraryHomeBrowseItemIdentifier];
    self.collectionView.dataSource = self;
    self.collectionView.delegate = self;
    [self reloadData];
}

- (VLCLibraryCarouselView *)newCollectionView
{
    return [[VLCLibraryHomeBrowseCarouselView alloc] initWithFrame:NSZeroRect];
}

- (void)openLibrary:(id)sender
{
    [VLCMain.sharedInstance.libraryWindow goToBrowseSection:sender];
}

- (void)connect
{
    if (!self.observingBookmarks) {
        NSNotificationCenter * const center = NSNotificationCenter.defaultCenter;
        [center addObserver:self selector:@selector(bookmarksChanged:)
                       name:VLCLibraryBookmarkedLocationsChanged object:nil];
        // Sidebar rename observation also updates defaults without posting the
        // bookmark notification. Compare the saved array before refreshing.
        [center addObserver:self selector:@selector(bookmarksChanged:)
                       name:NSUserDefaultsDidChangeNotification object:nil];
        self.observingBookmarks = YES;
    }
    [self reloadData];
}

- (void)disconnect
{
    NSNotificationCenter * const center = NSNotificationCenter.defaultCenter;
    [center removeObserver:self name:VLCLibraryBookmarkedLocationsChanged object:nil];
    [center removeObserver:self name:NSUserDefaultsDidChangeNotification object:nil];
    self.observingBookmarks = NO;
}

- (void)reloadData
{
    self.bookmarkedLocations = [NSUserDefaults.standardUserDefaults stringArrayForKey:VLCLibraryBookmarkedLocationsKey] ?: @[];
    self.headerView.itemCount = self.bookmarkedLocations.count;
    self.hidden = self.bookmarkedLocations.count == 0;
    self.collectionView.selectionIndexPaths = NSSet.set;
    [self.collectionView reloadData];
}

- (void)bookmarksChanged:(__unused NSNotification *)notification
{
    if (!NSThread.isMainThread) {
        __weak typeof(self) weakSelf = self;
        dispatch_async(dispatch_get_main_queue(), ^{
            [weakSelf bookmarksChanged:nil];
        });
        return;
    }

    NSArray<NSString *> * const locations =
        [NSUserDefaults.standardUserDefaults stringArrayForKey:VLCLibraryBookmarkedLocationsKey] ?: @[];
    if (self.observingBookmarks && ![locations isEqualToArray:self.bookmarkedLocations]) {
        [self reloadData];
    }
}

- (NSInteger)collectionView:(__unused NSCollectionView *)collectionView
    numberOfItemsInSection:(__unused NSInteger)section
{
    return self.bookmarkedLocations.count;
}

- (NSCollectionViewItem *)collectionView:(NSCollectionView *)collectionView
   itemForRepresentedObjectAtIndexPath:(NSIndexPath *)indexPath
{
    NSCollectionViewItem * const item = [collectionView makeItemWithIdentifier:VLCLibraryHomeBrowseItemIdentifier
                                                                 forIndexPath:indexPath];
    NSString * const mrl = self.bookmarkedLocations[indexPath.item];
    item.representedObject = mrl;
    VLCLibraryHomeViewBrowseCarouselItemView * const card = (VLCLibraryHomeViewBrowseCarouselItemView *)item.view;
    card.mrl = mrl;
    card.carouselView = (VLCCarouselView *)collectionView;
    card.target = self;
    card.action = @selector(browseBookmarkedLocation:);
    return item;
}

- (NSSize)collectionView:(__unused NSCollectionView *)collectionView
                 layout:(__unused NSCollectionViewLayout *)layout
 sizeForItemAtIndexPath:(__unused NSIndexPath *)indexPath
{
    return VLCUIUnits.libraryCompactCardSize;
}

- (void)browseBookmarkedLocation:(VLCLibraryHomeViewBrowseCarouselItemView *)sender
{
    [VLCMain.sharedInstance.libraryWindow browseFolderByMrl:sender.mrl];
}

@end
