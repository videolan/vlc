/*****************************************************************************
 * avformat.h: muxer and demuxer using libavformat
 *****************************************************************************
 * Copyright (C) 2001-2008 the VideoLAN team
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
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

void LibavcodecCallback( void *p_opaque, int i_level,
                             const char *psz_format, va_list va );

/* Demux module */
int  OpenDemux ( vlc_object_t * );
void CloseDemux( vlc_object_t * );

/* Mux module */
int  OpenMux ( vlc_object_t * );
void CloseMux( vlc_object_t * );

#define MUX_TEXT N_("Ffmpeg mux")
#define MUX_LONGTEXT N_("Force use of ffmpeg muxer.")
