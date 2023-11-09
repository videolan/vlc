/*****************************************************************************
 * VLCLibraryAllAudioGroupsMediaLibraryItem.m: MacOS X interface module
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

#import "VLCLibraryAllAudioGroupsMediaLibraryItem.h"

#import "extensions/NSString+Helpers.h"

#import "main/VLCMain.h"

#import "library/VLCLibraryController.h"
#import "library/VLCLibraryModel.h"

@implementation VLCLibraryAllAudioGroupsMediaLibraryItem

@synthesize albums = _albums;
@synthesize artists = _artists;
@synthesize genres = _genres;
@synthesize numberOfTracks = _numberOfTracks;
@synthesize mediaItems = _mediaItems;
@synthesize matchingParentType = _matchingParentType;

- (instancetype)initWithDisplayString:(NSString *)displayString
{
     return [self initWithDisplayString:displayString
                  accordingToParentType:VLCMediaLibraryParentGroupTypeAudioLibrary];
}

- (instancetype)initWithDisplayString:(NSString *)displayString
                accordingToParentType:(const VLCMediaLibraryParentGroupType)parentType
{
    VLCLibraryModel * const libraryModel = VLCMain.sharedInstance.libraryController.libraryModel;
    _matchingParentType = VLCMediaLibraryParentGroupTypeAudioLibrary;
    _albums = libraryModel.listOfAlbums;
    _artists = libraryModel.listOfArtists;
    _mediaItems = [libraryModel listOfMediaItemsForParentType:parentType];
    _numberOfTracks = _mediaItems.count;

    const NSUInteger numberOfAlbums = libraryModel.numberOfAlbums;
    NSString * const primaryDetailString = [NSString stringWithFormat:_NS("%li albums, %li songs"),
                                                     numberOfAlbums,
                                                     _numberOfTracks];

    return [super initWithDisplayString:displayString
                withPrimaryDetailString:primaryDetailString
              withSecondaryDetailString:nil];
}

- (void)iterateMediaItemsWithBlock:(nonnull void (^)(VLCMediaLibraryMediaItem * _Nonnull))mediaItemBlock
{
    // Iterate by album
    NSArray<id<VLCMediaLibraryItemProtocol>> * const childItems = self.albums;
    for(id<VLCMediaLibraryItemProtocol> childItem in childItems) {
        [childItem iterateMediaItemsWithBlock:mediaItemBlock];
    }
}

@end
