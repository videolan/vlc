/*****************************************************************************
 * ogg.c: ogg muxer module for vlc
 *****************************************************************************
 * Copyright (C) 2001, 2002 VideoLAN
 * $Id: ogg.c,v 1.19 2003/10/22 17:12:30 gbazin Exp $
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Gildas Bazin <gbazin@netcourrier.com>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_TIME_H
#   include <time.h>
#endif

#include <vlc/vlc.h>
#include <vlc/input.h>
#include <vlc/sout.h>

#include "codecs.h"

#include <ogg/ogg.h>

/*****************************************************************************
 * Exported prototypes
 *****************************************************************************/
static int  Open   ( vlc_object_t * );
static void Close  ( vlc_object_t * );

static int Capability(sout_mux_t *, int, void *, void * );
static int AddStream( sout_mux_t *, sout_input_t * );
static int DelStream( sout_mux_t *, sout_input_t * );
static int Mux      ( sout_mux_t * );

static sout_buffer_t *OggCreateHeader( sout_mux_t *, mtime_t );
static sout_buffer_t *OggCreateFooter( sout_mux_t *, mtime_t );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( _("Ogg/ogm muxer") );
    set_capability( "sout mux", 10 );
    add_shortcut( "ogg" );
    add_shortcut( "ogm" );
    set_callbacks( Open, Close );
vlc_module_end();

/*****************************************************************************
 * Misc declarations
 *****************************************************************************/
#define FREE( p ) if( p ) { free( p ); (p) = NULL; }

/* Structures used for OggDS headers used in ogm files */

#define PACKET_TYPE_HEADER   0x01
#define PACKET_TYPE_COMMENT  0x03
#define PACKET_IS_SYNCPOINT  0x08

typedef struct
#ifdef HAVE_ATTRIBUTE_PACKED
    __attribute__((__packed__))
#endif
{
    int32_t i_width;
    int32_t i_height;
} oggds_header_video_t;

typedef struct
#ifdef HAVE_ATTRIBUTE_PACKED
    __attribute__((__packed__))
#endif
{
    int16_t i_channels;
    int16_t i_block_align;
    int32_t i_avgbytespersec;
} oggds_header_audio_t;

typedef struct
#ifdef HAVE_ATTRIBUTE_PACKED
    __attribute__((__packed__))
#endif
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

/* Helper writer functions */

#define SetWLE( p, v ) _SetWLE( (uint8_t*)p, v)
static void _SetWLE( uint8_t *p, uint16_t i_dw )
{
    p[1] = ( i_dw >>  8 )&0xff;
    p[0] = ( i_dw       )&0xff;
}

#define SetDWLE( p, v ) _SetDWLE( (uint8_t*)p, v)
static void _SetDWLE( uint8_t *p, uint32_t i_dw )
{
    p[3] = ( i_dw >> 24 )&0xff;
    p[2] = ( i_dw >> 16 )&0xff;
    p[1] = ( i_dw >>  8 )&0xff;
    p[0] = ( i_dw       )&0xff;
}
#define SetQWLE( p, v ) _SetQWLE( (uint8_t*)p, v)
static void _SetQWLE( uint8_t *p, uint64_t i_qw )
{
    SetDWLE( p,   i_qw&0xffffffff );
    SetDWLE( p+4, ( i_qw >> 32)&0xffffffff );
}

/*
 * TODO  move this function to src/stream_output.c (used by nearly all muxers)
 */
