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

//#define DISABLE_BANDWIDTH_ADAPTATION

typedef struct item_s
{
    uint64_t value;
    struct item_s *next;

} item_t;

typedef struct sms_queue_s
{
    int length;
    item_t *first;
} sms_queue_t;

typedef struct chunk_s
{
    int64_t     duration;   /* chunk duration (seconds / TimeScale) */
    int64_t     start_time; /* PTS (seconds / TimeScale) */
    int         size;       /* chunk size in bytes */
    unsigned    sequence;   /* unique sequence number */
    uint64_t    offset;     /* offset in the media */
    int         read_pos;   /* position in the chunk */
    int         type;       /* video, audio, or subtitles */

    uint8_t     *data;
} chunk_t;

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
    unsigned        id;
    char            *CodecPrivateData; /* hex encoded string */

} quality_level_t;

typedef struct sms_stream_s
{
    vlc_array_t    *qlevels;       /* list of available Quality Levels */
    vlc_array_t    *chunks;        /* list of chunks */
    uint32_t       default_FourCC;
    unsigned       vod_chunks_nb;  /* total num of chunks of the VOD stream */
    unsigned       timescale;
    unsigned       qlevel_nb;      /* number of quality levels */
    unsigned       id;             /* track id, will be set arbitrarily */
    char           *name;
    char           *url_template;
    int            type;
    unsigned       download_qlvl; /* current quality level ID for Download() */

} sms_stream_t;

struct stream_sys_t
{
    char         *base_url;    /* URL common part for chunks */
    vlc_thread_t thread;       /* SMS chunk download thread */

    vlc_array_t  *sms_streams; /* available streams */
    vlc_array_t  *selected_st; /* selected streams */
    vlc_array_t  *init_chunks;
    unsigned     i_tracks;     /* Total number of tracks in the Manifest */
    sms_queue_t  *bws;         /* Measured bandwidths of the N last chunks */
    uint64_t     vod_duration; /* total duration of the VOD media */
    int64_t      time_pos;
    unsigned     timescale;

    /* Download */
    struct sms_download_s
    {
        uint64_t     lead[3];     /* how much audio/video/text data is available
                                     (downloaded), in seconds / TimeScale */

        unsigned     ck_index[3]; /* current chunk for download */

        uint64_t     next_chunk_offset;
        vlc_array_t  *chunks;     /* chunks that have been downloaded */
        vlc_mutex_t  lock_wait;   /* protect chunk download counter. */
        vlc_cond_t   wait;        /* some condition to wait on */
    } download;

    /* Playback */
    struct sms_playback_s
    {
        uint64_t    boffset;     /* current byte offset in media */
        uint64_t    toffset;     /* current time offset in media */
        unsigned    index;       /* current chunk for playback */
    } playback;

    /* state */
    bool        b_cache;     /* can cache files */
    bool        b_live;      /* live stream? or vod? */
    bool        b_error;     /* parsing error */
    bool        b_close;     /* set by Close() */
    bool        b_tseek;     /* time seeking */
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
    sms_get_stream_by_cat( p_sys->selected_st, cat )

#define NO_MORE_CHUNKS ( !p_sys->b_live && \
    no_more_chunks( p_sys->download.ck_index, p_sys->selected_st ) )

void sms_queue_free( sms_queue_t* );
sms_queue_t *sms_queue_init( const int );
int sms_queue_put( sms_queue_t *, const uint64_t );
uint64_t sms_queue_avg( sms_queue_t *);
quality_level_t *get_qlevel( sms_stream_t *, const unsigned );
void* sms_Thread( void *);
quality_level_t * ql_New( void );
void ql_Free( quality_level_t *);
chunk_t *chunk_New( sms_stream_t* , uint64_t , uint64_t );
void chunk_Free( chunk_t *);
sms_stream_t * sms_New( void );
void sms_Free( sms_stream_t *);
uint8_t *decode_string_hex_to_binary( const char * );
sms_stream_t * sms_get_stream_by_cat( vlc_array_t *, int );
bool no_more_chunks( unsigned[], vlc_array_t *);
int index_to_es_cat( int );
int es_cat_to_index( int );

#endif
