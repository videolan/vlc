/*****************************************************************************
 * dvd.h: structure of the dvdplay plugin
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: dvd.h,v 1.2 2002/11/06 18:07:57 sam Exp $
 *
 * Author: Stéphane Borel <stef@via.ecp.fr>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <dvdread/dvd_reader.h>
#include <dvdread/ifo_types.h>
#include <dvdread/ifo_read.h>
#include <dvdread/nav_read.h>
#include <dvdread/nav_print.h>

#include <dvdplay/dvdplay.h>
#include <dvdplay/info.h>
#include <dvdplay/nav.h>
#include <dvdplay/state.h>

#define LB2OFF(x) ((off_t)(x) * (off_t)(DVD_VIDEO_LB_LEN))
#define OFF2LB(x) ((x) / DVD_VIDEO_LB_LEN)


/*****************************************************************************
 * dvd_data_t: structure for communication between dvdplay access, demux
 * and intf.
 *****************************************************************************/
typedef struct
{
    dvdplay_ptr             vmg;
    intf_thread_t *         p_intf;

    int                     i_audio_nb;
    int                     i_spu_nb;

    int                     i_still_time;
    vlc_bool_t              b_end_of_cell;

    dvdplay_event_t         event;
    dvdplay_ctrl_t          control;   
    dvdplay_highlight_t     hli;

} dvd_data_t;

