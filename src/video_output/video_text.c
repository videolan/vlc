/*****************************************************************************
 * video_text.c : text manipulation functions
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: video_text.c,v 1.49 2003/12/22 14:32:57 sam Exp $
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
#include <osd.h>

/**
 * \brief Show text on the video for some time
 * \param p_vout pointer to the vout the text is to be showed on
 * \param psz_string The text to be shown
 * \param p_style Pointer to a struct with text style info
 * \param i_flags flags for alignment and such
 * \param i_hmargin horizontal margin in pixels
 * \param i_vmargin vertical margin in pixels
 * \param i_duration Amount of time the text is to be shown.
 */
subpicture_t *vout_ShowTextRelative( vout_thread_t *p_vout, char *psz_string,
                              text_style_t *p_style, int i_flags,
                              int i_hmargin, int i_vmargin,
                              mtime_t i_duration )
{
    subpicture_t *p_subpic = NULL;
    mtime_t i_now = mdate();

    if ( p_vout->pf_add_string )
    {
        p_subpic = p_vout->pf_add_string( p_vout, psz_string, p_style, i_flags,
                             i_hmargin, i_vmargin, i_now, i_now + i_duration );
    }
    else
    {
        msg_Warn( p_vout, "No text renderer found" );
    }

    return p_subpic;
}

/**
 * \brief Show text on the video from a given start date to a given end date
 * \param p_vout pointer to the vout the text is to be showed on
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
void vout_ShowTextAbsolute( vout_thread_t *p_vout, char *psz_string,
                              text_style_t *p_style, int i_flags,
                              int i_hmargin, int i_vmargin, mtime_t i_start,
                              mtime_t i_stop )
{
    if ( p_vout->pf_add_string )
    {
        p_vout->pf_add_string( p_vout, psz_string, p_style, i_flags, i_hmargin,
                               i_vmargin, i_start, i_stop );
    }
    else
    {
        msg_Warn( p_vout, "No text renderer found" );
    }
}


/**
 * \brief Write an informative message at the default location,
 *        for the default duration and only if the OSD option is enabled.
 * \param p_caller The object that called the function.
 * \param psz_string The text to be shown
 **/
void vout_OSDMessage( vlc_object_t *p_caller, char *psz_string )
{
    vout_thread_t *p_vout;

    if( !config_GetInt( p_caller, "osd" ) ) return;

    p_vout = vlc_object_find( p_caller, VLC_OBJECT_VOUT, FIND_ANYWHERE );

    if( p_vout )
    {
        vlc_mutex_lock( &p_vout->change_lock );

        if( p_vout->p_last_osd_message )
        {
            vout_DestroySubPicture( p_vout, p_vout->p_last_osd_message );
        }

        p_vout->p_last_osd_message = vout_ShowTextRelative( p_vout, psz_string,
                        NULL, OSD_ALIGN_TOP|OSD_ALIGN_RIGHT, 30,20,1000000 );

        vlc_mutex_unlock( &p_vout->change_lock );

        vlc_object_release( p_vout );
    }
}

