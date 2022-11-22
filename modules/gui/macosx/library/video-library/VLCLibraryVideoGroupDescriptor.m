/*****************************************************************************
 * VLCLibraryVideoGroupDescriptor.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2022 VLC authors and VideoLAN
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

#import "VLCLibraryVideoGroupDescriptor.h"

#import "extensions/NSString+Helpers.h"

#import "library/VLCLibraryModel.h"

@implementation VLCLibraryVideoCollectionViewGroupDescriptor

- (instancetype)initWithVLCVideoLibraryGroup:(VLCLibraryVideoGroup)group
{
    self = [super init];

    if (self) {
        _group = group;

        switch (_group) {
            case VLCLibraryVideoRecentsGroup:
                _libraryModelUpdatedNotificationName = VLCLibraryModelRecentMediaListUpdated;
                _libraryModelDataSelector = @selector(listOfRecentMedia);
                _isHorizontalBarCollectionView = YES;
                _name = _NS("Recents");
                break;
            case VLCLibraryVideoLibraryGroup:
                _libraryModelUpdatedNotificationName = VLCLibraryModelVideoMediaListUpdated;
                _libraryModelDataSelector = @selector(listOfVideoMedia);
                _isHorizontalBarCollectionView = NO;
                _name = _NS("Library");
                break;
            default:
                NSAssert(1, @"Cannot construct group descriptor from invalid VLCLibraryVideoGroup value");
                _group = VLCLibraryVideoInvalidGroup;
                break;
        }

        _libraryModelDataMethodSignature = [VLCLibraryModel instanceMethodSignatureForSelector:_libraryModelDataSelector];
    }

    return self;
}

@end
