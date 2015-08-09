/*****************************************************************************
 * freetype.h : Put text on the video, using freetype2
 *****************************************************************************
 * Copyright (C) 2015 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Sigmund Augdal Helberg <dnumgis@videolan.org>
 *          Gildas Bazin <gbazin@videolan.org>
 *          Jean-Baptiste Kempf <jb@videolan.org>
 *          Laurent Aimar <fenrir@videolan.org>
 *          Salah-Eddin Shaban <salshaaban@gmail.com>
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

#ifndef VLC_FREETYPE_H
#define VLC_FREETYPE_H

#include <vlc_text_style.h>                                   /* text_style_t*/

typedef struct faces_cache_t
{
    FT_Face        *p_faces;
    text_style_t   *p_styles;
    int            i_faces_count;
    int            i_cache_size;
} faces_cache_t;

/*****************************************************************************
 * filter_sys_t: freetype local data
 *****************************************************************************
 * This structure is part of the video output thread descriptor.
 * It describes the freetype specific properties of an output thread.
 *****************************************************************************/
struct filter_sys_t
{
    FT_Library     p_library;   /* handle to library     */
    FT_Face        p_face;      /* handle to face object */
    FT_Stroker     p_stroker;   /* handle to path stroker object */

    text_style_t  *p_default_style;
    text_style_t  *p_style;       /* Current Style */
    text_style_t  *p_forced_style;/* Renderer overridings */

    /* More styles... */
    float          f_shadow_vector_x;
    float          f_shadow_vector_y;

    /* Attachments */
    input_attachment_t **pp_font_attachments;
    int                  i_font_attachments;

    /* Font faces cache */
    faces_cache_t  faces_cache;

    char * (*pf_select) (filter_t *, const char* family,
                               bool bold, bool italic, int size,
                               int *index);

};

#define FT_FLOOR(X)     ((X & -64) >> 6)
#define FT_CEIL(X)      (((X + 63) & -64) >> 6)
#ifndef FT_MulFix
 #define FT_MulFix(v, s) (((v)*(s))>>16)
#endif

#ifdef __OS2__
typedef uint16_t uni_char_t;
# define FREETYPE_TO_UCS    "UCS-2LE"
#else
typedef uint32_t uni_char_t;
# if defined(WORDS_BIGENDIAN)
#  define FREETYPE_TO_UCS   "UCS-4BE"
# else
#  define FREETYPE_TO_UCS   "UCS-4LE"
# endif
#endif


FT_Face LoadFace( filter_t *p_filter, const text_style_t *p_style );

bool FaceStyleEquals( const text_style_t *p_style1,
                      const text_style_t *p_style2 );

#endif
