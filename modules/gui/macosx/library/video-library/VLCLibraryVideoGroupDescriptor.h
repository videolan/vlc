/*****************************************************************************
 * VLCLibraryVideoCollectionViewGroupDescriptor.h: MacOS X interface module
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

#import <Cocoa/Cocoa.h>

#import "library/video-library/VLCLibraryVideoCollectionViewsDataSource.h"

typedef NS_ENUM(NSUInteger, VLCLibraryVideoGroup) {
    VLCLibraryVideoInvalidGroup = 0,
    VLCLibraryVideoRecentsGroup,
    VLCLibraryVideoLibraryGroup,
};

NS_ASSUME_NONNULL_BEGIN

@interface VLCLibraryVideoCollectionViewGroupDescriptor : NSObject

@property (readonly) VLCLibraryVideoGroup group;
@property (readonly) SEL libraryModelDataSelector;
@property (readonly) NSMethodSignature *libraryModelDataMethodSignature;
@property (readonly) NSNotificationName libraryModelUpdatedNotificationName;
@property (readonly) NSString *name;
@property (readonly) BOOL isHorizontalBarCollectionView;

- (instancetype)initWithVLCVideoLibraryGroup:(VLCLibraryVideoGroup)group;

@end

NS_ASSUME_NONNULL_END
