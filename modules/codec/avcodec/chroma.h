/*****************************************************************************
 * chroma.h: decoder and encoder using libavcodec
 *****************************************************************************
 * Copyright (C) 2001-2008 VLC authors and VideoLAN
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

/* VLC <-> avutil tables */

#ifndef VLC_AVUTIL_CHROMA_H_
#define VLC_AVUTIL_CHROMA_H_

int FindFfmpegChroma( vlc_fourcc_t );
int GetFfmpegChroma( int *i_ffmpeg_chroma, const video_format_t *fmt );

vlc_fourcc_t FindVlcChroma( int );
int GetVlcChroma( video_format_t *fmt, int i_ffmpeg_chroma );

#endif
