/*****************************************************************************
 * avi.h : AVI file Stream input module for vlc
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: avi.h,v 1.10 2003/05/03 01:12:13 fenrir Exp $
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
    vlc_fourcc_t i_fourcc;
    off_t        i_pos;
    uint32_t     i_size;
    vlc_fourcc_t i_type;     // only for AVIFOURCC_LIST

    uint8_t      i_peek[8];  //first 8 bytes

    unsigned int i_stream;
    unsigned int i_cat;
} avi_packet_t;


typedef struct AVIIndexEntry_s
{
    vlc_fourcc_t i_id;
    uint32_t     i_flags;
    off_t        i_pos;
    uint32_t     i_length;
    uint32_t     i_lengthtotal;

} AVIIndexEntry_t;

typedef struct avi_stream_s
{
    vlc_bool_t      b_activated;

    unsigned int    i_cat; /* AUDIO_ES, VIDEO_ES */
    vlc_fourcc_t    i_fourcc;
    vlc_fourcc_t    i_codec;

    int             i_rate;
    int             i_scale;
    int             i_samplesize;

    es_descriptor_t     *p_es;

    AVIIndexEntry_t     *p_index;
    unsigned int        i_idxnb;
    unsigned int        i_idxmax;

    unsigned int        i_idxposc;  /* numero of chunk */
    unsigned int        i_idxposb;  /* byte in the current chunk */

} avi_stream_t;

struct demux_sys_t
{
    mtime_t i_time;
    mtime_t i_length;
    mtime_t i_pcr;

    vlc_bool_t  b_seekable;
    avi_chunk_t ck_root;

    vlc_bool_t  b_odml;

    off_t   i_movi_begin;
    off_t   i_movi_lastchunk_pos; /* XXX position of last valid chunk */

    /* number of streams and information */
    unsigned int i_streams;
    avi_stream_t  **pp_info;

#ifdef __AVI_SUBTITLE__
    subtitle_demux_t    *p_sub;
#endif

};

