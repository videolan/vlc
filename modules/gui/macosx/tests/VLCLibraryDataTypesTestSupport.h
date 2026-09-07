/****************************************************************************
 * VLCLibraryDataTypesTestSupport.h: VLC library data type test support
 ****************************************************************************
 * Copyright (C) 2026 VLC authors and VideoLAN
 *
 * Authors: Claudio Cambra <developer@claudiocambra.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any later
 * version.
 ****************************************************************************/

#import "library/VLCLibraryDataTypes.h"

VLCMediaLibraryMediaItem * _Nonnull VLCLibraryDataTypesTestMediaItemWithSubtype(
    vlc_ml_media_subtype_t subtype);
VLCMediaLibraryMediaItem * _Nonnull VLCLibraryDataTypesTestMediaItemWithEmptyTitle(
    vlc_ml_media_subtype_t subtype);
VLCMediaLibraryMediaItem * _Nonnull VLCLibraryDataTypesTestMediaItemWithTypeAndSubtype(
    vlc_ml_media_type_t type,
    vlc_ml_media_subtype_t subtype);

FOUNDATION_EXPORT NSString * _Nonnull const VLCLibraryDataTypesTestInputItemNameKey;
FOUNDATION_EXPORT NSString * _Nonnull const VLCLibraryDataTypesTestInputItemTitleKey;
FOUNDATION_EXPORT NSString * _Nonnull const VLCLibraryDataTypesTestInputItemArtistKey;
FOUNDATION_EXPORT NSString * _Nonnull const VLCLibraryDataTypesTestInputItemAlbumKey;
FOUNDATION_EXPORT NSString * _Nonnull const VLCLibraryDataTypesTestInputItemTrackNumberKey;
FOUNDATION_EXPORT NSString * _Nonnull const VLCLibraryDataTypesTestInputItemGenreKey;
FOUNDATION_EXPORT NSString * _Nonnull const VLCLibraryDataTypesTestInputItemCopyrightKey;
FOUNDATION_EXPORT NSString * _Nonnull const VLCLibraryDataTypesTestInputItemPublisherKey;
FOUNDATION_EXPORT NSString * _Nonnull const VLCLibraryDataTypesTestInputItemLanguageKey;
FOUNDATION_EXPORT NSString * _Nonnull const VLCLibraryDataTypesTestInputItemDateKey;
FOUNDATION_EXPORT NSString * _Nonnull const VLCLibraryDataTypesTestInputItemDescriptionKey;
FOUNDATION_EXPORT NSString * _Nonnull const VLCLibraryDataTypesTestInputItemDirectorKey;
FOUNDATION_EXPORT NSString * _Nonnull const VLCLibraryDataTypesTestInputItemShowNameKey;
FOUNDATION_EXPORT NSString * _Nonnull const VLCLibraryDataTypesTestInputItemActorsKey;
FOUNDATION_EXPORT NSString * _Nonnull const VLCLibraryDataTypesTestInputItemArtworkURLKey;

VLCMediaLibraryMediaItem * _Nonnull VLCLibraryDataTypesTestMediaItemWithInputMetadata(
    vlc_ml_media_subtype_t subtype,
    NSDictionary<NSString *, id> * _Nullable metadata);

VLCMediaLibraryMediaItem * _Nonnull VLCLibraryDataTypesTestMediaItemWithInputMetadataAndFileURLs(
    vlc_ml_media_subtype_t subtype,
    NSDictionary<NSString *, id> * _Nullable metadata,
    NSArray<NSURL *> * _Nonnull fileURLs);

void VLCLibraryDataTypesTestResetTrashState(void);
void VLCLibraryDataTypesTestSetTrashFailure(BOOL shouldFail);
void VLCLibraryDataTypesTestSetTrashFailureOnCall(NSUInteger callNumber);
NSArray<NSURL *> * _Nonnull VLCLibraryDataTypesTestTrashedSourceURLs(void);
NSArray<NSURL *> * _Nonnull VLCLibraryDataTypesTestTrashDestinationURLs(void);
BOOL VLCLibraryDataTypesTestMoveItemToTrash(NSURL * _Nonnull url,
                                            NSURL * _Nullable * _Nullable resultingURL,
                                            NSError * _Nullable * _Nullable error);