static int MuxGetStream( sout_mux_t *p_mux, int *pi_stream, mtime_t *pi_dts )
{
    mtime_t i_dts;
    int     i_stream;
    int     i;

    for( i = 0, i_dts = 0, i_stream = -1; i < p_mux->i_nb_inputs; i++ )
    {
        sout_fifo_t  *p_fifo;

        p_fifo = p_mux->pp_inputs[i]->p_fifo;

        /* We don't really need to have anything in the SPU fifo */
        if( p_mux->pp_inputs[i]->p_fmt->i_cat == SPU_ES &&
            p_fifo->i_depth == 0 ) continue;

        if( p_fifo->i_depth > 2 ||
            /* Special case for SPUs */
            ( p_mux->pp_inputs[i]->p_fmt->i_cat == SPU_ES &&
              p_fifo->i_depth > 0 ) )
        {
            sout_buffer_t *p_buf;

            p_buf = sout_FifoShow( p_fifo );
            if( i_stream < 0 || p_buf->i_dts < i_dts )
            {
                i_dts = p_buf->i_dts;
                i_stream = i;
            }
        }
        else
        {
            // wait that all fifo have at least 3 packets (3 vorbis headers)
            return( -1 );
        }
    }
    if( pi_stream )
    {
        *pi_stream = i_stream;
    }
    if( pi_dts )
    {
        *pi_dts = i_dts;
    }
    return( i_stream );
}

/*****************************************************************************
 * Definitions of structures and functions used by this plugins 
 *****************************************************************************/
typedef struct
{
    int i_cat;
    int i_fourcc;

    int b_new;

    mtime_t i_dts;
    mtime_t i_length;
    int     i_packet_no;
    int     i_serial_no;
    int     i_keyframe_granule_shift; /* Theora only */
    ogg_stream_state os;

    oggds_header_t oggds_header;

    sout_buffer_t *pp_sout_headers[3];
    int           i_sout_headers;

} ogg_stream_t;

struct sout_mux_sys_t
{
    int     i_streams;

    mtime_t i_start_dts;
    int     i_next_serial_no;

    /* number of logical streams pending to be added */
    int i_add_streams;

    /* logical streams pending to be deleted */
    int i_del_streams;
    ogg_stream_t **pp_del_streams;
};

static void OggSetDate( sout_buffer_t *, mtime_t , mtime_t  );
static sout_buffer_t *OggStreamFlush( sout_mux_t *, ogg_stream_state *,
                                      mtime_t );

/*****************************************************************************
 * Open: Open muxer
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    sout_mux_t      *p_mux = (sout_mux_t*)p_this;
    sout_mux_sys_t  *p_sys;

    msg_Info( p_mux, "Open" );

    p_sys                 = malloc( sizeof( sout_mux_sys_t ) );
    p_sys->i_streams      = 0;
    p_sys->i_add_streams  = 0;
    p_sys->i_del_streams  = 0;
    p_sys->pp_del_streams = 0;

    p_mux->p_sys        = p_sys;
    p_mux->pf_capacity  = Capability;
    p_mux->pf_addstream = AddStream;
    p_mux->pf_delstream = DelStream;
    p_mux->pf_mux       = Mux;
    p_mux->i_preheader  = 1;

    /* First serial number is random.
     * (Done like this because on win32 you need to seed the random number
     *  generator once per thread). */
    srand( (unsigned int)time( NULL ) );
    p_sys->i_next_serial_no = rand();

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
        sout_buffer_t *p_og = NULL;
        mtime_t i_dts = -1;
        int i;

        /* Close the current ogg stream */
        msg_Dbg( p_mux, "writing footer" );
        sout_BufferChain( &p_og, OggCreateFooter( p_mux, 0 ) );

        /* Remove deleted logical streams */
        for( i = 0; i < p_sys->i_del_streams; i++ )
        {
            i_dts = p_sys->pp_del_streams[i]->i_dts;
            ogg_stream_clear( &p_sys->pp_del_streams[i]->os );
            FREE( p_sys->pp_del_streams[i] );
        }
        FREE( p_sys->pp_del_streams );
        p_sys->i_streams -= p_sys->i_del_streams;

        /* Write footer */
        OggSetDate( p_og, i_dts, 0 );
        sout_AccessOutWrite( p_mux->p_access, p_og );
    }

    free( p_sys );
}

