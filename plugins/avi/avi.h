/*****************************************************************************
 * avi.h : AVI file Stream input module for vlc
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: avi.h,v 1.4 2002/05/02 10:54:34 fenrir Exp $
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

/* flags for use in <dwFlags> in AVIFileHdr */
#define AVIF_HASINDEX       0x00000010  /* Index at end of file? */
#define AVIF_MUSTUSEINDEX   0x00000020
#define AVIF_ISINTERLEAVED  0x00000100
#define AVIF_TRUSTCKTYPE    0x00000800  /* Use CKType to find key frames? */
#define AVIF_WASCAPTUREFILE 0x00010000
#define AVIF_COPYRIGHTED    0x00020000

/* Flags for index */
#define AVIIF_LIST          0x00000001L /* chunk is a 'LIST' */
#define AVIIF_KEYFRAME      0x00000010L /* this frame is a key frame.*/
#define AVIIF_NOTIME        0x00000100L /* this frame doesn't take any time */
#define AVIIF_COMPUSE       0x0FFF0000L /* these bits are for compressor use */

/* Sound formats */
#define WAVE_FORMAT_UNKNOWN         0x0000
#define WAVE_FORMAT_PCM             0x0001
#define WAVE_FORMAT_MPEG            0x0050
#define WAVE_FORMAT_MPEGLAYER3      0x0055
#define WAVE_FORMAT_AC3             0x2000

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

typedef struct waveformatex_s
{
    u16 i_formattag;
    u16 i_channels;
    u32 i_samplespersec;
    u32 i_avgbytespersec;
    u16 i_blockalign;
    u16 i_bitspersample;
    u16 i_size; /* the extra size in bytes */
} waveformatex_t;


typedef struct MainAVIHeader_s
{
    u32 i_microsecperframe;
    u32 i_maxbytespersec;
    u32 i_reserved1; /* dwPaddingGranularity;    pad to multiples of this
                         size; normally 2K */
    u32 i_flags;
    u32 i_totalframes;
    u32 i_initialframes;
    u32 i_streams;
    u32 i_suggestedbuffersize;
    u32 i_width;
    u32 i_height;
    u32 i_scale;
    u32 i_rate;
    u32 i_start;
    u32 i_length;

} MainAVIHeader_t;

typedef struct AVIStreamHeader_s
{
    u32 i_type;
    u32 i_handler;
    u32 i_flags;
    u32 i_reserved1;    /* wPriority wLanguage */
    u32 i_initialframes;
    u32 i_scale;
    u32 i_rate;
    u32 i_start;
    u32 i_length;       /* In units above... */
    u32 i_suggestedbuffersize;
    u32 i_quality;
    u32 i_samplesize;

} AVIStreamHeader_t;

typedef struct AVIIndexEntry_s
{
    u32 i_id;
    u32 i_flags;
    u32 i_offset;
    u32 i_length;
    u32 i_lengthtotal;
} AVIIndexEntry_t;

typedef struct AVIStreamInfo_s
{

    riffchunk_t *p_strl;
    riffchunk_t *p_strh;
    riffchunk_t *p_strf;
    riffchunk_t *p_strd; /* not used */
    
    AVIStreamHeader_t header;
    
    u8 i_cat;           /* AUDIO_ES, VIDEO_ES */
    bitmapinfoheader_t  video_format;
    waveformatex_t      audio_format;
    es_descriptor_t     *p_es;   
    int                 b_selected; /* newly selected */
    AVIIndexEntry_t     *p_index;
    int                 i_idxnb;
    int                 i_idxmax; 
    int                 i_idxposc;  /* numero of chunk */
    int                 i_idxposb;  /* byte in the current chunk */
    off_t               i_idxoffset; /* how many to add to index.i_pos */
} AVIStreamInfo_t;

typedef struct demux_data_avi_file_s
{
    mtime_t i_date; 
    int     i_rate;
    riffchunk_t *p_riff;
    riffchunk_t *p_hdrl;
    riffchunk_t *p_movi;
    riffchunk_t *p_idx1;

    /* Info extrated from avih */
    MainAVIHeader_t avih;

    /* number of stream and informations*/
    int i_streams;
    AVIStreamInfo_t   **pp_info; 

    /* current audio and video es */
    AVIStreamInfo_t *p_info_video;
    AVIStreamInfo_t *p_info_audio;

} demux_data_avi_file_t;
 
