/*****************************************************************************
 * subsdec.h : text/ASS-SSA/USF subtitles headers
 *****************************************************************************
 * Copyright (C) 2000-2006 the VideoLAN team
 * $Id$
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
 *          Samuel Hocevar <sam@zoy.org>
 *          Derk-Jan Hartman <hartman at videolan dot org>
 *          Bernie Purcell <bitmap@videolan.org>
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

#ifndef SUBSDEC_HEADER_H
#define SUBSDEC_HEADER_H

#include <vlc_common.h>
#include <vlc_vout.h>
#include <vlc_codec.h>
#include <vlc_input.h>

#include <vlc_osd.h>
#include <vlc_filter.h>
#include <vlc_image.h>
#include <vlc_charset.h>
#include <vlc_stream.h>
#include <vlc_xml.h>
#include <errno.h>
#include <string.h>


#define DEFAULT_NAME "Default"
#define MAX_LINE 8192
#define NO_BREAKING_SPACE  "&#160;"

enum
{
    ATTRIBUTE_ALIGNMENT = (1 << 0),
    ATTRIBUTE_X         = (1 << 1),
    ATTRIBUTE_X_PERCENT = (1 << 2),
    ATTRIBUTE_Y         = (1 << 3),
    ATTRIBUTE_Y_PERCENT = (1 << 4),
};

typedef struct
{
    char       *psz_filename;
    picture_t  *p_pic;
} image_attach_t;

typedef struct
{
    char *          psz_stylename; /* The name of the style, no comma's allowed */
    text_style_t    font_style;
    int             i_align;
    int             i_margin_h;
    int             i_margin_v;
    int             i_margin_percent_h;
    int             i_margin_percent_v;
}  ssa_style_t;

/*****************************************************************************
 * decoder_sys_t : decoder descriptor
 *****************************************************************************/
struct decoder_sys_t
{
    bool          b_ass;                           /* The subs are ASS */
    int                 i_original_height;
    int                 i_original_width;
    int                 i_align;          /* Subtitles alignment on the vout */
    vlc_iconv_t         iconv_handle;            /* handle to iconv instance */
    bool          b_autodetect_utf8;

    ssa_style_t         **pp_ssa_styles;
    int                 i_ssa_styles;

    image_attach_t      **pp_images;
    int                 i_images;
};


char                *GotoNextLine( char *psz_text );

void                ParseSSAHeader ( decoder_t * );
void                ParseSSAString ( decoder_t *, char *, subpicture_t * );

#endif
