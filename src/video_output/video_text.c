/*****************************************************************************
 * video_text.c : text manipulation functions
 *****************************************************************************
 * Copyright (C) 1999-2004 VideoLAN
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
#include "vlc_block.h"
#include "vlc_filter.h"
#include "osd.h"

/**
 * \brief Show text on the video for some time
 * \param p_vout pointer to the vout the text is to be showed on
 * \param i_channel Subpicture channel
 * \param psz_string The text to be shown
 * \param p_style Pointer to a struct with text style info
 * \param i_flags flags for alignment and such
 * \param i_hmargin horizontal margin in pixels
 * \param i_vmargin vertical margin in pixels
 * \param i_duration Amount of time the text is to be shown.
 */
int vout_ShowTextRelative( vout_thread_t *p_vout, int i_channel,
                           char *psz_string, text_style_t *p_style,
                           int i_flags, int i_hmargin, int i_vmargin,
                           mtime_t i_duration )
{
    subpicture_t *p_subpic = NULL;
    mtime_t i_now = mdate();

    if( p_vout->p_text && p_vout->p_text->p_module &&
        p_vout->p_text->pf_render_string )
    {
        block_t *p_block = block_New( p_vout, strlen(psz_string) + 1 );
        if( p_block )
        {
            memcpy( p_block->p_buffer, psz_string, p_block->i_buffer );
            p_block->i_pts = p_block->i_dts = i_now;
            p_block->i_length = i_duration;

            p_subpic = p_vout->p_text->pf_render_string( p_vout->p_text,
                                                         p_block );
            if( p_subpic )
            {
                p_subpic->i_x = i_hmargin;
                p_subpic->i_y = i_vmargin;
                p_subpic->i_flags = i_flags;
                p_subpic->i_channel = i_channel;

                vout_DisplaySubPicture( p_vout, p_subpic );
                return VLC_SUCCESS;
            }
        }
        return VLC_EGENERIC;
    }
    else
    {
        msg_Warn( p_vout, "No text renderer found" );
        return VLC_EGENERIC;
    }
}

/**
 * \brief Show text on the video from a given start date to a given end date
 * \param p_vout pointer to the vout the text is to be showed on
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
int vout_ShowTextAbsolute( vout_thread_t *p_vout, int i_channel,
                           char *psz_string, text_style_t *p_style,
                           int i_flags, int i_hmargin, int i_vmargin,
                           mtime_t i_start, mtime_t i_stop )
{
    subpicture_t *p_subpic = NULL;

    if( p_vout->p_text && p_vout->p_text->p_module &&
        p_vout->p_text->pf_render_string )
    {
        block_t *p_block = block_New( p_vout, strlen(psz_string) + 1 );
        if( p_block )
        {
            memcpy( p_block->p_buffer, psz_string, p_block->i_buffer );
            p_block->i_pts = p_block->i_dts = i_start;
            p_block->i_length = i_stop - i_start;

            p_subpic = p_vout->p_text->pf_render_string( p_vout->p_text,
                                                         p_block );
            if( p_subpic )
            {
                p_subpic->i_x = i_hmargin;
                p_subpic->i_y = i_vmargin;
                p_subpic->i_flags = i_flags;
                p_subpic->i_channel = i_channel;

                vout_DisplaySubPicture( p_vout, p_subpic );
                return VLC_SUCCESS;
            }
        }
        return VLC_EGENERIC;
    }
    else
    {
        msg_Warn( p_vout, "No text renderer found" );
        return VLC_EGENERIC;
    }
}


/**
 * \brief Write an informative message at the default location,
 *        for the default duration and only if the OSD option is enabled.
 * \param p_caller The object that called the function.
 * \param i_channel Subpicture channel
 * \param psz_format printf style formatting
 **/
void __vout_OSDMessage( vlc_object_t *p_caller, int i_channel,
                        char *psz_format, ... )
{
    vout_thread_t *p_vout;
    char *psz_string;
    va_list args;

    if( !config_GetInt( p_caller, "osd" ) ) return;

    p_vout = vlc_object_find( p_caller, VLC_OBJECT_VOUT, FIND_ANYWHERE );

    if( p_vout )
    {
        va_start( args, psz_format );
        vasprintf( &psz_string, psz_format, args );

        vout_ShowTextRelative( p_vout, i_channel, psz_string, NULL,
                               OSD_ALIGN_TOP|OSD_ALIGN_RIGHT, 30,20,1000000 );

        vlc_object_release( p_vout );
        free( psz_string );
        va_end( args );
    }
}

