/*****************************************************************************
 * ogg.c: ogg muxer module for vlc
 *****************************************************************************
 * Copyright (C) 2001, 2002, 2006 the VideoLAN team
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Gildas Bazin <gbazin@videolan.org>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
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

#include <ogg/ogg.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open   ( vlc_object_t * );
static void Close  ( vlc_object_t * );

vlc_module_begin ()
    set_description( N_("Ogg/OGM muxer") )
    set_capability( "sout mux", 10 )
    set_category( CAT_SOUT )
    set_subcategory( SUBCAT_SOUT_MUX )
    add_shortcut( "ogg" )
    add_shortcut( "ogm" )
    set_callbacks( Open, Close )
vlc_module_end ()


/*****************************************************************************
 * Exported prototypes
 *****************************************************************************/
static int Control  ( sout_mux_t *, int, va_list );
static int AddStream( sout_mux_t *, sout_input_t * );
static int DelStream( sout_mux_t *, sout_input_t * );
static int Mux      ( sout_mux_t * );
static int MuxBlock ( sout_mux_t *, sout_input_t * );

static block_t *OggCreateHeader( sout_mux_t * );
static block_t *OggCreateFooter( sout_mux_t * );

/*****************************************************************************
 * Misc declarations
 *****************************************************************************/

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
    int     i_last_keyframe; /* dirac and theora */
    int     i_num_frames; /* Theora only */
    uint64_t u_last_granulepos; /* Used for correct EOS page */
    int64_t i_num_keyframes;
    ogg_stream_state os;

    oggds_header_t *p_oggds_header;

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