static int Capability( sout_mux_t *p_mux, int i_query, void *p_args,
                       void *p_answer )
{
   switch( i_query )
   {
        case SOUT_MUX_CAP_GET_ADD_STREAM_ANY_TIME:
            *(vlc_bool_t*)p_answer = VLC_TRUE;
            return( SOUT_MUX_CAP_ERR_OK );
        default:
            return( SOUT_MUX_CAP_ERR_UNIMPLEMENTED );
   }
}

/*****************************************************************************
 * AddStream: Add an elementary stream to the muxed stream
 *****************************************************************************/
static int AddStream( sout_mux_t *p_mux, sout_input_t *p_input )
{
    sout_mux_sys_t *p_sys = p_mux->p_sys;
    ogg_stream_t   *p_stream;

    msg_Dbg( p_mux, "adding input" );

    p_input->p_sys = (void *)p_stream = malloc( sizeof( ogg_stream_t ) );

    p_stream->i_cat       = p_input->p_fmt->i_cat;
    p_stream->i_fourcc    = p_input->p_fmt->i_fourcc;
    p_stream->i_serial_no = p_sys->i_next_serial_no++;
    p_stream->i_packet_no = 0;

    p_stream->i_sout_headers = 0;

    memset( &p_stream->oggds_header, 0, sizeof(p_stream->oggds_header) );
    p_stream->oggds_header.i_packet_type = PACKET_TYPE_HEADER;
    switch( p_input->p_fmt->i_cat )
    {
    case VIDEO_ES:
        switch( p_stream->i_fourcc )
        {
        case VLC_FOURCC( 'm', 'p','4', 'v' ):
        case VLC_FOURCC( 'D', 'I','V', '3' ):
            memcpy( p_stream->oggds_header.stream_type, "video", 5 );
            if( p_stream->i_fourcc == VLC_FOURCC( 'm', 'p','4', 'v' ) )
            {
                memcpy( p_stream->oggds_header.sub_type, "XVID", 4 );
            }
            else if( p_stream->i_fourcc == VLC_FOURCC( 'D', 'I','V', '3' ) )
            {
                memcpy( p_stream->oggds_header.sub_type, "DIV3", 4 );
            }
            SetDWLE( &p_stream->oggds_header.i_size,
                     sizeof( oggds_header_t ) - 1);
            SetQWLE( &p_stream->oggds_header.i_time_unit,
                     I64C(10000000)/(int64_t)25 );  // FIXME (25fps)
            SetQWLE( &p_stream->oggds_header.i_samples_per_unit, 1 );
            SetDWLE( &p_stream->oggds_header.i_default_len, 1 ); /* ??? */
            SetDWLE( &p_stream->oggds_header.i_buffer_size, 1024*1024 );
            SetWLE( &p_stream->oggds_header.i_bits_per_sample, 0 );
            SetDWLE( &p_stream->oggds_header.header.video.i_width,
                     p_input->p_fmt->i_width );
            SetDWLE( &p_stream->oggds_header.header.video.i_height,
                     p_input->p_fmt->i_height );
            msg_Dbg( p_mux, "mp4v/div3 stream" );
            break;

        case VLC_FOURCC( 't', 'h', 'e', 'o' ):
            msg_Dbg( p_mux, "theora stream" );
            break;

        default:
            FREE( p_input->p_sys );
            return( VLC_EGENERIC );
        }
        break;

    case AUDIO_ES:
        switch( p_stream->i_fourcc )
        {
        case VLC_FOURCC( 'm', 'p','g', 'a' ):
        case VLC_FOURCC( 'a', '5','2', ' ' ):
            memcpy( p_stream->oggds_header.stream_type, "audio", 5 );
            if( p_stream->i_fourcc == VLC_FOURCC( 'm', 'p','g', 'a' ) )
            {
                memcpy( p_stream->oggds_header.sub_type, "55  ", 4 );
            }
            else if( p_stream->i_fourcc == VLC_FOURCC( 'a', '5','2', ' ' ) )
            {
                memcpy( p_stream->oggds_header.sub_type, "2000", 4 );
            }
            SetDWLE( &p_stream->oggds_header.i_size,
                     sizeof( oggds_header_t ) - 1);
            SetQWLE( &p_stream->oggds_header.i_time_unit, 0 /* not used */ );
            SetDWLE( &p_stream->oggds_header.i_default_len, 1 );
            SetDWLE( &p_stream->oggds_header.i_buffer_size, 30*1024 );
            SetQWLE( &p_stream->oggds_header.i_samples_per_unit,
                     p_input->p_fmt->i_sample_rate );
            SetWLE( &p_stream->oggds_header.i_bits_per_sample, 0 );
            SetDWLE( &p_stream->oggds_header.header.audio.i_channels,
                     p_input->p_fmt->i_channels );
            SetDWLE( &p_stream->oggds_header.header.audio.i_block_align,
                     p_input->p_fmt->i_block_align );
            SetDWLE( &p_stream->oggds_header.header.audio.i_avgbytespersec, 0);
            msg_Dbg( p_mux, "mpga/a52 stream" );
            break;

        case VLC_FOURCC( 'v', 'o', 'r', 'b' ):
            msg_Dbg( p_mux, "vorbis stream" );
            break;

        case VLC_FOURCC( 's', 'p', 'x', ' ' ):
            msg_Dbg( p_mux, "speex stream" );
            break;

        default:
            FREE( p_input->p_sys );
            return( VLC_EGENERIC );
        }
        break;

    case SPU_ES:
        switch( p_stream->i_fourcc )
        {
        case VLC_FOURCC( 's', 'u','b', 't' ):
            memcpy( p_stream->oggds_header.stream_type, "text", 4 );
            msg_Dbg( p_mux, "subtitles stream" );
            break;

        default:
            FREE( p_input->p_sys );
            return( VLC_EGENERIC );
        }
        break;
    default:
        FREE( p_input->p_sys );
        return( VLC_EGENERIC );
    }

    p_stream->b_new = VLC_TRUE;

    p_sys->i_add_streams++;

    return( VLC_SUCCESS );
}

