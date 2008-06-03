/*****************************************************************************
 * chroma.h: libavutil <-> libvlc conversion routines
 *****************************************************************************
 * Copyright (C) 1999-2008 the VideoLAN team
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Gildas Bazin <gbazin@videolan.org>
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

/*****************************************************************************
 * Chroma fourcc -> ffmpeg_id mapping
 *****************************************************************************/
static struct
{
    vlc_fourcc_t  i_chroma;
    int  i_chroma_id;

} chroma_table[] =
{
    /* Planar YUV formats */
    { VLC_FOURCC('Y','U','V','A'), PIX_FMT_YUV444P }, /* Hack */
    { VLC_FOURCC('I','4','4','4'), PIX_FMT_YUV444P },
    { VLC_FOURCC('J','4','4','4'), PIX_FMT_YUVJ444P },
    { VLC_FOURCC('I','4','2','2'), PIX_FMT_YUV422P },
    { VLC_FOURCC('J','4','2','2'), PIX_FMT_YUVJ422P },
    { VLC_FOURCC('I','4','2','0'), PIX_FMT_YUV420P },
    { VLC_FOURCC('Y','V','1','2'), PIX_FMT_YUV420P },
    { VLC_FOURCC('I','Y','U','V'), PIX_FMT_YUV420P },
    { VLC_FOURCC('J','4','2','0'), PIX_FMT_YUVJ420P },
    { VLC_FOURCC('I','4','1','1'), PIX_FMT_YUV411P },
    { VLC_FOURCC('I','4','1','0'), PIX_FMT_YUV410P },
    { VLC_FOURCC('Y','V','U','9'), PIX_FMT_YUV410P },

    /* Packed YUV formats */
    { VLC_FOURCC('Y','U','Y','2'), PIX_FMT_YUV422 },
    { VLC_FOURCC('Y','U','Y','V'), PIX_FMT_YUV422 },
    { VLC_FOURCC('U','Y','V','Y'), PIX_FMT_UYVY422 },

    /* Packed RGB formats */
#if defined(WORDS_BIGENDIAN)
    { VLC_FOURCC('R','G','B','8'), PIX_FMT_BGR8 },
    { VLC_FOURCC('R','V','1','5'), PIX_FMT_BGR555 },
    { VLC_FOURCC('R','V','1','6'), PIX_FMT_BGR565 },
    { VLC_FOURCC('R','V','2','4'), PIX_FMT_BGR24 },
#else
#if defined(PIX_FMT_RGB8)
    { VLC_FOURCC('R','G','B','8'), PIX_FMT_RGB8 },
#endif
    { VLC_FOURCC('R','V','1','5'), PIX_FMT_RGB555 },
    { VLC_FOURCC('R','V','1','6'), PIX_FMT_RGB565 },
    { VLC_FOURCC('R','V','2','4'), PIX_FMT_RGB24 },
#endif
    { VLC_FOURCC('R','V','3','2'), PIX_FMT_RGBA32 },
#if defined(PIX_FMT_RGBA)
    { VLC_FOURCC('R','G','B','A'), PIX_FMT_RGBA },
#endif
    { VLC_FOURCC('G','R','E','Y'), PIX_FMT_GRAY8 },

    { 0, 0 }
};

static int GetFfmpegChroma( vlc_fourcc_t i_chroma )
{
    int i;

    for( i = 0; chroma_table[i].i_chroma != 0; i++ )
    {
        if( chroma_table[i].i_chroma == i_chroma )
            return chroma_table[i].i_chroma_id;
    }
    return -1;
}

static vlc_fourcc_t GetVlcChroma( int i_ffmpeg_chroma )
{
    int i;

    for( i = 0; chroma_table[i].i_chroma != 0; i++ )
    {
        if( chroma_table[i].i_chroma_id == i_ffmpeg_chroma )
            return chroma_table[i].i_chroma;
    }
    return 0;
}
