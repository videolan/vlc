/*****************************************************************************
 * screen.h: Screen capture module.
 *****************************************************************************
 * Copyright (C) 2004 VideoLAN (Centrale Réseaux) and its contributors
 * $Id$
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
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

typedef struct screen_data_t screen_data_t;

struct demux_sys_t
{
    es_format_t fmt;
    es_out_id_t *es;

    float f_fps;
    mtime_t i_next_date;
    int i_incr;

    screen_data_t *p_data;
};

int      screen_InitCapture ( demux_t * );
int      screen_CloseCapture( demux_t * );
block_t *screen_Capture( demux_t * );