/*****************************************************************************
 * DelStream: Delete an elementary stream from the muxed stream
 *****************************************************************************/
static int DelStream( sout_mux_t *p_mux, sout_input_t *p_input )
{
    sout_mux_sys_t *p_sys  = p_mux->p_sys;
    ogg_stream_t   *p_stream = (ogg_stream_t*)p_input->p_sys;
    sout_buffer_t  *p_og;

    msg_Dbg( p_mux, "removing input" );

    /* flush all remaining data */
    if( p_input->p_sys )
    {
        int i;

        if( ( p_og = OggStreamFlush( p_mux, &p_stream->os, 0 ) ) )
        {
            OggSetDate( p_og, p_stream->i_dts, p_stream->i_length );
            sout_AccessOutWrite( p_mux->p_access, p_og );
        }

        for( i = 0; i < p_stream->i_sout_headers; i++ )
        {
            sout_BufferDelete( p_mux->p_sout, p_stream->pp_sout_headers[i] );
            p_stream->i_sout_headers = 0;
        }

        /* move input in delete queue */
        if( !p_stream->b_new )
        {
            p_sys->pp_del_streams = realloc( p_sys->pp_del_streams,
                                             (p_sys->i_del_streams + 1) *
                                             sizeof(ogg_stream_t *) );
            p_sys->pp_del_streams[p_sys->i_del_streams++] = p_stream;
        }
        else
        {
            /* Wasn't already added so get rid of it */
            ogg_stream_clear( &p_stream->os );
            FREE( p_stream );
            p_sys->i_add_streams--;
        }
    }

    p_input->p_sys = NULL;

    return( 0 );
}

/*****************************************************************************
 * Ogg bitstream manipulation routines
 *****************************************************************************/
