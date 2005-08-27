/*****************************************************************************
 * osd_text.c : text manipulation functions
 *****************************************************************************
 * Copyright (C) 1999-2005 the VideoLAN team
 * $Id$
 *
 * Author: Sigmund Augdal <sigmunau@idi.ntnu.no>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/
 
#include <vlc/vout.h>
#include <vlc_block.h>
#include <vlc_filter.h>
#include <vlc_osd.h>

/**
 * \brief Show text on the video for some time
 * \param p_spu pointer to the subpicture queue the text is to be showed on
 * \param i_channel Subpicture channel
 * \param psz_string The text to be shown
 * \param p_style Pointer to a struct with text style info
 * \param i_flags flags for alignment and such
 * \param i_hmargin horizontal margin in pixels
 * \param i_vmargin vertical margin in pixels
 * \param i_duration Amount of time the text is to be shown.
 */
int osd_ShowTextRelative( spu_t *p_spu, int i_channel,
                           char *psz_string, text_style_t *p_style,
                           int i_flags, int i_hmargin, int i_vmargin,
                           mtime_t i_duration )
{
    mtime_t i_now = mdate();

    return osd_ShowTextAbsolute( p_spu, i_channel, psz_string,
                                  p_style, i_flags, i_hmargin, i_vmargin,
                                  i_now, i_now + i_duration );
}

/**
 * \brief Show text on the video from a given start date to a given end date
 * \param p_spu pointer to the subpicture queue the text is to be showed on
 * \param i_channel Subpicture channel
 * \param psz_string The text to be shown
 * \param p_style Pointer to a struct with text style info
 * \param i_flags flags for alignment and such
 * \param i_hmargin horizontal margin in pixels
 * \param i_vmargin vertical margin in pixels
 * \param i_start the time when this string is to appear on the video
 * \param i_stop the time when this string should stop to be displayed
 *               if this is 0 the string will be shown untill the next string
 *               is about to be shown
 */
int osd_ShowTextAbsolute( spu_t *p_spu_channel, int i_channel,
                           char *psz_string, text_style_t *p_style,
                           int i_flags, int i_hmargin, int i_vmargin,
                           mtime_t i_start, mtime_t i_stop )
{
    subpicture_t *p_spu;
    video_format_t fmt;

    if( !psz_string ) return VLC_EGENERIC;

    p_spu = spu_CreateSubpicture( p_spu_channel );
    if( !p_spu ) return VLC_EGENERIC;

    /* Create a new subpicture region */
    memset( &fmt, 0, sizeof(video_format_t) );
    fmt.i_chroma = VLC_FOURCC('T','E','X','T');
    fmt.i_aspect = 0;
    fmt.i_width = fmt.i_height = 0;
    fmt.i_x_offset = fmt.i_y_offset = 0;
    p_spu->p_region = p_spu->pf_create_region( VLC_OBJECT(p_spu_channel), &fmt );
    if( !p_spu->p_region )
    {
        msg_Err( p_spu_channel, "cannot allocate SPU region" );
        spu_DestroySubpicture( p_spu_channel, p_spu );
        return VLC_EGENERIC;
    }

    p_spu->p_region->psz_text = strdup( psz_string );
    p_spu->i_start = i_start;
    p_spu->i_stop = i_stop;
    p_spu->b_ephemer = VLC_TRUE;
    p_spu->b_absolute = VLC_FALSE;

    p_spu->i_x = i_hmargin;
    p_spu->i_y = i_vmargin;
    p_spu->i_flags = i_flags;
    p_spu->i_channel = i_channel;

    spu_DisplaySubpicture( p_spu_channel, p_spu );

    return VLC_SUCCESS;
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
    char *psz_string;
    va_list args;

    if( p_spu )
    {
        va_start( args, psz_format );
        vasprintf( &psz_string, psz_format, args );

        osd_ShowTextRelative( p_spu, i_channel, psz_string, NULL,
                               OSD_ALIGN_TOP|OSD_ALIGN_RIGHT, 30,20,1000000 );

        free( psz_string );
        va_end( args );
    }
}
