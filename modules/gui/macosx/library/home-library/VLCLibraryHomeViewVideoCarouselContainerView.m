/*****************************************************************************
 * VLCLibraryHomeViewVideoCarouselContainerView.m: MacOS X interface module
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

#import "VLCLibraryHomeViewVideoCarouselContainerView.h"

#import "extensions/NSFont+VLCAdditions.h"
#import "extensions/NSString+Helpers.h"

#import "library/VLCLibraryCollectionView.h"
#import "library/VLCLibraryCollectionViewDelegate.h"
#import "library/VLCLibraryCarouselCollectionViewItem.h"
#import "library/VLCLibraryDataTypes.h"
#import "library/VLCLibraryWindow.h"
#import "library/VLCLibraryWindowSplitViewController.h"
#import "library/VLCLibrarySegment.h"
#import "library/VLCLibraryWindowNavigationSidebarViewController.h"

#import "library/home-library/VLCLibraryHomeViewVideoContainerViewDataSource.h"

#import "views/VLCUIUnits.h"
#import "main/VLCMain.h"

#include "library/video-library/VLCLibraryVideoGroupDescriptor.h"

@implementation VLCLibraryHomeViewVideoCarouselContainerView

@synthesize groupDescriptor = _groupDescriptor;
@synthesize videoGroup = _videoGroup;
@synthesize dataSource = _dataSource;

- (NSNotificationName)displayedCollectionChangedNotification
{
    return VLCLibraryVideoCollectionViewDataSourceDisplayedCollectionChangedNotification;
}

- (void)setupDataSource
{
    self.collectionViewDelegate.itemsAspectRatio = VLCLibraryCollectionViewItemAspectRatioVideoItem;
    self.collectionViewDelegate.staticItemSize = NSMakeSize(VLCUIUnits.carouselViewVideoItemViewWidth,
                                                            VLCUIUnits.carouselViewItemViewHeight);
    self.itemHeight = VLCUIUnits.carouselViewItemViewHeight;
    _dataSource = [[VLCLibraryHomeViewVideoContainerViewDataSource alloc] init];
    _dataSource.collectionViewItemIdentifier = VLCLibraryCarouselCollectionViewItemIdentifier;
    self.dataSource.collectionView = self.collectionView;
    [self.dataSource setup];
}

- (void)setGroupDescriptor:(VLCLibraryVideoCollectionViewGroupDescriptor *)groupDescriptor
{
    _groupDescriptor = groupDescriptor;
    _videoGroup = groupDescriptor.group;
    self.dataSource.groupDescriptor = groupDescriptor;
    if (groupDescriptor.group == VLCMediaLibraryParentGroupTypeRecentVideos) {
        self.headerView.title = _NS("Recent videos");
    } else {
        self.headerView.title = groupDescriptor.name;
    }
}

- (void)openLibrary:(id)sender
{
    [VLCMain.sharedInstance.libraryWindow.splitViewController.navSidebarViewController
        selectSegment:VLCLibraryVideoSegmentType];
}

- (void)setVideoGroup:(VLCMediaLibraryParentGroupType)group
{
    if (_groupDescriptor.group == group) {
        return;
    }

    VLCLibraryVideoCollectionViewGroupDescriptor * const descriptor = [[VLCLibraryVideoCollectionViewGroupDescriptor alloc] initWithVLCVideoLibraryGroup:group];
    [self setGroupDescriptor:descriptor];
}

@end
