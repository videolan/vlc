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

#import <vlc_media_library.h>

BOOL VLCLibraryDataTypesIntegrationStart(void);
void VLCLibraryDataTypesIntegrationStop(void);
vlc_medialibrary_t *VLCLibraryDataTypesIntegrationMediaLibrary(void);
int64_t VLCLibraryDataTypesIntegrationMediaID(void);
int64_t VLCLibraryDataTypesIntegrationCreatePlaylist(void);
BOOL VLCLibraryDataTypesIntegrationPrepareFactoryFixtures(void);
int64_t VLCLibraryDataTypesIntegrationFactoryAudioMediaID(void);
int64_t VLCLibraryDataTypesIntegrationFactoryVideoMediaID(void);
int64_t VLCLibraryDataTypesIntegrationAlbumID(void);
int64_t VLCLibraryDataTypesIntegrationGenreID(void);
int64_t VLCLibraryDataTypesIntegrationShowID(void);
int64_t VLCLibraryDataTypesIntegrationGroupID(void);
