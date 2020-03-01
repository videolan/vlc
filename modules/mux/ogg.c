/*****************************************************************************
 * ogg.c: ogg muxer module for vlc
 *****************************************************************************
 * Copyright (C) 2001, 2002, 2006 VLC authors and VideoLAN
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Gildas Bazin <gbazin@videolan.org>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_sout.h>
#include <vlc_block.h>
#include <vlc_codecs.h>
#include <limits.h>
#include <vlc_rand.h>
#include "../demux/xiph.h"

#include <ogg/ogg.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define INDEXINTVL_TEXT N_("Index interval")
#define INDEXINTVL_LONGTEXT N_("Minimal index interval, in milliseconds. " \
    "Set to 0 to disable index creation.")
#define INDEXRATIO_TEXT N_("Index size ratio")
#define INDEXRATIO_LONGTEXT N_(\
    "Set index size ratio. Alters default (60min content) or estimated size." )

static int  Open   ( vlc_object_t * );
static void Close  ( vlc_object_t * );

#define SOUT_CFG_PREFIX "sout-ogg-"

vlc_module_begin ()
    set_description( N_("Ogg/OGM muxer") )
    set_capability( "sout mux", 10 )
    set_category( CAT_SOUT )
    set_subcategory( SUBCAT_SOUT_MUX )
    add_shortcut( "ogg", "ogm" )
    add_integer_with_range( SOUT_CFG_PREFIX "indexintvl", 1000, 0, INT_MAX,
                            INDEXINTVL_TEXT, INDEXINTVL_LONGTEXT, true )
    add_float_with_range( SOUT_CFG_PREFIX "indexratio", 1.0, 1.0, 1000,
                          INDEXRATIO_TEXT, INDEXRATIO_LONGTEXT, true )
    set_callbacks( Open, Close )
vlc_module_end ()


/*****************************************************************************
 * Exported prototypes
 *****************************************************************************/
static int Control  ( sout_mux_t *, int, va_list );
static int AddStream( sout_mux_t *, sout_input_t * );
static void DelStream( sout_mux_t *, sout_input_t * );
static int Mux      ( sout_mux_t * );
static int MuxBlock ( sout_mux_t *, sout_input_t * );

static bool OggCreateHeaders( sout_mux_t * );
static void OggFillSkeletonFishead( uint8_t *p_buffer, sout_mux_t *p_mux );

/*****************************************************************************
 * Misc declarations
 *****************************************************************************/

/* Skeleton */
#define FISBONE_BASE_SIZE 52
#define FISBONE_BASE_OFFSET 44
#define INDEX_BASE_SIZE 42

/* Structures used for OggDS headers used in ogm files */

#define PACKET_TYPE_HEADER   0x01
#define PACKET_TYPE_COMMENT  0x03
#define PACKET_IS_SYNCPOINT  0x08

typedef struct
{
    int32_t i_width;
    int32_t i_height;
} oggds_header_video_t;

typedef struct
{
    int16_t i_channels;
    int16_t i_block_align;
    int32_t i_avgbytespersec;
} oggds_header_audio_t;

typedef struct
{
    uint8_t i_packet_type;

    char stream_type[8];
    char sub_type[4];

    int32_t i_size;

    int64_t i_time_unit;
    int64_t i_samples_per_unit;
    int32_t i_default_len;

    int32_t i_buffer_size;
    int16_t i_bits_per_sample;

    int16_t i_padding_0; /* Because the original is using MSVC packing style */

    union
    {
        oggds_header_video_t video;
        oggds_header_audio_t audio;
    } header;

    int32_t i_padding_1; /* Because the original is using MSVC packing style */

} oggds_header_t;

/*****************************************************************************
 * Definitions of structures and functions used by this plugins
 *****************************************************************************/
typedef struct
{
    es_format_t fmt;

    int b_new;

    vlc_tick_t i_dts;
    vlc_tick_t i_length;
    int     i_packet_no;
    int     i_serial_no;
    int     i_keyframe_granule_shift; /* Theora and Daala only */
    int     i_last_keyframe; /* dirac and theora */
    int     i_num_frames; /* Theora only */
    uint64_t u_last_granulepos; /* Used for correct EOS page */
    int64_t i_num_keyframes;
    ogg_stream_state os;

    oggds_header_t *p_oggds_header;
    bool b_started;
    bool b_finished;

    struct
    {
         bool b_fisbone_done;
         bool b_index_done;
         /* Skeleton index storage */
         unsigned char *p_index;
         uint64_t i_index_size;
         uint64_t i_index_payload; /* real index size */
         uint64_t i_index_count;
         /* backup values for rewriting index page later */
         uint64_t i_index_offset;  /* sout offset of the index page */
         int64_t i_index_packetno;  /* index packet no */
         int i_index_pageno;
         /* index creation tracking values */
         uint64_t i_last_keyframe_pos;
         vlc_tick_t i_last_keyframe_time;
    } skeleton;

    int             i_dirac_last_pt;
    int             i_dirac_last_dt;
    vlc_tick_t      i_baseptsdelay;

} ogg_stream_t;

typedef struct
{
    int     i_streams;

    vlc_tick_t i_start_dts;
    int     i_next_serial_no;

    /* number of logical streams pending to be added */
    int i_add_streams;
    bool b_can_add_streams;

    /* logical streams pending to be deleted */
    int i_del_streams;
    ogg_stream_t **pp_del_streams;

    /* Skeleton */
    struct
    {
        bool b_create;
        int i_serial_no;
        int i_packet_no;
        ogg_stream_state os;
        bool b_head_done;
        /* backup values for rewriting fishead page later */
        uint64_t i_fishead_offset;  /* sout offset of the fishead page */
        vlc_tick_t i_index_intvl;
        float i_index_ratio;
    } skeleton;

    /* access position */
    ssize_t i_pos;
    ssize_t i_data_start;
    ssize_t i_segment_start;
} sout_mux_sys_t;

static void OggSetDate( block_t *, vlc_tick_t , vlc_tick_t );
static block_t *OggStreamFlush( sout_mux_t *, ogg_stream_state *, vlc_tick_t );
static void OggCreateStreamFooter( sout_mux_t *p_mux, ogg_stream_t *p_stream );
static void OggRewriteFisheadPage( sout_mux_t *p_mux );
static bool AllocateIndex( sout_mux_t *p_mux, sout_input_t *p_input );

/*****************************************************************************
 * Open: Open muxer
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    sout_mux_t      *p_mux = (sout_mux_t*)p_this;
    sout_mux_sys_t  *p_sys;

    msg_Info( p_mux, "Open" );

    p_sys                 = malloc( sizeof( sout_mux_sys_t ) );
    if( !p_sys )
        return VLC_ENOMEM;
    p_sys->i_streams      = 0;
    p_sys->i_add_streams  = 0;
    p_sys->b_can_add_streams = true;
    p_sys->i_del_streams  = 0;
    p_sys->pp_del_streams = 0;
    p_sys->i_pos = 0;
    p_sys->skeleton.b_create = false;
    p_sys->skeleton.b_head_done = false;
    p_sys->skeleton.i_index_intvl =
            VLC_TICK_FROM_MS(var_InheritInteger( p_this, SOUT_CFG_PREFIX "indexintvl" ));
    p_sys->skeleton.i_index_ratio =
            var_InheritFloat( p_this, SOUT_CFG_PREFIX "indexratio" );
    p_sys->i_data_start = 0;
    p_sys->i_segment_start = 0;
    p_mux->p_sys        = p_sys;
    p_mux->pf_control   = Control;
    p_mux->pf_addstream = AddStream;
    p_mux->pf_delstream = DelStream;
    p_mux->pf_mux       = Mux;

    /* First serial number is random.
     * (Done like this because on win32 you need to seed the random number
     *  generator once per thread). */
    uint32_t r;
    vlc_rand_bytes(&r, sizeof(r));
    p_sys->i_next_serial_no = r & INT_MAX;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: Finalize ogg bitstream and close muxer
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    sout_mux_t     *p_mux = (sout_mux_t*)p_this;
    sout_mux_sys_t *p_sys = p_mux->p_sys;

    msg_Info( p_mux, "Close" );

    if( p_sys->i_del_streams )
    {
        /* Close the current ogg stream */
        msg_Dbg( p_mux, "writing footers" );

        /* Remove deleted logical streams */
        for(int i = 0; i < p_sys->i_del_streams; i++ )
        {
            ogg_stream_t *p_stream = p_sys->pp_del_streams[i];

            es_format_Clean( &p_stream->fmt );
            OggCreateStreamFooter( p_mux, p_stream );
            free( p_stream->p_oggds_header );
            free( p_stream->skeleton.p_index );
            free( p_stream );
        }
        free( p_sys->pp_del_streams );
        p_sys->i_streams -= p_sys->i_del_streams;
    }

    /* rewrite fishead with final values */
    if ( p_sys->skeleton.b_create && p_sys->skeleton.b_head_done )
    {
        OggRewriteFisheadPage( p_mux );
    }

    free( p_sys );
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( sout_mux_t *p_mux, int i_query, va_list args )
{
    VLC_UNUSED(p_mux);
    bool *pb_bool;
    char **ppsz;

   switch( i_query )
   {
       case MUX_CAN_ADD_STREAM_WHILE_MUXING:
           pb_bool = va_arg( args, bool * );
           *pb_bool = true;
           return VLC_SUCCESS;

       case MUX_GET_MIME:
           ppsz = va_arg( args, char ** );
           *ppsz = strdup( "application/ogg" );
           return VLC_SUCCESS;

        default:
            return VLC_EGENERIC;
   }
}
/*****************************************************************************
 * AddStream: Add an elementary stream to the muxed stream
 *****************************************************************************/