static sout_buffer_t *OggStreamFlush( sout_mux_t *p_mux,
                                      ogg_stream_state *p_os, mtime_t i_pts )
{
    sout_buffer_t *p_og, *p_og_first = NULL;
    ogg_page      og;

    for( ;; )
    {
        /* flush all data */
        int i_result;
        int i_size;
        if( ( i_result = ogg_stream_flush( p_os, &og ) ) == 0 )
        {
            break;
        }

        i_size = og.header_len + og.body_len;
        p_og = sout_BufferNew( p_mux->p_sout, i_size);

        memcpy( p_og->p_buffer, og.header, og.header_len );
        memcpy( p_og->p_buffer + og.header_len, og.body, og.body_len );
        p_og->i_size    = i_size;
        p_og->i_dts     = 0;
        p_og->i_pts     = i_pts;
        p_og->i_length  = 0;

        i_pts   = 0; // write it only once

        sout_BufferChain( &p_og_first, p_og );
    }

    return( p_og_first );
}

static sout_buffer_t *OggStreamPageOut( sout_mux_t *p_mux,
                                        ogg_stream_state *p_os, mtime_t i_pts )
{
    sout_buffer_t *p_og, *p_og_first = NULL;
    ogg_page      og;

    for( ;; )
    {
        /* flush all data */
        int i_result;
        int i_size;
        if( ( i_result = ogg_stream_pageout( p_os, &og ) ) == 0 )
        {
            break;
        }

        i_size = og.header_len + og.body_len;
        p_og = sout_BufferNew( p_mux->p_sout, i_size);

        memcpy( p_og->p_buffer, og.header, og.header_len );
        memcpy( p_og->p_buffer + og.header_len, og.body, og.body_len );
        p_og->i_size    = i_size;
        p_og->i_dts     = 0;
        p_og->i_pts     = i_pts;
        p_og->i_length  = 0;

        i_pts   = 0; // write them only once

        sout_BufferChain( &p_og_first, p_og );
    }

    return( p_og_first );
}

