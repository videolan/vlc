/*****************************************************************************
 * ffmpeg_vdec.h: video decoder using ffmpeg library
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: ffmpeg.h,v 1.4 2002/07/15 19:33:02 fenrir Exp $
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/* Pour un flux video */
typedef struct bitmapinfoheader_s
{
    u32 i_size; /* size of header */
    u32 i_width;
    u32 i_height;
    u16 i_planes;
    u16 i_bitcount;
    u32 i_compression;
    u32 i_sizeimage;
    u32 i_xpelspermeter;
    u32 i_ypelspermeter;
    u32 i_clrused;
    u32 i_clrimportant;
} bitmapinfoheader_t;

static struct 
{
    int i_vlc_codec;
    int i_ffmpeg_codec;
    char *psz_name;
} ffmpeg_Codecs [] = {

#if LIBAVCODEC_BUILD >= 4608 
    { MSMPEG4v1_VIDEO_ES,   CODEC_ID_MSMPEG4V1, "MS MPEG-4 v1" },
    { MSMPEG4v2_VIDEO_ES,   CODEC_ID_MSMPEG4V2, "MS MPEG-4 v2" },
    { MSMPEG4v3_VIDEO_ES,   CODEC_ID_MSMPEG4V3, "MS MPEG-4 v3" },
#else
    { MSMPEG4v3_VIDEO_ES,   CODEC_ID_MSMPEG4,   "MS MPEG-4 v3" },
#endif
#if LIBAVCODEC_BUILD >= 4615
    { SVQ1_VIDEO_ES,    CODEC_ID_SVQ1,  "SVQ-1 (Sorenson Video v1)" },
#endif
    { MPEG4_VIDEO_ES,   CODEC_ID_MPEG4, "MPEG-4" },
    { H263_VIDEO_ES,    CODEC_ID_H263,  "H263" },
    { I263_VIDEO_ES,    CODEC_ID_H263I, "H263.I" },
/* this entry is to recognize the end */
    { 0,                CODEC_ID_NONE,  "Unknown" }
};


static int ffmpeg_GetFfmpegCodec( int i_vlc_codec, 
                                  int *pi_ffmpeg_codec,
                                  char **ppsz_name )
{
    int i_codec;

    for( i_codec = 0; ; i_codec++ )
    {
        if( ( ffmpeg_Codecs[i_codec].i_vlc_codec == i_vlc_codec )||
            ( ffmpeg_Codecs[i_codec].i_ffmpeg_codec == CODEC_ID_NONE ) )
        {
            break;
        }
    }
    if( pi_ffmpeg_codec )
        *pi_ffmpeg_codec = ffmpeg_Codecs[i_codec].i_ffmpeg_codec;
    if( ppsz_name )
        *ppsz_name = ffmpeg_Codecs[i_codec].psz_name;

    return( ffmpeg_Codecs[i_codec].i_ffmpeg_codec 
                == CODEC_ID_NONE ? 0 : 1);
}




typedef struct videodec_thread_s
{
    decoder_fifo_t      *p_fifo;    

    bitmapinfoheader_t  format;

    AVCodecContext      context, *p_context;
    AVCodec             *p_codec;
    vout_thread_t       *p_vout; 

    char *psz_namecodec;
    /* private */
    mtime_t i_pts;
    int     i_framesize;
    byte_t  *p_framedata;
} videodec_thread_t;
