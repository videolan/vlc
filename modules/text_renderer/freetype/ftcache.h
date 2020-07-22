/*****************************************************************************
 * ftcache.h : Font Face and glyph cache freetype2
 *****************************************************************************
 * Copyright (C) 2020 - VideoLabs, VLC authors and VideoLAN
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
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/
#ifndef FTCACHE_H
#define FTCACHE_H

#include FT_CACHE_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct vlc_ftcache_t vlc_ftcache_t;

typedef struct
{
    char *psz_filename;
    unsigned idx;
    unsigned int charmap_index;

} vlc_face_id_t;

vlc_ftcache_t * vlc_ftcache_New( vlc_object_t *, FT_Library, unsigned );
void vlc_ftcache_Delete( vlc_ftcache_t * );

/* Glyphs managed by the cache. Always use vlc_ftcache_Init/Release */
typedef struct
{
    FT_Glyph p_glyph;
    FTC_Node ref;
} vlc_ftcache_glyph_t;

void vlc_ftcache_Glyph_Init( vlc_ftcache_glyph_t * );
void vlc_ftcache_Glyph_Release( vlc_ftcache_t *, vlc_ftcache_glyph_t * );

vlc_face_id_t * vlc_ftcache_GetFaceID( vlc_ftcache_t *, const char *psz_fontfile, int i_idx );

typedef struct
{
    int width_px;
    int height_px;
} vlc_ftcache_metrics_t;

vlc_face_id_t * vlc_ftcache_GetActiveFaceInfo( vlc_ftcache_t *, vlc_ftcache_metrics_t * );
int vlc_ftcache_GetGlyphForCurrentFace( vlc_ftcache_t *, FT_UInt charmap_index,
                                        vlc_ftcache_glyph_t *, FT_Long * );
FT_UInt vlc_ftcache_LookupCMapIndex( vlc_ftcache_t *, vlc_face_id_t *faceid, FT_UInt codepoint );
/* Big fat warning : Do not store FT_Face.
 * Faces are fully managed by cache and possibly invalidated when changing face */
FT_Face vlc_ftcache_LoadFaceByID( vlc_ftcache_t *, vlc_face_id_t *faceid,
                                  const vlc_ftcache_metrics_t * );
int vlc_ftcache_LoadFaceByIDNoSize( vlc_ftcache_t *ftcache, vlc_face_id_t *faceid );

#ifdef __cplusplus
}
#endif

#endif
