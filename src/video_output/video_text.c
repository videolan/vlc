/*****************************************************************************
 * video_text.c : text manipulation functions
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: video_text.c,v 1.45 2003/08/04 23:35:25 gbazin Exp $
 *
 * Authors: Sigmund Augdal <sigmunau@idi.ntnu.no>
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
void vout_ShowTextRelative( vout_thread_t *p_vout, char *psz_string, 
			      text_style_t *p_style, int i_flags, 
			      int i_hmargin, int i_vmargin, 
			      mtime_t i_duration )
{
    mtime_t i_now = mdate();
    if ( p_vout->pf_add_string )
    {
	p_vout->pf_add_string( p_vout, psz_string, p_style, i_flags, i_hmargin,
			       i_vmargin, i_now, i_now + i_duration );
    }
    else
    {
	msg_Warn( p_vout, "No text renderer found" );
    }
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
