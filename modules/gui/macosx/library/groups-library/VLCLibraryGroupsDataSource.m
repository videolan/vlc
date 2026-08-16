/*****************************************************************************
 * VLCLibraryGroupsDataSource.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2024 VLC authors and VideoLAN
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

#import "VLCLibraryGroupsDataSource.h"

#import "extensions/NSString+Helpers.h"
#import "library/VLCLibraryModel.h"

@implementation VLCLibraryGroupsDataSource

- (instancetype)init
{
    self = [super init];
    if(self) {
        [self connect];
    }
    return self;
}

- (void)connect
{
    NSNotificationCenter * const notificationCenter = NSNotificationCenter.defaultCenter;

    [notificationCenter addObserver:self
                           selector:@selector(libraryModelGroupsListReset:)
                               name:VLCLibraryModelListOfGroupsReset
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(libraryModelGroupsListReset:)
                               name:VLCLibraryModelAllCachesDropped
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(libraryModelGroupDeleted:)
                               name:VLCLibraryModelGroupDeleted
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(libraryModelGroupUpdated:)
                               name:VLCLibraryModelGroupUpdated
                             object:nil];

    [self reloadData];
}

- (void)disconnect
{
    [NSNotificationCenter.defaultCenter removeObserver:self];
}

- (void)libraryModelGroupsListReset:(NSNotification *)notification
{
    [self reloadData];
}

- (NSArray<VLCMediaLibraryGroup *> *)sourceBackingArray
{
    return self.libraryModel.listOfGroups;
}

- (VLCMediaLibraryParentGroupType)currentParentType
{
    return VLCMediaLibraryParentGroupTypeGroup;
}

- (void)libraryModelGroupUpdated:(NSNotification *)notification
{
    VLCMediaLibraryGroup * const group = notification.object;
    [self applySnapshotForChangedItemID:group.libraryID isDeletion:NO];
}

- (void)libraryModelGroupDeleted:(NSNotification *)notification
{
    VLCMediaLibraryGroup * const group = notification.object;
    [self applySnapshotForChangedItemID:group.libraryID isDeletion:YES];
}

- (NSString *)dataSourceTypeDisplayString
{
    return _NS("Group");
}

@end
