/*****************************************************************************
 * ogg.c : ogg stream input module for vlc
 *****************************************************************************
 * Copyright (C) 2001-2003 VideoLAN
 * $Id: ogg.c,v 1.47 2003/11/26 08:18:09 gbazin Exp $
 *
 * Authors: Gildas Bazin <gbazin@netcourrier.com>
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

#define OGG_BLOCK_SIZE 4096

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
    int              b_activated;

    /* the header of some logical streams (eg vorbis) contain essential
     * data for the decoder. We back them up here in case we need to re-feed
     * them to the decoder. */
    int              b_force_backup;
    int              i_packets_backup;
    ogg_packet       *p_packets_backup;

    /* program clock reference (in units of 90kHz) derived from the previous
     * granulepos */
    mtime_t          i_pcr;
    mtime_t          i_interpolated_pcr;
    mtime_t          i_previous_pcr;

    /* Misc */
    int b_reinit;
    int i_theora_keyframe_granule_shift;

} logical_stream_t;

struct demux_sys_t
{
    ogg_sync_state oy;        /* sync and verify incoming physical bitstream */

    int i_streams;                           /* number of logical bitstreams */
    logical_stream_t **pp_stream;  /* pointer to an array of logical streams */

    /* program clock reference (in units of 90kHz) derived from the pcr of
     * the sub-streams */
    mtime_t i_pcr;
    int     b_reinit;