static int AddStream( sout_mux_t *p_mux, sout_input_t *p_input )
{
    sout_mux_sys_t *p_sys = p_mux->p_sys;
    ogg_stream_t   *p_stream;
    uint16_t i_tag;

    msg_Dbg( p_mux, "adding input" );

    p_input->p_sys = p_stream = calloc( 1, sizeof( ogg_stream_t ) );
    if( !p_stream )
        return VLC_ENOMEM;

    if( es_format_Copy( &p_stream->fmt, &p_input->fmt ) != VLC_SUCCESS )
    {
        free( p_stream );
        return VLC_ENOMEM;
    }

    p_stream->i_serial_no = p_sys->i_next_serial_no++;
    p_stream->i_packet_no = 0;
    p_stream->i_last_keyframe = 0;
    p_stream->i_num_keyframes = 0;
    p_stream->i_num_frames = 0;

    p_stream->p_oggds_header = 0;

    p_stream->i_baseptsdelay = -1;
    p_stream->i_dirac_last_pt = -1;
    p_stream->i_dirac_last_dt = -1;

    switch( p_input->p_fmt->i_cat )
    {
    case VIDEO_ES:
    {
        if( !p_stream->fmt.video.i_frame_rate ||
            !p_stream->fmt.video.i_frame_rate_base )
        {
            msg_Warn( p_mux, "Missing frame rate, assuming 25fps" );
            p_stream->fmt.video.i_frame_rate = 25;
            p_stream->fmt.video.i_frame_rate_base = 1;
        }

        switch( p_stream->fmt.i_codec )
        {
        case VLC_CODEC_MP4V:
        case VLC_CODEC_MPGV:
        case VLC_CODEC_MP1V:
        case VLC_CODEC_MP2V:
        case VLC_CODEC_DIV3:
        case VLC_CODEC_MJPG:
        case VLC_CODEC_WMV1:
        case VLC_CODEC_WMV2:
        case VLC_CODEC_WMV3:
            p_stream->p_oggds_header = calloc( 1, sizeof(oggds_header_t) );
            if( !p_stream->p_oggds_header )
            {
                free( p_stream );
                return VLC_ENOMEM;
            }
            p_stream->p_oggds_header->i_packet_type = PACKET_TYPE_HEADER;

            memcpy( p_stream->p_oggds_header->stream_type, "video", 5 );
            if( p_stream->fmt.i_codec == VLC_CODEC_MP4V )
            {
                memcpy( p_stream->p_oggds_header->sub_type, "XVID", 4 );
            }
            else if( p_stream->fmt.i_codec == VLC_CODEC_DIV3 )
            {
                memcpy( p_stream->p_oggds_header->sub_type, "DIV3", 4 );
            }
            else
            {
                memcpy( p_stream->p_oggds_header->sub_type,
                        &p_stream->fmt.i_codec, 4 );
            }
            p_stream->p_oggds_header->i_size = 0 ;
            p_stream->p_oggds_header->i_time_unit =
                     MSFTIME_FROM_SEC(1) * p_stream->fmt.video.i_frame_rate_base /
                     (int64_t)p_stream->fmt.video.i_frame_rate;
            p_stream->p_oggds_header->i_samples_per_unit = 1;
            p_stream->p_oggds_header->i_default_len = 1 ; /* ??? */
            p_stream->p_oggds_header->i_buffer_size = 1024*1024;
            p_stream->p_oggds_header->i_bits_per_sample = 0;
            p_stream->p_oggds_header->header.video.i_width = p_input->p_fmt->video.i_width;
            p_stream->p_oggds_header->header.video.i_height = p_input->p_fmt->video.i_height;
            msg_Dbg( p_mux, "%4.4s stream", (char *)&p_stream->fmt.i_codec );
            break;

        case VLC_CODEC_DIRAC:
            msg_Dbg( p_mux, "dirac stream" );
            break;

        case VLC_CODEC_THEORA:
            msg_Dbg( p_mux, "theora stream" );
            break;

        case VLC_CODEC_DAALA:
            msg_Dbg( p_mux, "daala stream" );
            break;

        case VLC_CODEC_VP8:
            msg_Dbg( p_mux, "VP8 stream" );
            break;

        default:
            FREENULL( p_input->p_sys );
            return VLC_EGENERIC;
        }
    }
        break;

    case AUDIO_ES:
        switch( p_stream->fmt.i_codec )
        {
        case VLC_CODEC_OPUS:
            msg_Dbg( p_mux, "opus stream" );
            break;

        case VLC_CODEC_VORBIS:
            msg_Dbg( p_mux, "vorbis stream" );
            break;

        case VLC_CODEC_SPEEX:
            msg_Dbg( p_mux, "speex stream" );
            break;

        case VLC_CODEC_FLAC:
            msg_Dbg( p_mux, "flac stream" );
            break;

        default:
            fourcc_to_wf_tag( p_stream->fmt.i_codec, &i_tag );
            if( i_tag == WAVE_FORMAT_UNKNOWN )
            {
                FREENULL( p_input->p_sys );
                return VLC_EGENERIC;
            }

            p_stream->p_oggds_header =
                malloc( sizeof(oggds_header_t) + p_input->p_fmt->i_extra );
            if( !p_stream->p_oggds_header )
            {
                free( p_stream );
                return VLC_ENOMEM;
            }
            memset( p_stream->p_oggds_header, 0, sizeof(oggds_header_t) );
            p_stream->p_oggds_header->i_packet_type = PACKET_TYPE_HEADER;

            p_stream->p_oggds_header->i_size = p_input->p_fmt->i_extra;

            if( p_input->p_fmt->i_extra )
            {
                memcpy( &p_stream->p_oggds_header[1],
                        p_input->p_fmt->p_extra, p_input->p_fmt->i_extra );
            }

            memcpy( p_stream->p_oggds_header->stream_type, "audio", 5 );

            char buf[5];
            memset( buf, 0, sizeof(buf) );
            snprintf( buf, sizeof(buf), "%"PRIx16, i_tag );

            memcpy( p_stream->p_oggds_header->sub_type, buf, 4 );

            p_stream->p_oggds_header->i_time_unit = MSFTIME_FROM_SEC(1);
            p_stream->p_oggds_header->i_default_len = 1;
            p_stream->p_oggds_header->i_buffer_size = 30*1024 ;
            p_stream->p_oggds_header->i_samples_per_unit = p_input->p_fmt->audio.i_rate;
            p_stream->p_oggds_header->i_bits_per_sample = p_input->p_fmt->audio.i_bitspersample;
            p_stream->p_oggds_header->header.audio.i_channels = p_input->p_fmt->audio.i_channels;
            p_stream->p_oggds_header->header.audio.i_block_align =  p_input->p_fmt->audio.i_blockalign;
            p_stream->p_oggds_header->header.audio.i_avgbytespersec =  p_input->p_fmt->i_bitrate / 8;
            msg_Dbg( p_mux, "%4.4s stream", (char *)&p_stream->fmt.i_codec );
            break;
        }
        break;

    case SPU_ES:
        switch( p_stream->fmt.i_codec )
        {
        case VLC_CODEC_SUBT:
            p_stream->p_oggds_header = calloc( 1, sizeof(oggds_header_t) );
            if( !p_stream->p_oggds_header )
            {
                free( p_stream );
                return VLC_ENOMEM;
            }
            p_stream->p_oggds_header->i_packet_type = PACKET_TYPE_HEADER;

            memcpy( p_stream->p_oggds_header->stream_type, "text", 4 );
            msg_Dbg( p_mux, "subtitles stream" );
            break;

        default:
            FREENULL( p_input->p_sys );
            return VLC_EGENERIC;
        }
        break;
    default:
        FREENULL( p_input->p_sys );
        return VLC_EGENERIC;
    }

    p_stream->b_new = true;

    p_sys->i_add_streams++;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * DelStream: Delete an elementary stream from the muxed stream
 *****************************************************************************/
static void DelStream( sout_mux_t *p_mux, sout_input_t *p_input )
{
    sout_mux_sys_t *p_sys  = p_mux->p_sys;
    ogg_stream_t   *p_stream = (ogg_stream_t*)p_input->p_sys;
    block_t *p_og;

    msg_Dbg( p_mux, "removing input" );

    /* flush all remaining data */
    if( p_input->p_sys )
    {
        if( !p_stream->b_new )
        {
            while( block_FifoCount( p_input->p_fifo ) )
                MuxBlock( p_mux, p_input );
        }

        if( !p_stream->b_new &&
            ( p_og = OggStreamFlush( p_mux, &p_stream->os, 0 ) ) )
        {
            OggSetDate( p_og, p_stream->i_dts, p_stream->i_length );
            p_sys->i_pos += sout_AccessOutWrite( p_mux->p_access, p_og );
        }

        /* move input in delete queue */
        if( !p_stream->b_new )
        {
            p_sys->pp_del_streams = xrealloc( p_sys->pp_del_streams,
                        (p_sys->i_del_streams + 1) * sizeof(ogg_stream_t *) );
            p_sys->pp_del_streams[p_sys->i_del_streams++] = p_stream;
        }
        else
        {
            /* wasn't already added so get rid of it */
            FREENULL( p_stream->p_oggds_header );
            FREENULL( p_stream );
            p_sys->i_add_streams--;
        }
    }

    p_input->p_sys = NULL;
}

/*****************************************************************************
 * Ogg Skeleton helpers
 *****************************************************************************/
static int WriteQWVariableLE( uint64_t i_64, uint64_t i_offset,
                              uint8_t *p_buffer, int i_buffer_size )
{
    uint8_t *p_dest = p_buffer + i_offset;
    int i_written = 0;

    for(;;)
    {
        if ( p_dest - p_buffer >= i_buffer_size ) return -1;

        *p_dest = (uint8_t) ( i_64 & 0x7F );
        i_64 >>= 7;
        i_written++;

        if ( i_64 == 0 )
        {
            *p_dest |= 0x80;
            return i_written;
        }

        p_dest++;
    }
}

static bool AddIndexEntry( sout_mux_t *p_mux, vlc_tick_t i_time, sout_input_t *p_input )
{
    sout_mux_sys_t *p_sys = p_mux->p_sys;
    ogg_stream_t *p_stream = (ogg_stream_t *) p_input->p_sys;
    uint64_t i_posdelta;
    vlc_tick_t i_timedelta;
    if ( !p_sys->skeleton.b_create || p_sys->skeleton.i_index_intvl == 0
         || !p_stream->skeleton.p_index )
        return false;

    if ( p_stream->skeleton.i_last_keyframe_pos == 0 )
        p_stream->skeleton.i_last_keyframe_pos = p_sys->i_segment_start;
    i_posdelta = p_sys->i_pos - p_stream->skeleton.i_last_keyframe_pos;
    i_timedelta = i_time - p_stream->skeleton.i_last_keyframe_time;

    if ( i_timedelta <= p_sys->skeleton.i_index_intvl || i_posdelta <= 0xFFFF )
        return false;

    /* do inserts */
    int i_ret;
    if ( !p_stream->skeleton.p_index ) return false;
    uint64_t i_offset = p_stream->skeleton.i_index_payload;
    i_ret = WriteQWVariableLE( i_posdelta, i_offset, p_stream->skeleton.p_index,
                               p_stream->skeleton.i_index_size );
    if ( i_ret == -1 ) return false;
    i_offset += i_ret;
    i_ret = WriteQWVariableLE( i_timedelta, i_offset, p_stream->skeleton.p_index,
                               p_stream->skeleton.i_index_size );
    if ( i_ret == -1 ) return false;
    p_stream->skeleton.i_index_payload = i_offset + i_ret;
    p_stream->skeleton.i_index_count++;

    /* update diff points */
    p_stream->skeleton.i_last_keyframe_pos = p_sys->i_pos;
    p_stream->skeleton.i_last_keyframe_time = i_time;
    msg_Dbg( p_mux, "Added index on stream %d entry %zd %"PRIu64,
             p_stream->i_serial_no, p_sys->i_pos - p_sys->i_segment_start, i_time );

    return true;
}

/*****************************************************************************
 * Ogg bitstream manipulation routines
 *****************************************************************************/
static block_t *OggStreamGetPage( sout_mux_t *p_mux,
                                  ogg_stream_state *p_os, vlc_tick_t i_pts,
                                  bool flush )
{
    (void)p_mux;
    block_t *p_og, *p_og_first = NULL;
    ogg_page og;
    int (*pager)( ogg_stream_state*, ogg_page* ) = flush ? ogg_stream_flush : ogg_stream_pageout;

    while( pager( p_os, &og ) )
    {
        /* Flush all data */
        p_og = block_Alloc( og.header_len + og.body_len );

        memcpy( p_og->p_buffer, og.header, og.header_len );
        memcpy( p_og->p_buffer + og.header_len, og.body, og.body_len );
        p_og->i_dts     = 0;
        p_og->i_pts     = i_pts;
        p_og->i_length  = 0;

        i_pts = 0; // write it only once

        block_ChainAppend( &p_og_first, p_og );
    }

    return p_og_first;
}

static block_t *OggStreamFlush( sout_mux_t *p_mux,
                                ogg_stream_state *p_os, vlc_tick_t i_pts )
{
    return OggStreamGetPage( p_mux, p_os, i_pts, true );
}

static block_t *OggStreamPageOut( sout_mux_t *p_mux,
                                  ogg_stream_state *p_os, vlc_tick_t i_pts )
{
    return OggStreamGetPage( p_mux, p_os, i_pts, false );
}

static void OggGetSkeletonIndex( uint8_t **pp_buffer, long *pi_size, ogg_stream_t *p_stream )
{
    uint8_t *p_buffer = calloc( INDEX_BASE_SIZE + p_stream->skeleton.i_index_size, sizeof(uint8_t) );
    if ( !p_buffer ) return;
    *pp_buffer = p_buffer;

    memcpy( p_buffer, "index", 6 );
    SetDWLE( &p_buffer[6], p_stream->i_serial_no );
    SetQWLE( &p_buffer[10], p_stream->skeleton.i_index_count ); /* num keypoints */
    SetQWLE( &p_buffer[18], CLOCK_FREQ );
    SetQWLE( &p_buffer[34], p_stream->i_length );
    memcpy( p_buffer + INDEX_BASE_SIZE, p_stream->skeleton.p_index, p_stream->skeleton.i_index_payload );
    *pi_size = INDEX_BASE_SIZE + p_stream->skeleton.i_index_size;
}

static void OggGetSkeletonFisbone( uint8_t **pp_buffer, long *pi_size,
                                   sout_input_t *p_input, sout_mux_t *p_mux )
{
    uint8_t *psz_header;
    uint8_t *p_buffer;
    const char *psz_value = NULL;
    ogg_stream_t *p_stream = (ogg_stream_t *) p_input->p_sys;
    struct
    {
        char *psz_content_type;
        char *psz_role;
        long int i_size;
        unsigned int i_count;
    } headers = { NULL, NULL, 0, 0 };
    *pi_size = 0;

    switch( p_stream->fmt.i_codec )
    {
        case VLC_CODEC_VORBIS:
            psz_value = "audio/vorbis";
            break;
        case VLC_CODEC_THEORA:
            psz_value = "video/theora";
            break;
        case VLC_CODEC_DAALA:
            psz_value = "video/daala";
            break;
        case VLC_CODEC_SPEEX:
            psz_value = "audio/speex";
            break;
        case VLC_CODEC_FLAC:
            psz_value = "audio/flac";
            break;
        case VLC_CODEC_CMML:
            psz_value = "text/cmml";
            break;
        case VLC_CODEC_KATE:
            psz_value = "application/kate";
            break;
        case VLC_CODEC_VP8:
            psz_value = "video/x-vp8";
            break;
        default:
            psz_value = "application/octet-stream";
            msg_Warn( p_mux, "Unknown fourcc for stream %s, setting Content-Type to %s",
                  vlc_fourcc_GetDescription( p_stream->fmt.i_cat, p_stream->fmt.i_codec ),
                  psz_value );
    }

    /* Content Type Header */
    if ( asprintf( &headers.psz_content_type, "Content-Type: %s\r\n", psz_value ) != -1 )
    {
        headers.i_size += strlen( headers.psz_content_type );
        headers.i_count++;
    }

    /* Set Role Header */
    if ( p_input->p_fmt->i_priority > ES_PRIORITY_NOT_SELECTABLE )
    {
        int i_max_prio = ES_PRIORITY_MIN;
        for ( int i=0; i< p_mux->i_nb_inputs; i++ )
        {
            if ( p_mux->pp_inputs[i]->p_fmt->i_cat != p_input->p_fmt->i_cat ) continue;
            i_max_prio = __MAX( p_mux->pp_inputs[i]->p_fmt->i_priority, i_max_prio );
        }

        psz_value = NULL;
        if ( p_input->p_fmt->i_cat == AUDIO_ES || p_input->p_fmt->i_cat == VIDEO_ES )
        {
            if ( p_input->p_fmt->i_priority == i_max_prio && i_max_prio >= ES_PRIORITY_SELECTABLE_MIN )
                psz_value = ( p_input->p_fmt->i_cat == VIDEO_ES ) ?
                            "video/main" : "audio/main";
            else
                psz_value = ( p_input->p_fmt->i_cat == VIDEO_ES ) ?
                            "video/alternate" : "audio/alternate";
        }
        else if ( p_input->p_fmt->i_cat == SPU_ES )
        {
            psz_value = ( p_input->p_fmt->i_codec == VLC_CODEC_KATE ) ?
                        "text/karaoke" : "text/subtitle";
        }

        if ( psz_value && asprintf( &headers.psz_role, "Role: %s\r\n", psz_value ) != -1 )
        {
            headers.i_size += strlen( headers.psz_role );
            headers.i_count++;
        }
    }

    *pp_buffer = calloc( FISBONE_BASE_SIZE + headers.i_size, sizeof(uint8_t) );
    if ( !*pp_buffer ) return;
    p_buffer = *pp_buffer;

    memcpy( p_buffer, "fisbone", 8 );
    SetDWLE( &p_buffer[8], FISBONE_BASE_OFFSET ); /* offset to message headers */
    SetDWLE( &p_buffer[12], p_stream->i_serial_no );
    SetDWLE( &p_buffer[16], headers.i_count );

    /* granulerate den */
    switch ( p_input->p_fmt->i_cat )
    {
        case VIDEO_ES:
            SetQWLE( &(*pp_buffer)[20], p_stream->fmt.video.i_frame_rate );
            SetQWLE( &(*pp_buffer)[28], p_stream->fmt.video.i_frame_rate_base );
        break;
        case AUDIO_ES:
            SetQWLE( &(*pp_buffer)[20], p_input->p_fmt->audio.i_rate );
            SetQWLE( &(*pp_buffer)[28], 1 );
        break;
        default:
            SetQWLE( &(*pp_buffer)[20], 1000 );
            SetQWLE( &(*pp_buffer)[28], 1 );
    }

    /* preroll */
    if ( p_input->p_fmt->p_extra )
        SetDWLE( &(*pp_buffer)[44],
                xiph_CountUnknownHeaders( p_input->p_fmt->p_extra,
                                          p_input->p_fmt->i_extra,
                                          p_input->p_fmt->i_codec ) );

    if ( headers.i_size > 0 )
    {
        psz_header = *pp_buffer + FISBONE_BASE_SIZE;
        memcpy( psz_header, headers.psz_content_type, strlen( headers.psz_content_type ) );
        psz_header += strlen( headers.psz_content_type );
        if ( headers.psz_role )
            memcpy( psz_header, headers.psz_role, strlen( headers.psz_role ) );
    }
    *pi_size = FISBONE_BASE_SIZE + headers.i_size;

    free( headers.psz_content_type );
    free( headers.psz_role );
}

static void OggFillSkeletonFishead( uint8_t *p_buffer, sout_mux_t *p_mux )
{
    sout_mux_sys_t *p_sys = p_mux->p_sys;
    memcpy( p_buffer, "fishead", 8 );
    SetWLE( &p_buffer[8], 4 );
    SetWLE( &p_buffer[10], 0 );
    SetQWLE( &p_buffer[20], 1000 );
    SetQWLE( &p_buffer[36], 1000 );
    SetQWLE( &p_buffer[64], p_sys->i_pos - p_sys->i_segment_start ); /* segment length */
    SetQWLE( &p_buffer[72], p_sys->i_data_start - p_sys->i_segment_start ); /* data start offset */
}

static int32_t OggFillDsHeader( uint8_t *p_buffer, oggds_header_t *p_oggds_header, int i_cat )
{
    int index = 0;
    p_buffer[index] = p_oggds_header->i_packet_type;
    index++;
    memcpy( &p_buffer[index], p_oggds_header->stream_type, sizeof(p_oggds_header->stream_type) );
    index += sizeof(p_oggds_header->stream_type);
    memcpy(&p_buffer[index], p_oggds_header->sub_type, sizeof(p_oggds_header->sub_type) );
    index += sizeof(p_oggds_header->sub_type);

    /* The size is filled at the end */
    uint8_t *p_isize = &p_buffer[index];
    index += 4;

    SetQWLE( &p_buffer[index], p_oggds_header->i_time_unit );
    index += 8;
    SetQWLE( &p_buffer[index], p_oggds_header->i_samples_per_unit );
    index += 8;
    SetDWLE( &p_buffer[index], p_oggds_header->i_default_len );
    index += 4;
    SetDWLE( &p_buffer[index], p_oggds_header->i_buffer_size );
    index += 4;
    SetWLE( &p_buffer[index], p_oggds_header->i_bits_per_sample );
    index += 2;
    SetWLE( &p_buffer[index], p_oggds_header->i_padding_0 );
    index += 2;
    /* audio or video */
    switch( i_cat )
    {
    case VIDEO_ES:
        SetDWLE( &p_buffer[index], p_oggds_header->header.video.i_width );
        SetDWLE( &p_buffer[index+4], p_oggds_header->header.video.i_height );
        break;
    case AUDIO_ES:
        SetWLE( &p_buffer[index], p_oggds_header->header.audio.i_channels );
        SetWLE( &p_buffer[index+2], p_oggds_header->header.audio.i_block_align );
        SetDWLE( &p_buffer[index+4], p_oggds_header->header.audio.i_avgbytespersec );
        break;
    }
    index += 8;
    SetDWLE( &p_buffer[index], p_oggds_header->i_padding_1 );
    index += 4;

    /* extra header */
    if( p_oggds_header->i_size > 0 )
    {
        memcpy( &p_buffer[index], (uint8_t *) p_oggds_header + sizeof(*p_oggds_header), p_oggds_header->i_size );
        index += p_oggds_header->i_size;
    }

    SetDWLE( p_isize, index-1 );
    return index;
}

static void OggFillVP8Header( uint8_t *p_buffer, sout_input_t *p_input )
{
    ogg_stream_t *p_stream = (ogg_stream_t *) p_input->p_sys;

    memcpy( p_buffer, "OVP80\x01\x01\x00", 8 );
    SetWBE( &p_buffer[8], p_input->p_fmt->video.i_width );
    SetDWBE( &p_buffer[14], p_input->p_fmt->video.i_sar_den );/* 24 bits, 15~ */
    SetDWBE( &p_buffer[11], p_input->p_fmt->video.i_sar_num );/* 24 bits, 12~ */
    SetWBE( &p_buffer[10], p_input->p_fmt->video.i_height );
    SetDWBE( &p_buffer[18], p_stream->fmt.video.i_frame_rate );
    SetDWBE( &p_buffer[22], p_stream->fmt.video.i_frame_rate_base );
}

static bool OggCreateHeaders( sout_mux_t *p_mux )
{
    block_t *p_hdr = NULL;
    block_t *p_og = NULL;
    ogg_packet op;
    sout_mux_sys_t *p_sys = p_mux->p_sys;

    if( sout_AccessOutControl( p_mux->p_access,
                               ACCESS_OUT_CAN_SEEK,
                               &p_sys->skeleton.b_create ) )
    {
        p_sys->skeleton.b_create = false;
    }

    p_sys->skeleton.b_create &= !! p_mux->i_nb_inputs;

    /* no skeleton for solo vorbis/speex/opus tracks */
    if ( p_mux->i_nb_inputs == 1 && p_mux->pp_inputs[0]->p_fmt->i_cat == AUDIO_ES )
    {
        p_sys->skeleton.b_create = false;
    }
    else
    {
        for ( int i=0; i< p_mux->i_nb_inputs; i++ )
        {
            ogg_stream_t *p_stream = p_mux->pp_inputs[i]->p_sys;
            if( p_stream->p_oggds_header )
            {
                /* We don't want skeleton for OggDS */
                p_sys->skeleton.b_create = false;
                break;
            }
        }
    }

    /* Skeleton's Fishead must be the first page of the stream */
    if ( p_sys->skeleton.b_create && !p_sys->skeleton.b_head_done )
    {
        msg_Dbg( p_mux, "creating header for skeleton" );
        p_sys->skeleton.i_serial_no = p_sys->i_next_serial_no++;
        ogg_stream_init( &p_sys->skeleton.os, p_sys->skeleton.i_serial_no );
        op.bytes = 80;
        op.packet = calloc( 1, op.bytes );
        if ( op.packet == NULL ) return false;
        op.b_o_s = 1;
        op.e_o_s = 0;
        op.granulepos = 0;
        op.packetno = 0;
        OggFillSkeletonFishead( op.packet, p_mux );
        ogg_stream_packetin( &p_sys->skeleton.os, &op );
        ogg_packet_clear( &op );
        p_og = OggStreamFlush( p_mux, &p_sys->skeleton.os, 0 );
        block_ChainAppend( &p_hdr, p_og );
        p_sys->skeleton.b_head_done = true;
        p_sys->skeleton.i_fishead_offset = p_sys->i_pos;
    }

    /* Write header for each stream. All b_o_s (beginning of stream) packets
     * must appear first in the ogg stream so we take care of them first. */
    for( int pass = 0; pass < 2; pass++ )
    {
        for( int i = 0; i < p_mux->i_nb_inputs; i++ )
        {
            sout_input_t *p_input = p_mux->pp_inputs[i];
            ogg_stream_t *p_stream = p_input->p_sys;

            bool video = ( p_stream->fmt.i_codec == VLC_CODEC_THEORA ||
                           p_stream->fmt.i_codec == VLC_CODEC_DIRAC ||
                           p_stream->fmt.i_codec == VLC_CODEC_DAALA );
            if( ( ( pass == 0 && !video ) || ( pass == 1 && video ) ) )
                continue;

            msg_Dbg( p_mux, "creating header for %4.4s",
                     (char *)&p_stream->fmt.i_codec );

            ogg_stream_init( &p_stream->os, p_stream->i_serial_no );
            p_stream->b_new = false;
            p_stream->i_packet_no = 0;
            p_stream->b_started = true;

            if( p_stream->fmt.i_codec == VLC_CODEC_VORBIS ||
                p_stream->fmt.i_codec == VLC_CODEC_SPEEX ||
                p_stream->fmt.i_codec == VLC_CODEC_OPUS ||
                p_stream->fmt.i_codec == VLC_CODEC_THEORA ||
                p_stream->fmt.i_codec == VLC_CODEC_DAALA )
            {
                /* First packet in order: vorbis/speex/opus/theora/daala info */
                unsigned pi_size[XIPH_MAX_HEADER_COUNT];
                const void *pp_data[XIPH_MAX_HEADER_COUNT];
                unsigned i_count;

                if( xiph_SplitHeaders( pi_size, pp_data, &i_count,
                                       p_input->p_fmt->i_extra, p_input->p_fmt->p_extra ) )
                {
                    i_count = 0;
                    pi_size[0] = 0;
                    pp_data[0] = NULL;
                }

                op.bytes  = pi_size[0];
                op.packet = (void *)pp_data[0];
                if( pi_size[0] <= 0 )
                    msg_Err( p_mux, "header data corrupted");

                op.b_o_s  = 1;
                op.e_o_s  = 0;
                op.granulepos = 0;
                op.packetno = p_stream->i_packet_no++;
                ogg_stream_packetin( &p_stream->os, &op );
                p_og = OggStreamFlush( p_mux, &p_stream->os, 0 );

                /* Get keyframe_granule_shift for theora or daala granulepos calculation */
                if( p_stream->fmt.i_codec == VLC_CODEC_THEORA ||
                    p_stream->fmt.i_codec == VLC_CODEC_DAALA )
                {
                    p_stream->i_keyframe_granule_shift =
                        ( (op.packet[40] & 0x03) << 3 ) | ( (op.packet[41] & 0xe0) >> 5 );
                }
            }
            else if( p_stream->fmt.i_codec == VLC_CODEC_DIRAC )
            {
                op.packet = p_input->p_fmt->p_extra;
                op.bytes  = p_input->p_fmt->i_extra;
                op.b_o_s  = 1;
                op.e_o_s  = 0;
                op.granulepos = ~0;
                op.packetno = p_stream->i_packet_no++;
                ogg_stream_packetin( &p_stream->os, &op );
                p_og = OggStreamFlush( p_mux, &p_stream->os, 0 );
            }
            else if( p_stream->fmt.i_codec == VLC_CODEC_FLAC )
            {
                /* flac stream marker (yeah, only that in the 1st packet) */
                op.packet = (unsigned char *)"fLaC";
                op.bytes  = 4;
                op.b_o_s  = 1;
                op.e_o_s  = 0;
                op.granulepos = 0;
                op.packetno = p_stream->i_packet_no++;
                ogg_stream_packetin( &p_stream->os, &op );
                p_og = OggStreamFlush( p_mux, &p_stream->os, 0 );
            }
            else if( p_stream->fmt.i_codec == VLC_CODEC_VP8 )
            {
                /* VP8 Header */
                op.packet = malloc( 26 );
                if( !op.packet )
                    return false;
                op.bytes = 26;
                OggFillVP8Header( op.packet, p_input );
                op.b_o_s = 1;
                op.e_o_s = 0;
                op.granulepos = 0;
                op.packetno = p_stream->i_packet_no++;
                ogg_stream_packetin( &p_stream->os, &op );
                p_og = OggStreamFlush( p_mux, &p_stream->os, 0 );
                free( op.packet );
            }
            else if( p_stream->p_oggds_header )
            {
                /* ds header */
                op.packet = malloc( sizeof(*p_stream->p_oggds_header) + p_stream->p_oggds_header->i_size );
                if( !op.packet )
                    return false;
                op.bytes  = OggFillDsHeader( op.packet, p_stream->p_oggds_header, p_stream->fmt.i_cat );
                op.b_o_s  = 1;
                op.e_o_s  = 0;
                op.granulepos = 0;
                op.packetno = p_stream->i_packet_no++;
                ogg_stream_packetin( &p_stream->os, &op );
                p_og = OggStreamFlush( p_mux, &p_stream->os, 0 );
                free( op.packet );
            }

            block_ChainAppend( &p_hdr, p_og );
        }
    }

    /* Create fisbones if any */
    if ( p_sys->skeleton.b_create )
    {
        for( int i = 0; i < p_mux->i_nb_inputs; i++ )
        {
            sout_input_t *p_input = p_mux->pp_inputs[i];
            ogg_stream_t *p_stream = p_input->p_sys;
            if ( p_stream->skeleton.b_fisbone_done ) continue;
            OggGetSkeletonFisbone( &op.packet, &op.bytes, p_input, p_mux );
            if ( op.packet == NULL ) return false;
            op.b_o_s = 0;
            op.e_o_s = 0;
            op.granulepos = 0;
            op.packetno = p_sys->skeleton.i_packet_no++;
            ogg_stream_packetin( &p_sys->skeleton.os, &op );
            ogg_packet_clear( &op );
            p_og = OggStreamFlush( p_mux, &p_sys->skeleton.os, 0 );
            block_ChainAppend( &p_hdr, p_og );
            p_stream->skeleton.b_fisbone_done = true;
        }
    }

    /* Write previous headers */
    for( p_og = p_hdr; p_og != NULL; p_og = p_og->p_next )
    {
        /* flag headers to be resent for streaming clients */
        p_og->i_flags |= BLOCK_FLAG_HEADER;
    }
    p_sys->i_pos += sout_AccessOutWrite( p_mux->p_access, p_hdr );
    p_hdr = NULL;

    /* Create indexes if any */
    for( int i = 0; i < p_mux->i_nb_inputs; i++ )
    {
        sout_input_t *p_input = p_mux->pp_inputs[i];
        ogg_stream_t *p_stream = (ogg_stream_t*)p_input->p_sys;
        /* flush stream && save offset */
        if ( p_sys->skeleton.b_create && !p_stream->skeleton.b_index_done )
        {
            if ( !p_stream->skeleton.p_index ) AllocateIndex( p_mux, p_input );
            if ( p_stream->skeleton.p_index )
            {
                msg_Dbg( p_mux, "Creating index for stream %d", p_stream->i_serial_no );
                OggGetSkeletonIndex( &op.packet, &op.bytes, p_stream );
                if ( op.packet == NULL ) return false;
                op.b_o_s = 0;
                op.e_o_s = 0;
                op.granulepos = 0;
                op.packetno = p_sys->skeleton.i_packet_no++;

                /* backup some values */
                p_stream->skeleton.i_index_offset = p_sys->i_pos;
                p_stream->skeleton.i_index_packetno = p_sys->skeleton.os.packetno;
                p_stream->skeleton.i_index_pageno = p_sys->skeleton.os.pageno;

                ogg_stream_packetin( &p_sys->skeleton.os, &op );
                ogg_packet_clear( &op );
                p_og = OggStreamFlush( p_mux, &p_sys->skeleton.os, 0 );
                p_sys->i_pos += sout_AccessOutWrite( p_mux->p_access, p_og );
            }
            p_stream->skeleton.b_index_done = true;
        }
    }

    /* Take care of the non b_o_s headers */
    for( int i = 0; i < p_mux->i_nb_inputs; i++ )
    {
        sout_input_t *p_input = p_mux->pp_inputs[i];
        ogg_stream_t *p_stream = (ogg_stream_t*)p_input->p_sys;

        if( p_stream->fmt.i_codec == VLC_CODEC_VORBIS ||
            p_stream->fmt.i_codec == VLC_CODEC_SPEEX ||
            p_stream->fmt.i_codec == VLC_CODEC_OPUS ||
            p_stream->fmt.i_codec == VLC_CODEC_THEORA ||
            p_stream->fmt.i_codec == VLC_CODEC_DAALA )
        {
            unsigned pi_size[XIPH_MAX_HEADER_COUNT];
            const void *pp_data[XIPH_MAX_HEADER_COUNT];
            unsigned i_count;

            if( xiph_SplitHeaders( pi_size, pp_data, &i_count,
                                   p_input->p_fmt->i_extra, p_input->p_fmt->p_extra ) )
                i_count = 0;

            /* Special case, headers are already there in the incoming stream.
             * We need to gather them an mark them as headers. */
            for( unsigned j = 1; j < i_count; j++ )
            {
                op.bytes  = pi_size[j];
                op.packet = (void *)pp_data[j];
                if( pi_size[j] <= 0 )
                    msg_Err( p_mux, "header data corrupted");

                op.b_o_s  = 0;
                op.e_o_s  = 0;
                op.granulepos = 0;
                op.packetno = p_stream->i_packet_no++;
                ogg_stream_packetin( &p_stream->os, &op );
                msg_Dbg( p_mux, "adding non bos, secondary header" );
                if( j == i_count - 1 )
                    p_og = OggStreamFlush( p_mux, &p_stream->os, 0 );
                else
                    p_og = OggStreamPageOut( p_mux, &p_stream->os, 0 );
                if( p_og )
                    block_ChainAppend( &p_hdr, p_og );
            }
        }
        else if( p_stream->fmt.i_codec != VLC_CODEC_FLAC &&
                 p_stream->fmt.i_codec != VLC_CODEC_DIRAC )
        {
            uint8_t com[128];
            int     i_com;

            /* comment */
            com[0] = PACKET_TYPE_COMMENT;
            i_com = snprintf( (char *)(com+1), 127,
                              PACKAGE_VERSION" stream output" )
                     + 1;
            op.packet = com;
            op.bytes  = i_com;
            op.b_o_s  = 0;
            op.e_o_s  = 0;
            op.granulepos = 0;
            op.packetno = p_stream->i_packet_no++;
            ogg_stream_packetin( &p_stream->os, &op );
            p_og = OggStreamFlush( p_mux, &p_stream->os, 0 );
            block_ChainAppend( &p_hdr, p_og );
        }

        /* Special case for mp4v and flac */
        if( ( p_stream->fmt.i_codec == VLC_CODEC_MP4V ||
              p_stream->fmt.i_codec == VLC_CODEC_FLAC ) &&
            p_input->p_fmt->i_extra )
        {
            /* Send a packet with the VOL data for mp4v
             * or STREAMINFO for flac */
            msg_Dbg( p_mux, "writing extra data" );
            op.bytes  = p_input->p_fmt->i_extra;
            op.packet = p_input->p_fmt->p_extra;
            uint8_t flac_streaminfo[34 + 4];
            if( p_stream->fmt.i_codec == VLC_CODEC_FLAC )
            {
                if (op.bytes == 42 && !memcmp(op.packet, "fLaC", 4)) {
                    op.bytes -= 4;
                    memcpy(flac_streaminfo, op.packet + 4, 38);
                    op.packet = flac_streaminfo;
                } else if (op.bytes == 34) {
                    op.bytes += 4;
                    memcpy(flac_streaminfo + 4, op.packet, 34);
                    flac_streaminfo[0] = 0x80; /* last block, streaminfo */
                    flac_streaminfo[1] = 0;
                    flac_streaminfo[2] = 0;
                    flac_streaminfo[3] = 34; /* block size */
                    op.packet = flac_streaminfo;
                } else {
                    msg_Err(p_mux, "Invalid FLAC streaminfo (%ld bytes)",
                            op.bytes);
                }
            }
            op.b_o_s  = 0;
            op.e_o_s  = 0;
            op.granulepos = 0;
            op.packetno = p_stream->i_packet_no++;
            ogg_stream_packetin( &p_stream->os, &op );
            p_og = OggStreamFlush( p_mux, &p_stream->os, 0 );
            block_ChainAppend( &p_hdr, p_og );
        }
    }

    if ( p_sys->skeleton.b_create )
    {
        msg_Dbg( p_mux, "ending skeleton" );
        op.packet = NULL;
        op.bytes = 0;
        op.b_o_s = 0;
        op.e_o_s = 1;
        op.granulepos = 0;
        op.packetno = p_sys->skeleton.i_packet_no++;
        ogg_stream_packetin( &p_sys->skeleton.os, &op );
        p_og = OggStreamFlush( p_mux, &p_sys->skeleton.os, 0 );
        block_ChainAppend( &p_hdr, p_og );
    }

    /* set HEADER flag */
    /* flag headers to be resent for streaming clients */
    for( p_og = p_hdr; p_og != NULL; p_og = p_og->p_next )
    {
        p_og->i_flags |= BLOCK_FLAG_HEADER;
    }

    /* Write previous headers */
    p_sys->i_pos += sout_AccessOutWrite( p_mux->p_access, p_hdr );

    return true;
}

static void OggCreateStreamFooter( sout_mux_t *p_mux, ogg_stream_t *p_stream )
{
    block_t *p_og;
    ogg_packet op;
    sout_mux_sys_t *p_sys = p_mux->p_sys;

    /* as stream is finished, overwrite the index, if there was any */
    if ( p_sys->skeleton.b_create && p_stream->skeleton.p_index
         && p_stream->skeleton.i_index_payload )
    {
        sout_AccessOutSeek( p_mux->p_access, p_stream->skeleton.i_index_offset );
        OggGetSkeletonIndex( &op.packet, &op.bytes, p_stream );
        if ( op.packet != NULL )
        {
            msg_Dbg(p_mux, "Rewriting index at %"PRId64, p_stream->skeleton.i_index_offset );
            ogg_stream_reset_serialno( &p_sys->skeleton.os, p_sys->skeleton.i_serial_no );
            op.b_o_s = 0;
            op.e_o_s = 0;
            op.granulepos = 0;
            op.packetno = p_stream->skeleton.i_index_packetno + 1;
            /* fake our stream state */
            p_sys->skeleton.os.pageno = p_stream->skeleton.i_index_pageno;
            p_sys->skeleton.os.packetno = p_stream->skeleton.i_index_packetno;
            p_sys->skeleton.os.granulepos = 0;
            p_sys->skeleton.os.b_o_s = 1;
            p_sys->skeleton.os.e_o_s = 0;
            ogg_stream_packetin( &p_sys->skeleton.os, &op );
            ogg_packet_clear( &op );
            p_og = OggStreamFlush( p_mux, &p_sys->skeleton.os, 0 );
            sout_AccessOutWrite( p_mux->p_access, p_og );
        }
        sout_AccessOutSeek( p_mux->p_access, p_sys->i_pos );
    }

    /* clear skeleton */
    p_stream->skeleton.b_fisbone_done = false;
    p_stream->skeleton.b_index_done = false;
    p_stream->skeleton.i_index_offset = 0;
    p_stream->skeleton.i_index_payload = 0;
    p_stream->skeleton.i_last_keyframe_pos = 0;
    p_stream->skeleton.i_last_keyframe_time = 0;
    /* clear accounting */
    p_stream->i_num_frames = 0;
    p_stream->i_num_keyframes = 0;

    /* Write eos packet for stream. */
    op.packet = NULL;
    op.bytes  = 0;
    op.b_o_s  = 0;
    op.e_o_s  = 1;
    op.granulepos = p_stream->u_last_granulepos;
    op.packetno = p_stream->i_packet_no++;
    ogg_stream_packetin( &p_stream->os, &op );

    /* flush it with all remaining data */
    if( ( p_og = OggStreamFlush( p_mux, &p_stream->os, 0 ) ) )
    {
        /* Write footer */
        OggSetDate( p_og, p_stream->i_dts, p_stream->i_length );
        p_sys->i_pos += sout_AccessOutWrite( p_mux->p_access, p_og );
    }

    ogg_stream_clear( &p_stream->os );
}

static void OggSetDate( block_t *p_og, vlc_tick_t i_dts, vlc_tick_t i_length )
{
    int i_count;
    block_t *p_tmp;
    vlc_tick_t i_delta;

    for( p_tmp = p_og, i_count = 0; p_tmp != NULL; p_tmp = p_tmp->p_next )
    {
        i_count++;
    }

    if( i_count == 0 ) return; /* ignore. */

    i_delta = i_length / i_count;

    for( p_tmp = p_og; p_tmp != NULL; p_tmp = p_tmp->p_next )
    {
        p_tmp->i_dts    = i_dts;
        p_tmp->i_length = i_delta;

        i_dts += i_delta;
    }
}

static void OggRewriteFisheadPage( sout_mux_t *p_mux )
{
    sout_mux_sys_t *p_sys = p_mux->p_sys;
    ogg_packet op;
    op.bytes = 80;
    op.packet = calloc( 1, op.bytes );
    if ( op.packet != NULL )
    {
        op.b_o_s = 1;
        op.e_o_s = 0;
        op.granulepos = 0;
        op.packetno = 0;
        ogg_stream_reset_serialno( &p_sys->skeleton.os, p_sys->skeleton.i_serial_no );
        OggFillSkeletonFishead( op.packet, p_mux );
        ogg_stream_packetin( &p_sys->skeleton.os, &op );
        ogg_packet_clear( &op );
        msg_Dbg( p_mux, "rewriting fishead at %"PRId64, p_sys->skeleton.i_fishead_offset );
        sout_AccessOutSeek( p_mux->p_access, p_sys->skeleton.i_fishead_offset );
        sout_AccessOutWrite( p_mux->p_access,
                             OggStreamFlush( p_mux, &p_sys->skeleton.os, 0 ) );
        sout_AccessOutSeek( p_mux->p_access, p_sys->i_pos );
    }
}

static bool AllocateIndex( sout_mux_t *p_mux, sout_input_t *p_input )
{
    sout_mux_sys_t *p_sys = p_mux->p_sys;
    ogg_stream_t *p_stream = (ogg_stream_t *) p_input->p_sys;
    size_t i_size;

    if ( p_stream->i_length )
    {
        vlc_tick_t i_interval = p_sys->skeleton.i_index_intvl;
        uint64_t i;

        if( p_input->p_fmt->i_cat == VIDEO_ES &&
                p_stream->fmt.video.i_frame_rate )
        {
            /* optimize for fps < 1 */
            i_interval= __MAX( i_interval,
                       VLC_TICK_FROM_SEC(10) *
                       p_stream->fmt.video.i_frame_rate_base /
                       p_stream->fmt.video.i_frame_rate );
        }

        size_t i_tuple_size = 0;
        /* estimate length of pos value */
        if ( p_input->p_fmt->i_bitrate )
        {
            i = samples_from_vlc_tick(i_interval, p_input->p_fmt->i_bitrate);
            while ( i <<= 1 ) i_tuple_size++;
        }
        else
        {
            /* Likely 64KB<<keyframe interval<<16MB */
            /* We can't really guess due to muxing */
            i_tuple_size = 24 / 8;
        }

        /* add length of interval value */
        i = i_interval;
        while ( i <<= 1 ) i_tuple_size++;

        i_size = i_tuple_size * ( p_stream->i_length / i_interval + 2 );
    }
    else
    {
        i_size = ( INT64_C(3600) * 11.2 * CLOCK_FREQ / p_sys->skeleton.i_index_intvl )
                * p_sys->skeleton.i_index_ratio;
        msg_Dbg( p_mux, "No stream length, using default allocation for index" );
    }
    i_size *= ( 8.0 / 7 ); /* 7bits encoding overhead */
    msg_Dbg( p_mux, "allocating %zu bytes for index", i_size );
    p_stream->skeleton.p_index = calloc( i_size, sizeof(uint8_t) );
    if ( !p_stream->skeleton.p_index ) return false;
    p_stream->skeleton.i_index_size = i_size;
    p_stream->skeleton.i_index_payload = 0;
    return true;
}

/*****************************************************************************
 * Mux: multiplex available data in input fifos into the Ogg bitstream
 *****************************************************************************/
static int Mux( sout_mux_t *p_mux )
{
    sout_mux_sys_t *p_sys = p_mux->p_sys;
    vlc_tick_t     i_dts;

    /* End any stream that ends in that group */
    if ( p_sys->i_del_streams )
    {
        /* Remove deleted logical streams */
        for( int i = 0; i < p_sys->i_del_streams; i++ )
        {
            OggCreateStreamFooter( p_mux, p_sys->pp_del_streams[i] );
            FREENULL( p_sys->pp_del_streams[i]->p_oggds_header );
            FREENULL( p_sys->pp_del_streams[i] );
        }
        FREENULL( p_sys->pp_del_streams );
        p_sys->i_del_streams = 0;
    }

    if ( p_sys->i_streams == 0 )
    {
        /* All streams have been deleted, or none have ever been created
           From this point, we are allowed to start a new group of logical streams */
        p_sys->skeleton.b_head_done = false;
        p_sys->b_can_add_streams = true;
        p_sys->i_segment_start = p_sys->i_pos;
    }

    if ( p_sys->i_add_streams )
    {
        if ( !p_sys->b_can_add_streams )
        {
            msg_Warn( p_mux, "Can't add new stream %d/%d: Considerer increasing sout-mux-caching variable", p_sys->i_del_streams, p_sys->i_streams);
            msg_Warn( p_mux, "Resetting and setting new identity to current streams");

            /* resetting all active streams */
            for ( int i=0; i < p_sys->i_streams; i++ )
            {
                ogg_stream_t * p_stream = (ogg_stream_t *) p_mux->pp_inputs[i]->p_sys;
                if ( p_stream->b_finished || !p_stream->b_started ) continue;
                OggCreateStreamFooter( p_mux, p_stream );
                p_stream->i_serial_no = p_sys->i_next_serial_no++;
                p_stream->i_packet_no = 0;
                p_stream->b_finished = true;
            }

            /* rewrite fishead with final values */
            if ( p_sys->skeleton.b_head_done )
            {
                OggRewriteFisheadPage( p_mux );
            }

            p_sys->b_can_add_streams = true;
            p_sys->skeleton.b_head_done = false;
            p_sys->i_segment_start = p_sys->i_pos;
        }

        /* Open new ogg stream */
        if( sout_MuxGetStream( p_mux, 1, &i_dts) < 0 )
        {
            msg_Dbg( p_mux, "waiting for data..." );
            return VLC_SUCCESS;
        }
        msg_Dbg( p_mux, "writing streams headers" );
        p_sys->i_start_dts = i_dts;
        p_sys->i_streams = p_mux->i_nb_inputs;
        p_sys->i_del_streams = 0;
        p_sys->i_add_streams = 0;
        p_sys->skeleton.b_create = true;

        if ( ! OggCreateHeaders( p_mux ) )
            return VLC_ENOMEM;

        /* If we're switching to end of headers, then that's data start */
        if ( p_sys->b_can_add_streams )
        {
            msg_Dbg( p_mux, "data starts from %zu", p_sys->i_pos );
            p_sys->i_data_start = p_sys->i_pos;
        }

        /* Since we started sending secondaryheader or data pages,
             * we're no longer allowed to create new streams, until all streams end */
        p_sys->b_can_add_streams = false;
    }

    /* Do the regular data mux thing */
    for( ;; )
    {
        int i_stream = sout_MuxGetStream( p_mux, 1, NULL );
        if( i_stream < 0 )
            return VLC_SUCCESS;
        MuxBlock( p_mux, p_mux->pp_inputs[i_stream] );
    }

    return VLC_SUCCESS;
}

static int MuxBlock( sout_mux_t *p_mux, sout_input_t *p_input )
{
    sout_mux_sys_t *p_sys = p_mux->p_sys;
    ogg_stream_t *p_stream = (ogg_stream_t*)p_input->p_sys;
    block_t *p_data = block_FifoGet( p_input->p_fifo );
    block_t *p_og = NULL;
    ogg_packet op;
    vlc_tick_t i_time;

    if( p_stream->fmt.i_codec != VLC_CODEC_VORBIS &&
        p_stream->fmt.i_codec != VLC_CODEC_FLAC &&
        p_stream->fmt.i_codec != VLC_CODEC_SPEEX &&
        p_stream->fmt.i_codec != VLC_CODEC_OPUS &&
        p_stream->fmt.i_codec != VLC_CODEC_THEORA &&
        p_stream->fmt.i_codec != VLC_CODEC_DAALA &&
        p_stream->fmt.i_codec != VLC_CODEC_VP8 &&
        p_stream->fmt.i_codec != VLC_CODEC_DIRAC )
    {
        p_data = block_Realloc( p_data, 1, p_data->i_buffer );
        p_data->p_buffer[0] = PACKET_IS_SYNCPOINT;      // FIXME
    }

    if ( p_stream->fmt.i_codec == VLC_CODEC_DIRAC && p_stream->i_baseptsdelay < 0 )
        p_stream->i_baseptsdelay = p_data->i_pts - p_data->i_dts;

    op.packet   = p_data->p_buffer;
    op.bytes    = p_data->i_buffer;
    op.b_o_s    = 0;
    op.e_o_s    = 0;
    op.packetno = p_stream->i_packet_no++;
    op.granulepos = -1;

    if( p_stream->fmt.i_cat == AUDIO_ES )
    {
        if( p_stream->fmt.i_codec == VLC_CODEC_VORBIS ||
            p_stream->fmt.i_codec == VLC_CODEC_FLAC ||
            p_stream->fmt.i_codec == VLC_CODEC_OPUS ||
            p_stream->fmt.i_codec == VLC_CODEC_SPEEX )
        {
            /* number of sample from begining + current packet */
            op.granulepos =
                samples_from_vlc_tick( p_data->i_dts - p_sys->i_start_dts + p_data->i_length,
                                       p_input->p_fmt->audio.i_rate );

            i_time = p_data->i_dts - p_sys->i_start_dts;
            AddIndexEntry( p_mux, i_time, p_input );
        }
        else if( p_stream->p_oggds_header )
        {
            /* number of sample from begining */
            op.granulepos = samples_from_vlc_tick( p_data->i_dts - p_sys->i_start_dts,
                                  p_stream->p_oggds_header->i_samples_per_unit );
        }
    }
    else if( p_stream->fmt.i_cat == VIDEO_ES )
    {
        if( p_stream->fmt.i_codec == VLC_CODEC_THEORA ||
            p_stream->fmt.i_codec == VLC_CODEC_DAALA )
        {
            p_stream->i_num_frames++;
            if( p_data->i_flags & BLOCK_FLAG_TYPE_I )
            {
                p_stream->i_num_keyframes++;
                p_stream->i_last_keyframe = p_stream->i_num_frames;

                /* presentation time */
                i_time = vlc_tick_from_samples( (p_stream->i_num_frames - 1 ) *
                        p_stream->fmt.video.i_frame_rate_base,
                        p_stream->fmt.video.i_frame_rate );
                AddIndexEntry( p_mux, i_time, p_input );
            }

            op.granulepos = (p_stream->i_last_keyframe << p_stream->i_keyframe_granule_shift )
                          | (p_stream->i_num_frames-p_stream->i_last_keyframe);
        }
        else if( p_stream->fmt.i_codec == VLC_CODEC_DIRAC )
        {

#define FRAME_ROUND(a) \
    if ( ( a + 5000 / CLOCK_FREQ ) > ( a / CLOCK_FREQ ) )\
        a += 5000;\
    a /= CLOCK_FREQ;

            int64_t dt = (p_data->i_dts - p_sys->i_start_dts) * p_stream->fmt.video.i_frame_rate /
                    p_stream->fmt.video.i_frame_rate_base;
            FRAME_ROUND( dt );

            int64_t pt = (p_data->i_pts - p_sys->i_start_dts - p_stream->i_baseptsdelay ) *
                    p_stream->fmt.video.i_frame_rate / p_stream->fmt.video.i_frame_rate_base;
            FRAME_ROUND( pt );

            /* (shro) some PTS could be repeated within 1st frames */
            if ( pt == p_stream->i_dirac_last_pt )
                pt++;
            else
                p_stream->i_dirac_last_pt = pt;

            /* (shro) some DTS could be repeated within 1st frames */
            if ( dt == p_stream->i_dirac_last_dt )
                dt++;
            else
                p_stream->i_dirac_last_dt = dt;

            if( p_data->i_flags & BLOCK_FLAG_TYPE_I )
                p_stream->i_last_keyframe = dt;
            int64_t dist = dt - p_stream->i_last_keyframe;

            /* Everything increments by two for progressive */
            if ( true )
            {
                pt *=2;
                dt *=2;
            }

            int64_t delay = llabs(pt - dt);

            op.granulepos = (pt - delay) << 31 | (dist&0xff00) << 14
                          | (delay&0x1fff) << 9 | (dist&0xff);
#ifndef NDEBUG
            msg_Dbg( p_mux, "dts %"PRId64" pts %"PRId64" dt %"PRId64" pt %"PRId64" delay %"PRId64" granule %"PRId64,
                     (p_data->i_dts - p_sys->i_start_dts),
                     (p_data->i_pts - p_sys->i_start_dts ),
                     dt, pt, delay, op.granulepos );
#endif

            AddIndexEntry( p_mux, dt, p_input );
        }
        else if( p_stream->fmt.i_codec == VLC_CODEC_VP8 )
        {
            p_stream->i_num_frames++;
            if( p_data->i_flags & BLOCK_FLAG_TYPE_I )
            {
                p_stream->i_num_keyframes++;
                p_stream->i_last_keyframe = p_stream->i_num_frames;

                /* presentation time */
                i_time = vlc_tick_from_samples( ( p_stream->i_num_frames - 1 ) *
                         p_stream->fmt.video.i_frame_rate_base, p_stream->fmt.video.i_frame_rate );
                AddIndexEntry( p_mux, i_time, p_input );
            }
            op.granulepos = ( ((int64_t)p_stream->i_num_frames) << 32 ) |
            ( ( ( p_stream->i_num_frames - p_stream->i_last_keyframe ) & 0x07FFFFFF ) << 3 );
        }
        else if( p_stream->p_oggds_header )
            op.granulepos = MSFTIME_FROM_VLC_TICK( p_data->i_dts - p_sys->i_start_dts ) /
                p_stream->p_oggds_header->i_time_unit;
    }
    else if( p_stream->fmt.i_cat == SPU_ES )
    {
        /* granulepos is in millisec */
        op.granulepos = MS_FROM_VLC_TICK( p_data->i_dts - p_sys->i_start_dts );
    }
    else
        return VLC_EGENERIC;

    p_stream->u_last_granulepos = op.granulepos;
    ogg_stream_packetin( &p_stream->os, &op );

    if( p_stream->fmt.i_cat == SPU_ES ||
        p_stream->fmt.i_codec == VLC_CODEC_SPEEX ||
        p_stream->fmt.i_codec == VLC_CODEC_DIRAC )
    {
        /* Subtitles or Speex packets are quite small so they
         * need to be flushed to be sent on time */
        /* The OggDirac mapping suggests ever so strongly that a
         * page flush occurs after each OggDirac packet, so to make
         * the timestamps unambiguous */
        p_og = OggStreamFlush( p_mux, &p_stream->os, p_data->i_dts );
    }
    else
    {
        p_og = OggStreamPageOut( p_mux, &p_stream->os, p_data->i_dts );
    }

    if( p_og )
    {
        OggSetDate( p_og, p_stream->i_dts, p_stream->i_length );
        p_stream->i_dts = -1;
        p_stream->i_length = 0;
        p_sys->i_pos += sout_AccessOutWrite( p_mux->p_access, p_og );
    }
    else
    {
        if( p_stream->i_dts < 0 )
        {
            p_stream->i_dts = p_data->i_dts;
        }
        p_stream->i_length += p_data->i_length;
    }

    block_Release( p_data );
    return VLC_SUCCESS;
}
