/*****************************************************************************
 * vlc_opengl.h: OpenGL provider interface
 *****************************************************************************
 * Copyright (C) 2004 VideoLAN
 * $Id$
 *
 * Authors: Cyril Deguet  <asmax@videolan.org>
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

#ifndef _VLC_OPENGL_H
#define _VLC_OPENGL_H 1


struct opengl_t
{
    VLC_COMMON_MEMBERS

    int             i_width;        /* window width */
    int             i_height;       /* window height */
    int             b_fullscreen;   /* fullscreen flag */

    opengl_sys_t    *p_sys;         /* private data */

    /* Create an OpenGL window of the requested size (if possible) */
    int     ( *pf_init )( opengl_t *, int i_width, int i_height );

    /* Swap front/back buffers */
    void    ( *pf_swap )( opengl_t * );

    /* Handle window events */
    int     ( *pf_handle_events )( opengl_t * );
};

#endif
