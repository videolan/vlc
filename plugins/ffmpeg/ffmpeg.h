/*****************************************************************************
 * ffmpeg_vdec.h: video decoder using ffmpeg library
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: ffmpeg.h,v 1.1 2002/04/23 23:44:36 fenrir Exp $
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

typedef struct videodec_thread_s
{
    decoder_config_t    *p_config;
    decoder_fifo_t      *p_fifo;    

    bitmapinfoheader_t  format;

    AVCodecContext      context, *p_context;
    AVCodec             *p_codec;
    vout_thread_t       *p_vout; 
    int                 i_aspect;
    u32                 i_chroma;
    char *psz_namecodec;
    /* private */
    data_packet_t       *p_data;
    byte_t              *p_buff;
    int                 i_data_size;
} videodec_thread_t;
