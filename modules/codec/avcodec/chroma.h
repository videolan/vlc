/*****************************************************************************
 * chroma.h: decoder and encoder using libavcodec
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

/* VLC <-> avutil tables */

#ifndef _VLC_AVUTIL_CHROMA_H
#define _VLC_AVUTIL_CHROMA_H 1

int TestFfmpegChroma( const int i_ffmpeg_id, const vlc_fourcc_t i_vlc_fourcc );
int GetFfmpegChroma( int *i_ffmpeg_chroma, const video_format_t fmt );
int GetVlcChroma( video_format_t *fmt, const int i_ffmpeg_chroma );

#endif