    /* stream state */
    int     i_eos;
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

/* Some defines from OggDS */
#define PACKET_TYPE_HEADER   0x01
#define PACKET_TYPE_BITS     0x07
#define PACKET_LEN_BITS01    0xc0
#define PACKET_LEN_BITS2     0x02
#define PACKET_IS_SYNCPOINT  0x08

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Activate  ( vlc_object_t * );
static void Deactivate( vlc_object_t * );
static int  Demux     ( input_thread_t * );
static int  Control   ( input_thread_t *, int, va_list );

/* Bitstream manipulation */
static int  Ogg_Check        ( input_thread_t *p_input );
static int  Ogg_ReadPage     ( input_thread_t *, demux_sys_t *, ogg_page * );
static void Ogg_UpdatePCR    ( logical_stream_t *, ogg_packet * );
static void Ogg_DecodePacket ( input_thread_t *p_input,
                               logical_stream_t *p_stream, ogg_packet * );

static int Ogg_BeginningOfStream( input_thread_t *p_input, demux_sys_t *p_ogg);
static int Ogg_FindLogicalStreams( input_thread_t *p_input,demux_sys_t *p_ogg);
static void Ogg_EndOfStream( input_thread_t *p_input, demux_sys_t *p_ogg );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( _("ogg stream demuxer" ) );
    set_capability( "demux", 50 );
    set_callbacks( Activate, Deactivate );
    add_shortcut( "ogg" );
vlc_module_end();

/****************************************************************************
 * Ogg_Check: Check we are dealing with an ogg stream.
 ****************************************************************************/
static int Ogg_Check( input_thread_t *p_input )
{
    uint8_t *p_peek;
    int i_size = input_Peek( p_input, &p_peek, 4 );

    /* Check for the Ogg capture pattern */
    if( !(i_size>3) || !(p_peek[0] == 'O') || !(p_peek[1] == 'g') ||
        !(p_peek[2] == 'g') || !(p_peek[3] == 'S') )
        return VLC_EGENERIC;

    /* FIXME: Capture pattern might not be enough so we can also check for the
     * the first complete page */

    return VLC_SUCCESS;
}

/****************************************************************************
 * Ogg_ReadPage: Read a full Ogg page from the physical bitstream.
 ****************************************************************************
 * Returns VLC_SUCCESS if a page has been read. An error might happen if we
 * are at the end of stream.
 ****************************************************************************/
static int Ogg_ReadPage( input_thread_t *p_input, demux_sys_t *p_ogg,
                         ogg_page *p_oggpage )
{
    int i_read = 0;
    data_packet_t *p_data;
    byte_t *p_buffer;

    while( ogg_sync_pageout( &p_ogg->oy, p_oggpage ) != 1 )
    {
        i_read = input_SplitBuffer( p_input, &p_data, OGG_BLOCK_SIZE );
        if( i_read <= 0 )
            return VLC_EGENERIC;

        p_buffer = ogg_sync_buffer( &p_ogg->oy, i_read );
        p_input->p_vlc->pf_memcpy( p_buffer, p_data->p_payload_start, i_read );
        ogg_sync_wrote( &p_ogg->oy, i_read );
        input_DeletePacket( p_input->p_method_data, p_data );
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
            p_stream->i_pcr = p_oggpacket->granulepos * 90000
                              / p_stream->f_rate;
        }
        else
        {
            ogg_int64_t iframe = p_oggpacket->granulepos >>
              p_stream->i_theora_keyframe_granule_shift;
            ogg_int64_t pframe = p_oggpacket->granulepos -
              ( iframe << p_stream->i_theora_keyframe_granule_shift );

            p_stream->i_pcr = ( iframe + pframe ) * 90000
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
            p_stream->i_interpolated_pcr += (90000 / p_stream->f_rate);
        else if( p_stream->fmt.i_bitrate )
            p_stream->i_interpolated_pcr += ( p_oggpacket->bytes * 90000
                                              / p_stream->fmt.i_bitrate / 8 );
    }
}

/****************************************************************************
 * Ogg_DecodePacket: Decode an Ogg packet.
 ****************************************************************************/
static void Ogg_DecodePacket( input_thread_t *p_input,
                              logical_stream_t *p_stream,
                              ogg_packet *p_oggpacket )
{
    block_t *p_block;
    vlc_bool_t b_selected;
    int i_header_len = 0;
    mtime_t i_pts;

    /* Sanity check */
    if( !p_oggpacket->bytes )
    {
        msg_Dbg( p_input, "discarding 0 sized packet" );
        return;
    }

    if( p_stream->b_force_backup )
    {
        ogg_packet *p_packet_backup;
        p_stream->i_packets_backup++;
        switch( p_stream->fmt.i_codec )
        {
        case VLC_FOURCC( 'v','o','r','b' ):
        case VLC_FOURCC( 's','p','x',' ' ):
        case VLC_FOURCC( 't','h','e','o' ):
          if( p_stream->i_packets_backup == 3 ) p_stream->b_force_backup = 0;
          break;

        case VLC_FOURCC( 'f','l','a','c' ):
          if( p_stream->i_packets_backup == 1 ) return;
          else if( p_stream->i_packets_backup == 2 )
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
                      p_stream->f_rate = p_stream->fmt.audio.i_rate =
                          bs_read( &s, 20 );
                      p_stream->fmt.audio.i_channels =
                          bs_read( &s, 3 ) + 1;

                      msg_Dbg( p_input, "Flac header, channels: %i, rate: %i",
                               p_stream->fmt.audio.i_channels,
                               (int)p_stream->f_rate );
                  }
                  else
                  {
                      msg_Dbg( p_input, "FLAC STREAMINFO metadata too short" );
                  }

                  /* Store STREAMINFO for the decoder and packetizer */
                  p_stream->fmt.i_extra = p_oggpacket->bytes + 4;
                  p_stream->fmt.p_extra = malloc( p_stream->fmt.i_extra );
                  memcpy( p_stream->fmt.p_extra, "fLaC", 4);
                  memcpy( ((uint8_t *)p_stream->fmt.p_extra) + 4,
                          p_oggpacket->packet, p_oggpacket->bytes );

                  /* Fake this as the last metadata block */
                  ((uint8_t*)p_stream->fmt.p_extra)[4] |= 0x80;

                  p_stream->p_es = es_out_Add( p_input->p_es_out,
                                               &p_stream->fmt );
              }
              else
              {
                  /* This ain't a STREAMINFO metadata */
                  msg_Dbg( p_input, "Invalid FLAC STREAMINFO metadata" );
              }
              p_stream->b_force_backup = 0;
              p_stream->i_packets_backup = 0;
              return;
          }
          break;

