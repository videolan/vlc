/*****************************************************************************
 * plugin.h:
 *****************************************************************************
 * Copyright (C) 2004 VideoLAN
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

#ifndef _GALAKTOS_PLUGIN_H_
#define _GALAKTOS_PLUGIN_H_

#include <vlc/vlc.h>
#include <vlc/aout.h>

#define MAX_BLOCKS 10

typedef struct
{
    VLC_COMMON_MEMBERS
    vout_thread_t *p_vout;

    char          *psz_title;

    vlc_mutex_t   lock;
    vlc_cond_t    wait;

    /* Audio properties */
    int i_channels;

    /* Audio samples queue */
    block_t       *pp_blocks[MAX_BLOCKS];
    int           i_blocks;

    audio_date_t  date;

    /* OS specific data */
    void          *p_os_data;

} galaktos_thread_t;

#endif
