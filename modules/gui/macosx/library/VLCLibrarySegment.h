/*****************************************************************************
 * VLCLibrarySegment.h: MacOS X interface module
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

#import <Cocoa/Cocoa.h>

NS_ASSUME_NONNULL_BEGIN

@class VLCLibraryAbstractSegmentViewController;
@protocol VLCMediaLibraryItemProtocol;

extern NSString * const VLCLibraryBookmarkedLocationsKey;
extern NSString * const VLCLibraryBookmarkedLocationsChanged;

typedef NS_ENUM(NSInteger, VLCLibrarySegmentType) {
    VLCLibraryLowSentinelSegment = -1,
    VLCLibraryHomeSegmentType,
    VLCLibraryHeaderSegmentType,
    VLCLibraryVideoSegmentType,
    VLCLibraryShowsVideoSubSegmentType,
    VLCLibraryMusicSegmentType,
    VLCLibraryArtistsMusicSubSegmentType,
    VLCLibraryAlbumsMusicSubSegmentType,
    VLCLibrarySongsMusicSubSegmentType,
    VLCLibraryGenresMusicSubSegmentType,
    VLCLibraryPlaylistsSegmentType,
    VLCLibraryPlaylistsMusicOnlyPlaylistsSubSegmentType,
    VLCLibraryPlaylistsVideoOnlyPlaylistsSubSegmentType,
    VLCLibraryGroupsSegmentType,
    VLCLibraryGroupsGroupSubSegmentType,
    VLCLibraryBrowseSegmentType,
    VLCLibraryBrowseBookmarkedLocationSubSegmentType,
    VLCLibraryStreamsSegmentType,
    VLCLibraryHighSentinelSegment,
};

@interface VLCLibrarySegment : NSTreeNode

@property (class, readonly) NSArray<VLCLibrarySegment*> *librarySegments;
@property (readonly) VLCLibrarySegmentType segmentType;
@property (readonly) NSString *displayString;
@property (readonly) NSImage *displayImage;
@property (readonly) NSInteger childCount;
@property (readonly, nullable) Class libraryViewControllerClass;
@property (readwrite) NSInteger viewMode;
@property (readonly) NSUInteger toolbarDisplayFlags;

+ (instancetype)segmentWithSegmentType:(VLCLibrarySegmentType)segmentType;
+ (instancetype)segmentForLibraryItem:(id<VLCMediaLibraryItemProtocol>)libraryItem;
- (nullable VLCLibraryAbstractSegmentViewController *)newLibraryViewController;
- (void)presentLibraryViewUsingController:(VLCLibraryAbstractSegmentViewController *)controller;

@end

NS_ASSUME_NONNULL_END
