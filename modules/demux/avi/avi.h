/*****************************************************************************
 * avi.h : AVI file Stream input module for vlc
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: avi.h,v 1.5 2002/10/27 15:37:16 fenrir Exp $
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


typedef struct avi_packet_s
{
    u32     i_fourcc;
    off_t   i_pos;
    u32     i_size;
    u32     i_type;     // only for AVIFOURCC_LIST
    
    u8      i_peek[8];  //first 8 bytes

    int     i_stream;
    int     i_cat;
} avi_packet_t;


typedef struct AVIIndexEntry_s
{
    u32 i_id;
    u32 i_flags;
    u32 i_pos;
    u32 i_length;
    u32 i_lengthtotal;

} AVIIndexEntry_t;

typedef struct avi_stream_s
{
    int i_activated;

    int i_cat;           /* AUDIO_ES, VIDEO_ES */
    vlc_fourcc_t    i_fourcc;
    vlc_fourcc_t    i_codec;

    int             i_rate;
    int             i_scale;
    int             i_samplesize;
    
    es_descriptor_t     *p_es;

    AVIIndexEntry_t     *p_index;
    int                 i_idxnb;
    int                 i_idxmax; 

    int                 i_idxposc;  /* numero of chunk */
    int                 i_idxposb;  /* byte in the current chunk */

} avi_stream_t;

struct demux_sys_t
{
    mtime_t i_time;
    mtime_t i_length;
    mtime_t i_pcr; 

    int     b_seekable;
    avi_chunk_t ck_root;
    
    off_t   i_movi_begin;
    off_t   i_movi_lastchunk_pos; /* XXX position of last valid chunk */
    
    /* number of stream and informations*/
    int i_streams;
    avi_stream_t  **pp_info; 

};

