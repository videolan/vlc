/*****************************************************************************
 * VLCLibraryRepresentedItem.m: MacOS X interface module
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

#import "VLCLibraryRepresentedItem.h"

#import "extensions/NSString+Helpers.h"

#import "library/VLCLibraryController.h"
#import "library/VLCLibraryDataTypes.h"
#import "library/VLCLibraryModel.h"

#import "library/audio-library/VLCLibraryAllAudioGroupsMediaLibraryItem.h"

#import "main/VLCMain.h"

#import "playlist/VLCPlaylistController.h"

@interface VLCLibraryRepresentedItem ()
{
    NSInteger _itemIndexInParent; // Call self.itemIndexInParent, don't access directly
    NSArray<VLCMediaLibraryMediaItem *> * _parentMediaArray;
}

@property (readwrite) enum vlc_ml_media_type_t mediaType;

@end

@implementation VLCLibraryRepresentedItem

- (instancetype)initWithItem:(const id<VLCMediaLibraryItemProtocol>)item
                  parentType:(const VLCMediaLibraryParentGroupType)parentType
{
    self = [self init];
    if (self) {
        _item = item;
        _parentType = parentType;
        _mediaType = item.firstMediaItem.mediaType;
    }
    return self;
}

- (instancetype)init
{
    self = [super init];
    if (self) {
        [self setup];
    }
    return self;
}

- (void)setup
{
    _itemIndexInParent = NSNotFound;
}

- (NSInteger)itemIndexInParent
{
    @synchronized(self) {
        if (_itemIndexInParent == NSNotFound) {
            _itemIndexInParent = [self findItemIndexInParent];
        }

        return _itemIndexInParent;
    }
}

- (NSInteger)findItemIndexInParent
{
    NSArray<VLCMediaLibraryMediaItem *> * items = nil;

    if (self.parentType == VLCMediaLibraryParentGroupTypeUnknown) {
        VLCLibraryModel * const libraryModel = VLCMain.sharedInstance.libraryController.libraryModel;
        const BOOL isVideo = self.mediaType == VLC_ML_MEDIA_TYPE_VIDEO;
        items = isVideo ? libraryModel.listOfVideoMedia : libraryModel.listOfAudioMedia;
    } else {
        items = self.parentMediaArray;
    }

    // We search by mediaItem, so we want to find the index of the item in the parent.
    // Specifically, the first media item. For a media item, we will just receive its self.
    // For other types, we will get the first item. Albums, genres, and artists all provide
    // their media items in an album-by-album manner, so this works for us.
    //
    // Remember that library IDs for albums, genres, and artists are specific to them and we
    // cannot use these to search the list of audio media

    const int64_t itemId = self.item.firstMediaItem.libraryID;
    return [items indexOfObjectPassingTest:^BOOL(VLCMediaLibraryMediaItem * const mediaItem,
                                                 const NSUInteger idx,
                                                 BOOL * const stop) {
        return mediaItem.libraryID == itemId;
    }];
}

- (int64_t)parentItemIdForAudioItem:(const id<VLCMediaLibraryItemProtocol>)item
{
    // Decide which other items we are going to be adding to the playlist when playing the item.
    // Key for playing in library mode, not in individual mode
    int64_t parentItemId = NSNotFound;

    if ([item isKindOfClass:VLCMediaLibraryMediaItem.class]) {
        VLCMediaLibraryMediaItem * const mediaItem = (VLCMediaLibraryMediaItem *)item;

        switch (self.parentType) {
        case VLCMediaLibraryParentGroupTypeAlbum:
            parentItemId = mediaItem.albumID;
            break;
        case VLCMediaLibraryParentGroupTypeArtist:
            parentItemId = mediaItem.artistID;
            break;
        case VLCMediaLibraryParentGroupTypeGenre:
            parentItemId = mediaItem.genreID;
            break;
        default:
            break;
        }
    } else if ([item isKindOfClass:VLCMediaLibraryAlbum.class]) {
        VLCMediaLibraryAlbum * const album = (VLCMediaLibraryAlbum *)item;

        switch (self.parentType) {
        case VLCMediaLibraryParentGroupTypeArtist:
            parentItemId = album.artistID;
            break;
        case VLCMediaLibraryParentGroupTypeGenre:
        {
            VLCMediaLibraryMediaItem * const firstChildItem = album.firstMediaItem;
            parentItemId = firstChildItem.genreID;
            break;
        }
        default:
            break;
        }
    }

    return parentItemId;
}

- (const id<VLCMediaLibraryItemProtocol>)parentItemForAudioItem:(const id<VLCMediaLibraryItemProtocol>)item
{
    // If we have no defined parent type, use the all audio groups item.
    // This item essentially represents the entirety of the library and all of its media items.
    const VLCMediaLibraryParentGroupType parentType = self.parentType;
    if (parentType == VLCMediaLibraryParentGroupTypeUnknown || parentType == VLCMediaLibraryParentGroupTypeAudioLibrary) {
        return [[VLCLibraryAllAudioGroupsMediaLibraryItem alloc] initWithDisplayString:_NS("All items")];
    } else if ([self.item conformsToProtocol:@protocol(VLCMediaLibraryAudioGroupProtocol)]) {
        // If the parent item class and the actual item class are the same, we likely want
        // to also play the entirety of the library -- think of playing an album within the
        // albums view, or playing a song within the songs view.
        const id<VLCMediaLibraryAudioGroupProtocol> audioGroupItem = (id<VLCMediaLibraryAudioGroupProtocol>)self.item;
        if (audioGroupItem.matchingParentType == parentType) {
            return [[VLCLibraryAllAudioGroupsMediaLibraryItem alloc] initWithDisplayString:_NS("All items")
                                                                     accordingToParentType:self.parentType];
        }
    }

    const int64_t parentItemId = [self parentItemIdForAudioItem:item];
    if (parentItemId == NSNotFound) {
        return nil;
    }

    switch (parentType) {
    case VLCMediaLibraryParentGroupTypeAlbum:
        return [VLCMediaLibraryAlbum albumWithID:parentItemId];
    case VLCMediaLibraryParentGroupTypeArtist:
        return [VLCMediaLibraryArtist artistWithID:parentItemId];
    case VLCMediaLibraryParentGroupTypeGenre:
        return [VLCMediaLibraryGenre genreWithID:parentItemId];
    default:
        return nil;
    }
}

- (NSArray<VLCMediaLibraryMediaItem *> *)parentMediaArrayForItem:(const id<VLCMediaLibraryItemProtocol>)item
{
    const BOOL isVideo = self.mediaType == VLC_ML_MEDIA_TYPE_VIDEO;
    if (isVideo) {
        VLCLibraryModel * const libraryModel = VLCMain.sharedInstance.libraryController.libraryModel;
        return [libraryModel listOfMediaItemsForParentType:self.parentType];
    } else {
        const id<VLCMediaLibraryItemProtocol> parentAudioItem = [self parentItemForAudioItem:item];
        return parentAudioItem.mediaItems;
    }
}

- (NSArray<VLCMediaLibraryMediaItem *> *)parentMediaArray
{
    @synchronized(self) {
        if (self.parentType != VLCMediaLibraryParentGroupTypeUnknown &&
            (_parentMediaArray == nil || _parentMediaArray.count == 0)) {
            _parentMediaArray = [self parentMediaArrayForItem:self.item];
        }

        return _parentMediaArray;
    }
}

- (void)playIndividualModeImmediately:(BOOL)playImmediately
{
    VLCLibraryController * const libraryController = VLCMain.sharedInstance.libraryController;

    // If play immediately, play first item, queue following items
    // If not then just queue all items
    __block BOOL startingPlayImmediately = playImmediately;

    [self.item iterateMediaItemsWithBlock:^(VLCMediaLibraryMediaItem* mediaItem) {
        [libraryController appendItemToPlaylist:mediaItem playImmediately:startingPlayImmediately];

        if (startingPlayImmediately) {
            startingPlayImmediately = NO;
        }
    }];
}

- (void)playLibraryModeImmediately:(BOOL)playImmediately
{
    VLCPlaylistController * const playlistController = VLCMain.sharedInstance.playlistController;
    VLCLibraryController * const libraryController = VLCMain.sharedInstance.libraryController;

    // If play immediately, play first item, queue following items
    // If not then just queue all items
    __block BOOL startingPlayImmediately = playImmediately;

    NSArray<VLCMediaLibraryMediaItem *> * const parentItems = self.parentMediaArray;
    const NSUInteger parentItemCount = parentItems.count;

    if (parentItemCount == 0) {
        [self playIndividualModeImmediately:playImmediately];
        return;
    }

    const NSUInteger itemIndexInParent = self.itemIndexInParent;
    const NSUInteger startingIndex = itemIndexInParent == NSNotFound ? 0 : itemIndexInParent;

    for (NSUInteger i = startingIndex; i < parentItemCount; i++) {
        const id<VLCMediaLibraryItemProtocol> mediaItem = [parentItems objectAtIndex:i];
        [libraryController appendItemToPlaylist:mediaItem playImmediately:startingPlayImmediately];

        if (startingPlayImmediately) {
            startingPlayImmediately = NO;
        }
    }

    if (playlistController.playbackRepeat != VLC_PLAYLIST_PLAYBACK_REPEAT_NONE) {
        for (NSUInteger i = 0; i < startingIndex; i++) {
            const id<VLCMediaLibraryItemProtocol> mediaItem = [parentItems objectAtIndex:i];
            [libraryController appendItemToPlaylist:mediaItem playImmediately:NO];
        }
    }
}

- (void)playImmediately:(BOOL)playImmediately
{
    VLCPlaylistController * const playlistController = VLCMain.sharedInstance.playlistController;
    if (playlistController.libraryPlaylistMode || self.parentType != VLCMediaLibraryParentGroupTypeUnknown) {
        [self playLibraryModeImmediately:playImmediately];
    } else {
        [self playIndividualModeImmediately:playImmediately];
    }
}

- (void)play
{
    [self playImmediately:YES];
}

- (void)queue
{
    [self playImmediately:NO];
}

- (void)revealInFinder
{
    [self.item revealInFinder];
}

- (void)moveToTrash
{
    [self.item moveToTrash];
}

@end
