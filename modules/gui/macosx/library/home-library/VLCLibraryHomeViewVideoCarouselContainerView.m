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

#import "library/VLCLibraryDataTypes.h"
#import "library/VLCLibraryCarouselViewItemView.h"
#import "library/VLCLibraryUIUnits.h"

#import "library/home-library/VLCLibraryHomeViewVideoContainerViewDataSource.h"

#include "library/video-library/VLCLibraryVideoGroupDescriptor.h"

@implementation VLCLibraryHomeViewVideoCarouselContainerView

@synthesize groupDescriptor = _groupDescriptor;
@synthesize videoGroup = _videoGroup;
@synthesize dataSource = _dataSource;

- (void)setup
{
    [super setup];
    [self setupDataSource];
}

- (void)setupDataSource
{
    _dataSource = [[VLCLibraryHomeViewVideoContainerViewDataSource alloc] init];
    self.dataSource.carouselView = self.carouselView;
    [self.dataSource setup];
}

- (void)setGroupDescriptor:(VLCLibraryVideoCollectionViewGroupDescriptor *)groupDescriptor
{
    _groupDescriptor = groupDescriptor;
    _videoGroup = groupDescriptor.group;
    self.dataSource.groupDescriptor = groupDescriptor;
    if (groupDescriptor.group == VLCMediaLibraryParentGroupTypeRecentVideos) {
        self.titleView.stringValue = _NS("Recent videos");
    } else {
        self.titleView.stringValue = groupDescriptor.name;
    }
 }

- (void)setVideoGroup:(VLCMediaLibraryParentGroupType)group
{
    if (_groupDescriptor.group == group) {
        return;
    }

    VLCLibraryVideoCollectionViewGroupDescriptor * const descriptor = [[VLCLibraryVideoCollectionViewGroupDescriptor alloc] initWithVLCVideoLibraryGroup:group];
    [self setGroupDescriptor:descriptor];
}

- (void)presentLibraryItem:(id<VLCMediaLibraryItemProtocol>)libraryItem
{
    if (libraryItem == nil) {
        return;
    }

    NSIndexPath * const indexPath = [self.dataSource indexPathForLibraryItem:libraryItem];
    if (indexPath == nil) {
        return;
    }

    [self.carouselView scrollToItemAtIndex:indexPath.item animated:YES];
}

@end