static sout_buffer_t *OggCreateHeader( sout_mux_t *p_mux, mtime_t i_dts )
{
    sout_buffer_t *p_hdr = NULL;
    sout_buffer_t *p_og;
    ogg_packet    op;
    int i;

    /* Write header for each stream. All b_o_s (beginning of stream) packets
     * must appear first in the ogg stream so we take care of them first. */
    for( i = 0; i < p_mux->i_nb_inputs; i++ )
    {
        ogg_stream_t *p_stream = (ogg_stream_t*)p_mux->pp_inputs[i]->p_sys;
        p_stream->b_new = VLC_FALSE;

        msg_Dbg( p_mux, "creating header for %4.4s",
                 (char *)&p_stream->i_fourcc );

        ogg_stream_init( &p_stream->os, p_stream->i_serial_no );
        p_stream->i_packet_no = 0;

        if( p_stream->i_fourcc == VLC_FOURCC( 'v', 'o', 'r', 'b' ) ||
            p_stream->i_fourcc == VLC_FOURCC( 's', 'p', 'x', ' ' ) ||
            p_stream->i_fourcc == VLC_FOURCC( 't', 'h', 'e', 'o' ) )
        {
            /* Special case, headers are already there in the
             * incoming stream or we backed them up earlier */

            /* first packet in order: vorbis/speex/theora info */
            if( !p_stream->i_sout_headers )
            {
                p_og = sout_FifoGet( p_mux->pp_inputs[i]->p_fifo );
                op.packet = p_og->p_buffer;
                op.bytes  = p_og->i_size;
                op.b_o_s  = 1;
                op.e_o_s  = 0;
                op.granulepos = 0;
                op.packetno = p_stream->i_packet_no++;
                ogg_stream_packetin( &p_stream->os, &op );
                p_stream->pp_sout_headers[0] =
                    OggStreamFlush( p_mux, &p_stream->os, 0 );
                p_stream->i_sout_headers++;
            }
            p_og = sout_BufferDuplicate( p_mux->p_sout,
                                         p_stream->pp_sout_headers[0] );

            /* Get keyframe_granule_shift for theora granulepos calculation */
            if( p_stream->i_fourcc == VLC_FOURCC( 't', 'h', 'e', 'o' ) )
            {
                int i_keyframe_frequency_force = 1 << (op.packet[36] >> 3);

                /* granule_shift = i_log( frequency_force -1 ) */
                p_stream->i_keyframe_granule_shift = 0;
                i_keyframe_frequency_force--;
                while( i_keyframe_frequency_force )
                {
                    p_stream->i_keyframe_granule_shift++;
                    i_keyframe_frequency_force >>= 1;
                }
            }
        }
        else
        {
            /* ds header */
            op.packet = (uint8_t*)&p_stream->oggds_header;
            op.bytes  = sizeof( oggds_header_t );
            op.b_o_s  = 1;
            op.e_o_s  = 0;
            op.granulepos = 0;
            op.packetno = p_stream->i_packet_no++;
            ogg_stream_packetin( &p_stream->os, &op );
            p_og = OggStreamFlush( p_mux, &p_stream->os, 0 );
        }

        sout_BufferChain( &p_hdr, p_og );
    }

    /* Take care of the non b_o_s headers */
    for( i = 0; i < p_mux->i_nb_inputs; i++ )
    {
        ogg_stream_t *p_stream = (ogg_stream_t*)p_mux->pp_inputs[i]->p_sys;

        if( p_stream->i_fourcc == VLC_FOURCC( 'v', 'o', 'r', 'b' ) ||
            p_stream->i_fourcc == VLC_FOURCC( 's', 'p', 'x', ' ' ) ||
            p_stream->i_fourcc == VLC_FOURCC( 't', 'h', 'e', 'o' ) )
        {
            /* Special case, headers are already there in the incoming stream.
             * We need to gather them an mark them as headers. */
            int j;
            for( j = 0; j < 2; j++ )
            {
                if( p_stream->i_sout_headers < j + 2 )
                {
                    /* next packets in order: comments and codebooks */
                    p_og = sout_FifoGet( p_mux->pp_inputs[i]->p_fifo );
                    op.packet = p_og->p_buffer;
                    op.bytes  = p_og->i_size;
                    op.b_o_s  = 0;
                    op.e_o_s  = 0;
                    op.granulepos = 0;
                    op.packetno = p_stream->i_packet_no++;
                    ogg_stream_packetin( &p_stream->os, &op );
                    p_stream->pp_sout_headers[j+1] =
                        OggStreamFlush( p_mux, &p_stream->os, 0 );
                    p_stream->i_sout_headers++;
                }

                p_og = sout_BufferDuplicate( p_mux->p_sout,
                                             p_stream->pp_sout_headers[j+1] );
                sout_BufferChain( &p_hdr, p_og );
            }
        }
        else
        {
            uint8_t com[128];
            int     i_com;

            /* comment */
            com[0] = PACKET_TYPE_COMMENT;
            i_com = snprintf( &com[1], 128, VERSION" stream output" ) + 1;
            op.packet = com;
            op.bytes  = i_com;
            op.b_o_s  = 0;
            op.e_o_s  = 0;
            op.granulepos = 0;
            op.packetno = p_stream->i_packet_no++;
            ogg_stream_packetin( &p_stream->os, &op );
            p_og = OggStreamFlush( p_mux, &p_stream->os, 0 );
            sout_BufferChain( &p_hdr, p_og );
        }
    }

    /* set HEADER flag */
    for( p_og = p_hdr; p_og != NULL; p_og = p_og->p_next )
    {
        p_og->i_flags |= SOUT_BUFFER_FLAGS_HEADER;
    }
    return( p_hdr );
}

