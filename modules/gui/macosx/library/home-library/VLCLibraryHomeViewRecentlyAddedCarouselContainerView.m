/*****************************************************************************
 * VLCLibraryHomeViewRecentlyAddedCarouselContainerView.m: MacOS X interface
 *****************************************************************************
 * Copyright (C) 2026 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#import "VLCLibraryHomeViewRecentlyAddedCarouselContainerView.h"

#import "library/VLCLibraryCarouselCollectionViewItem.h"
#import "library/VLCLibraryCollectionViewDelegate.h"
#import "library/home-library/VLCLibraryHomeViewMediaContainerViewDataSource.h"
#import "library/VLCLibraryMediaGroupDescriptor.h"

#import "views/VLCUIUnits.h"

@implementation VLCLibraryHomeViewRecentlyAddedCarouselContainerView
{
    VLCLibraryMediaCollectionViewGroupDescriptor *_groupDescriptor;
}

@synthesize dataSource = _dataSource;

- (instancetype)initWithGroup:(VLCMediaLibraryParentGroupType)group
{
    NSParameterAssert(group == VLCMediaLibraryParentGroupTypeRecentlyAddedVideos ||
                      group == VLCMediaLibraryParentGroupTypeRecentlyAddedAudios);
    self = [super init];
    if (self) {
        if (group == VLCMediaLibraryParentGroupTypeRecentlyAddedVideos) {
            self.collectionViewDelegate.itemsAspectRatio = VLCLibraryCollectionViewItemAspectRatioVideoItem;
            self.collectionViewDelegate.staticItemSize = NSMakeSize(VLCUIUnits.carouselViewVideoItemViewWidth,
                                                                  self.itemHeight);
        }
        _groupDescriptor = [[VLCLibraryMediaCollectionViewGroupDescriptor alloc] initWithLibraryGroup:group];
        ((VLCLibraryHomeViewMediaContainerViewDataSource *)self.dataSource).groupDescriptor = _groupDescriptor;
        [self setupHeader];
    }
    return self;
}

- (NSNotificationName)displayedCollectionChangedNotification
{
    return VLCLibraryMediaCollectionViewDataSourceDisplayedCollectionChangedNotification;
}

- (void)setupDataSource
{
    VLCLibraryHomeViewMediaContainerViewDataSource * const source =
        [[VLCLibraryHomeViewMediaContainerViewDataSource alloc] init];
    source.collectionViewItemIdentifier = VLCLibraryCarouselCollectionViewItemIdentifier;
    source.collectionView = self.collectionView;
    [source setup];
    _dataSource = source;
}

- (void)setupHeader
{
    [super setupHeader];
    if (_groupDescriptor != nil) {
        self.headerView.title = _groupDescriptor.name;
    }
    self.headerView.navigable = NO;
}

- (void)displayedCollectionChanged:(NSNotification *)notification
{
    [super displayedCollectionChanged:notification];
    self.hidden = self.headerView.itemCount == 0;
}

@end
