/*****************************************************************************
 * VLCLibraryController.h: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2019 VLC authors and VideoLAN
 *
 * Authors: Felix Paul Kühne <fkuehne # videolan -dot- org>
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

#import <Foundation/Foundation.h>

#import <vlc_media_library.h>

@class VLCLibraryModel;
@class VLCMediaLibraryMediaItem;

NS_ASSUME_NONNULL_BEGIN

@interface VLCLibraryController : NSObject

@property (readonly, nullable) VLCLibraryModel *libraryModel;

- (int)appendItemToPlaylist:(VLCMediaLibraryMediaItem *)mediaItem playImmediately:(BOOL)playImmediately;
- (int)appendItemsToPlaylist:(NSArray <VLCMediaLibraryMediaItem *> *)mediaItemArray playFirstItemImmediately:(BOOL)playFirstItemImmediately;
- (void)showItemInFinder:(VLCMediaLibraryMediaItem *)mediaItem;

- (int)addFolderWithFileURL:(NSURL *)fileURL;
- (int)banFolderWithFileURL:(NSURL *)fileURL;
- (int)unbanFolderWithFileURL:(NSURL *)fileURL;
- (int)removeFolderWithFileURL:(NSURL *)fileURL;

- (int)clearHistory;

/**
 * Sort the entire library representation based on:
 * @param sortCriteria the criteria used for sorting
 * @param descending sort ascending or descending.
 */
- (void)sortByCriteria:(enum vlc_ml_sorting_criteria_t)sortCriteria andDescending:(bool)descending;

/**
 * Initially, the library is unsorted until the user decides to do so
 * Until then, the unsorted state is retained.
 */
@property (readonly) BOOL unsorted;

/**
 * The last key used for sorting
 */
@property (readonly) enum vlc_ml_sorting_criteria_t lastSortingCriteria;

/**
 * The last order used for sorting
 */
@property (readonly) bool descendingLibrarySorting;

@end

NS_ASSUME_NONNULL_END
