/*****************************************************************************
 * freetype.h : Put text on the video, using freetype2
 *****************************************************************************
 * Copyright (C) 2015 VLC authors and VideoLAN
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

/** \defgroup freetype Freetype text renderer
 * Freetype text rendering cross platform
 * @{
 * \file
 * Freetype module
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_text_style.h>                             /* text_style_t */
#include <vlc_arrays.h>                                 /* vlc_dictionary_t */
#include <vlc_vector.h>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H
#include FT_STROKER_H

/* Consistency between Freetype versions and platforms */
#define FT_FLOOR(X)     ((X & -64) >> 6)
#define FT_CEIL(X)      (((X + 63) & -64) >> 6)
#ifndef FT_MulFix
# define FT_MulFix(v, s) (((v)*(s))>>16)
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

typedef struct vlc_font_select_t vlc_font_select_t;

/*****************************************************************************
 * filter_sys_t: freetype local data
 *****************************************************************************
 * This structure is part of the video output thread descriptor.
 * It describes the freetype specific properties of an output thread.
 *****************************************************************************/
typedef struct VLC_VECTOR(char *) fontfamilies_t;
typedef struct vlc_family_t vlc_family_t;
typedef struct
{
    FT_Library     p_library;       /* handle to library     */
    FT_Face        p_face;          /* handle to face object */
    FT_Stroker     p_stroker;       /* handle to path stroker object */

    text_style_t  *p_default_style;
    text_style_t  *p_forced_style;  /* Renderer overridings */

    char *psz_fontfile;
    char *psz_monofontfile;

    /* More styles... */
    float          f_shadow_vector_x;
    float          f_shadow_vector_y;

    /* Attachments */
    input_attachment_t **pp_font_attachments;
    int                  i_font_attachments;

    /** Font face cache */
    vlc_dictionary_t  face_map;

    /* Current scaling of the text, default is 100 (%) */
    int               i_scale;

    vlc_font_select_t *fs;

} filter_sys_t;

/**
 * Selects and loads the right font
 *
 * \param p_filter the Freetype module [IN]
 * \param p_style the requested style (fonts can be different for italic or bold) [IN]
 * \param codepoint the codepoint needed [IN]
 */
FT_Face SelectAndLoadFace( filter_t *p_filter, const text_style_t *p_style,
                           uni_char_t codepoint );

static inline void BBoxInit( FT_BBox *p_box )
{
    p_box->xMin = INT_MAX;
    p_box->yMin = INT_MAX;
    p_box->xMax = INT_MIN;
    p_box->yMax = INT_MIN;
}

static inline void BBoxEnlarge( FT_BBox *p_max, const FT_BBox *p )
{
    p_max->xMin = __MIN(p_max->xMin, p->xMin);
    p_max->yMin = __MIN(p_max->yMin, p->yMin);
    p_max->xMax = __MAX(p_max->xMax, p->xMax);
    p_max->yMax = __MAX(p_max->yMax, p->yMax);
}


#endif
