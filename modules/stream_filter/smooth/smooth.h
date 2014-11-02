/*****************************************************************************
 * smooth.h: misc. stuff
 *****************************************************************************
 * Copyright (C) 1996-2012 VLC authors and VideoLAN
 * $Id$
 *
 * Author: Frédéric Yhuel <fyhuel _AT_ viotech _DOT_ net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 *****************************************************************************/

#ifndef _VLC_SMOOTH_H
#define _VLC_SMOOTH_H 1

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_arrays.h>

//#define DISABLE_BANDWIDTH_ADAPTATION

typedef struct item_s
{
    uint64_t value;
    struct item_s *next;

} item_t;

typedef struct sms_queue_s
{
    unsigned length;
    item_t *first;
} sms_queue_t;

#define CHUNK_OFFSET_UNSET 0
#define CHUNK_OFFSET_0     1

typedef struct chunk_s chunk_t;
struct chunk_s
{
    uint64_t    duration;   /* chunk duration (seconds / TimeScale) */
    uint64_t    start_time; /* PTS (seconds / TimeScale) */
    uint64_t    size;       /* chunk size in bytes */
    uint64_t    offset;     /* offset in the media */
    uint64_t    read_pos;   /* position in the chunk */
    int         type;       /* video, audio, or subtitles */

    uint8_t     *data;
    chunk_t     *p_next;
};

typedef struct
{
    char *psz_key;
    char *psz_value;
} custom_attrs_t;

typedef struct quality_level_s
{
    int             Index;
    uint32_t        FourCC;
    unsigned        Bitrate;
    unsigned        MaxWidth;
    unsigned        MaxHeight;
    unsigned        SamplingRate;
    unsigned        Channels;
    unsigned        BitsPerSample;
    unsigned        AudioTag;
    unsigned        nBlockAlign;
    char            *CodecPrivateData; /* hex encoded string */
    DECL_ARRAY(custom_attrs_t *) custom_attrs;

} quality_level_t;

typedef struct sms_stream_s
{
    DECL_ARRAY( quality_level_t * ) qlevels; /* list of available Quality Levels */
    chunk_t        *p_chunks;      /* Time Ordered list of chunks */
    chunk_t        *p_lastchunk;   /* Time Ordered list of chunks's tail */
    chunk_t        *p_nextdownload; /* Pointer to next chunk to download */
    chunk_t        *p_playback;     /* Pointer to current playback chunk */
    vlc_mutex_t    chunks_lock;     /* chunks global lock */
    uint32_t       default_FourCC;
    unsigned       vod_chunks_nb;  /* total num of chunks of the VOD stream */
    unsigned       timescale;
    unsigned       qlevel_nb;      /* declared number of quality levels */
    unsigned       id;             /* track id, will be set arbitrarily */
    char           *name;
    char           *url_template;
    int            type;
    const quality_level_t *current_qlvl; /* current quality level for Download() */
} sms_stream_t;

struct stream_sys_t
{
    DECL_ARRAY( sms_stream_t * ) sms;          /* available streams */
    DECL_ARRAY( sms_stream_t * ) sms_selected; /* selected streams */

    uint64_t     vod_duration; /* total duration of the VOD media (seconds / TimeScale) */
    uint64_t     time_pos;
    unsigned     timescale;

    /* Download */
    struct
    {
        char        *base_url;    /* URL common part for chunks */
        unsigned     lookahead_count;/* max number of fragments ahead on server on live streaming */
        vlc_thread_t thread;      /* SMS chunk download thread */
        sms_queue_t *bws;         /* Measured bandwidths of the N last chunks */
        vlc_cond_t   wait;        /* some condition to wait on */
    } download;

    /* Playback */
    sms_stream_t *p_current_stream;
    vlc_mutex_t  lock;

    struct
    {
        uint64_t    boffset;     /* current byte offset in media */
        uint64_t    toffset;     /* current time offset in media */
        uint64_t    next_chunk_offset;
        struct
        {
            chunk_t *p_datachunk; /* the (re)init data chunk */
            const chunk_t *p_startchunk; /* reinit must be sent before this one */
        } init;
        vlc_mutex_t lock;
        vlc_cond_t  wait;         /* some condition to wait on */
    } playback;

    /* state */
    bool        b_live;      /* live stream? or vod? */
    bool        b_error;     /* parsing error */
    bool        b_close;     /* set by Close() */
};

#define SMS_GET4BYTES( dst ) do { \
    dst = U32_AT( slice ); \
    slice += 4; \
  } while(0)

#define SMS_GET1BYTE( dst ) do { \
    dst = *slice; \
    slice += 1; \
  } while(0)

#define SMS_GET3BYTES( dst ) do { \
    dst = Get24bBE( slice ); \
    slice += 3; \
  } while(0)

#define SMS_GET8BYTES( dst ) do { \
    dst = U64_AT( slice ); \
    slice += 8; \
  } while(0)

#define SMS_GET4or8BYTES( dst ) \
    if( (version) == 0 ) \
        SMS_GET4BYTES( dst ); \
    else \
        SMS_GET8BYTES( dst ); \

#define SMS_GETFOURCC( dst ) do { \
    memcpy( &dst, slice, 4 ); \
    slice += 4; \
  } while(0)

#define SMS_GET_SELECTED_ST( cat ) \
    sms_get_stream_by_cat( p_sys, cat )

void sms_queue_free( sms_queue_t* );
sms_queue_t *sms_queue_init( const unsigned int );
int sms_queue_put( sms_queue_t *, const uint64_t );
uint64_t sms_queue_avg( sms_queue_t *);
void* sms_Thread( void *);
quality_level_t * ql_New( void );
void ql_Free( quality_level_t *);
chunk_t *chunk_AppendNew( sms_stream_t* , uint64_t , uint64_t );
void chunk_Free( chunk_t *);
sms_stream_t * sms_New( void );
void sms_Free( sms_stream_t *);
uint8_t *decode_string_hex_to_binary( const char * );
sms_stream_t * sms_get_stream_by_cat( stream_sys_t *, int );
bool no_more_chunks( stream_sys_t * );
int index_to_es_cat( int );
int es_cat_to_index( int );
void resetChunksState( stream_sys_t * );
#endif
