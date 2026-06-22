/*****************************************************************************
 * dvd_description.h: DVD audio/subtitle code_extension description tables
 *****************************************************************************
 * Copyright (C) 2026 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include <dvdread/version.h>

/**
 * DVD audio track code_extension values as defined in the DVD-Video spec.
 * Maps audio_attr_t.code_extension to translatable description strings.
 */
static const char *const dvd_audio_code_ext[] = {
    /* 0 unspecified */ NULL,
    /* 1 normal */      NULL,
    /* 2 */ N_("Audio for visually impaired"),
    /* 3 */ N_("Director's comments"),
    /* 4 */ N_("Alternate director's comments"),
};

/* older libdvdread does not have this enum yet */
#if DVDREAD_VERSION < DVDREAD_VERSION_CODE(7, 1, 0)
# define DVD_SUBP_CODE_EXT_FORCED 9
#endif

/**
 * DVD subtitle track code_extension values as defined in the DVD-Video spec.
 * Maps subp_attr_t.code_extension to translatable description strings.
 */
static const char *const dvd_spu_code_ext[] = {
    /* 0 unspecified */ NULL,
    /* 1 normal */      NULL,
    /* 2 */ N_("Large"),
    /* 3 */ N_("For children"),
    /* 4 */ NULL,
    /* 5 */ N_("Closed caption"),
    /* 6 */ N_("Closed caption (large)"),
    /* 7 */ N_("Closed caption (children)"),
    /* 8 */ NULL,
    /* 9 */ N_("Forced caption"),
    /* 10 */ NULL,
    /* 11 */ NULL,
    /* 12 */ NULL,
    /* 13 */ N_("Director's comments"),
    /* 14 */ N_("Director's comments (large)"),
    /* 15 */ N_("Director's comments (children)"),
};
