/*****************************************************************************
 * VLCLibraryModel.h: MacOS X interface module
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

#import <Cocoa/Cocoa.h>

#import <vlc_media_library.h>

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSInteger, VLCLibraryMode) {
    VLCLibraryModeAudio,
    VLCLibraryModeVideo,
};

@class VLCMediaLibraryMediaItem;
@class VLCMediaLibraryEntryPoint;

extern NSString *VLCLibraryModelAudioMediaListUpdated;
extern NSString *VLCLibraryModelVideoMediaListUpdated;
extern NSString *VLCLibraryModelRecentMediaListUpdated;
extern NSString *VLCLibraryModelMediaItemUpdated;

@interface VLCLibraryModel : NSObject

- (instancetype)initWithLibrary:(vlc_medialibrary_t *)library;

@property (readwrite) VLCLibraryMode libraryMode;

@property (readonly) size_t numberOfAudioMedia;
@property (readonly) NSArray <VLCMediaLibraryMediaItem *> *listOfAudioMedia;

@property (readonly) size_t numberOfVideoMedia;
@property (readonly) NSArray <VLCMediaLibraryMediaItem *> *listOfVideoMedia;

@property (readonly) size_t numberOfRecentMedia;
@property (readonly) NSArray <VLCMediaLibraryMediaItem *> *listOfRecentMedia;

@property (readonly) NSArray <VLCMediaLibraryEntryPoint *> *listOfMonitoredFolders;

@end

NS_ASSUME_NONNULL_END