static sout_buffer_t *OggCreateFooter( sout_mux_t *p_mux, mtime_t i_dts )
{
    sout_mux_sys_t *p_sys = p_mux->p_sys;
    sout_buffer_t *p_hdr = NULL;
    sout_buffer_t *p_og;
    ogg_packet    op;
    int i;

    /* flush all remaining data */
    for( i = 0; i < p_mux->i_nb_inputs; i++ )
    {
        ogg_stream_t *p_stream = p_mux->pp_inputs[i]->p_sys;

        /* skip newly added streams */
        if( p_stream->b_new ) continue;

        if( ( p_og = OggStreamFlush( p_mux, &p_stream->os, 0 ) ) )
        {
            OggSetDate( p_og, p_stream->i_dts, p_stream->i_length );
            sout_AccessOutWrite( p_mux->p_access, p_og );
        }
    }

    /* Write eos packets for each stream. */
    for( i = 0; i < p_mux->i_nb_inputs; i++ )
    {
        ogg_stream_t *p_stream = p_mux->pp_inputs[i]->p_sys;

        /* skip newly added streams */
        if( p_stream->b_new ) continue;

        op.packet = NULL;
        op.bytes  = 0;
        op.b_o_s  = 0;
        op.e_o_s  = 1;
        op.granulepos = -1;
        op.packetno = p_stream->i_packet_no++;
        ogg_stream_packetin( &p_stream->os, &op );

        p_og = OggStreamFlush( p_mux, &p_stream->os, 0 );
        sout_BufferChain( &p_hdr, p_og );
        ogg_stream_clear( &p_stream->os );
    }

    for( i = 0; i < p_sys->i_del_streams; i++ )
    {
        op.packet = NULL;
        op.bytes  = 0;
        op.b_o_s  = 0;
        op.e_o_s  = 1;
        op.granulepos = -1;
        op.packetno = p_sys->pp_del_streams[i]->i_packet_no++;
        ogg_stream_packetin( &p_sys->pp_del_streams[i]->os, &op );

        p_og = OggStreamFlush( p_mux, &p_sys->pp_del_streams[i]->os, 0 );
        sout_BufferChain( &p_hdr, p_og );
        ogg_stream_clear( &p_sys->pp_del_streams[i]->os );
    }

    return( p_hdr );
}

static void OggSetDate( sout_buffer_t *p_og, mtime_t i_dts, mtime_t i_length )
{
    int i_count;
    sout_buffer_t *p_tmp;
    mtime_t i_delta;

    for( p_tmp = p_og, i_count = 0; p_tmp != NULL; p_tmp = p_tmp->p_next )
    {
        i_count++;
    }
    i_delta = i_length / i_count;

    for( p_tmp = p_og; p_tmp != NULL; p_tmp = p_tmp->p_next )
    {
        p_tmp->i_dts    = i_dts;
        p_tmp->i_length = i_delta;

        i_dts += i_delta;
    }
}

/*****************************************************************************
 * Mux: multiplex available data in input fifos into the Ogg bitstream
 *****************************************************************************/
