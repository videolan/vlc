/*****************************************************************************
 * avi.h : AVI file Stream input module for vlc
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: avi.h,v 1.4 2002/10/15 00:55:07 fenrir Exp $
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

#define 	MAX_PACKETS_IN_FIFO 	2

typedef struct AVIIndexEntry_s
{
    u32 i_id;
    u32 i_flags;
    u32 i_pos;
    u32 i_length;
    u32 i_lengthtotal;
} AVIIndexEntry_t;

typedef struct AVIESBuffer_s
{
    struct AVIESBuffer_s *p_next;

    pes_packet_t *p_pes;
    int i_posc;
    int i_posb;
} AVIESBuffer_t;


typedef struct AVIStreamInfo_s
{
    int i_cat;           /* AUDIO_ES, VIDEO_ES */
    int i_activated;
    vlc_fourcc_t    i_fourcc;
    vlc_fourcc_t    i_codec;

    int             i_rate;
    int             i_scale;
    int             i_samplesize;
    
    es_descriptor_t     *p_es;   
    int                 b_selected; /* newly selected */
    AVIIndexEntry_t     *p_index;
    int                 i_idxnb;
    int                 i_idxmax; 

    int                 i_idxposc;  /* numero of chunk */
    int                 i_idxposb;  /* byte in the current chunk */

    /* add some buffering */
    AVIESBuffer_t       *p_pes_first;
    AVIESBuffer_t       *p_pes_last;
    int                 i_pes_count;
    int                 i_pes_totalsize;
} AVIStreamInfo_t;

struct demux_sys_t
{
    mtime_t i_time;
    mtime_t i_length;
    mtime_t i_pcr; 
    int     i_rate;
    riffchunk_t *p_movi;

    int     b_seekable;
    avi_chunk_t ck_root;
    
    /* Info extrated from avih */

    /* number of stream and informations*/
    int i_streams;
    AVIStreamInfo_t   **pp_info; 

    /* current audio and video es */
    AVIStreamInfo_t *p_info_video;
    AVIStreamInfo_t *p_info_audio;
};

