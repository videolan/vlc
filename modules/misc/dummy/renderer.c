/*****************************************************************************
 * renderer.c : dummy text rendering functions
 *****************************************************************************
 * Copyright (C) 2000, 2001 VideoLAN
 * $Id: renderer.c,v 1.1 2003/07/14 22:25:13 sigmunau Exp $
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

#include <vlc/vlc.h>
#include <vlc/vout.h>

static int  AddText   ( vout_thread_t *, char *, text_style_t *, int, 
			int, int, mtime_t, mtime_t );

int E_(OpenRenderer)( vlc_object_t *p_this )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;
    p_vout->pf_add_string = AddText;
    return VLC_SUCCESS;
}

static int  AddText   ( vout_thread_t *p_vout, char *psz_string,
			text_style_t *p_style , int i_flags, int i_x_margin,
			int i_y_margin, mtime_t i_start, mtime_t i_stop )
{
    return VLC_SUCCESS;
}