static void OggSetDate( block_t *, mtime_t , mtime_t  );
static block_t *OggStreamFlush( sout_mux_t *, ogg_stream_state *, mtime_t );

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
    p_sys->i_del_streams  = 0;
    p_sys->pp_del_streams = 0;

    p_mux->p_sys        = p_sys;
    p_mux->pf_control   = Control;
    p_mux->pf_addstream = AddStream;
    p_mux->pf_delstream = DelStream;
    p_mux->pf_mux       = Mux;

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
        block_t *p_og = NULL;
        mtime_t i_dts = p_sys->pp_del_streams[p_sys->i_del_streams - 1]->i_dts;

        /* Close the current ogg stream */
        msg_Dbg( p_mux, "writing footer" );
        block_ChainAppend( &p_og, OggCreateFooter( p_mux ) );

        /* Remove deleted logical streams */
        for(int i = 0; i < p_sys->i_del_streams; i++ )
        {
            ogg_stream_clear( &p_sys->pp_del_streams[i]->os );
            FREENULL( p_sys->pp_del_streams[i]->p_oggds_header );
            FREENULL( p_sys->pp_del_streams[i] );
        }
        FREENULL( p_sys->pp_del_streams );
        p_sys->i_streams -= p_sys->i_del_streams;

        /* Write footer */
        OggSetDate( p_og, i_dts, 0 );
        sout_AccessOutWrite( p_mux->p_access, p_og );
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
           pb_bool = (bool*)va_arg( args, bool * );
           *pb_bool = true;
           return VLC_SUCCESS;

       case MUX_GET_ADD_STREAM_WAIT:
           pb_bool = (bool*)va_arg( args, bool * );
           *pb_bool = true;
           return VLC_SUCCESS;

       case MUX_GET_MIME:
           ppsz = (char**)va_arg( args, char ** );
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

    p_stream->i_cat       = p_input->p_fmt->i_cat;
    p_stream->i_fourcc    = p_input->p_fmt->i_codec;
    p_stream->i_serial_no = p_sys->i_next_serial_no++;
    p_stream->i_packet_no = 0;
    p_stream->i_last_keyframe = 0;
    p_stream->i_num_keyframes = 0;
    p_stream->i_num_frames = 0;

    p_stream->p_oggds_header = 0;

    switch( p_input->p_fmt->i_cat )
    {
    case VIDEO_ES:
        if( !p_input->p_fmt->video.i_frame_rate ||
            !p_input->p_fmt->video.i_frame_rate_base )
        {
            msg_Warn( p_mux, "Missing frame rate, assuming 25fps" );
            p_input->p_fmt->video.i_frame_rate = 25;
            p_input->p_fmt->video.i_frame_rate_base = 1;
        }

        switch( p_stream->i_fourcc )
        {
        case VLC_CODEC_MP4V:
        case VLC_CODEC_MPGV:
        case VLC_CODEC_DIV3:
        case VLC_CODEC_MJPG:
        case VLC_CODEC_WMV1:
        case VLC_CODEC_WMV2:
        case VLC_CODEC_WMV3:
        case VLC_CODEC_SNOW:
            p_stream->p_oggds_header = calloc( 1, sizeof(oggds_header_t) );
            if( !p_stream->p_oggds_header )
            {
                free( p_stream );
                return VLC_ENOMEM;
            }
            p_stream->p_oggds_header->i_packet_type = PACKET_TYPE_HEADER;

            memcpy( p_stream->p_oggds_header->stream_type, "video", 5 );
            if( p_stream->i_fourcc == VLC_CODEC_MP4V )
            {
                memcpy( p_stream->p_oggds_header->sub_type, "XVID", 4 );
            }
            else if( p_stream->i_fourcc == VLC_CODEC_DIV3 )
            {
                memcpy( p_stream->p_oggds_header->sub_type, "DIV3", 4 );
            }
            else
            {
                memcpy( p_stream->p_oggds_header->sub_type,
                        &p_stream->i_fourcc, 4 );
            }
            SetDWLE( &p_stream->p_oggds_header->i_size,
                     sizeof( oggds_header_t ) - 1 );
            SetQWLE( &p_stream->p_oggds_header->i_time_unit,
                     INT64_C(10000000) * p_input->p_fmt->video.i_frame_rate_base /
                     (int64_t)p_input->p_fmt->video.i_frame_rate );
            SetQWLE( &p_stream->p_oggds_header->i_samples_per_unit, 1 );
            SetDWLE( &p_stream->p_oggds_header->i_default_len, 1 ); /* ??? */
            SetDWLE( &p_stream->p_oggds_header->i_buffer_size, 1024*1024 );
            SetWLE( &p_stream->p_oggds_header->i_bits_per_sample, 0 );
            SetDWLE( &p_stream->p_oggds_header->header.video.i_width,
                     p_input->p_fmt->video.i_width );
            SetDWLE( &p_stream->p_oggds_header->header.video.i_height,
                     p_input->p_fmt->video.i_height );
            msg_Dbg( p_mux, "%4.4s stream", (char *)&p_stream->i_fourcc );
            break;

        case VLC_CODEC_DIRAC:
            msg_Dbg( p_mux, "dirac stream" );
            break;

        case VLC_CODEC_THEORA:
            msg_Dbg( p_mux, "theora stream" );
            break;

        default:
            FREENULL( p_input->p_sys );
            return VLC_EGENERIC;
        }
        break;

    case AUDIO_ES:
        switch( p_stream->i_fourcc )
        {
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
            fourcc_to_wf_tag( p_stream->i_fourcc, &i_tag );
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

            SetDWLE( &p_stream->p_oggds_header->i_size,
                     sizeof( oggds_header_t ) - 1 + p_input->p_fmt->i_extra );

            if( p_input->p_fmt->i_extra )
            {
                memcpy( &p_stream->p_oggds_header[1],
                        p_input->p_fmt->p_extra, p_input->p_fmt->i_extra );
            }

            memcpy( p_stream->p_oggds_header->stream_type, "audio", 5 );

            memset( p_stream->p_oggds_header->sub_type, 0, 4 );
            sprintf( p_stream->p_oggds_header->sub_type, "%-x", i_tag );

            SetQWLE( &p_stream->p_oggds_header->i_time_unit, INT64_C(10000000) );
            SetDWLE( &p_stream->p_oggds_header->i_default_len, 1 );
            SetDWLE( &p_stream->p_oggds_header->i_buffer_size, 30*1024 );
            SetQWLE( &p_stream->p_oggds_header->i_samples_per_unit,
                     p_input->p_fmt->audio.i_rate );
            SetWLE( &p_stream->p_oggds_header->i_bits_per_sample,
                    p_input->p_fmt->audio.i_bitspersample );
            SetDWLE( &p_stream->p_oggds_header->header.audio.i_channels,
                     p_input->p_fmt->audio.i_channels );
            SetDWLE( &p_stream->p_oggds_header->header.audio.i_block_align,
                     p_input->p_fmt->audio.i_blockalign );
            SetDWLE( &p_stream->p_oggds_header->header.audio.i_avgbytespersec,
                     p_input->p_fmt->i_bitrate / 8);
            msg_Dbg( p_mux, "%4.4s stream", (char *)&p_stream->i_fourcc );
            break;
        }
        break;

    case SPU_ES:
        switch( p_stream->i_fourcc )
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
static int DelStream( sout_mux_t *p_mux, sout_input_t *p_input )
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
            sout_AccessOutWrite( p_mux->p_access, p_og );
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

    return 0;
}

