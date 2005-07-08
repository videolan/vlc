/*****************************************************************************
 * ogg.c : ogg stream demux module for vlc
 *****************************************************************************
 * Copyright (C) 2001-2003 VideoLAN (Centrale Réseaux) and its contributors
 * $Id$
 *
 * Authors: Gildas Bazin <gbazin@netcourrier.com>
 *          Andre Pang <Andre.Pang@csiro.au> (Annodex support)
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
#include <vlc/vlc.h>
#include <vlc/input.h>

#include <ogg/ogg.h>

#include "codecs.h"
#include "vlc_bits.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

vlc_module_begin();
    set_description( _("Ogg stream demuxer" ) );
    set_category( CAT_INPUT );
    set_subcategory( SUBCAT_INPUT_DEMUX );
    set_capability( "demux2", 50 );
    set_callbacks( Open, Close );
    add_shortcut( "ogg" );
vlc_module_end();


/*****************************************************************************
 * Definitions of structures and functions used by this plugins
 *****************************************************************************/
typedef struct logical_stream_s
{
    ogg_stream_state os;                        /* logical stream of packets */

    es_format_t      fmt;
    es_out_id_t      *p_es;
    double           f_rate;

    int              i_serial_no;

    /* the header of some logical streams (eg vorbis) contain essential
     * data for the decoder. We back them up here in case we need to re-feed
     * them to the decoder. */
    int              b_force_backup;
    int              i_packets_backup;
    uint8_t          *p_headers;
    int              i_headers;

    /* program clock reference (in units of 90kHz) derived from the previous
     * granulepos */
    mtime_t          i_pcr;
    mtime_t          i_interpolated_pcr;
    mtime_t          i_previous_pcr;

    /* Misc */
    int b_reinit;
    int i_theora_keyframe_granule_shift;

    /* for Annodex logical bitstreams */
    int secondary_header_packets;

} logical_stream_t;

struct demux_sys_t
{
    ogg_sync_state oy;        /* sync and verify incoming physical bitstream */

    int i_streams;                           /* number of logical bitstreams */
    logical_stream_t **pp_stream;  /* pointer to an array of logical streams */

    /* program clock reference (in units of 90kHz) derived from the pcr of
     * the sub-streams */
    mtime_t i_pcr;

    /* stream state */
    int     i_eos;

    /* bitrate */
    int     i_bitrate;
};

/* OggDS headers for the new header format (used in ogm files) */
typedef struct stream_header_video
{
    ogg_int32_t width;
    ogg_int32_t height;
} stream_header_video;

typedef struct stream_header_audio
{
    ogg_int16_t channels;
    ogg_int16_t blockalign;
    ogg_int32_t avgbytespersec;
} stream_header_audio;

typedef struct stream_header
{
    char        streamtype[8];
    char        subtype[4];

    ogg_int32_t size;                               /* size of the structure */

    ogg_int64_t time_unit;                              /* in reference time */
    ogg_int64_t samples_per_unit;
    ogg_int32_t default_len;                                /* in media time */

    ogg_int32_t buffersize;
    ogg_int16_t bits_per_sample;

    union
    {
        /* Video specific */
        stream_header_video video;
        /* Audio specific */
        stream_header_audio audio;
    } sh;
} stream_header;

#define OGG_BLOCK_SIZE 4096

/* Some defines from OggDS */
#define PACKET_TYPE_HEADER   0x01
#define PACKET_TYPE_BITS     0x07
#define PACKET_LEN_BITS01    0xc0
#define PACKET_LEN_BITS2     0x02
#define PACKET_IS_SYNCPOINT  0x08

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Demux  ( demux_t * );
static int  Control( demux_t *, int, va_list );

/* Bitstream manipulation */
static int  Ogg_ReadPage     ( demux_t *, ogg_page * );
static void Ogg_UpdatePCR    ( logical_stream_t *, ogg_packet * );
static void Ogg_DecodePacket ( demux_t *, logical_stream_t *, ogg_packet * );

static int Ogg_BeginningOfStream( demux_t *p_demux );
static int Ogg_FindLogicalStreams( demux_t *p_demux );
static void Ogg_EndOfStream( demux_t *p_demux );

/* Logical bitstream headers */
static void Ogg_ReadTheoraHeader( logical_stream_t *, ogg_packet * );
static void Ogg_ReadVorbisHeader( logical_stream_t *, ogg_packet * );
static void Ogg_ReadSpeexHeader( logical_stream_t *, ogg_packet * );
static void Ogg_ReadFlacHeader( demux_t *, logical_stream_t *, ogg_packet * );
static void Ogg_ReadAnnodexHeader( vlc_object_t *, logical_stream_t *, ogg_packet * );

/*****************************************************************************
 * Open: initializes ogg demux structures
 *****************************************************************************/
static int Open( vlc_object_t * p_this )
{
    demux_t *p_demux = (demux_t *)p_this;
    demux_sys_t    *p_sys;
    uint8_t        *p_peek;


    /* Check if we are dealing with an ogg stream */
    if( stream_Peek( p_demux->s, &p_peek, 4 ) < 4 ) return VLC_EGENERIC;
    if( strcmp( p_demux->psz_demux, "ogg" ) && strncmp( p_peek, "OggS", 4 ) )
    {
        return VLC_EGENERIC;
    }

    /* Set exported functions */
    p_demux->pf_demux = Demux;
    p_demux->pf_control = Control;
    p_demux->p_sys = p_sys = malloc( sizeof( demux_sys_t ) );

    memset( p_sys, 0, sizeof( demux_sys_t ) );
    p_sys->i_bitrate = 0;
    p_sys->pp_stream = NULL;

    /* Begnning of stream, tell the demux to look for elementary streams. */
    p_sys->i_eos = 0;

    /* Initialize the Ogg physical bitstream parser */
    ogg_sync_init( &p_sys->oy );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: frees unused data
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    demux_t *p_demux = (demux_t *)p_this;
    demux_sys_t *p_sys = p_demux->p_sys  ;

    /* Cleanup the bitstream parser */
    ogg_sync_clear( &p_sys->oy );

    Ogg_EndOfStream( p_demux );

    free( p_sys );
}

/*****************************************************************************
 * Demux: reads and demuxes data packets
 *****************************************************************************
 * Returns -1 in case of error, 0 in case of EOF, 1 otherwise
 *****************************************************************************/
