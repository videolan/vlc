/*****************************************************************************
 * osd_text.c : text manipulation functions
 *****************************************************************************
 * Copyright (C) 1999-2007 VLC authors and VideoLAN
 * $Id$
 *
 * Author: Sigmund Augdal Helberg <dnumgis@videolan.org>
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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_vout.h>
#include <vlc_block.h>
#include <vlc_filter.h>
#include <vlc_osd.h>

/**
 * \brief Show text on the video from a given start date to a given end date
 * \param p_spu pointer to the subpicture queue the text is to be showed on
 * \param i_channel Subpicture channel
 * \param psz_string The text to be shown
 * \param p_style Pointer to a struct with text style info (it is duplicated)
 * \param i_flags flags for alignment and such
 * \param i_hmargin horizontal margin in pixels
 * \param i_vmargin vertical margin in pixels
 * \param i_start the time when this string is to appear on the video
 * \param i_stop the time when this string should stop to be displayed
 *               if this is 0 the string will be shown untill the next string
 *               is about to be shown
 */
static
int osd_ShowTextAbsolute( spu_t *p_spu_channel, int i_channel,
                           const char *psz_string, const text_style_t *p_style,
                           int i_flags, int i_hmargin, int i_vmargin,
                           mtime_t i_start, mtime_t i_stop )
{
    subpicture_t *p_spu;
    video_format_t fmt;
    (void)p_style;

    if( !psz_string ) return VLC_EGENERIC;

    p_spu = subpicture_New( NULL );
    if( !p_spu )
        return VLC_EGENERIC;

    p_spu->i_channel = i_channel;
    p_spu->i_start = i_start;
    p_spu->i_stop = i_stop;
    p_spu->b_ephemer = true;
    p_spu->b_absolute = false;

    /* Create a new subpicture region */
    memset( &fmt, 0, sizeof(video_format_t) );
    fmt.i_chroma = VLC_CODEC_TEXT;
    fmt.i_width = fmt.i_height = 0;
    fmt.i_x_offset = fmt.i_y_offset = 0;
    p_spu->p_region = subpicture_region_New( &fmt );
    if( !p_spu->p_region )
    {
        msg_Err( p_spu_channel, "cannot allocate SPU region" );
        subpicture_Delete( p_spu );
        return VLC_EGENERIC;
    }

    p_spu->p_region->psz_text = strdup( psz_string );
    p_spu->p_region->i_align = i_flags & SUBPICTURE_ALIGN_MASK;
    p_spu->p_region->i_x = i_hmargin;
    p_spu->p_region->i_y = i_vmargin;

    spu_PutSubpicture( p_spu_channel, p_spu );

    return VLC_SUCCESS;
}

/**
 * \brief Show text on the video for some time
 * \param p_spu pointer to the subpicture queue the text is to be showed on
 * \param i_channel Subpicture channel
 * \param psz_string The text to be shown
 * \param p_style Pointer to a struct with text style info (it is duplicated)
 * \param i_flags flags for alignment and such
 * \param i_hmargin horizontal margin in pixels
 * \param i_vmargin vertical margin in pixels
 * \param i_duration Amount of time the text is to be shown.
 */
static
int osd_ShowTextRelative( spu_t *p_spu, int i_channel,
                           const char *psz_string, const text_style_t *p_style,
                           int i_flags, int i_hmargin, int i_vmargin,
                           mtime_t i_duration )
{
    mtime_t i_now = mdate();

    return osd_ShowTextAbsolute( p_spu, i_channel, psz_string,
                                  p_style, i_flags, i_hmargin, i_vmargin,
                                  i_now, i_now + i_duration );
}

/**
 * \brief Write an informative message at the default location,
 *        for the default duration and only if the OSD option is enabled.
 * \param p_caller The object that called the function.
 * \param i_channel Subpicture channel
 * \param psz_format printf style formatting
 **/
void osd_Message( spu_t *p_spu, int i_channel,
                        char *psz_format, ... )
{
    va_list args;

    if( p_spu )
    {
        char *psz_string;
        va_start( args, psz_format );
        if( vasprintf( &psz_string, psz_format, args ) != -1 )
        {
            osd_ShowTextRelative( p_spu, i_channel, psz_string, NULL,
                    SUBPICTURE_ALIGN_TOP|SUBPICTURE_ALIGN_RIGHT, 30,20,1000000 );

            free( psz_string );
        }
        va_end( args );
    }
}
