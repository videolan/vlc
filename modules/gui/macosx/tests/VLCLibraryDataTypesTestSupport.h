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
VLCMediaLibraryMediaItem * _Nonnull VLCLibraryDataTypesTestMediaItemWithTypeAndSubtype(
    vlc_ml_media_type_t type,
    vlc_ml_media_subtype_t subtype);
