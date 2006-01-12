/*****************************************************************************
 * plugin.h:
 *****************************************************************************
 * Copyright (C) 2004 the VideoLAN team
 * $Id$
 *
 * Authors: Cyril Deguet <asmax@videolan.org>
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

#ifndef _GALAKTOS_PLUGIN_H_
#define _GALAKTOS_PLUGIN_H_

#include <vlc/vlc.h>
#include <vlc/aout.h>
#include <vlc/vout.h>

#define MAX_BLOCKS 10

typedef struct
{
    VLC_COMMON_MEMBERS

    char          *psz_title;

    /* OpenGL provider */
    vout_thread_t *p_opengl;
    module_t      *p_module;

    /* Window properties */
    int           i_width;
    int           i_height;
    int           b_fullscreen;

    /* Audio properties */
    int           i_channels;

    /* Audio buffer */
    int16_t       p_data[2][512];
    int           i_cur_sample;

    /* OS specific data */
    void          *p_os_data;

} galaktos_thread_t;

#endif
