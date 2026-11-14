/*****************************************************************************
 * VLCLibraryMusicCarouselContainerView.m: MacOS X interface module
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

#import "VLCLibraryMusicCarouselContainerView.h"

#import "library/VLCLibraryWindow.h"
#import "library/VLCLibraryWindowNavigationSidebarViewController.h"
#import "library/VLCLibraryWindowSplitViewController.h"

#import "main/VLCMain.h"

NSNotificationName const VLCLibraryMusicCarouselItemsChangedNotification = @"VLCLibraryMusicCarouselItemsChangedNotification";

@implementation VLCLibraryMusicCarouselContainerView
{
    VLCLibrarySegmentType _librarySegment;
}

@synthesize dataSource = _dataSource;

- (instancetype)initWithTitle:(NSString *)title
                   parentType:(VLCMediaLibraryParentGroupType)parentType
               librarySegment:(VLCLibrarySegmentType)librarySegment
                itemsProvider:(VLCLibraryMusicCarouselItemsProvider)itemsProvider
           resetNotifications:(NSArray<NSNotificationName> *)resetNotifications
{
    self = [super initWithFrame:NSZeroRect];
    if (self) {
        _librarySegment = librarySegment;
        self.headerView.title = title;
        self.headerView.navigable = librarySegment != VLCLibraryLowSentinelSegment;
        self.hidden = YES;

        VLCLibraryMusicCarouselDataSource * const dataSource = [[VLCLibraryMusicCarouselDataSource alloc]
            initWithParentType:parentType
                 itemsProvider:itemsProvider
            resetNotifications:resetNotifications];
        dataSource.collectionView = self.collectionView;
        self.collectionView.dataSource = dataSource;
        _dataSource = dataSource;
    }
    return self;
}

- (instancetype)initWithTitle:(NSString *)title
                   parentType:(VLCMediaLibraryParentGroupType)parentType
               librarySegment:(VLCLibrarySegmentType)librarySegment
                itemsProvider:(VLCLibraryMusicCarouselItemsProvider)itemsProvider
           resetNotifications:(NSArray<NSNotificationName> *)resetNotifications
                 numberOfRows:(NSUInteger)numberOfRows
{
    self = [self initWithTitle:title
                    parentType:parentType
                librarySegment:librarySegment
                 itemsProvider:itemsProvider
            resetNotifications:resetNotifications];
    if (self) {
        self.numberOfRows = numberOfRows;
    }
    return self;
}

- (NSNotificationName)displayedCollectionChangedNotification
{
    return VLCLibraryMusicCarouselDataSourceReloaded;
}

- (void)displayedCollectionChanged:(NSNotification *)notification
{
    [super displayedCollectionChanged:notification];
    self.hidden = self.headerView.itemCount == 0;
    if (notification != nil) {
        _hasLoadedItems = YES;
        [NSNotificationCenter.defaultCenter postNotificationName:VLCLibraryMusicCarouselItemsChangedNotification
                                                          object:self];
    }
}

- (void)openLibrary:(id)sender
{
    if (_librarySegment == VLCLibraryLowSentinelSegment) {
        return;
    }
    VLCLibraryWindowNavigationSidebarViewController * const navigationController =
        VLCMain.sharedInstance.libraryWindow.splitViewController.navSidebarViewController;
    [navigationController selectSegment:_librarySegment];
}

@end
