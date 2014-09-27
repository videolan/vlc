/*****************************************************************************
 * essetup.h: es setup from stsd and extensions parsing
 *****************************************************************************
 * Copyright (C) 2001-2004, 2010, 2014 VLC authors and VideoLAN
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/
#ifndef _VLC_MP4_ESSETUP_H
#define _VLC_MP4_ESSETUP_H 1

#include "mp4.h"

int SetupVideoES( demux_t *p_demux, mp4_track_t *p_track, MP4_Box_t *p_sample );
int SetupAudioES( demux_t *p_demux, mp4_track_t *p_track, MP4_Box_t *p_sample );
int SetupSpuES( demux_t *p_demux, mp4_track_t *p_track, MP4_Box_t *p_sample );

#endif