        default:
          p_stream->b_force_backup = 0;
          break;
        }

        /* Backup the ogg packet (likely an header packet) */
        p_stream->p_packets_backup =
            realloc( p_stream->p_packets_backup, p_stream->i_packets_backup *
                     sizeof(ogg_packet) );

        p_packet_backup =
            &p_stream->p_packets_backup[p_stream->i_packets_backup - 1];

        p_packet_backup->bytes = p_oggpacket->bytes;
        p_packet_backup->granulepos = p_oggpacket->granulepos;

        if( p_oggpacket->granulepos >= 0 )
        {
            /* Because of vorbis granulepos scheme we must set the pcr for the
             * 1st header packet so it doesn't get discarded in the
             * packetizer */
            Ogg_UpdatePCR( p_stream, p_oggpacket );
        }

        p_packet_backup->packet = malloc( p_oggpacket->bytes );
        if( !p_packet_backup->packet ) return;
        memcpy( p_packet_backup->packet, p_oggpacket->packet,
                p_oggpacket->bytes );
    }

    /* Check the ES is selected */
    es_out_Control( p_input->p_es_out, ES_OUT_GET_SELECT,
                    p_stream->p_es, &b_selected );

    if( b_selected && !p_stream->b_activated )
    {
        p_stream->b_activated = VLC_TRUE;

        /* Newly activated stream, feed the backup headers to the decoder */
        if( !p_stream->b_force_backup )
        {
            int i;
            for( i = 0; i < p_stream->i_packets_backup; i++ )
            {
                /* Set correct starting date in header packets */
                p_stream->p_packets_backup[i].granulepos =
                    p_stream->i_interpolated_pcr * p_stream->f_rate / 90000;

                Ogg_DecodePacket( p_input, p_stream,
                                  &p_stream->p_packets_backup[i] );
            }
        }
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
                p_stream->i_pcr  > 3 * DEFAULT_PTS_DELAY * 9/100 )
                p_input->stream.p_selected_program->i_synchro_state =
                    SYNCHRO_REINIT;

            p_stream->i_previous_pcr = p_stream->i_pcr;

            /* Call the pace control */
            if( p_input->stream.p_selected_program->i_synchro_state ==
                SYNCHRO_REINIT )
            input_ClockManageRef( p_input,
                                  p_input->stream.p_selected_program,
                                  p_stream->i_pcr );
        }

        /* The granulepos is the end date of the sample */
        i_pts = ( p_stream->i_pcr < 0 ) ? 0 :
            input_ClockGetTS( p_input, p_input->stream.p_selected_program,
                              p_stream->i_pcr );

        /* Convert the granulepos into the next pcr */
        Ogg_UpdatePCR( p_stream, p_oggpacket );
    }
    else
    {
        /* Convert the granulepos into the current pcr */
        Ogg_UpdatePCR( p_stream, p_oggpacket );

        if( p_stream->i_pcr >= 0 )
        {
            /* This is for streams where the granulepos of the header packets
             * doesn't match these of the data packets (eg. ogg web radios). */
            if( p_stream->i_previous_pcr == 0 &&
                p_stream->i_pcr  > 3 * DEFAULT_PTS_DELAY * 9/100 )
                p_input->stream.p_selected_program->i_synchro_state =
                    SYNCHRO_REINIT;

            p_stream->i_previous_pcr = p_stream->i_pcr;

            /* Call the pace control */
            if( p_input->stream.p_selected_program->i_synchro_state ==
                SYNCHRO_REINIT )
            input_ClockManageRef( p_input,
                                  p_input->stream.p_selected_program,
                                  p_stream->i_pcr );
        }

        /* The granulepos is the start date of the sample */
        i_pts = ( p_stream->i_pcr < 0 ) ? 0 :
            input_ClockGetTS( p_input, p_input->stream.p_selected_program,
                              p_stream->i_pcr );
    }

    if( !b_selected )
    {
        /* This stream isn't currently selected so we don't need to decode it,
         * but we did need to store its pcr as it might be selected later on */
        p_stream->b_activated = VLC_FALSE;
        return;
    }

    if( !( p_block = block_New( p_input, p_oggpacket->bytes ) ) )
    {
        return;
    }

    if( p_stream->fmt.i_cat == AUDIO_ES )
        p_block->i_dts = p_block->i_pts = i_pts;
    else if( p_stream->fmt.i_cat == SPU_ES )
    {
        p_block->i_pts = i_pts;
        p_block->i_dts = 0;
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
        p_stream->fmt.i_codec != VLC_FOURCC( 't','h','e','o' ) )
    {
        /* Remove the header from the packet */
        i_header_len = (*p_oggpacket->packet & PACKET_LEN_BITS01) >> 6;
        i_header_len |= (*p_oggpacket->packet & PACKET_LEN_BITS2) << 1;
        i_header_len++;

        p_block->i_buffer -= i_header_len;
    }

    if( p_stream->fmt.i_codec == VLC_FOURCC( 't','a','r','k' ) )
    {
        /* FIXME: the biggest hack I've ever done */
        msg_Warn( p_input, "tark pts: "I64Fd", granule: "I64Fd,
                  p_block->i_pts, p_block->i_dts );
        msleep(10000);
    }

    memcpy( p_block->p_buffer, p_oggpacket->packet + i_header_len,
            p_oggpacket->bytes - i_header_len );

    es_out_Send( p_input->p_es_out, p_stream->p_es, p_block );
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
static int Ogg_FindLogicalStreams( input_thread_t *p_input, demux_sys_t *p_ogg)
{
    ogg_packet oggpacket;
    ogg_page oggpage;
    int i_stream;

#define p_stream p_ogg->pp_stream[p_ogg->i_streams - 1]

    while( Ogg_ReadPage( p_input, p_ogg, &oggpage ) == VLC_SUCCESS )
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

                es_format_Init( &p_stream->fmt, 0, 0 );
                p_stream->b_activated = VLC_TRUE;

                /* Setup the logical stream */
                p_stream->i_serial_no = ogg_page_serialno( &oggpage );
                ogg_stream_init( &p_stream->os, p_stream->i_serial_no );

                /* Extract the initial header from the first page and verify
                 * the codec type of tis Ogg bitstream */
                if( ogg_stream_pagein( &p_stream->os, &oggpage ) < 0 )
                {
                    /* error. stream version mismatch perhaps */
                    msg_Err( p_input, "Error reading first page of "
                             "Ogg bitstream data" );
                    return VLC_EGENERIC;
                }

                /* FIXME: check return value */
                ogg_stream_packetpeek( &p_stream->os, &oggpacket );

                /* Check for Vorbis header */
                if( oggpacket.bytes >= 7 &&
                    ! strncmp( &oggpacket.packet[1], "vorbis", 6 ) )
                {
                    oggpack_buffer opb;

                    msg_Dbg( p_input, "found vorbis header" );
                    p_stream->fmt.i_cat = AUDIO_ES;
                    p_stream->fmt.i_codec = VLC_FOURCC( 'v','o','r','b' );

                    /* Signal that we want to keep a backup of the vorbis
                     * stream headers. They will be used when switching between
                     * audio streams. */
                    p_stream->b_force_backup = 1;

                    /* Cheat and get additionnal info ;) */
                    oggpack_readinit( &opb, oggpacket.packet, oggpacket.bytes);
                    oggpack_adv( &opb, 88 );
                    p_stream->fmt.audio.i_channels = oggpack_read( &opb, 8 );
                    p_stream->f_rate = p_stream->fmt.audio.i_rate =
                        oggpack_read( &opb, 32 );
                    oggpack_adv( &opb, 32 );
                    p_stream->fmt.i_bitrate = oggpack_read( &opb, 32 );
                }
                /* Check for Speex header */
                else if( oggpacket.bytes >= 7 &&
                    ! strncmp( &oggpacket.packet[0], "Speex", 5 ) )
                {
                    oggpack_buffer opb;

                    p_stream->fmt.i_cat = AUDIO_ES;
                    p_stream->fmt.i_codec = VLC_FOURCC( 's','p','x',' ' );

                    /* Signal that we want to keep a backup of the vorbis
                     * stream headers. They will be used when switching between
                     * audio streams. */
                    p_stream->b_force_backup = 1;

                    /* Cheat and get additionnal info ;) */
                    oggpack_readinit( &opb, oggpacket.packet, oggpacket.bytes);
                    oggpack_adv( &opb, 224 );
                    oggpack_adv( &opb, 32 ); /* speex_version_id */
                    oggpack_adv( &opb, 32 ); /* header_size */
                    p_stream->f_rate = p_stream->fmt.audio.i_rate =
                        oggpack_read( &opb, 32 );
                    oggpack_adv( &opb, 32 ); /* mode */
                    oggpack_adv( &opb, 32 ); /* mode_bitstream_version */
                    p_stream->fmt.audio.i_channels = oggpack_read( &opb, 32 );
                    p_stream->fmt.i_bitrate = oggpack_read( &opb, 32 );

                    msg_Dbg( p_input, "found speex header, channels: %i, "
                             "rate: %i,  bitrate: %i",
                             p_stream->fmt.audio.i_channels,
                             (int)p_stream->f_rate, p_stream->fmt.i_bitrate );
                }
                /* Check for Flac header */
                else if( oggpacket.bytes >= 4 &&
                    ! strncmp( &oggpacket.packet[0], "fLaC", 4 ) )
                {
                    msg_Dbg( p_input, "found Flac header" );

                    /* Grrrr!!!! Did they really have to put all the
                     * important info in the second header packet!!!
                     * (STREAMINFO metadata is in the following packet) */
                    p_stream->b_force_backup = 1;

                    p_stream->fmt.i_cat = AUDIO_ES;
                    p_stream->fmt.i_codec = VLC_FOURCC( 'f','l','a','c' );
                }
                /* Check for Theora header */
                else if( oggpacket.bytes >= 7 &&
                         ! strncmp( &oggpacket.packet[1], "theora", 6 ) )
                {
                    bs_t bitstream;
                    int i_fps_numerator;
                    int i_fps_denominator;
                    int i_keyframe_frequency_force;

                    msg_Dbg( p_input, "found theora header" );
                    p_stream->fmt.i_cat = VIDEO_ES;
                    p_stream->fmt.i_codec = VLC_FOURCC( 't','h','e','o' );

                    /* Signal that we want to keep a backup of the vorbis
                     * stream headers. They will be used when switching between
                     * audio streams. */
                    p_stream->b_force_backup = 1;

                    /* Cheat and get additionnal info ;) */
                    bs_init( &bitstream, oggpacket.packet, oggpacket.bytes );
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
                    i_keyframe_frequency_force = 1 << bs_read( &bitstream, 5 );
                    bs_read( &bitstream, 8 ); /* colorspace */
                    p_stream->fmt.i_bitrate = bs_read( &bitstream, 24 );
                    bs_read( &bitstream, 6 ); /* quality */

                    /* granule_shift = i_log( frequency_force -1 ) */
                    p_stream->i_theora_keyframe_granule_shift = 0;
                    i_keyframe_frequency_force--;
                    while( i_keyframe_frequency_force )
                    {
                        p_stream->i_theora_keyframe_granule_shift++;
                        i_keyframe_frequency_force >>= 1;
                    }

                    p_stream->f_rate = ((float)i_fps_numerator) /
                                                i_fps_denominator;
                    msg_Dbg( p_input,
                             "found theora header, bitrate: %i, rate: %f",
                             p_stream->fmt.i_bitrate, p_stream->f_rate );
                }
                /* Check for Tarkin header */
                else if( oggpacket.bytes >= 7 &&
                         ! strncmp( &oggpacket.packet[1], "tarkin", 6 ) )
                {
                    oggpack_buffer opb;

                    msg_Dbg( p_input, "found tarkin header" );
                    p_stream->fmt.i_cat = VIDEO_ES;
                    p_stream->fmt.i_codec = VLC_FOURCC( 't','a','r','k' );

                    /* Cheat and get additionnal info ;) */
                    oggpack_readinit( &opb, oggpacket.packet, oggpacket.bytes);
                    oggpack_adv( &opb, 88 );
                    oggpack_adv( &opb, 104 );
                    p_stream->fmt.i_bitrate = oggpack_read( &opb, 32 );
                    p_stream->f_rate = 2; /* FIXME */
                    msg_Dbg( p_input,
                             "found tarkin header, bitrate: %i, rate: %f",
                             p_stream->fmt.i_bitrate, p_stream->f_rate );
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
                        msg_Dbg( p_input, "found video header of type: %.4s",
                                 (char *)&p_stream->fmt.i_codec );

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

                        msg_Dbg( p_input,
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

                        switch( i_format_tag )
                        {
                        case WAVE_FORMAT_PCM:
                            p_stream->fmt.i_codec =
                                VLC_FOURCC( 'a', 'r', 'a', 'w' );
                            break;
                        case WAVE_FORMAT_MPEG:
                        case WAVE_FORMAT_MPEGLAYER3:
                            p_stream->fmt.i_codec =
                                VLC_FOURCC( 'm', 'p', 'g', 'a' );
                            break;
                        case WAVE_FORMAT_A52:
                            p_stream->fmt.i_codec =
                                VLC_FOURCC( 'a', '5', '2', ' ' );
                            break;
                        case WAVE_FORMAT_WMA1:
                            p_stream->fmt.i_codec =
                                VLC_FOURCC( 'w', 'm', 'a', '1' );
                            break;
                        case WAVE_FORMAT_WMA2:
                            p_stream->fmt.i_codec =
                                VLC_FOURCC( 'w', 'm', 'a', '2' );
                            break;
                        default:
                            p_stream->fmt.i_codec = VLC_FOURCC( 'm', 's',
                                ( i_format_tag >> 8 ) & 0xff,
                                i_format_tag & 0xff );
                        }

                        msg_Dbg( p_input, "found audio header of type: %.4s",
                                 (char *)&p_stream->fmt.i_codec );
                        msg_Dbg( p_input, "audio:0x%4.4x channels:%d %dHz "
                                 "%dbits/sample %dkb/s",
                                 i_format_tag,
                                 p_stream->fmt.audio.i_channels,
                                 p_stream->fmt.audio.i_rate,
                                 p_stream->fmt.audio.i_bitspersample,
                                 p_stream->fmt.i_bitrate / 1024 );

                    }
                    else
                    {
                        msg_Dbg( p_input, "stream %d has an old header "
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
                        msg_Dbg( p_input, "found video header of type: %.4s",
                                 (char *)&p_stream->fmt.i_codec );

                        p_stream->f_rate = 10000000.0 /
                            GetQWLE(&st->time_unit);
                        p_stream->fmt.video.i_bits_per_pixel =
                            GetWLE(&st->bits_per_sample);
                        p_stream->fmt.video.i_width =
                            GetDWLE(&st->sh.video.width);
                        p_stream->fmt.video.i_height =
                            GetDWLE(&st->sh.video.height);

                        msg_Dbg( p_input,
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

                        switch( i_format_tag )
                        {
                        case WAVE_FORMAT_PCM:
                            p_stream->fmt.i_codec =
                                VLC_FOURCC( 'a', 'r', 'a', 'w' );
                            break;
                        case WAVE_FORMAT_MPEG:
                        case WAVE_FORMAT_MPEGLAYER3:
                            p_stream->fmt.i_codec =
                                VLC_FOURCC( 'm', 'p', 'g', 'a' );
                            break;
                        case WAVE_FORMAT_A52:
                            p_stream->fmt.i_codec =
                                VLC_FOURCC( 'a', '5', '2', ' ' );
                            break;
                        case WAVE_FORMAT_WMA1:
                            p_stream->fmt.i_codec =
                                VLC_FOURCC( 'w', 'm', 'a', '1' );
                            break;
                        case WAVE_FORMAT_WMA2:
                            p_stream->fmt.i_codec =
                                VLC_FOURCC( 'w', 'm', 'a', '2' );
                            break;
                        default:
                            p_stream->fmt.i_codec = VLC_FOURCC( 'm', 's',
                                ( i_format_tag >> 8 ) & 0xff,
                                i_format_tag & 0xff );
                        }

                        msg_Dbg( p_input, "found audio header of type: %.4s",
                                 (char *)&p_stream->fmt.i_codec );
                        msg_Dbg( p_input, "audio:0x%4.4x channels:%d %dHz "
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

                        msg_Dbg( p_input, "found text subtitles header" );
                        p_stream->fmt.i_cat = SPU_ES;
                        p_stream->fmt.i_codec = VLC_FOURCC('s','u','b','t');
                        p_stream->f_rate = 1000; /* granulepos is in milisec */
                    }
                    else
                    {
                        msg_Dbg( p_input, "stream %d has a header marker "
                            "but is of an unknown type", p_ogg->i_streams-1 );
                        free( p_stream );
                        p_ogg->i_streams--;
                    }
                }
                else
                {
                    msg_Dbg( p_input, "stream %d is of unknown type",
                             p_ogg->i_streams-1 );
                    free( p_stream );
                    p_ogg->i_streams--;
                }

                if( Ogg_ReadPage( p_input, p_ogg, &oggpage ) != VLC_SUCCESS )
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

/*****************************************************************************
 * Activate: initializes ogg demux structures
 *****************************************************************************/
static int Activate( vlc_object_t * p_this )
{
    input_thread_t *p_input = (input_thread_t *)p_this;
    demux_sys_t    *p_ogg;
    int            b_forced;

    p_input->p_demux_data = NULL;
    b_forced = ( ( *p_input->psz_demux )&&
                 ( !strncmp( p_input->psz_demux, "ogg", 10 ) ) ) ? 1 : 0;

    /* Check if we are dealing with an ogg stream */
    if( !b_forced && ( Ogg_Check( p_input ) != VLC_SUCCESS ) )
        return -1;

    /* Allocate p_ogg */
    if( !( p_ogg = malloc( sizeof( demux_sys_t ) ) ) )
    {
        msg_Err( p_input, "out of memory" );
        goto error;
    }
    memset( p_ogg, 0, sizeof( demux_sys_t ) );
    p_input->p_demux_data = p_ogg;
    p_ogg->pp_stream = NULL;

    /* Initialize the Ogg physical bitstream parser */
    ogg_sync_init( &p_ogg->oy );

    /* Set exported functions */
    p_input->pf_demux = Demux;
    p_input->pf_demux_control = Control;

    /* Initialize access plug-in structures. */
    if( p_input->i_mtu == 0 )
    {
        /* Improve speed. */
        p_input->i_bufsize = INPUT_DEFAULT_BUFSIZE;
    }

    /* Create one program */
    vlc_mutex_lock( &p_input->stream.stream_lock );
    if( input_InitStream( p_input, 0 ) == -1)
    {
        vlc_mutex_unlock( &p_input->stream.stream_lock );
        msg_Err( p_input, "cannot init stream" );
        goto error;
    }
    if( input_AddProgram( p_input, 0, 0) == NULL )
    {
        vlc_mutex_unlock( &p_input->stream.stream_lock );
        msg_Err( p_input, "cannot add program" );
        goto error;
    }
    p_input->stream.p_selected_program = p_input->stream.pp_programs[0];
    vlc_mutex_unlock( &p_input->stream.stream_lock );

    /* Begnning of stream, tell the demux to look for elementary streams. */
    p_ogg->i_eos = 0;

    return 0;

 error:
    Deactivate( (vlc_object_t *)p_input );
    return -1;

}

/****************************************************************************
 * Ogg_BeginningOfStream: Look for Beginning of Stream ogg pages and add
 *                        Elementary streams.
 ****************************************************************************/
static int Ogg_BeginningOfStream( input_thread_t *p_input, demux_sys_t *p_ogg)
{
    int i_stream;

    /* Find the logical streams embedded in the physical stream and
     * initialize our p_ogg structure. */
    if( Ogg_FindLogicalStreams( p_input, p_ogg ) != VLC_SUCCESS )
    {
        msg_Warn( p_input, "couldn't find any ogg logical stream" );
        return VLC_EGENERIC;
    }

    vlc_mutex_lock( &p_input->stream.stream_lock );
    p_input->stream.i_mux_rate = 0;
    vlc_mutex_unlock( &p_input->stream.stream_lock );

    for( i_stream = 0 ; i_stream < p_ogg->i_streams; i_stream++ )
    {
#define p_stream p_ogg->pp_stream[i_stream]
        if( p_stream->fmt.i_codec != VLC_FOURCC('f','l','a','c') )
            p_stream->p_es = es_out_Add( p_input->p_es_out, &p_stream->fmt );

        vlc_mutex_lock( &p_input->stream.stream_lock );
        p_input->stream.i_mux_rate += (p_stream->fmt.i_bitrate / ( 8 * 50 ));
        vlc_mutex_unlock( &p_input->stream.stream_lock );

        p_stream->i_pcr  = p_stream->i_previous_pcr = -1;
        p_stream->b_reinit = 0;
#undef p_stream
    }

    return VLC_SUCCESS;
}

/****************************************************************************
 * Ogg_EndOfStream: clean up the ES when an End of Stream is detected.
 ****************************************************************************/
static void Ogg_EndOfStream( input_thread_t *p_input, demux_sys_t *p_ogg )
{
    int i_stream, j;

#define p_stream p_ogg->pp_stream[i_stream]
    for( i_stream = 0 ; i_stream < p_ogg->i_streams; i_stream++ )
    {
        es_out_Del( p_input->p_es_out, p_stream->p_es );

        vlc_mutex_lock( &p_input->stream.stream_lock );
        p_input->stream.i_mux_rate -= (p_stream->fmt.i_bitrate / ( 8 * 50 ));
        vlc_mutex_unlock( &p_input->stream.stream_lock );

        ogg_stream_clear( &p_ogg->pp_stream[i_stream]->os );
        for( j = 0; j < p_ogg->pp_stream[i_stream]->i_packets_backup; j++ )
        {
            free( p_ogg->pp_stream[i_stream]->p_packets_backup[j].packet );
        }
        if( p_ogg->pp_stream[i_stream]->p_packets_backup)
            free( p_ogg->pp_stream[i_stream]->p_packets_backup );

        free( p_ogg->pp_stream[i_stream] );
    }
#undef p_stream

    /* Reinit p_ogg */
    if( p_ogg->pp_stream ) free( p_ogg->pp_stream );
    p_ogg->pp_stream = NULL;
    p_ogg->i_streams = 0;
}

/*****************************************************************************
 * Deactivate: frees unused data
 *****************************************************************************/
static void Deactivate( vlc_object_t *p_this )
{
    input_thread_t *p_input = (input_thread_t *)p_this;
    demux_sys_t *p_ogg = (demux_sys_t *)p_input->p_demux_data  ;

    if( p_ogg )
    {
        /* Cleanup the bitstream parser */
        ogg_sync_clear( &p_ogg->oy );

        Ogg_EndOfStream( p_input, p_ogg );

        free( p_ogg );
    }
}

/*****************************************************************************
 * Demux: reads and demuxes data packets
 *****************************************************************************
 * Returns -1 in case of error, 0 in case of EOF, 1 otherwise
 *****************************************************************************/
static int Demux( input_thread_t * p_input )
{
    demux_sys_t *p_ogg = (demux_sys_t *)p_input->p_demux_data;
    ogg_page    oggpage;
    ogg_packet  oggpacket;
    int         i_stream;

#define p_stream p_ogg->pp_stream[i_stream]

    if( p_ogg->i_eos == p_ogg->i_streams )
    {
        if( p_ogg->i_eos )
        {
            msg_Dbg( p_input, "end of a group of logical streams" );
            Ogg_EndOfStream( p_input, p_ogg );
        }

        if( Ogg_BeginningOfStream( p_input, p_ogg ) != VLC_SUCCESS ) return 0;
        p_ogg->i_eos = 0;

        msg_Dbg( p_input, "beginning of a group of logical streams" );

        p_input->stream.p_selected_program->i_synchro_state = SYNCHRO_REINIT;
        input_ClockManageRef( p_input, p_input->stream.p_selected_program, 0 );
    }

    if( p_input->stream.p_selected_program->i_synchro_state == SYNCHRO_REINIT )
    {
        msg_Warn( p_input, "synchro reinit" );

        for( i_stream = 0; i_stream < p_ogg->i_streams; i_stream++ )
        {
            /* we'll trash all the data until we find the next pcr */
            p_stream->b_reinit = 1;
            p_stream->i_pcr = -1;
            p_stream->i_interpolated_pcr = -1;
            ogg_stream_reset( &p_stream->os );
        }
        ogg_sync_reset( &p_ogg->oy );
    }

    /*
     * Demux an ogg page from the stream
     */
    if( Ogg_ReadPage( p_input, p_ogg, &oggpage ) != VLC_SUCCESS )
    {
        return 0; /* EOF */
    }

    /* Test for End of Stream */
    if( ogg_page_eos( &oggpage ) ) p_ogg->i_eos++;


    for( i_stream = 0; i_stream < p_ogg->i_streams; i_stream++ )
    {
        if( ogg_stream_pagein( &p_stream->os, &oggpage ) != 0 )
            continue;

        while( ogg_stream_packetout( &p_stream->os, &oggpacket ) > 0 )
        {
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
                        Ogg_DecodePacket( p_input, p_stream, &oggpacket );
                    }
                    else
                    {
                        input_ClockManageRef( p_input,
				      p_input->stream.p_selected_program,
				      p_stream->i_pcr );
                    }
                    continue;
                }
            }

            Ogg_DecodePacket( p_input, p_stream, &oggpacket );
        }
        break;
    }

    i_stream = 0; p_ogg->i_pcr = -1;
    for( ; i_stream < p_ogg->i_streams; i_stream++ )
    {
        if( p_stream->fmt.i_cat == SPU_ES )
            continue;
        if( p_stream->i_interpolated_pcr < 0 )
            continue;

        if( p_ogg->i_pcr < 0 || p_stream->i_interpolated_pcr < p_ogg->i_pcr )
            p_ogg->i_pcr = p_stream->i_interpolated_pcr;
    }

    if( p_ogg->i_pcr >= 0 )
    {
        input_ClockManageRef( p_input, p_input->stream.p_selected_program,
                              p_ogg->i_pcr );
    }

#undef p_stream

    return 1;
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( input_thread_t *p_input, int i_query, va_list args )
{
    demux_sys_t *p_ogg  = (demux_sys_t *)p_input->p_demux_data;
    int64_t *pi64;

    switch( i_query )
    {
        case DEMUX_GET_TIME:
            pi64 = (int64_t*)va_arg( args, int64_t * );
            *pi64 = p_ogg->i_pcr * 100 / 9;
            return VLC_SUCCESS;

        default:
            return demux_vaControlDefault( p_input, i_query, args );
    }
}
