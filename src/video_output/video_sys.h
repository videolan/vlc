/*****************************************************************************
 * video_sys.h: system dependant video output display method API
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 *
 * Authors:
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *****************************************************************************/

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
int     vout_SysCreate   ( p_vout_thread_t p_vout, char *psz_display, int i_root_window );
int     vout_SysInit     ( p_vout_thread_t p_vout );
void    vout_SysEnd      ( p_vout_thread_t p_vout );
void    vout_SysDestroy  ( p_vout_thread_t p_vout );
int     vout_SysManage   ( p_vout_thread_t p_vout );
void    vout_SysDisplay  ( p_vout_thread_t p_vout );