static int Demux( demux_t * p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    ogg_page    oggpage;
    ogg_packet  oggpacket;
    int         i_stream;


    if( p_sys->i_eos == p_sys->i_streams )
    {
        if( p_sys->i_eos )
        {
            msg_Dbg( p_demux, "end of a group of logical streams" );
            Ogg_EndOfStream( p_demux );
        }

        p_sys->i_eos = 0;
        if( Ogg_BeginningOfStream( p_demux ) != VLC_SUCCESS ) return 0;

        msg_Dbg( p_demux, "beginning of a group of logical streams" );
        es_out_Control( p_demux->out, ES_OUT_RESET_PCR );
    }

    /*
     * Demux an ogg page from the stream
     */
    if( Ogg_ReadPage( p_demux, &oggpage ) != VLC_SUCCESS )
    {
        return 0; /* EOF */
    }

    /* Test for End of Stream */
    if( ogg_page_eos( &oggpage ) ) p_sys->i_eos++;


    for( i_stream = 0; i_stream < p_sys->i_streams; i_stream++ )
    {
        logical_stream_t *p_stream = p_sys->pp_stream[i_stream];

        if( ogg_stream_pagein( &p_stream->os, &oggpage ) != 0 )
            continue;

        while( ogg_stream_packetout( &p_stream->os, &oggpacket ) > 0 )
        {
            /* Read info from any secondary header packets, if there are any */
            if( p_stream->secondary_header_packets > 0 )
            {
                if( p_stream->fmt.i_codec == VLC_FOURCC('t','h','e','o') &&
                        oggpacket.bytes >= 7 &&
                        ! strncmp( &oggpacket.packet[1], "theora", 6 ) )
                {
                    Ogg_ReadTheoraHeader( p_stream, &oggpacket );
                    p_stream->secondary_header_packets = 0;
                }
                else if( p_stream->fmt.i_codec == VLC_FOURCC('v','o','r','b') &&
                        oggpacket.bytes >= 7 &&
                        ! strncmp( &oggpacket.packet[1], "vorbis", 6 ) )
                {
                    Ogg_ReadVorbisHeader( p_stream, &oggpacket );
                    p_stream->secondary_header_packets = 0;
                }
                else if ( p_stream->fmt.i_codec == VLC_FOURCC('c','m','m','l') )
                {
                    p_stream->secondary_header_packets = 0;
                }
            }

            if( p_stream->b_reinit )
            {
                /* If synchro is re-initialized we need to drop all the packets
                 * until we find a new dated one. */
                Ogg_UpdatePCR( p_stream, &oggpacket );

                if( p_stream->i_pcr >= 0 )
                {
                    p_stream->b_reinit = 0;
                }
                else
                {
                    p_stream->i_interpolated_pcr = -1;
                    continue;
                }

                /* An Ogg/vorbis packet contains an end date granulepos */
                if( p_stream->fmt.i_codec == VLC_FOURCC( 'v','o','r','b' ) ||
                    p_stream->fmt.i_codec == VLC_FOURCC( 's','p','x',' ' ) ||
                    p_stream->fmt.i_codec == VLC_FOURCC( 'f','l','a','c' ) )
                {
                    if( ogg_stream_packetout( &p_stream->os, &oggpacket ) > 0 )
                    {
                        Ogg_DecodePacket( p_demux, p_stream, &oggpacket );
                    }
                    else
                    {
                        es_out_Control( p_demux->out, ES_OUT_SET_PCR,
                                        p_stream->i_pcr );
                    }
                    continue;
                }
            }

            Ogg_DecodePacket( p_demux, p_stream, &oggpacket );
        }
        break;
    }

    i_stream = 0; p_sys->i_pcr = -1;
    for( ; i_stream < p_sys->i_streams; i_stream++ )
    {
        logical_stream_t *p_stream = p_sys->pp_stream[i_stream];

        if( p_stream->fmt.i_cat == SPU_ES )
            continue;
        if( p_stream->i_interpolated_pcr < 0 )
            continue;

        if( p_sys->i_pcr < 0 || p_stream->i_interpolated_pcr < p_sys->i_pcr )
            p_sys->i_pcr = p_stream->i_interpolated_pcr;
    }

    if( p_sys->i_pcr >= 0 )
    {
        es_out_Control( p_demux->out, ES_OUT_SET_PCR, p_sys->i_pcr );
    }


    return 1;
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( demux_t *p_demux, int i_query, va_list args )
{
    demux_sys_t *p_sys  = p_demux->p_sys;
    int64_t *pi64;
    int i;

    switch( i_query )
    {
        case DEMUX_GET_TIME:
            pi64 = (int64_t*)va_arg( args, int64_t * );
            *pi64 = p_sys->i_pcr;
            return VLC_SUCCESS;

        case DEMUX_SET_TIME:
            return VLC_EGENERIC;

        case DEMUX_SET_POSITION:
            for( i = 0; i < p_sys->i_streams; i++ )
            {
                logical_stream_t *p_stream = p_sys->pp_stream[i];

                /* we'll trash all the data until we find the next pcr */
                p_stream->b_reinit = 1;
                p_stream->i_pcr = -1;
                p_stream->i_interpolated_pcr = -1;
                ogg_stream_reset( &p_stream->os );
            }
            ogg_sync_reset( &p_sys->oy );

        default:
            return demux2_vaControlHelper( p_demux->s, 0, -1, p_sys->i_bitrate,
                                           1, i_query, args );
    }
}

/****************************************************************************
 * Ogg_ReadPage: Read a full Ogg page from the physical bitstream.
 ****************************************************************************
 * Returns VLC_SUCCESS if a page has been read. An error might happen if we
 * are at the end of stream.
 ****************************************************************************/
static int Ogg_ReadPage( demux_t *p_demux, ogg_page *p_oggpage )
{
    demux_sys_t *p_ogg = p_demux->p_sys  ;
    int i_read = 0;
    byte_t *p_buffer;

    while( ogg_sync_pageout( &p_ogg->oy, p_oggpage ) != 1 )
    {
        p_buffer = ogg_sync_buffer( &p_ogg->oy, OGG_BLOCK_SIZE );

        i_read = stream_Read( p_demux->s, p_buffer, OGG_BLOCK_SIZE );
        if( i_read <= 0 )
            return VLC_EGENERIC;

        ogg_sync_wrote( &p_ogg->oy, i_read );
    }

    return VLC_SUCCESS;
}

/****************************************************************************
 * Ogg_UpdatePCR: update the PCR (90kHz program clock reference) for the
 *                current stream.
 ****************************************************************************/
static void Ogg_UpdatePCR( logical_stream_t *p_stream,
                           ogg_packet *p_oggpacket )
{
    /* Convert the granulepos into a pcr */
    if( p_oggpacket->granulepos >= 0 )
    {
        if( p_stream->fmt.i_codec != VLC_FOURCC( 't','h','e','o' ) )
        {
            p_stream->i_pcr = p_oggpacket->granulepos * I64C(1000000)
                              / p_stream->f_rate;
        }
        else
        {
            ogg_int64_t iframe = p_oggpacket->granulepos >>
              p_stream->i_theora_keyframe_granule_shift;
            ogg_int64_t pframe = p_oggpacket->granulepos -
              ( iframe << p_stream->i_theora_keyframe_granule_shift );

            p_stream->i_pcr = ( iframe + pframe ) * I64C(1000000)
                              / p_stream->f_rate;
        }

        p_stream->i_interpolated_pcr = p_stream->i_pcr;
    }
    else
    {
        p_stream->i_pcr = -1;

        /* no granulepos available, try to interpolate the pcr.
         * If we can't then don't touch the old value. */
        if( p_stream->fmt.i_cat == VIDEO_ES )
            /* 1 frame per packet */
            p_stream->i_interpolated_pcr += (I64C(1000000) / p_stream->f_rate);
        else if( p_stream->fmt.i_bitrate )
            p_stream->i_interpolated_pcr +=
                ( p_oggpacket->bytes * I64C(1000000) /
                  p_stream->fmt.i_bitrate / 8 );
    }
}

/****************************************************************************
 * Ogg_DecodePacket: Decode an Ogg packet.
 ****************************************************************************/
static void Ogg_DecodePacket( demux_t *p_demux,
                              logical_stream_t *p_stream,
                              ogg_packet *p_oggpacket )
{
    block_t *p_block;
    vlc_bool_t b_selected;
    int i_header_len = 0;
    mtime_t i_pts = -1, i_interpolated_pts;

    /* Sanity check */
    if( !p_oggpacket->bytes )
    {
        msg_Dbg( p_demux, "discarding 0 sized packet" );
        return;
    }

    if( p_oggpacket->bytes >= 7 &&
        ! strncmp ( &p_oggpacket->packet[0], "Annodex", 7 ) )
    {
        /* it's an Annodex packet -- skip it (do nothing) */
        return; 
    }
    else if( p_oggpacket->bytes >= 7 &&
        ! strncmp ( &p_oggpacket->packet[0], "AnxData", 7 ) )
    {
        /* it's an AnxData packet -- skip it (do nothing) */
        return; 
    }

    /* Check the ES is selected */
    es_out_Control( p_demux->out, ES_OUT_GET_ES_STATE,
                    p_stream->p_es, &b_selected );

    if( p_stream->b_force_backup )
    {
        uint8_t *p_extra;
        vlc_bool_t b_store_size = VLC_TRUE;

        p_stream->i_packets_backup++;
        switch( p_stream->fmt.i_codec )
        {
        case VLC_FOURCC( 'v','o','r','b' ):
        case VLC_FOURCC( 's','p','x',' ' ):
        case VLC_FOURCC( 't','h','e','o' ):
          if( p_stream->i_packets_backup == 3 ) p_stream->b_force_backup = 0;
          break;

        case VLC_FOURCC( 'f','l','a','c' ):
          if( !p_stream->fmt.audio.i_rate && p_stream->i_packets_backup == 2 )
          {
              Ogg_ReadFlacHeader( p_demux, p_stream, p_oggpacket );
              p_stream->b_force_backup = 0;
          }
          else if( p_stream->fmt.audio.i_rate )
          {
              p_stream->b_force_backup = 0;
              p_oggpacket->packet += 9; p_oggpacket->bytes -= 9;
          }
          b_store_size = VLC_FALSE;
          break;

        default:
          p_stream->b_force_backup = 0;
          break;
        }

        /* Backup the ogg packet (likely an header packet) */
        p_stream->p_headers =
            realloc( p_stream->p_headers, p_stream->i_headers +
                     p_oggpacket->bytes + (b_store_size ? 2 : 0) );
        p_extra = p_stream->p_headers + p_stream->i_headers;
        if( b_store_size )
        {
            *(p_extra++) = p_oggpacket->bytes >> 8;
            *(p_extra++) = p_oggpacket->bytes & 0xFF;
        }
        memcpy( p_extra, p_oggpacket->packet, p_oggpacket->bytes );
        p_stream->i_headers += p_oggpacket->bytes + (b_store_size ? 2 : 0);

        if( !p_stream->b_force_backup )
        {
            /* Last header received, commit changes */
            p_stream->fmt.i_extra = p_stream->i_headers;
            p_stream->fmt.p_extra =
                realloc( p_stream->fmt.p_extra, p_stream->i_headers );
            memcpy( p_stream->fmt.p_extra, p_stream->p_headers,
                    p_stream->i_headers );
            es_out_Control( p_demux->out, ES_OUT_SET_FMT,
                            p_stream->p_es, &p_stream->fmt );
        }

        b_selected = VLC_FALSE; /* Discard the header packet */
    }

    /* Convert the pcr into a pts */
    if( p_stream->fmt.i_codec == VLC_FOURCC( 'v','o','r','b' ) ||
        p_stream->fmt.i_codec == VLC_FOURCC( 's','p','x',' ' ) ||
        p_stream->fmt.i_codec == VLC_FOURCC( 'f','l','a','c' ) )
    {
        if( p_stream->i_pcr >= 0 )
        {
            /* This is for streams where the granulepos of the header packets
             * doesn't match these of the data packets (eg. ogg web radios). */
            if( p_stream->i_previous_pcr == 0 &&
                p_stream->i_pcr  > 3 * DEFAULT_PTS_DELAY )
            {
                es_out_Control( p_demux->out, ES_OUT_RESET_PCR );

                /* Call the pace control */
                es_out_Control( p_demux->out, ES_OUT_SET_PCR,
                                p_stream->i_pcr );
            }

            p_stream->i_previous_pcr = p_stream->i_pcr;

            /* The granulepos is the end date of the sample */
            i_pts =  p_stream->i_pcr;
        }
    }

    /* Convert the granulepos into the next pcr */
    i_interpolated_pts = p_stream->i_interpolated_pcr;
    Ogg_UpdatePCR( p_stream, p_oggpacket );

    if( p_stream->i_pcr >= 0 )
    {
        /* This is for streams where the granulepos of the header packets
         * doesn't match these of the data packets (eg. ogg web radios). */
        if( p_stream->i_previous_pcr == 0 &&
            p_stream->i_pcr  > 3 * DEFAULT_PTS_DELAY )
        {
            es_out_Control( p_demux->out, ES_OUT_RESET_PCR );

            /* Call the pace control */
            es_out_Control( p_demux->out, ES_OUT_SET_PCR, p_stream->i_pcr );
        }
    }

    if( p_stream->fmt.i_codec != VLC_FOURCC( 'v','o','r','b' ) &&
        p_stream->fmt.i_codec != VLC_FOURCC( 's','p','x',' ' ) &&
        p_stream->fmt.i_codec != VLC_FOURCC( 'f','l','a','c' ) &&
        p_stream->i_pcr >= 0 )
    {
        p_stream->i_previous_pcr = p_stream->i_pcr;

        /* The granulepos is the start date of the sample */
        i_pts = p_stream->i_pcr;
    }

    if( !b_selected )
    {
        /* This stream isn't currently selected so we don't need to decode it,
         * but we did need to store its pcr as it might be selected later on */
        return;
    }

    if( !( p_block = block_New( p_demux, p_oggpacket->bytes ) ) ) return;

    /* Normalize PTS */
    if( i_pts == 0 ) i_pts = 1;
    else if( i_pts == -1 && i_interpolated_pts == 0 ) i_pts = 1;
    else if( i_pts == -1 ) i_pts = 0;

    if( p_stream->fmt.i_cat == AUDIO_ES )
        p_block->i_dts = p_block->i_pts = i_pts;
    else if( p_stream->fmt.i_cat == SPU_ES )
    {
        p_block->i_dts = p_block->i_pts = i_pts;
        p_block->i_length = 0;
    }
    else if( p_stream->fmt.i_codec == VLC_FOURCC( 't','h','e','o' ) )
        p_block->i_dts = p_block->i_pts = i_pts;
    else
    {
        p_block->i_dts = i_pts;
        p_block->i_pts = 0;
    }

    if( p_stream->fmt.i_codec != VLC_FOURCC( 'v','o','r','b' ) &&
        p_stream->fmt.i_codec != VLC_FOURCC( 's','p','x',' ' ) &&
        p_stream->fmt.i_codec != VLC_FOURCC( 'f','l','a','c' ) &&
        p_stream->fmt.i_codec != VLC_FOURCC( 't','a','r','k' ) &&
        p_stream->fmt.i_codec != VLC_FOURCC( 't','h','e','o' ) &&
        p_stream->fmt.i_codec != VLC_FOURCC( 'c','m','m','l' ) )
    {
        /* We remove the header from the packet */
        i_header_len = (*p_oggpacket->packet & PACKET_LEN_BITS01) >> 6;
        i_header_len |= (*p_oggpacket->packet & PACKET_LEN_BITS2) << 1;

        if( p_stream->fmt.i_codec == VLC_FOURCC( 's','u','b','t' ))
        {
            /* But with subtitles we need to retrieve the duration first */
            int i, lenbytes = 0;
        
            if( i_header_len > 0 && p_oggpacket->bytes >= i_header_len + 1 )
            {
                for( i = 0, lenbytes = 0; i < i_header_len; i++ )
                {
                    lenbytes = lenbytes << 8;
                    lenbytes += *(p_oggpacket->packet + i_header_len - i);
                }
            }
            if( p_oggpacket->bytes - 1 - i_header_len > 2 ||
                ( p_oggpacket->packet[i_header_len + 1] != ' ' &&
                  p_oggpacket->packet[i_header_len + 1] != 0 && 
                  p_oggpacket->packet[i_header_len + 1] != '\n' &&
                  p_oggpacket->packet[i_header_len + 1] != '\r' ) )
            {
                p_block->i_length = (mtime_t)lenbytes * 1000;
            }
        }

        i_header_len++;
        p_block->i_buffer -= i_header_len;
    }

    if( p_stream->fmt.i_codec == VLC_FOURCC( 't','a','r','k' ) )
    {
        /* FIXME: the biggest hack I've ever done */
        msg_Warn( p_demux, "tarkin pts: "I64Fd", granule: "I64Fd,
                  p_block->i_pts, p_block->i_dts );
        msleep(10000);
    }

    memcpy( p_block->p_buffer, p_oggpacket->packet + i_header_len,
            p_oggpacket->bytes - i_header_len );

    es_out_Send( p_demux->out, p_stream->p_es, p_block );
}

/****************************************************************************
 * Ogg_FindLogicalStreams: Find the logical streams embedded in the physical
 *                         stream and fill p_ogg.
 *****************************************************************************
 * The initial page of a logical stream is marked as a 'bos' page.
 * Furthermore, the Ogg specification mandates that grouped bitstreams begin
 * together and all of the initial pages must appear before any data pages.
 *
 * On success this function returns VLC_SUCCESS.
 ****************************************************************************/
static int Ogg_FindLogicalStreams( demux_t *p_demux )
{
    demux_sys_t *p_ogg = p_demux->p_sys  ;
    ogg_packet oggpacket;
    ogg_page oggpage;
    int i_stream;

#define p_stream p_ogg->pp_stream[p_ogg->i_streams - 1]

    while( Ogg_ReadPage( p_demux, &oggpage ) == VLC_SUCCESS )
    {
        if( ogg_page_bos( &oggpage ) )
        {

            /* All is wonderful in our fine fine little world.
             * We found the beginning of our first logical stream. */
            while( ogg_page_bos( &oggpage ) )
            {
                p_ogg->i_streams++;
                p_ogg->pp_stream =
                    realloc( p_ogg->pp_stream, p_ogg->i_streams *
                             sizeof(logical_stream_t *) );

                p_stream = malloc( sizeof(logical_stream_t) );
                memset( p_stream, 0, sizeof(logical_stream_t) );
                p_stream->p_headers = 0;
                p_stream->secondary_header_packets = 0;

                es_format_Init( &p_stream->fmt, 0, 0 );

                /* Setup the logical stream */
                p_stream->i_serial_no = ogg_page_serialno( &oggpage );
                ogg_stream_init( &p_stream->os, p_stream->i_serial_no );

                /* Extract the initial header from the first page and verify
                 * the codec type of tis Ogg bitstream */
                if( ogg_stream_pagein( &p_stream->os, &oggpage ) < 0 )
                {
                    /* error. stream version mismatch perhaps */
                    msg_Err( p_demux, "error reading first page of "
                             "Ogg bitstream data" );
                    return VLC_EGENERIC;
                }

                /* FIXME: check return value */
                ogg_stream_packetpeek( &p_stream->os, &oggpacket );

                /* Check for Vorbis header */
                if( oggpacket.bytes >= 7 &&
                    ! strncmp( &oggpacket.packet[1], "vorbis", 6 ) )
                {
                    Ogg_ReadVorbisHeader( p_stream, &oggpacket );
                    msg_Dbg( p_demux, "found vorbis header" );
                }
                /* Check for Speex header */
                else if( oggpacket.bytes >= 7 &&
                    ! strncmp( &oggpacket.packet[0], "Speex", 5 ) )
                {
                    Ogg_ReadSpeexHeader( p_stream, &oggpacket );
                    msg_Dbg( p_demux, "found speex header, channels: %i, "
                             "rate: %i,  bitrate: %i",
                             p_stream->fmt.audio.i_channels,
                             (int)p_stream->f_rate, p_stream->fmt.i_bitrate );
                }
                /* Check for Flac header (< version 1.1.1) */
                else if( oggpacket.bytes >= 4 &&
                    ! strncmp( &oggpacket.packet[0], "fLaC", 4 ) )
                {
                    msg_Dbg( p_demux, "found FLAC header" );

                    /* Grrrr!!!! Did they really have to put all the
                     * important info in the second header packet!!!
                     * (STREAMINFO metadata is in the following packet) */
                    p_stream->b_force_backup = 1;

                    p_stream->fmt.i_cat = AUDIO_ES;
                    p_stream->fmt.i_codec = VLC_FOURCC( 'f','l','a','c' );
                }
                /* Check for Flac header (>= version 1.1.1) */
                else if( oggpacket.bytes >= 13 && oggpacket.packet[0] ==0x7F &&
                    ! strncmp( &oggpacket.packet[1], "FLAC", 4 ) &&
                    ! strncmp( &oggpacket.packet[9], "fLaC", 4 ) )
                {
                    int i_packets = ((int)oggpacket.packet[7]) << 8 |
                        oggpacket.packet[8];
                    msg_Dbg( p_demux, "found FLAC header version %i.%i "
                             "(%i header packets)",
                             oggpacket.packet[5], oggpacket.packet[6],
                             i_packets );

                    p_stream->b_force_backup = 1;

                    p_stream->fmt.i_cat = AUDIO_ES;
                    p_stream->fmt.i_codec = VLC_FOURCC( 'f','l','a','c' );
                    oggpacket.packet += 13; oggpacket.bytes -= 13;
                    Ogg_ReadFlacHeader( p_demux, p_stream, &oggpacket );
                }
                /* Check for Theora header */
                else if( oggpacket.bytes >= 7 &&
                         ! strncmp( &oggpacket.packet[1], "theora", 6 ) )
                {
                    Ogg_ReadTheoraHeader( p_stream, &oggpacket );

                    msg_Dbg( p_demux,
                             "found theora header, bitrate: %i, rate: %f",
                             p_stream->fmt.i_bitrate, p_stream->f_rate );
                }
                /* Check for Tarkin header */
                else if( oggpacket.bytes >= 7 &&
                         ! strncmp( &oggpacket.packet[1], "tarkin", 6 ) )
                {
                    oggpack_buffer opb;

                    msg_Dbg( p_demux, "found tarkin header" );
                    p_stream->fmt.i_cat = VIDEO_ES;
                    p_stream->fmt.i_codec = VLC_FOURCC( 't','a','r','k' );

                    /* Cheat and get additionnal info ;) */
                    oggpack_readinit( &opb, oggpacket.packet, oggpacket.bytes);
                    oggpack_adv( &opb, 88 );
                    oggpack_adv( &opb, 104 );
                    p_stream->fmt.i_bitrate = oggpack_read( &opb, 32 );
                    p_stream->f_rate = 2; /* FIXME */
                    msg_Dbg( p_demux,
                             "found tarkin header, bitrate: %i, rate: %f",
                             p_stream->fmt.i_bitrate, p_stream->f_rate );
                }
                /* Check for Annodex header */
                else if( oggpacket.bytes >= 7 &&
                         ! strncmp( &oggpacket.packet[0], "Annodex", 7 ) )
                {
                    Ogg_ReadAnnodexHeader( VLC_OBJECT(p_demux), p_stream,
                                           &oggpacket );
                    /* kill annodex track */
                    free( p_stream );
                    p_ogg->i_streams--;
                }
                /* Check for Annodex header */
                else if( oggpacket.bytes >= 7 &&
                         ! strncmp( &oggpacket.packet[0], "AnxData", 7 ) )
                {
                    Ogg_ReadAnnodexHeader( VLC_OBJECT(p_demux), p_stream,
                                           &oggpacket );
                }
                else if( oggpacket.bytes >= 142 &&
                         !strncmp( &oggpacket.packet[1],
                                   "Direct Show Samples embedded in Ogg", 35 ))
                {
                    /* Old header type */

                    /* Check for video header (old format) */
                    if( GetDWLE((oggpacket.packet+96)) == 0x05589f80 &&
                        oggpacket.bytes >= 184 )
                    {
                        p_stream->fmt.i_cat = VIDEO_ES;
                        p_stream->fmt.i_codec =
                            VLC_FOURCC( oggpacket.packet[68],
                                        oggpacket.packet[69],
                                        oggpacket.packet[70],
                                        oggpacket.packet[71] );
                        msg_Dbg( p_demux, "found video header of type: %.4s",
                                 (char *)&p_stream->fmt.i_codec );

                        p_stream->fmt.video.i_frame_rate = 10000000;
                        p_stream->fmt.video.i_frame_rate_base =
                            GetQWLE((oggpacket.packet+164));
                        p_stream->f_rate = 10000000.0 /
                            GetQWLE((oggpacket.packet+164));
                        p_stream->fmt.video.i_bits_per_pixel =
                            GetWLE((oggpacket.packet+182));
                        if( !p_stream->fmt.video.i_bits_per_pixel )
                            /* hack, FIXME */
                            p_stream->fmt.video.i_bits_per_pixel = 24;
                        p_stream->fmt.video.i_width =
                            GetDWLE((oggpacket.packet+176));
                        p_stream->fmt.video.i_height =
                            GetDWLE((oggpacket.packet+180));

                        msg_Dbg( p_demux,
                                 "fps: %f, width:%i; height:%i, bitcount:%i",
                                 p_stream->f_rate,
                                 p_stream->fmt.video.i_width,
                                 p_stream->fmt.video.i_height,
                                 p_stream->fmt.video.i_bits_per_pixel);

                    }
                    /* Check for audio header (old format) */
                    else if( GetDWLE((oggpacket.packet+96)) == 0x05589F81 )
                    {
                        unsigned int i_extra_size;
                        unsigned int i_format_tag;

                        p_stream->fmt.i_cat = AUDIO_ES;

                        i_extra_size = GetWLE((oggpacket.packet+140));
                        if( i_extra_size )
                        {
                            p_stream->fmt.i_extra = i_extra_size;
                            p_stream->fmt.p_extra = malloc( i_extra_size );
                            memcpy( p_stream->fmt.p_extra,
                                    oggpacket.packet + 142, i_extra_size );
                        }

                        i_format_tag = GetWLE((oggpacket.packet+124));
                        p_stream->fmt.audio.i_channels =
                            GetWLE((oggpacket.packet+126));
                        p_stream->f_rate = p_stream->fmt.audio.i_rate =
                            GetDWLE((oggpacket.packet+128));
                        p_stream->fmt.i_bitrate =
                            GetDWLE((oggpacket.packet+132)) * 8;
                        p_stream->fmt.audio.i_blockalign =
                            GetWLE((oggpacket.packet+136));
                        p_stream->fmt.audio.i_bitspersample =
                            GetWLE((oggpacket.packet+138));

                        wf_tag_to_fourcc( i_format_tag,
                                          &p_stream->fmt.i_codec, 0 );

                        if( p_stream->fmt.i_codec ==
                            VLC_FOURCC('u','n','d','f') )
                        {
                            p_stream->fmt.i_codec = VLC_FOURCC( 'm', 's',
                                ( i_format_tag >> 8 ) & 0xff,
                                i_format_tag & 0xff );
                        }

                        msg_Dbg( p_demux, "found audio header of type: %.4s",
                                 (char *)&p_stream->fmt.i_codec );
                        msg_Dbg( p_demux, "audio:0x%4.4x channels:%d %dHz "
                                 "%dbits/sample %dkb/s",
                                 i_format_tag,
                                 p_stream->fmt.audio.i_channels,
                                 p_stream->fmt.audio.i_rate,
                                 p_stream->fmt.audio.i_bitspersample,
                                 p_stream->fmt.i_bitrate / 1024 );

                    }
                    else
                    {
                        msg_Dbg( p_demux, "stream %d has an old header "
                            "but is of an unknown type", p_ogg->i_streams-1 );
                        free( p_stream );
                        p_ogg->i_streams--;
                    }
                }
                else if( (*oggpacket.packet & PACKET_TYPE_BITS )
                         == PACKET_TYPE_HEADER &&
                         oggpacket.bytes >= (int)sizeof(stream_header)+1 )
                {
                    stream_header *st = (stream_header *)(oggpacket.packet+1);

                    /* Check for video header (new format) */
                    if( !strncmp( st->streamtype, "video", 5 ) )
                    {
                        p_stream->fmt.i_cat = VIDEO_ES;

                        /* We need to get rid of the header packet */
                        ogg_stream_packetout( &p_stream->os, &oggpacket );

                        p_stream->fmt.i_codec =
                            VLC_FOURCC( st->subtype[0], st->subtype[1],
                                        st->subtype[2], st->subtype[3] );
                        msg_Dbg( p_demux, "found video header of type: %.4s",
                                 (char *)&p_stream->fmt.i_codec );

                        p_stream->fmt.video.i_frame_rate = 10000000;
                        p_stream->fmt.video.i_frame_rate_base =
                            GetQWLE(&st->time_unit);
                        p_stream->f_rate = 10000000.0 /
                            GetQWLE(&st->time_unit);
                        p_stream->fmt.video.i_bits_per_pixel =
                            GetWLE(&st->bits_per_sample);
                        p_stream->fmt.video.i_width =
                            GetDWLE(&st->sh.video.width);
                        p_stream->fmt.video.i_height =
                            GetDWLE(&st->sh.video.height);

                        msg_Dbg( p_demux,
                                 "fps: %f, width:%i; height:%i, bitcount:%i",
                                 p_stream->f_rate,
                                 p_stream->fmt.video.i_width,
                                 p_stream->fmt.video.i_height,
                                 p_stream->fmt.video.i_bits_per_pixel );
                    }
                    /* Check for audio header (new format) */
                    else if( !strncmp( st->streamtype, "audio", 5 ) )
                    {
                        char p_buffer[5];
                        int i_format_tag;

                        p_stream->fmt.i_cat = AUDIO_ES;

                        /* We need to get rid of the header packet */
                        ogg_stream_packetout( &p_stream->os, &oggpacket );

                        p_stream->fmt.i_extra = GetQWLE(&st->size) -
                            sizeof(stream_header);
                        if( p_stream->fmt.i_extra )
                        {
                            p_stream->fmt.p_extra =
                                malloc( p_stream->fmt.i_extra );
                            memcpy( p_stream->fmt.p_extra, st + 1,
                                    p_stream->fmt.i_extra );
                        }

                        memcpy( p_buffer, st->subtype, 4 );
                        p_buffer[4] = '\0';
                        i_format_tag = strtol(p_buffer,NULL,16);
                        p_stream->fmt.audio.i_channels =
                            GetWLE(&st->sh.audio.channels);
                        p_stream->f_rate = p_stream->fmt.audio.i_rate =
                            GetQWLE(&st->samples_per_unit);
                        p_stream->fmt.i_bitrate =
                            GetDWLE(&st->sh.audio.avgbytespersec) * 8;
                        p_stream->fmt.audio.i_blockalign =
                            GetWLE(&st->sh.audio.blockalign);
                        p_stream->fmt.audio.i_bitspersample =
                            GetWLE(&st->bits_per_sample);

                        wf_tag_to_fourcc( i_format_tag,
                                          &p_stream->fmt.i_codec, 0 );

                        if( p_stream->fmt.i_codec ==
                            VLC_FOURCC('u','n','d','f') )
                        {
                            p_stream->fmt.i_codec = VLC_FOURCC( 'm', 's',
                                ( i_format_tag >> 8 ) & 0xff,
                                i_format_tag & 0xff );
                        }

                        msg_Dbg( p_demux, "found audio header of type: %.4s",
                                 (char *)&p_stream->fmt.i_codec );
                        msg_Dbg( p_demux, "audio:0x%4.4x channels:%d %dHz "
                                 "%dbits/sample %dkb/s",
                                 i_format_tag,
                                 p_stream->fmt.audio.i_channels,
                                 p_stream->fmt.audio.i_rate,
                                 p_stream->fmt.audio.i_bitspersample,
                                 p_stream->fmt.i_bitrate / 1024 );
                    }
                    /* Check for text (subtitles) header */
                    else if( !strncmp(st->streamtype, "text", 4) )
                    {
                        /* We need to get rid of the header packet */
                        ogg_stream_packetout( &p_stream->os, &oggpacket );

                        msg_Dbg( p_demux, "found text subtitles header" );
                        p_stream->fmt.i_cat = SPU_ES;
                        p_stream->fmt.i_codec = VLC_FOURCC('s','u','b','t');
                        p_stream->f_rate = 1000; /* granulepos is in milisec */
                    }
                    else
                    {
                        msg_Dbg( p_demux, "stream %d has a header marker "
                            "but is of an unknown type", p_ogg->i_streams-1 );
                        free( p_stream );
                        p_ogg->i_streams--;
                    }
                }
                else
                {
                    msg_Dbg( p_demux, "stream %d is of unknown type",
                             p_ogg->i_streams-1 );
                    free( p_stream );
                    p_ogg->i_streams--;
                }

                if( Ogg_ReadPage( p_demux, &oggpage ) != VLC_SUCCESS )
                    return VLC_EGENERIC;
            }

            /* This is the first data page, which means we are now finished
             * with the initial pages. We just need to store it in the relevant
             * bitstream. */
            for( i_stream = 0; i_stream < p_ogg->i_streams; i_stream++ )
            {
                if( ogg_stream_pagein( &p_ogg->pp_stream[i_stream]->os,
                                       &oggpage ) == 0 )
                {
                    break;
                }
            }

            return VLC_SUCCESS;
        }
    }
#undef p_stream

    return VLC_EGENERIC;
}

/****************************************************************************
 * Ogg_BeginningOfStream: Look for Beginning of Stream ogg pages and add
 *                        Elementary streams.
 ****************************************************************************/
static int Ogg_BeginningOfStream( demux_t *p_demux )
{
    demux_sys_t *p_ogg = p_demux->p_sys  ;
    int i_stream;

    /* Find the logical streams embedded in the physical stream and
     * initialize our p_ogg structure. */
    if( Ogg_FindLogicalStreams( p_demux ) != VLC_SUCCESS )
    {
        msg_Warn( p_demux, "couldn't find any ogg logical stream" );
        return VLC_EGENERIC;
    }

    p_ogg->i_bitrate = 0;

    for( i_stream = 0 ; i_stream < p_ogg->i_streams; i_stream++ )
    {
#define p_stream p_ogg->pp_stream[i_stream]
        p_stream->p_es = es_out_Add( p_demux->out, &p_stream->fmt );

        if( p_stream->fmt.i_codec == VLC_FOURCC('c','m','m','l') )
        {
            /* Set the CMML stream active */
            es_out_Control( p_demux->out, ES_OUT_SET_ES, p_stream->p_es );
        }

        p_ogg->i_bitrate += p_stream->fmt.i_bitrate;

        p_stream->i_pcr = p_stream->i_previous_pcr =
            p_stream->i_interpolated_pcr = -1;
        p_stream->b_reinit = 0;
#undef p_stream
    }

    return VLC_SUCCESS;
}

/****************************************************************************
 * Ogg_EndOfStream: clean up the ES when an End of Stream is detected.
 ****************************************************************************/
static void Ogg_EndOfStream( demux_t *p_demux )
{
    demux_sys_t *p_ogg = p_demux->p_sys  ;
    int i_stream;

#define p_stream p_ogg->pp_stream[i_stream]
    for( i_stream = 0 ; i_stream < p_ogg->i_streams; i_stream++ )
    {
        if( p_stream->p_es )
            es_out_Del( p_demux->out, p_stream->p_es );

        p_ogg->i_bitrate -= p_stream->fmt.i_bitrate;

        ogg_stream_clear( &p_ogg->pp_stream[i_stream]->os );
        if( p_ogg->pp_stream[i_stream]->p_headers)
            free( p_ogg->pp_stream[i_stream]->p_headers );

        es_format_Clean( &p_stream->fmt );

        free( p_ogg->pp_stream[i_stream] );
    }
#undef p_stream

    /* Reinit p_ogg */
    if( p_ogg->pp_stream ) free( p_ogg->pp_stream );
    p_ogg->pp_stream = NULL;
    p_ogg->i_streams = 0;
}

static void Ogg_ReadTheoraHeader( logical_stream_t *p_stream,
                                  ogg_packet *p_oggpacket )
{
    bs_t bitstream;
    int i_fps_numerator;
    int i_fps_denominator;
    int i_keyframe_frequency_force;

    p_stream->fmt.i_cat = VIDEO_ES;
    p_stream->fmt.i_codec = VLC_FOURCC( 't','h','e','o' );

    /* Signal that we want to keep a backup of the theora
     * stream headers. They will be used when switching between
     * audio streams. */
    p_stream->b_force_backup = 1;

    /* Cheat and get additionnal info ;) */
    bs_init( &bitstream, p_oggpacket->packet, p_oggpacket->bytes );
    bs_skip( &bitstream, 56 );
    bs_read( &bitstream, 8 ); /* major version num */
    bs_read( &bitstream, 8 ); /* minor version num */
    bs_read( &bitstream, 8 ); /* subminor version num */
    bs_read( &bitstream, 16 ) /*<< 4*/; /* width */
    bs_read( &bitstream, 16 ) /*<< 4*/; /* height */
    bs_read( &bitstream, 24 ); /* frame width */
    bs_read( &bitstream, 24 ); /* frame height */
    bs_read( &bitstream, 8 ); /* x offset */
    bs_read( &bitstream, 8 ); /* y offset */

    i_fps_numerator = bs_read( &bitstream, 32 );
    i_fps_denominator = bs_read( &bitstream, 32 );
    bs_read( &bitstream, 24 ); /* aspect_numerator */
    bs_read( &bitstream, 24 ); /* aspect_denominator */

    p_stream->fmt.video.i_frame_rate = i_fps_numerator;
    p_stream->fmt.video.i_frame_rate_base = i_fps_denominator;

    bs_read( &bitstream, 8 ); /* colorspace */
    p_stream->fmt.i_bitrate = bs_read( &bitstream, 24 );
    bs_read( &bitstream, 6 ); /* quality */

    i_keyframe_frequency_force = 1 << bs_read( &bitstream, 5 );

    /* granule_shift = i_log( frequency_force -1 ) */
    p_stream->i_theora_keyframe_granule_shift = 0;
    i_keyframe_frequency_force--;
    while( i_keyframe_frequency_force )
    {
        p_stream->i_theora_keyframe_granule_shift++;
        i_keyframe_frequency_force >>= 1;
    }

    p_stream->f_rate = ((float)i_fps_numerator) / i_fps_denominator;
}

static void Ogg_ReadVorbisHeader( logical_stream_t *p_stream,
                                  ogg_packet *p_oggpacket )
{
    oggpack_buffer opb;

    p_stream->fmt.i_cat = AUDIO_ES;
    p_stream->fmt.i_codec = VLC_FOURCC( 'v','o','r','b' );

    /* Signal that we want to keep a backup of the vorbis
     * stream headers. They will be used when switching between
     * audio streams. */
    p_stream->b_force_backup = 1;

    /* Cheat and get additionnal info ;) */
    oggpack_readinit( &opb, p_oggpacket->packet, p_oggpacket->bytes);
    oggpack_adv( &opb, 88 );
    p_stream->fmt.audio.i_channels = oggpack_read( &opb, 8 );
    p_stream->f_rate = p_stream->fmt.audio.i_rate =
        oggpack_read( &opb, 32 );
    oggpack_adv( &opb, 32 );
    p_stream->fmt.i_bitrate = oggpack_read( &opb, 32 );
}

static void Ogg_ReadSpeexHeader( logical_stream_t *p_stream,
                                 ogg_packet *p_oggpacket )
{
    oggpack_buffer opb;

    p_stream->fmt.i_cat = AUDIO_ES;
    p_stream->fmt.i_codec = VLC_FOURCC( 's','p','x',' ' );

    /* Signal that we want to keep a backup of the speex
     * stream headers. They will be used when switching between
     * audio streams. */
    p_stream->b_force_backup = 1;

    /* Cheat and get additionnal info ;) */
    oggpack_readinit( &opb, p_oggpacket->packet, p_oggpacket->bytes);
    oggpack_adv( &opb, 224 );
    oggpack_adv( &opb, 32 ); /* speex_version_id */
    oggpack_adv( &opb, 32 ); /* header_size */
    p_stream->f_rate = p_stream->fmt.audio.i_rate = oggpack_read( &opb, 32 );
    oggpack_adv( &opb, 32 ); /* mode */
    oggpack_adv( &opb, 32 ); /* mode_bitstream_version */
    p_stream->fmt.audio.i_channels = oggpack_read( &opb, 32 );
    p_stream->fmt.i_bitrate = oggpack_read( &opb, 32 );
}

static void Ogg_ReadFlacHeader( demux_t *p_demux, logical_stream_t *p_stream,
                                ogg_packet *p_oggpacket )
{
    /* Parse the STREAMINFO metadata */
    bs_t s;

    bs_init( &s, p_oggpacket->packet, p_oggpacket->bytes );

    bs_read( &s, 1 );
    if( bs_read( &s, 7 ) == 0 )
    {
        if( bs_read( &s, 24 ) >= 34 /*size STREAMINFO*/ )
        {
            bs_skip( &s, 80 );
            p_stream->f_rate = p_stream->fmt.audio.i_rate = bs_read( &s, 20 );
            p_stream->fmt.audio.i_channels = bs_read( &s, 3 ) + 1;

            msg_Dbg( p_demux, "FLAC header, channels: %i, rate: %i",
                     p_stream->fmt.audio.i_channels, (int)p_stream->f_rate );
        }
        else msg_Dbg( p_demux, "FLAC STREAMINFO metadata too short" );

        /* Fake this as the last metadata block */
        *((uint8_t*)p_oggpacket->packet) |= 0x80;
    }
    else
    {
        /* This ain't a STREAMINFO metadata */
        msg_Dbg( p_demux, "Invalid FLAC STREAMINFO metadata" );
    }
}

static void Ogg_ReadAnnodexHeader( vlc_object_t *p_this,
                                   logical_stream_t *p_stream,
                                   ogg_packet *p_oggpacket )
{
    if( ! strncmp( &p_oggpacket->packet[0], "Annodex", 7 ) )
    {
        oggpack_buffer opb;

        uint16_t major_version;
        uint16_t minor_version;
        uint64_t timebase_numerator;
        uint64_t timebase_denominator;

        Ogg_ReadTheoraHeader( p_stream, p_oggpacket );

        oggpack_readinit( &opb, p_oggpacket->packet, p_oggpacket->bytes);
        oggpack_adv( &opb, 8*8 ); /* "Annodex\0" header */
        major_version = oggpack_read( &opb, 2*8 ); /* major version */
        minor_version = oggpack_read( &opb, 2*8 ); /* minor version */
        timebase_numerator = GetQWLE( &p_oggpacket->packet[16] );
        timebase_denominator = GetQWLE( &p_oggpacket->packet[24] );
    }
    else if( ! strncmp( &p_oggpacket->packet[0], "AnxData", 7 ) )
    {
        uint64_t granule_rate_numerator;
        uint64_t granule_rate_denominator;
        char content_type_string[1024];

        /* Read in Annodex header fields */

        granule_rate_numerator = GetQWLE( &p_oggpacket->packet[8] );
        granule_rate_denominator = GetQWLE( &p_oggpacket->packet[16] );
        p_stream->secondary_header_packets =
            GetDWLE( &p_oggpacket->packet[24] );

        /* we are guaranteed that the first header field will be
         * the content-type (by the Annodex standard) */
        if( !strncasecmp( &p_oggpacket->packet[28], "Content-Type: ", 14 ) )
        {
            sscanf( &p_oggpacket->packet[42], "%1024s\r\n",
                    content_type_string );
        }

        msg_Dbg( p_this, "AnxData packet info: "I64Fd" / "I64Fd", %d, ``%s''",
                 granule_rate_numerator, granule_rate_denominator,
                 p_stream->secondary_header_packets, content_type_string );

        p_stream->f_rate = (float) granule_rate_numerator /
            (float) granule_rate_denominator;

        /* What type of file do we have?
         * strcmp is safe to use here because we've extracted
         * content_type_string from the stream manually */
        if( !strncmp(content_type_string, "audio/x-wav", 11) )
        {
            /* n.b. WAVs are unsupported right now */
            p_stream->fmt.i_cat = UNKNOWN_ES;
        }
        else if( !strncmp(content_type_string, "audio/x-vorbis", 14) )
        {
            p_stream->fmt.i_cat = AUDIO_ES;
            p_stream->fmt.i_codec = VLC_FOURCC( 'v','o','r','b' );

            p_stream->b_force_backup = 1;
        }
        else if( !strncmp(content_type_string, "audio/x-speex", 14) )
        {
            p_stream->fmt.i_cat = AUDIO_ES;
            p_stream->fmt.i_codec = VLC_FOURCC( 's','p','x',' ' );

            p_stream->b_force_backup = 1;
        }
        else if( !strncmp(content_type_string, "video/x-theora", 14) )
        {
            p_stream->fmt.i_cat = VIDEO_ES;
            p_stream->fmt.i_codec = VLC_FOURCC( 't','h','e','o' );

            p_stream->b_force_backup = 1;
        }
        else if( !strncmp(content_type_string, "video/x-xvid", 14) )
        {
            p_stream->fmt.i_cat = VIDEO_ES;
            p_stream->fmt.i_codec = VLC_FOURCC( 'x','v','i','d' );

            p_stream->b_force_backup = 1;
        }
        else if( !strncmp(content_type_string, "video/mpeg", 14) )
        {
            /* n.b. MPEG streams are unsupported right now */
            p_stream->fmt.i_cat = VIDEO_ES;
            p_stream->fmt.i_codec = VLC_FOURCC( 'm','p','g','v' );
        }
        else if( !strncmp(content_type_string, "text/x-cmml", 11) )
        {
            ogg_stream_packetout( &p_stream->os, p_oggpacket );
            p_stream->fmt.i_cat = SPU_ES;
            p_stream->fmt.i_codec = VLC_FOURCC( 'c','m','m','l' );
        }
    }
}
