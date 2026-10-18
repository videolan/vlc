/*****************************************************************************
 * VLCLibraryHomeTestSupport.m: unused Home media-service test dependencies
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

#import "library/VLCLibraryCarouselCollectionViewItem.h"

// The shared carousel registers its media item even when Browse supplies all
// displayed cards. Keep that unused path separate from the production Browse
// item, without bringing playback and media-library menus into these tests.
NSString * const VLCLibraryCarouselCollectionViewItemIdentifier =
    @"VLCLibraryCarouselCollectionViewItemIdentifier";

// Bookmark identifiers normally live with the segment controller.
NSString * const VLCLibraryBookmarkedLocationsKey = @"VLCLibraryBookmarkedLocations";
NSString * const VLCLibraryBookmarkedLocationsChanged = @"VLCLibraryBookmarkedLocationsChanged";

// Browse does not create the media detail views referenced by the shared layout.
NSCollectionViewSupplementaryElementKind const VLCLibraryCollectionViewAudioGroupSupplementaryDetailViewKind =
    @"VLCLibraryCollectionViewAudioGroupSupplementaryDetailViewIdentifier";
NSCollectionViewSupplementaryElementKind const VLCLibraryCollectionViewMediaItemListSupplementaryDetailViewKind =
    @"VLCLibraryCollectionViewMediaItemListSupplementaryDetailViewIdentifier";
NSCollectionViewSupplementaryElementKind const VLCLibraryCollectionViewMediaItemSupplementaryDetailViewKind =
    @"VLCLibraryCollectionViewMediaItemSupplementaryDetailViewIdentifier";

@implementation VLCLibraryCarouselCollectionViewItem

- (void)loadView
{
    [NSException raise:NSInternalInconsistencyException
                format:@"Home Browse tests unexpectedly loaded a media carousel item"];
}

- (void)playRepresentedItem
{
    [NSException raise:NSInternalInconsistencyException
                format:@"Home Browse tests unexpectedly activated a media carousel item"];
}

- (void)openContextMenu:(__unused NSEvent *)event
{
    [NSException raise:NSInternalInconsistencyException
                format:@"Home Browse tests unexpectedly requested a media carousel menu"];
}

@end