static int Mux( sout_mux_t *p_mux )
{
    sout_mux_sys_t *p_sys = p_mux->p_sys;
    sout_buffer_t  *p_og = NULL;
    int            i_stream;
    mtime_t        i_dts;

    if( p_sys->i_add_streams || p_sys->i_del_streams )
    {
        /* Open new ogg stream */
        if( MuxGetStream( p_mux, &i_stream, &i_dts) < 0 )
        {
            msg_Dbg( p_mux, "waiting for data..." );
            return( VLC_SUCCESS );
        }

        if( p_sys->i_streams )
        {
            /* Close current ogg stream */
            int i;

            msg_Dbg( p_mux, "writing footer" );
            sout_BufferChain( &p_og, OggCreateFooter( p_mux, 0 ) );

            /* Remove deleted logical streams */
            for( i = 0; i < p_sys->i_del_streams; i++ )
            {
                FREE( p_sys->pp_del_streams[i] );
            }
            FREE( p_sys->pp_del_streams );
            p_sys->i_streams = 0;
        }

        msg_Dbg( p_mux, "writing header" );
        p_sys->i_start_dts = i_dts;
        p_sys->i_streams = p_mux->i_nb_inputs;
        p_sys->i_del_streams = 0;
        p_sys->i_add_streams = 0;
        sout_BufferChain( &p_og, OggCreateHeader( p_mux, i_dts ) );

        /* Write header and/or footer */
        OggSetDate( p_og, i_dts, 0 );
        sout_AccessOutWrite( p_mux->p_access, p_og );
        p_og = NULL;
    }

    for( ;; )
    {
        sout_input_t  *p_input;
        ogg_stream_t  *p_stream;
        sout_buffer_t *p_data;
        ogg_packet    op;

        if( MuxGetStream( p_mux, &i_stream, &i_dts) < 0 )
        {
            return( VLC_SUCCESS );
        }

        p_input  = p_mux->pp_inputs[i_stream];
        p_stream = (ogg_stream_t*)p_input->p_sys;
        p_data   = sout_FifoGet( p_input->p_fifo );

        if( p_stream->i_fourcc != VLC_FOURCC( 'v', 'o', 'r', 'b' ) &&
            p_stream->i_fourcc != VLC_FOURCC( 's', 'p', 'x', ' ' ) &&
            p_stream->i_fourcc != VLC_FOURCC( 't', 'h', 'e', 'o' ) )
        {
            sout_BufferReallocFromPreHeader( p_mux->p_sout, p_data, 1 );
            p_data->p_buffer[0] = PACKET_IS_SYNCPOINT;      // FIXME
        }

        op.packet   = p_data->p_buffer;
        op.bytes    = p_data->i_size;
        op.b_o_s    = 0;
        op.e_o_s    = 0;
        op.packetno = p_stream->i_packet_no++;

        if( p_stream->i_cat == AUDIO_ES )
        {
            if( p_stream->i_fourcc == VLC_FOURCC( 'v', 'o', 'r', 'b' ) ||
                p_stream->i_fourcc == VLC_FOURCC( 's', 'p', 'x', ' ' ) )
            {
                /* number of sample from begining + current packet */
                op.granulepos =
                    ( i_dts + p_data->i_length - p_sys->i_start_dts ) *
                    p_input->p_fmt->i_sample_rate / I64C(1000000);
            }
            else
            {
                /* number of sample from begining */
                op.granulepos = ( i_dts - p_sys->i_start_dts ) *
                    p_stream->oggds_header.i_samples_per_unit / I64C(1000000);
            }
        }
        else if( p_stream->i_cat == VIDEO_ES )
        {
            if( p_stream->i_fourcc == VLC_FOURCC( 't', 'h', 'e', 'o' ) )
            {
                /* FIXME, we assume only keyframes and 25fps */
                op.granulepos = ( ( i_dts - p_sys->i_start_dts ) * I64C(25)
                    / I64C(1000000) ) << p_stream->i_keyframe_granule_shift;
            }
            else
                op.granulepos = ( i_dts - p_sys->i_start_dts ) * I64C(10) /
                    p_stream->oggds_header.i_time_unit;
        }
        else if( p_stream->i_cat == SPU_ES )
        {
            /* granulepos is in milisec */
            op.granulepos = ( i_dts - p_sys->i_start_dts ) / 1000;
        }

        ogg_stream_packetin( &p_stream->os, &op );

        if( p_stream->i_cat == SPU_ES ||
            p_stream->i_fourcc == VLC_FOURCC( 's', 'p', 'x', ' ' ) )
        {
            /* Subtitles or Speex packets are quite small so they 
             * need to be flushed to be sent on time */
            sout_BufferChain( &p_og, OggStreamFlush( p_mux, &p_stream->os,
                                                     p_data->i_dts ) );
        }
        else
        {
            sout_BufferChain( &p_og, OggStreamPageOut( p_mux, &p_stream->os,
                                                       p_data->i_dts ) );
        }

        if( p_og )
        {
            OggSetDate( p_og, p_stream->i_dts, p_stream->i_length );
            p_stream->i_dts = -1;
            p_stream->i_length = 0;

            sout_AccessOutWrite( p_mux->p_access, p_og );

            p_og = NULL;
        }
        else
        {
            if( p_stream->i_dts < 0 )
            {
                p_stream->i_dts = p_data->i_dts;
            }
            p_stream->i_length += p_data->i_length;
        }

        sout_BufferDelete( p_mux->p_sout, p_data );
    }

    return( VLC_SUCCESS );
}