/*****************************************************************************
 * Ogg bitstream manipulation routines
 *****************************************************************************/
static block_t *OggStreamGetPage( sout_mux_t *p_mux,
                                  ogg_stream_state *p_os, mtime_t i_pts,
                                  bool flush )
{
    (void)p_mux;
    block_t *p_og, *p_og_first = NULL;
    ogg_page og;
    int (*pager)( ogg_stream_state*, ogg_page* ) = flush ? ogg_stream_flush : ogg_stream_pageout;

    while( pager( p_os, &og ) )
    {
        /* Flush all data */
        p_og = block_New( p_mux, og.header_len + og.body_len );

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
                                ogg_stream_state *p_os, mtime_t i_pts )
{
    return OggStreamGetPage( p_mux, p_os, i_pts, true );
}

static block_t *OggStreamPageOut( sout_mux_t *p_mux,
                                  ogg_stream_state *p_os, mtime_t i_pts )
{
    return OggStreamGetPage( p_mux, p_os, i_pts, false );
}

static block_t *OggCreateHeader( sout_mux_t *p_mux )
{
    block_t *p_hdr = NULL;
    block_t *p_og = NULL;
    ogg_packet op;
    uint8_t *p_extra;
    int i, i_extra;

    /* Write header for each stream. All b_o_s (beginning of stream) packets
     * must appear first in the ogg stream so we take care of them first. */
    for( int pass = 0; pass < 2; pass++ )
    {
        for( i = 0; i < p_mux->i_nb_inputs; i++ )
        {
            sout_input_t *p_input = p_mux->pp_inputs[i];
            ogg_stream_t *p_stream = (ogg_stream_t*)p_input->p_sys;

            bool video = ( p_stream->i_fourcc == VLC_CODEC_THEORA || p_stream->i_fourcc == VLC_CODEC_DIRAC );
            if( ( ( pass == 0 && !video ) || ( pass == 1 && video ) ) )
                continue;

            msg_Dbg( p_mux, "creating header for %4.4s",
                     (char *)&p_stream->i_fourcc );

            ogg_stream_init( &p_stream->os, p_stream->i_serial_no );
            p_stream->b_new = false;
            p_stream->i_packet_no = 0;

            if( p_stream->i_fourcc == VLC_CODEC_VORBIS ||
                p_stream->i_fourcc == VLC_CODEC_SPEEX ||
                p_stream->i_fourcc == VLC_CODEC_THEORA )
            {
                /* First packet in order: vorbis/speex/theora info */
                p_extra = p_input->p_fmt->p_extra;
                i_extra = p_input->p_fmt->i_extra;

                op.bytes = *(p_extra++) << 8;
                op.bytes |= (*(p_extra++) & 0xFF);
                op.packet = p_extra;
                i_extra -= (op.bytes + 2);
                if( i_extra < 0 )
                {
                    msg_Err( p_mux, "header data corrupted");
                    op.bytes += i_extra;
                }

                op.b_o_s  = 1;
                op.e_o_s  = 0;
                op.granulepos = 0;
                op.packetno = p_stream->i_packet_no++;
                ogg_stream_packetin( &p_stream->os, &op );
                p_og = OggStreamFlush( p_mux, &p_stream->os, 0 );

                /* Get keyframe_granule_shift for theora granulepos calculation */
                if( p_stream->i_fourcc == VLC_CODEC_THEORA )
                {
                    p_stream->i_keyframe_granule_shift =
                        ( (op.packet[40] & 0x03) << 3 ) | ( (op.packet[41] & 0xe0) >> 5 );
                }
            }
            else if( p_stream->i_fourcc == VLC_CODEC_DIRAC )
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
            else if( p_stream->i_fourcc == VLC_CODEC_FLAC )
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
            else if( p_stream->p_oggds_header )
            {
                /* ds header */
                op.packet = (uint8_t*)p_stream->p_oggds_header;
                op.bytes  = p_stream->p_oggds_header->i_size + 1;
                op.b_o_s  = 1;
                op.e_o_s  = 0;
                op.granulepos = 0;
                op.packetno = p_stream->i_packet_no++;
                ogg_stream_packetin( &p_stream->os, &op );
                p_og = OggStreamFlush( p_mux, &p_stream->os, 0 );
            }

            block_ChainAppend( &p_hdr, p_og );
        }
    }

    /* Take care of the non b_o_s headers */
    for( i = 0; i < p_mux->i_nb_inputs; i++ )
    {
        sout_input_t *p_input = p_mux->pp_inputs[i];
        ogg_stream_t *p_stream = (ogg_stream_t*)p_input->p_sys;

        if( p_stream->i_fourcc == VLC_CODEC_VORBIS ||
            p_stream->i_fourcc == VLC_CODEC_SPEEX ||
            p_stream->i_fourcc == VLC_CODEC_THEORA )
        {
            /* Special case, headers are already there in the incoming stream.
             * We need to gather them an mark them as headers. */
            int j = 2;

            if( p_stream->i_fourcc == VLC_CODEC_SPEEX ) j = 1;

            p_extra = p_input->p_fmt->p_extra;
            i_extra = p_input->p_fmt->i_extra;

            /* Skip 1 header */
            op.bytes = *(p_extra++) << 8;
            op.bytes |= (*(p_extra++) & 0xFF);
            op.packet = p_extra;
            p_extra += op.bytes;
            i_extra -= (op.bytes + 2);

            while( j-- )
            {
                op.bytes = *(p_extra++) << 8;
                op.bytes |= (*(p_extra++) & 0xFF);
                op.packet = p_extra;
                p_extra += op.bytes;
                i_extra -= (op.bytes + 2);
                if( i_extra < 0 )
                {
                    msg_Err( p_mux, "header data corrupted");
                    op.bytes += i_extra;
                }

                op.b_o_s  = 0;
                op.e_o_s  = 0;
                op.granulepos = 0;
                op.packetno = p_stream->i_packet_no++;
                ogg_stream_packetin( &p_stream->os, &op );

                if( j == 0 )
                    p_og = OggStreamFlush( p_mux, &p_stream->os, 0 );
                else
                    p_og = OggStreamPageOut( p_mux, &p_stream->os, 0 );
                if( p_og )
                    block_ChainAppend( &p_hdr, p_og );
            }
        }
        else if( p_stream->i_fourcc != VLC_CODEC_FLAC &&
                 p_stream->i_fourcc != VLC_CODEC_DIRAC )
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
        if( ( p_stream->i_fourcc == VLC_CODEC_MP4V ||
              p_stream->i_fourcc == VLC_CODEC_FLAC ) &&
            p_input->p_fmt->i_extra )
        {
            /* Send a packet with the VOL data for mp4v
             * or STREAMINFO for flac */
            msg_Dbg( p_mux, "writing extra data" );
            op.bytes  = p_input->p_fmt->i_extra;
            op.packet = p_input->p_fmt->p_extra;
            if( p_stream->i_fourcc == VLC_CODEC_FLAC )
            {
                /* Skip the flac stream marker */
                op.bytes -= 4;
                op.packet+= 4;
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

    /* set HEADER flag */
    for( p_og = p_hdr; p_og != NULL; p_og = p_og->p_next )
    {
        p_og->i_flags |= BLOCK_FLAG_HEADER;
    }
    return p_hdr;
}

static block_t *OggCreateFooter( sout_mux_t *p_mux )
{
    sout_mux_sys_t *p_sys = p_mux->p_sys;
    block_t *p_hdr = NULL;
    block_t *p_og;
    ogg_packet    op;
    int     i;

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
        op.granulepos = p_stream->u_last_granulepos;
        op.packetno = p_stream->i_packet_no++;
        ogg_stream_packetin( &p_stream->os, &op );

        p_og = OggStreamFlush( p_mux, &p_stream->os, 0 );
        block_ChainAppend( &p_hdr, p_og );
        ogg_stream_clear( &p_stream->os );
    }

    for( i = 0; i < p_sys->i_del_streams; i++ )
    {
        op.packet = NULL;
        op.bytes  = 0;
        op.b_o_s  = 0;
        op.e_o_s  = 1;
        op.granulepos = p_sys->pp_del_streams[i]->u_last_granulepos;
        op.packetno = p_sys->pp_del_streams[i]->i_packet_no++;
        ogg_stream_packetin( &p_sys->pp_del_streams[i]->os, &op );

        p_og = OggStreamFlush( p_mux, &p_sys->pp_del_streams[i]->os, 0 );
        block_ChainAppend( &p_hdr, p_og );
        ogg_stream_clear( &p_sys->pp_del_streams[i]->os );
    }

    return p_hdr;
}

static void OggSetDate( block_t *p_og, mtime_t i_dts, mtime_t i_length )
{
    int i_count;
    block_t *p_tmp;
    mtime_t i_delta;

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

/*****************************************************************************
 * Mux: multiplex available data in input fifos into the Ogg bitstream
 *****************************************************************************/
static int Mux( sout_mux_t *p_mux )
{
    sout_mux_sys_t *p_sys = p_mux->p_sys;
    block_t        *p_og = NULL;
    mtime_t        i_dts;

    if( p_sys->i_add_streams || p_sys->i_del_streams )
    {
        /* Open new ogg stream */
        if( sout_MuxGetStream( p_mux, 1, &i_dts) < 0 )
        {
            msg_Dbg( p_mux, "waiting for data..." );
            return VLC_SUCCESS;
        }

        if( p_sys->i_streams )
        {
            /* Close current ogg stream */
            int i;

            msg_Dbg( p_mux, "writing footer" );
            block_ChainAppend( &p_og, OggCreateFooter( p_mux ) );

            /* Remove deleted logical streams */
            for( i = 0; i < p_sys->i_del_streams; i++ )
            {
                FREENULL( p_sys->pp_del_streams[i]->p_oggds_header );
                FREENULL( p_sys->pp_del_streams[i] );
            }
            FREENULL( p_sys->pp_del_streams );
            p_sys->i_streams = 0;
        }

        msg_Dbg( p_mux, "writing header" );
        p_sys->i_start_dts = i_dts;
        p_sys->i_streams = p_mux->i_nb_inputs;
        p_sys->i_del_streams = 0;
        p_sys->i_add_streams = 0;
        block_ChainAppend( &p_og, OggCreateHeader( p_mux ) );

        /* Write header and/or footer */
        OggSetDate( p_og, i_dts, 0 );
        sout_AccessOutWrite( p_mux->p_access, p_og );
        p_og = NULL;
    }

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

    if( p_stream->i_fourcc != VLC_CODEC_VORBIS &&
        p_stream->i_fourcc != VLC_CODEC_FLAC &&
        p_stream->i_fourcc != VLC_CODEC_SPEEX &&
        p_stream->i_fourcc != VLC_CODEC_THEORA &&
        p_stream->i_fourcc != VLC_CODEC_DIRAC )
    {
        p_data = block_Realloc( p_data, 1, p_data->i_buffer );
        p_data->p_buffer[0] = PACKET_IS_SYNCPOINT;      // FIXME
    }

    op.packet   = p_data->p_buffer;
    op.bytes    = p_data->i_buffer;
    op.b_o_s    = 0;
    op.e_o_s    = 0;
    op.packetno = p_stream->i_packet_no++;

    if( p_stream->i_cat == AUDIO_ES )
    {
        if( p_stream->i_fourcc == VLC_CODEC_VORBIS ||
            p_stream->i_fourcc == VLC_CODEC_FLAC ||
            p_stream->i_fourcc == VLC_CODEC_SPEEX )
        {
            /* number of sample from begining + current packet */
            op.granulepos =
                ( p_data->i_dts - p_sys->i_start_dts + p_data->i_length ) *
                (mtime_t)p_input->p_fmt->audio.i_rate / INT64_C(1000000);
        }
        else if( p_stream->p_oggds_header )
        {
            /* number of sample from begining */
            op.granulepos = ( p_data->i_dts - p_sys->i_start_dts ) *
                p_stream->p_oggds_header->i_samples_per_unit / INT64_C(1000000);
        }
    }
    else if( p_stream->i_cat == VIDEO_ES )
    {
        if( p_stream->i_fourcc == VLC_CODEC_THEORA )
        {
            p_stream->i_num_frames++;
            if( p_data->i_flags & BLOCK_FLAG_TYPE_I )
            {
                p_stream->i_num_keyframes++;
                p_stream->i_last_keyframe = p_stream->i_num_frames;
            }

            op.granulepos = (p_stream->i_last_keyframe << p_stream->i_keyframe_granule_shift )
                          | (p_stream->i_num_frames-p_stream->i_last_keyframe);
        }
        else if( p_stream->i_fourcc == VLC_CODEC_DIRAC )
        {
            mtime_t dt = (p_data->i_dts - p_sys->i_start_dts + 1)
                       * p_input->p_fmt->video.i_frame_rate *2
                       / p_input->p_fmt->video.i_frame_rate_base
                       / INT64_C(1000000);
            mtime_t delay = (p_data->i_pts - p_data->i_dts + 1)
                          * p_input->p_fmt->video.i_frame_rate *2
                          / p_input->p_fmt->video.i_frame_rate_base
                          / INT64_C(1000000);
            if( p_data->i_flags & BLOCK_FLAG_TYPE_I )
                p_stream->i_last_keyframe = dt;
            mtime_t dist = dt - p_stream->i_last_keyframe;
            op.granulepos = dt << 31 | (dist&0xff00) << 14
                          | (delay&0x1fff) << 9 | (dist&0xff);
        }
        else if( p_stream->p_oggds_header )
            op.granulepos = ( p_data->i_dts - p_sys->i_start_dts ) * INT64_C(10) /
                p_stream->p_oggds_header->i_time_unit;
    }
    else if( p_stream->i_cat == SPU_ES )
    {
        /* granulepos is in millisec */
        op.granulepos = ( p_data->i_dts - p_sys->i_start_dts ) / 1000;
    }

    p_stream->u_last_granulepos = op.granulepos;
    ogg_stream_packetin( &p_stream->os, &op );

    if( p_stream->i_cat == SPU_ES ||
        p_stream->i_fourcc == VLC_CODEC_SPEEX ||
        p_stream->i_fourcc == VLC_CODEC_DIRAC )
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

        sout_AccessOutWrite( p_mux->p_access, p_og );
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
