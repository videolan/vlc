/****************************************************************************
 * VLCLibraryDataTypesIntegrationTestSupport.h: medialibrary test lifecycle
 ****************************************************************************
 * Copyright (C) 2026 VLC authors and VideoLAN
 *
 * Authors: Claudio Cambra <developer@claudiocambra.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any later
 * version.
 *****************************************************************************/

#import <Foundation/Foundation.h>
#import <vlc_media_library.h>

BOOL VLCLibraryDataTypesIntegrationStart(void);
void VLCLibraryDataTypesIntegrationStop(void);
vlc_medialibrary_t *VLCLibraryDataTypesIntegrationMediaLibrary(void);
int64_t VLCLibraryDataTypesIntegrationMediaID(void);
int64_t VLCLibraryDataTypesIntegrationCreateExternalMedia(const char *mrl);
int64_t VLCLibraryDataTypesIntegrationCreatePlaylist(void);
BOOL VLCLibraryDataTypesIntegrationPrepareFactoryFixtures(void);
BOOL VLCLibraryDataTypesIntegrationPrepareMovieFixture(void);
BOOL VLCLibraryDataTypesIntegrationConvertMediaToMovie(int64_t mediaID);
int64_t VLCLibraryDataTypesIntegrationFactoryAudioMediaID(void);
int64_t VLCLibraryDataTypesIntegrationFactoryVideoMediaID(void);
int64_t VLCLibraryDataTypesIntegrationFactoryMovieMediaID(void);
int64_t VLCLibraryDataTypesIntegrationSortingAudioAlphaMediaID(void);
int64_t VLCLibraryDataTypesIntegrationSortingAudioZuluMediaID(void);
int64_t VLCLibraryDataTypesIntegrationSortingVideoAlphaMediaID(void);
int64_t VLCLibraryDataTypesIntegrationSortingVideoZuluMediaID(void);
int64_t VLCLibraryDataTypesIntegrationAlbumID(void);
int64_t VLCLibraryDataTypesIntegrationGenreID(void);
int64_t VLCLibraryDataTypesIntegrationShowID(void);
int64_t VLCLibraryDataTypesIntegrationGroupID(void);
int64_t VLCLibraryDataTypesIntegrationCreateScannedShow(NSString *name);
// Delete persisted fixture entities and relationships. Callers replay model
// events separately to control their order relative to a pending cache query.
BOOL VLCLibraryDataTypesIntegrationDeleteEntity(enum vlc_ml_event_type eventType, int64_t libraryID);
BOOL VLCLibraryDataTypesIntegrationRenameGroup(int64_t libraryID, NSString *name);
int64_t VLCLibraryDataTypesIntegrationCreateNamedGroup(int64_t mediaID, NSString *name);
BOOL VLCLibraryDataTypesIntegrationSetAlbumDuration(int64_t libraryID, int64_t duration);
BOOL VLCLibraryDataTypesIntegrationSetAlbumYear(int64_t libraryID, unsigned int year);
BOOL VLCLibraryDataTypesIntegrationRenameAlbumArtist(int64_t libraryID, NSString *name);
BOOL VLCLibraryDataTypesIntegrationRenameAlbum(int64_t libraryID, NSString *title);
BOOL VLCLibraryDataTypesIntegrationRenameArtist(int64_t libraryID, NSString *name);
BOOL VLCLibraryDataTypesIntegrationRenameGenre(int64_t libraryID, NSString *name);
BOOL VLCLibraryDataTypesIntegrationRenameMedia(int64_t libraryID, NSString *title);
BOOL VLCLibraryDataTypesIntegrationClearEpisodeSubtype(int64_t mediaID);
BOOL VLCLibraryDataTypesIntegrationSetShowSummary(int64_t showID, NSString *summary);
BOOL VLCLibraryDataTypesIntegrationMoveMediaToAlbum(int64_t mediaID, int64_t albumID);
BOOL VLCLibraryDataTypesIntegrationMoveMediaToArtist(int64_t mediaID, int64_t artistID);
BOOL VLCLibraryDataTypesIntegrationSetMovieSummary(int64_t mediaID, NSString *summary);
BOOL VLCLibraryDataTypesIntegrationSetMediaPresent(int64_t libraryID, BOOL present);
BOOL VLCLibraryDataTypesIntegrationSetGroupDuration(int64_t libraryID, int64_t duration);
BOOL VLCLibraryDataTypesIntegrationSetGroupModificationDate(int64_t libraryID, NSDate *date);
