/*****************************************************************************
 * ogg.c : ogg stream input module for vlc
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: ogg.c,v 1.19 2003/01/29 12:59:23 gbazin Exp $
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
#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>

#include <vlc/vlc.h>
#include <vlc/input.h>

#include <sys/types.h>

#include <ogg/ogg.h>

#include <codecs.h>                        /* BITMAPINFOHEADER, WAVEFORMATEX */

#define OGG_BLOCK_SIZE 4096
#define PAGES_READ_ONCE 1

/*****************************************************************************
 * Definitions of structures and functions used by this plugins 
 *****************************************************************************/
typedef struct logical_stream_s
{
    ogg_stream_state os;                        /* logical stream of packets */

    int              i_serial_no;
    int              i_cat;                            /* AUDIO_ES, VIDEO_ES */
    int              i_activated;
    vlc_fourcc_t     i_fourcc;
    vlc_fourcc_t     i_codec;

    es_descriptor_t  *p_es;
    int              b_selected;                           /* newly selected */

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

    /* info from logical streams */
    double f_rate;
    int i_bitrate;
    int b_reinit;

    /* codec specific stuff */
    BITMAPINFOHEADER *p_bih;
    WAVEFORMATEX *p_wf;
    int i_theora_keyframe_granule_shift;

} logical_stream_t;

struct demux_sys_t
{
    ogg_sync_state oy;        /* sync and verify incoming physical bitstream */

    int i_streams;                           /* number of logical bitstreams */
    logical_stream_t **pp_stream;  /* pointer to an array of logical streams */

    /* current audio and video es */
    logical_stream_t *p_stream_video;
    logical_stream_t *p_stream_audio;
    logical_stream_t *p_stream_spu;

    /* program clock reference (in units of 90kHz) derived from the pcr of
     * the sub-streams */
    mtime_t i_pcr;

    mtime_t i_length;
    int     b_seekable;
    int     b_reinit;
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

/* Some functions to manipulate memory */
static uint16_t GetWLE( uint8_t *p_buff )
{
    return( (p_buff[0]) + ( p_buff[1] <<8 ) );
}

static uint32_t GetDWLE( uint8_t *p_buff )
{
    return( p_buff[0] + ( p_buff[1] <<8 ) +
            ( p_buff[2] <<16 ) + ( p_buff[3] <<24 ) );
}

static uint64_t GetQWLE( uint8_t *p_buff )
{
    return( GetDWLE( p_buff ) + ( ((uint64_t)GetDWLE( p_buff + 4 )) << 32 ) );
}
/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Activate  ( vlc_object_t * );
static void Deactivate( vlc_object_t * );
static int  Demux     ( input_thread_t * );

/* Stream managment */
static int  Ogg_StreamStart  ( input_thread_t *, demux_sys_t *, int );
static void Ogg_StreamStop   ( input_thread_t *, demux_sys_t *, int );

/* Bitstream manipulation */
static int  Ogg_Check        ( input_thread_t *p_input );
static int  Ogg_ReadPage     ( input_thread_t *, demux_sys_t *, ogg_page * );
static void Ogg_UpdatePCR    ( logical_stream_t *, ogg_packet * );
static void Ogg_DecodePacket ( input_thread_t *p_input,
                               logical_stream_t *p_stream, ogg_packet * );
static int  Ogg_FindLogicalStreams( input_thread_t *p_input,
                                    demux_sys_t *p_ogg );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( _("ogg stream demux" ) );
    set_capability( "demux", 50 );
    set_callbacks( Activate, Deactivate );
    add_shortcut( "ogg" );
vlc_module_end();

/*****************************************************************************
 * Stream managment
 *****************************************************************************/
static int Ogg_StreamStart( input_thread_t *p_input,
                            demux_sys_t *p_ogg, int i_stream )
{
#define p_stream p_ogg->pp_stream[i_stream]
    if( !p_stream->p_es )
    {
        msg_Warn( p_input, "stream[%d] unselectable", i_stream );
        return( 0 );
    }
    if( p_stream->i_activated )
    {
        msg_Warn( p_input, "stream[%d] already selected", i_stream );
        return( 1 );
    }

    if( !p_stream->p_es->p_decoder_fifo )
    {
        vlc_mutex_lock( &p_input->stream.stream_lock );
        input_SelectES( p_input, p_stream->p_es );
        vlc_mutex_unlock( &p_input->stream.stream_lock );
    }
    p_stream->i_activated = p_stream->p_es->p_decoder_fifo ? 1 : 0;

    /* Feed the backup header to the decoder */
    if( !p_stream->b_force_backup )
    {
        int i;
        for( i = 0; i < p_stream->i_packets_backup; i++ )
        {
            Ogg_DecodePacket( p_input, p_stream,
                              &p_stream->p_packets_backup[i] );
        }
    }

    return( p_stream->i_activated );
#undef  p_stream
}

static void Ogg_StreamStop( input_thread_t *p_input,
                            demux_sys_t *p_ogg, int i_stream )
{
#define p_stream    p_ogg->pp_stream[i_stream]

    if( !p_stream->i_activated )
    {
        msg_Warn( p_input, "stream[%d] already unselected", i_stream );
        return;
    }

    if( p_stream->p_es->p_decoder_fifo )
    {
        vlc_mutex_lock( &p_input->stream.stream_lock );
        input_UnselectES( p_input, p_stream->p_es );
        vlc_mutex_unlock( &p_input->stream.stream_lock );
    }

    p_stream->i_activated = 0;

#undef  p_stream
}

/****************************************************************************
 * Ogg_Check: Check we are dealing with an ogg stream.
 ****************************************************************************/
static int Ogg_Check( input_thread_t *p_input )
{
    u8 *p_peek;
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

    /* Convert the next granulepos into a pcr */
    if( p_oggpacket->granulepos >= 0 )
    {
        if( p_stream->i_fourcc != VLC_FOURCC( 't','h','e','o' ) )
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
        /* FIXME: ffmpeg doesn't like null pts */
        if( p_stream->i_cat == VIDEO_ES )
            /* 1 frame per packet */
            p_stream->i_pcr += (90000 / p_stream->f_rate);
        else
            p_stream->i_pcr = -1;

        /* no granulepos available, try to interpolate the pcr.
         * If we can't then don't touch the old value. */
        if( p_stream->i_bitrate )
            p_stream->i_interpolated_pcr += ( p_oggpacket->bytes * 90000
                                              / p_stream->i_bitrate / 8 );
    }
}

/****************************************************************************
 * Ogg_DecodePacket: Decode an Ogg packet.
 ****************************************************************************/
static void Ogg_DecodePacket( input_thread_t *p_input,
                              logical_stream_t *p_stream,
                              ogg_packet *p_oggpacket )
{
    pes_packet_t  *p_pes;
    data_packet_t *p_data;
    vlc_bool_t b_trash = VLC_FALSE;
    int i_header_len = 0;

    if( p_stream->b_force_backup )
    {
        /* Backup the ogg packet (likely an header packet) */
        ogg_packet *p_packet_backup;
        p_stream->i_packets_backup++;
        p_stream->p_packets_backup =
            realloc( p_stream->p_packets_backup, p_stream->i_packets_backup *
                     sizeof(ogg_packet) );

        p_packet_backup =
            &p_stream->p_packets_backup[p_stream->i_packets_backup - 1];

        p_packet_backup->bytes = p_oggpacket->bytes;
        p_packet_backup->granulepos = p_oggpacket->granulepos;
        p_packet_backup->packet = malloc( p_oggpacket->bytes );
        if( !p_packet_backup->packet ) return;
        memcpy( p_packet_backup->packet, p_oggpacket->packet,
                p_oggpacket->bytes );

        switch( p_stream->i_fourcc )
        {
        case VLC_FOURCC( 'v','o','r','b' ):
          if( p_stream->i_packets_backup == 3 ) p_stream->b_force_backup = 0;
          break;

        default:
          p_stream->b_force_backup = 0;
          break;
        }
    }

    vlc_mutex_lock( &p_input->stream.control.control_lock );
    if( p_stream->i_cat == AUDIO_ES && p_input->stream.control.b_mute )
    {
        b_trash = VLC_TRUE;
    }
    vlc_mutex_unlock( &p_input->stream.control.control_lock );

    if( !p_stream->p_es->p_decoder_fifo || b_trash )
    {
        /* This stream isn't currently selected so we don't need to decode it,
         * but we do need to store its pcr as it might be selected later on. */
        Ogg_UpdatePCR( p_stream, p_oggpacket );

        return;
    }

    if( !( p_pes = input_NewPES( p_input->p_method_data ) ) )
    {
        return;
    }
    if( !( p_data = input_NewPacket( p_input->p_method_data,
                                     p_oggpacket->bytes ) ) )
    {
        input_DeletePES( p_input->p_method_data, p_pes );
        return;
    }

    /* Convert the pcr into a pts */
    if( p_stream->i_cat != SPU_ES )
    {
        p_pes->i_pts = ( p_stream->i_pcr < 0 ) ? 0 :
            input_ClockGetTS( p_input, p_input->stream.p_selected_program,
                              p_stream->i_pcr );
    }
    else
    {
        /* Of course subtitles had to be different! */
        p_pes->i_pts = ( p_oggpacket->granulepos < 0 ) ? 0 :
            input_ClockGetTS( p_input, p_input->stream.p_selected_program,
                              p_oggpacket->granulepos * 90000 /
                              p_stream->f_rate );
    }

    /* Convert the next granulepos into a pcr */
    Ogg_UpdatePCR( p_stream, p_oggpacket );

    p_pes->i_nb_data = 1;
    p_pes->i_dts = p_oggpacket->granulepos;
    p_pes->p_first = p_pes->p_last = p_data;
    p_pes->i_pes_size = p_oggpacket->bytes;

    if( p_stream->i_fourcc != VLC_FOURCC( 'v','o','r','b' ) &&
        p_stream->i_fourcc != VLC_FOURCC( 't','a','r','k' ) &&
        p_stream->i_fourcc != VLC_FOURCC( 't','h','e','o' ) )
    {
        /* Remove the header from the packet */
        i_header_len = (*p_oggpacket->packet & PACKET_LEN_BITS01) >> 6;
        i_header_len |= (*p_oggpacket->packet & PACKET_LEN_BITS2) << 1;
        i_header_len++;

        p_pes->i_pes_size -= i_header_len;
        p_pes->i_dts = 0;
    }

    if( p_stream->i_fourcc == VLC_FOURCC( 't','a','r','k' ) )
    {
        /* FIXME: the biggest hack I've ever done */
        msg_Warn( p_input, "tark pts: "I64Fd", granule: "I64Fd,
                  p_pes->i_pts, p_pes->i_dts );
        msleep(10000);
    }

    memcpy( p_data->p_payload_start,
            p_oggpacket->packet + i_header_len,
            p_oggpacket->bytes - i_header_len );

    p_data->p_payload_end = p_data->p_payload_start + p_pes->i_pes_size;
    p_data->b_discard_payload = 0;

    input_DecodePES( p_stream->p_es->p_decoder_fifo, p_pes );
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

#define p_stream p_ogg->pp_stream[p_ogg->i_streams - 1]

                p_stream = malloc( sizeof(logical_stream_t) );
                memset( p_stream, 0, sizeof(logical_stream_t) );

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
                    p_stream->i_cat = AUDIO_ES;
                    p_stream->i_fourcc = VLC_FOURCC( 'v','o','r','b' );

                    /* Signal that we want to keep a backup of the vorbis
                     * stream headers. They will be used when switching between
                     * audio streams. */
                    p_stream->b_force_backup = 1;

                    /* Cheat and get additionnal info ;) */
                    oggpack_readinit( &opb, oggpacket.packet, oggpacket.bytes);
                    oggpack_adv( &opb, 96 );
                    p_stream->f_rate = oggpack_read( &opb, 32 );
                    oggpack_adv( &opb, 32 );
                    p_stream->i_bitrate = oggpack_read( &opb, 32 );
                    {
                        char title[sizeof("Stream") + 10];
                        input_info_category_t *p_cat;
                        sprintf( title, "Stream %d", p_ogg->i_streams );
                        p_cat = input_InfoCategory( p_input, title );
                        input_AddInfo( p_cat, "Type", "Audio" );
                        input_AddInfo( p_cat, "Codec", "vorbis" );
                        input_AddInfo( p_cat, "Sample Rate", "%f",
                                       p_stream->f_rate );
                        input_AddInfo( p_cat, "Bit Rate", "%d",
                                       p_stream->i_bitrate );
                    }
                }
                /* Check for Theora header */
                else if( oggpacket.bytes >= 7 &&
                         ! strncmp( &oggpacket.packet[1], "theora", 6 ) )
                {
#ifdef HAVE_OGGPACKB
                    oggpack_buffer opb;
                    int i_fps_numerator;
                    int i_fps_denominator;
                    int i_keyframe_frequency_force;
#endif

                    msg_Dbg( p_input, "found theora header" );
#ifdef HAVE_OGGPACKB
                    p_stream->i_cat = VIDEO_ES;
                    p_stream->i_fourcc = VLC_FOURCC( 't','h','e','o' );

                    /* Cheat and get additionnal info ;) */
                    oggpackB_readinit(&opb, oggpacket.packet, oggpacket.bytes);
                    oggpackB_adv( &opb, 56 );
                    oggpackB_read( &opb, 8 ); /* major version num */
                    oggpackB_read( &opb, 8 ); /* minor version num */
                    oggpackB_read( &opb, 8 ); /* subminor version num */
                    oggpackB_read( &opb, 16 ) /*<< 4*/; /* width */
                    oggpackB_read( &opb, 16 ) /*<< 4*/; /* height */
                    i_fps_numerator = oggpackB_read( &opb, 32 );
                    i_fps_denominator = oggpackB_read( &opb, 32 );
                    oggpackB_read( &opb, 24 ); /* aspect_numerator */
                    oggpackB_read( &opb, 24 ); /* aspect_denominator */
                    i_keyframe_frequency_force = 1 << oggpackB_read( &opb, 5 );
                    p_stream->i_bitrate = oggpackB_read( &opb, 24 );
                    oggpackB_read(&opb,6); /* quality */

                    /* granule_shift = i_log( frequency_force -1 ) */
                    p_stream->i_theora_keyframe_granule_shift = 0;
                    i_keyframe_frequency_force--;
                    while( i_keyframe_frequency_force )
                    {
                        p_stream->i_theora_keyframe_granule_shift++;
                        i_keyframe_frequency_force >>= 1;
                    }

                    p_stream->f_rate = (float)i_fps_numerator /
                                                i_fps_denominator;
                    msg_Dbg( p_input,
                             "found theora header, bitrate: %i, rate: %f",
                             p_stream->i_bitrate, p_stream->f_rate );
                    {
                        char title[sizeof("Stream") + 10];
                        input_info_category_t *p_cat;
                        sprintf( title, "Stream %d", p_ogg->i_streams );
                        p_cat = input_InfoCategory( p_input, title );
                        input_AddInfo( p_cat, "Type", "Video" );
                        input_AddInfo( p_cat, "Codec", "theora" );
                        input_AddInfo( p_cat, "Frame Rate", "%f",
                                       p_stream->f_rate );
                        input_AddInfo( p_cat, "Bit Rate", "%d",
                                       p_stream->i_bitrate );
                    }
#else /* HAVE_OGGPACKB */
                    msg_Dbg( p_input, "the ogg demuxer has been compiled "
                             "without support for the oggpackB extension."
                             "The theora stream won't be decoded." );
                    free( p_stream );
                    p_ogg->i_streams--;
                    continue;
#endif /* HAVE_OGGPACKB */
                }
                /* Check for Tarkin header */
                else if( oggpacket.bytes >= 7 &&
                         ! strncmp( &oggpacket.packet[1], "tarkin", 6 ) )
                {
                    oggpack_buffer opb;

                    msg_Dbg( p_input, "found tarkin header" );
                    p_stream->i_cat = VIDEO_ES;
                    p_stream->i_fourcc = VLC_FOURCC( 't','a','r','k' );

                    /* Cheat and get additionnal info ;) */
                    oggpack_readinit( &opb, oggpacket.packet, oggpacket.bytes);
                    oggpack_adv( &opb, 88 );
                    oggpack_adv( &opb, 104 );
                    p_stream->i_bitrate = oggpack_read( &opb, 32 );
                    p_stream->f_rate = 2; /* FIXME */
                    msg_Dbg( p_input,
                             "found tarkin header, bitrate: %i, rate: %f",
                             p_stream->i_bitrate, p_stream->f_rate );
                                        {
                        char title[sizeof("Stream") + 10];
                        input_info_category_t *p_cat;
                        sprintf( title, "Stream %d", p_ogg->i_streams );
                        p_cat = input_InfoCategory( p_input, title );
                        input_AddInfo( p_cat, "Type", "Video" );
                        input_AddInfo( p_cat, "Codec", "tarkin" );
                        input_AddInfo( p_cat, "Sample Rate", "%f",
                                       p_stream->f_rate );
                        input_AddInfo( p_cat, "Bit Rate", "%d",
                                       p_stream->i_bitrate );
                    }

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
                        p_stream->i_cat = VIDEO_ES;

                        p_stream->p_bih = (BITMAPINFOHEADER *)
                            malloc( sizeof(BITMAPINFOHEADER) );
                        if( !p_stream->p_bih )
                        {
                            /* Mem allocation error, just ignore the stream */
                            free( p_stream );
                            p_ogg->i_streams--;
                            continue;
                        }
                        p_stream->p_bih->biSize = sizeof(BITMAPINFOHEADER);
                        p_stream->p_bih->biCompression= p_stream->i_fourcc =
                            VLC_FOURCC( oggpacket.packet[68],
                                        oggpacket.packet[69],
                                        oggpacket.packet[70],
                                        oggpacket.packet[71] );
                        msg_Dbg( p_input, "found video header of type: %.4s",
                                 (char *)&p_stream->i_fourcc );

                        p_stream->f_rate = 10000000.0 /
                            GetQWLE((oggpacket.packet+164));
                        p_stream->p_bih->biBitCount =
                            GetWLE((oggpacket.packet+182));
                        if( !p_stream->p_bih->biBitCount )
                            p_stream->p_bih->biBitCount=24; // hack, FIXME
                        p_stream->p_bih->biWidth =
                            GetDWLE((oggpacket.packet+176));
                        p_stream->p_bih->biHeight =
                            GetDWLE((oggpacket.packet+180));
                        p_stream->p_bih->biPlanes= 1 ;
                        p_stream->p_bih->biSizeImage =
                            (p_stream->p_bih->biBitCount >> 3) *
                            p_stream->p_bih->biWidth *
                            p_stream->p_bih->biHeight;

                        msg_Dbg( p_input,
                             "fps: %f, width:%i; height:%i, bitcount:%i",
                            p_stream->f_rate, p_stream->p_bih->biWidth,
                            p_stream->p_bih->biHeight,
                            p_stream->p_bih->biBitCount);
                        {
                            char title[sizeof("Stream") + 10];
                            input_info_category_t *p_cat;
                            sprintf( title, "Stream %d", p_ogg->i_streams );
                            p_cat = input_InfoCategory( p_input, title );
                            input_AddInfo( p_cat, "Type", "Video" );
                            input_AddInfo( p_cat, "Codec", "%.4s",
                                           (char *)&p_stream->i_fourcc );
                            input_AddInfo( p_cat, "Frame Rate", "%f",
                                           p_stream->f_rate );
                            input_AddInfo( p_cat, "Bit Count", "%d",
                                           p_stream->p_bih->biBitCount );
                            input_AddInfo( p_cat, "Width", "%d",
                                           p_stream->p_bih->biWidth );
                            input_AddInfo( p_cat, "Height", "%d",
                                           p_stream->p_bih->biHeight );
                        }
                        p_stream->i_bitrate = 0;
                    }
                    /* Check for audio header (old format) */
                    else if( GetDWLE((oggpacket.packet+96)) == 0x05589F81 )
                    {
                        unsigned int i_extra_size;

                        p_stream->i_cat = AUDIO_ES;

                        i_extra_size = GetWLE((oggpacket.packet+140));

                        p_stream->p_wf = (WAVEFORMATEX *)
                            malloc( sizeof(WAVEFORMATEX) + i_extra_size );
                        if( !p_stream->p_wf )
                        {
                            /* Mem allocation error, just ignore the stream */
                            free( p_stream );
                            p_ogg->i_streams--;
                            continue;
                        }

                        p_stream->p_wf->wFormatTag =
                            GetWLE((oggpacket.packet+124));
                        p_stream->p_wf->nChannels =
                            GetWLE((oggpacket.packet+126));
                        p_stream->f_rate = p_stream->p_wf->nSamplesPerSec =
                            GetDWLE((oggpacket.packet+128));
                        p_stream->i_bitrate = p_stream->p_wf->nAvgBytesPerSec =
                            GetDWLE((oggpacket.packet+132));
                        p_stream->i_bitrate *= 8;
                        p_stream->p_wf->nBlockAlign =
                            GetWLE((oggpacket.packet+136));
                        p_stream->p_wf->wBitsPerSample =
                            GetWLE((oggpacket.packet+138));
                        p_stream->p_wf->cbSize = i_extra_size;

                        if( i_extra_size > 0 )
                            memcpy( p_stream->p_wf+sizeof(WAVEFORMATEX),
                                    oggpacket.packet+142, i_extra_size );

                        switch( p_stream->p_wf->wFormatTag )
                        {
                        case WAVE_FORMAT_PCM:
                            p_stream->i_fourcc =
                                VLC_FOURCC( 'a', 'r', 'a', 'w' );
                            break;
                        case WAVE_FORMAT_MPEG:
                        case WAVE_FORMAT_MPEGLAYER3:
                            p_stream->i_fourcc =
                                VLC_FOURCC( 'm', 'p', 'g', 'a' );
                            break;
                        case WAVE_FORMAT_A52:
                            p_stream->i_fourcc =
                                VLC_FOURCC( 'a', '5', '2', ' ' );
                            break;
                        case WAVE_FORMAT_WMA1:
                            p_stream->i_fourcc =
                                VLC_FOURCC( 'w', 'm', 'a', '1' );
                            break;
                        case WAVE_FORMAT_WMA2:
                            p_stream->i_fourcc =
                                VLC_FOURCC( 'w', 'm', 'a', '2' );
                            break;
                        default:
                            p_stream->i_fourcc = VLC_FOURCC( 'm', 's',
                                ( p_stream->p_wf->wFormatTag >> 8 ) & 0xff,
                                p_stream->p_wf->wFormatTag & 0xff );
                        }

                        msg_Dbg( p_input, "found audio header of type: %.4s",
                                 (char *)&p_stream->i_fourcc );
                        msg_Dbg( p_input, "audio:0x%4.4x channels:%d %dHz "
                                 "%dbits/sample %dkb/s",
                                 p_stream->p_wf->wFormatTag,
                                 p_stream->p_wf->nChannels,
                                 p_stream->p_wf->nSamplesPerSec,
                                 p_stream->p_wf->wBitsPerSample,
                                 p_stream->p_wf->nAvgBytesPerSec * 8 / 1024 );
                        {
                            char title[sizeof("Stream") + 10];
                            input_info_category_t *p_cat;
                            sprintf( title, "Stream %d", p_ogg->i_streams );
                            p_cat = input_InfoCategory( p_input, title );
                            input_AddInfo( p_cat, "Type", "Audio" );
                            input_AddInfo( p_cat, "Codec", "%.4s", 
                                           (char *)&p_stream->i_fourcc );
                            input_AddInfo( p_cat, "Sample Rate", "%d",
                                           p_stream->p_wf->nSamplesPerSec );
                            input_AddInfo( p_cat, "Bit Rate", "%d",
                                           p_stream->p_wf->nAvgBytesPerSec * 8
                                              / 1024 );
                            input_AddInfo( p_cat, "Channels", "%d",
                                           p_stream->p_wf->nChannels );
                            input_AddInfo( p_cat, "Bits per Sample", "%d",
                                           p_stream->p_wf->wBitsPerSample );
                        }

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
                        p_stream->i_cat = VIDEO_ES;

                        /* We need to get rid of the header packet */
                        ogg_stream_packetout( &p_stream->os, &oggpacket );

                        p_stream->p_bih = (BITMAPINFOHEADER *)
                            malloc( sizeof(BITMAPINFOHEADER) );
                        if( !p_stream->p_bih )
                        {
                            /* Mem allocation error, just ignore the stream */
                            free( p_stream );
                            p_ogg->i_streams--;
                            continue;
                        }
                        p_stream->p_bih->biSize = sizeof(BITMAPINFOHEADER);
                        p_stream->p_bih->biCompression=
                            p_stream->i_fourcc = VLC_FOURCC( st->subtype[0],
                                                             st->subtype[1],
                                                             st->subtype[2],
                                                             st->subtype[3] );
                        msg_Dbg( p_input, "found video header of type: %.4s",
                                 (char *)&p_stream->i_fourcc );

                        p_stream->f_rate = 10000000.0 /
                            GetQWLE((uint8_t *)&st->time_unit);
                        p_stream->p_bih->biBitCount =
                            GetWLE((uint8_t *)&st->bits_per_sample);
                        p_stream->p_bih->biWidth =
                            GetDWLE((uint8_t *)&st->sh.video.width);
                        p_stream->p_bih->biHeight =
                            GetDWLE((uint8_t *)&st->sh.video.height);
                        p_stream->p_bih->biPlanes= 1 ;
                        p_stream->p_bih->biSizeImage =
                            (p_stream->p_bih->biBitCount >> 3) *
                            p_stream->p_bih->biWidth *
                            p_stream->p_bih->biHeight;

                        msg_Dbg( p_input,
                             "fps: %f, width:%i; height:%i, bitcount:%i",
                            p_stream->f_rate, p_stream->p_bih->biWidth,
                            p_stream->p_bih->biHeight,
                            p_stream->p_bih->biBitCount);

                        {
                            char title[sizeof("Stream") + 10];
                            input_info_category_t *p_cat;
                            sprintf( title, "Stream %d", p_ogg->i_streams );
                            p_cat = input_InfoCategory( p_input, title );
                            input_AddInfo( p_cat, "Type", "Video" );
                            input_AddInfo( p_cat, "Codec", "%.4s",
                                           (char *)&p_stream->i_fourcc );
                            input_AddInfo( p_cat, "Frame Rate", "%f",
                                           p_stream->f_rate );
                            input_AddInfo( p_cat, "Bit Count", "%d",
                                           p_stream->p_bih->biBitCount );
                            input_AddInfo( p_cat, "Width", "%d",
                                           p_stream->p_bih->biWidth );
                            input_AddInfo( p_cat, "Height", "%d",
                                           p_stream->p_bih->biHeight );
                        }
                        p_stream->i_bitrate = 0;
                    }
                    /* Check for audio header (new format) */
                    else if( !strncmp( st->streamtype, "audio", 5 ) )
                    {
                        char p_buffer[5];

                        p_stream->i_cat = AUDIO_ES;

                        /* We need to get rid of the header packet */
                        ogg_stream_packetout( &p_stream->os, &oggpacket );

                        p_stream->p_wf = (WAVEFORMATEX *)
                            malloc( sizeof(WAVEFORMATEX) );
                        if( !p_stream->p_wf )
                        {
                            /* Mem allocation error, just ignore the stream */
                            free( p_stream );
                            p_ogg->i_streams--;
                            continue;
                        }

                        memcpy( p_buffer, st->subtype, 4 );
                        p_buffer[4] = '\0';
                        p_stream->p_wf->wFormatTag = strtol(p_buffer,NULL,16);
                        p_stream->p_wf->nChannels =
                            GetWLE((uint8_t *)&st->sh.audio.channels);
                        p_stream->f_rate = p_stream->p_wf->nSamplesPerSec =
                            GetQWLE((uint8_t *)&st->samples_per_unit);
                        p_stream->i_bitrate = p_stream->p_wf->nAvgBytesPerSec =
                            GetDWLE((uint8_t *)&st->sh.audio.avgbytespersec);
                        p_stream->i_bitrate *= 8;
                        p_stream->p_wf->nBlockAlign =
                            GetWLE((uint8_t *)&st->sh.audio.blockalign);
                        p_stream->p_wf->wBitsPerSample =
                            GetWLE((uint8_t *)&st->bits_per_sample);
                        p_stream->p_wf->cbSize = 0;

                        switch( p_stream->p_wf->wFormatTag )
                        {
                        case WAVE_FORMAT_PCM:
                            p_stream->i_fourcc =
                                VLC_FOURCC( 'a', 'r', 'a', 'w' );
                            break;
                        case WAVE_FORMAT_MPEG:
                        case WAVE_FORMAT_MPEGLAYER3:
                            p_stream->i_fourcc =
                                VLC_FOURCC( 'm', 'p', 'g', 'a' );
                            break;
                        case WAVE_FORMAT_A52:
                            p_stream->i_fourcc =
                                VLC_FOURCC( 'a', '5', '2', ' ' );
                            break;
                        case WAVE_FORMAT_WMA1:
                            p_stream->i_fourcc =
                                VLC_FOURCC( 'w', 'm', 'a', '1' );
                            break;
                        case WAVE_FORMAT_WMA2:
                            p_stream->i_fourcc =
                                VLC_FOURCC( 'w', 'm', 'a', '2' );
                            break;
                        default:
                            p_stream->i_fourcc = VLC_FOURCC( 'm', 's',
                                ( p_stream->p_wf->wFormatTag >> 8 ) & 0xff,
                                p_stream->p_wf->wFormatTag & 0xff );
                        }

                        msg_Dbg( p_input, "found audio header of type: %.4s",
                                 (char *)&p_stream->i_fourcc );
                        msg_Dbg( p_input, "audio:0x%4.4x channels:%d %dHz "
                                 "%dbits/sample %dkb/s",
                                 p_stream->p_wf->wFormatTag,
                                 p_stream->p_wf->nChannels,
                                 p_stream->p_wf->nSamplesPerSec,
                                 p_stream->p_wf->wBitsPerSample,
                                 p_stream->p_wf->nAvgBytesPerSec * 8 / 1024 );
                        {
                            char title[sizeof("Stream") + 10];
                            input_info_category_t *p_cat;
                            sprintf( title, "Stream %d", p_ogg->i_streams );
                            p_cat = input_InfoCategory( p_input, title );
                            input_AddInfo( p_cat, "Type", "Audio" );
                            input_AddInfo( p_cat, "Codec", "%.4s", 
                                           (char *)&p_stream->i_fourcc );
                            input_AddInfo( p_cat, "Sample Rate", "%d",
                                           p_stream->p_wf->nSamplesPerSec );
                            input_AddInfo( p_cat, "Bit Rate", "%d",
                                           p_stream->p_wf->nAvgBytesPerSec * 8
                                              / 1024 );
                            input_AddInfo( p_cat, "Channels", "%d",
                                           p_stream->p_wf->nChannels );
                            input_AddInfo( p_cat, "Bits per Sample", "%d",
                                           p_stream->p_wf->wBitsPerSample );
                        }
                    }
                    /* Check for text (subtitles) header */
                    else if( !strncmp(st->streamtype, "text", 4) )
                    {
                        /* We need to get rid of the header packet */
                        ogg_stream_packetout( &p_stream->os, &oggpacket );

                        msg_Dbg( p_input, "found text subtitles header" );
                        p_stream->i_cat = SPU_ES;
                        p_stream->i_fourcc =
                            VLC_FOURCC( 's', 'u', 'b', 't' );
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

#undef p_stream

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
    return VLC_EGENERIC;
}

/*****************************************************************************
 * Activate: initializes ogg demux structures
 *****************************************************************************/
static int Activate( vlc_object_t * p_this )
{
    int i_stream, b_forced;
    demux_sys_t    *p_ogg;
    input_thread_t *p_input = (input_thread_t *)p_this;

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

    p_ogg->i_pcr  = 0;
    p_ogg->b_seekable = ( ( p_input->stream.b_seekable )
                        &&( p_input->stream.i_method == INPUT_METHOD_FILE ) );

    /* Initialize the Ogg physical bitstream parser */
    ogg_sync_init( &p_ogg->oy );

    /* Find the logical streams embedded in the physical stream and
     * initialize our p_ogg structure. */
    if( Ogg_FindLogicalStreams( p_input, p_ogg ) != VLC_SUCCESS )
    {
        msg_Err( p_input, "couldn't find an ogg logical stream" );
        goto error;
    }

    /* Set the demux function */
    p_input->pf_demux = Demux;

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
    p_input->stream.i_mux_rate = 0;
    vlc_mutex_unlock( &p_input->stream.stream_lock );

    for( i_stream = 0 ; i_stream < p_ogg->i_streams; i_stream++ )
    {
#define p_stream p_ogg->pp_stream[i_stream]
        vlc_mutex_lock( &p_input->stream.stream_lock );
        p_stream->p_es = input_AddES( p_input,
                                      p_input->stream.p_selected_program,
                                      p_ogg->i_streams + 1, 0 );
        p_input->stream.i_mux_rate += (p_stream->i_bitrate / ( 8 * 50 ));
        vlc_mutex_unlock( &p_input->stream.stream_lock );
        p_stream->p_es->i_stream_id = p_stream->p_es->i_id = i_stream;
        p_stream->p_es->i_fourcc = p_stream->i_fourcc;
        p_stream->p_es->i_cat = p_stream->i_cat;
        p_stream->p_es->p_waveformatex      = (void*)p_stream->p_wf;
        p_stream->p_es->p_bitmapinfoheader  = (void*)p_stream->p_bih;
#undef p_stream
    }

    for( i_stream = 0; i_stream < p_ogg->i_streams; i_stream++ )
    {
#define p_stream  p_ogg->pp_stream[i_stream]
        switch( p_stream->p_es->i_cat )
        {
            case( VIDEO_ES ):
                if( (p_ogg->p_stream_video == NULL) )
                {
                    p_ogg->p_stream_video = p_stream;
                    /* TODO add test to see if a decoder has been found */
                    Ogg_StreamStart( p_input, p_ogg, i_stream );
                }
                break;

            case( AUDIO_ES ):
                if( (p_ogg->p_stream_audio == NULL) )
                {
                    int i_audio = config_GetInt( p_input, "audio-channel" );
                    if( i_audio == i_stream || i_audio <= 0 ||
                        i_audio >= p_ogg->i_streams ||
                        p_ogg->pp_stream[i_audio]->p_es->i_cat != AUDIO_ES )
                    {
                        p_ogg->p_stream_audio = p_stream;
                        Ogg_StreamStart( p_input, p_ogg, i_stream );
                    }
                }
                break;

            case( SPU_ES ):
                if( (p_ogg->p_stream_spu == NULL) )
                {
                    /* for spu, default is none */
                    int i_spu = config_GetInt( p_input, "spu-channel" );
                    if( i_spu < 0 || i_spu >= p_ogg->i_streams ||
                        p_ogg->pp_stream[i_spu]->p_es->i_cat != SPU_ES )
                    {
                        break;
                    }
                    else if( i_spu == i_stream )
                    {
                        p_ogg->p_stream_spu = p_stream;
                        Ogg_StreamStart( p_input, p_ogg, i_stream );
                    }
                }
                break;

            default:
                break;
        }
#undef p_stream
    }

    /* we select the first audio and video ES */
    vlc_mutex_lock( &p_input->stream.stream_lock );
    if( !p_ogg->p_stream_video )
    {
        msg_Warn( p_input, "no video stream found" );
    }
    if( !p_ogg->p_stream_audio )
    {
        msg_Warn( p_input, "no audio stream found!" );
    }
    p_input->stream.p_selected_program->b_is_ok = 1;
    vlc_mutex_unlock( &p_input->stream.stream_lock );

    /* Call the pace control */
    input_ClockManageRef( p_input,
                          p_input->stream.p_selected_program,
                          p_ogg->i_pcr );

    return 0;

 error:
    Deactivate( (vlc_object_t *)p_input );
    return -1;

}

/*****************************************************************************
 * Deactivate: frees unused data
 *****************************************************************************/
static void Deactivate( vlc_object_t *p_this )
{
    input_thread_t *p_input = (input_thread_t *)p_this;
    demux_sys_t *p_ogg = (demux_sys_t *)p_input->p_demux_data  ; 
    int i, j;

    if( p_ogg )
    {
        /* Cleanup the bitstream parser */
        ogg_sync_clear( &p_ogg->oy );

        for( i = 0; i < p_ogg->i_streams; i++ )
        {
            ogg_stream_clear( &p_ogg->pp_stream[i]->os );
            for( j = 0; j < p_ogg->pp_stream[i]->i_packets_backup; j++ )
            {
                free( p_ogg->pp_stream[i]->p_packets_backup[j].packet );
            }
            if( p_ogg->pp_stream[i]->p_packets_backup)
                free( p_ogg->pp_stream[i]->p_packets_backup );
#if 0 /* hmmm, it's already freed in input_DelES() */
            if( p_ogg->pp_stream[i]->p_bih )
                free( p_ogg->pp_stream[i]->p_bih );
            if( p_ogg->pp_stream[i]->p_wf )
                free( p_ogg->pp_stream[i]->p_wf );
#endif
            free( p_ogg->pp_stream[i] );
        }
        if( p_ogg->pp_stream ) free( p_ogg->pp_stream );

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
    int i, i_stream, b_eos = 0;
    ogg_page    oggpage;
    ogg_packet  oggpacket;
    demux_sys_t *p_ogg  = (demux_sys_t *)p_input->p_demux_data;

#define p_stream p_ogg->pp_stream[i_stream]
    /* detect new selected/unselected streams */
    for( i_stream = 0; i_stream < p_ogg->i_streams; i_stream++ )
    {
        if( p_stream->p_es )
        {
            if( p_stream->p_es->p_decoder_fifo &&
                !p_stream->i_activated )
            {
                Ogg_StreamStart( p_input, p_ogg, i_stream );
            }
            else
            if( !p_stream->p_es->p_decoder_fifo &&
                p_stream->i_activated )
            {
                Ogg_StreamStop( p_input, p_ogg, i_stream );
            }
        }
    }

    /* search for new video and audio stream to select
     * if current have been unselected */
    if( ( !p_ogg->p_stream_video )
            || ( !p_ogg->p_stream_video->p_es->p_decoder_fifo ) )
    {
        p_ogg->p_stream_video = NULL;
        for( i_stream = 0; i_stream < p_ogg->i_streams; i_stream++ )
        {
            if( ( p_stream->i_cat == VIDEO_ES )
                  &&( p_stream->p_es->p_decoder_fifo ) )
            {
                p_ogg->p_stream_video = p_stream;
                break;
            }
        }
    }
    if( ( !p_ogg->p_stream_audio )
            ||( !p_ogg->p_stream_audio->p_es->p_decoder_fifo ) )
    {
        p_ogg->p_stream_audio = NULL;
        for( i_stream = 0; i_stream < p_ogg->i_streams; i_stream++ )
        {
            if( ( p_stream->i_cat == AUDIO_ES )
                  &&( p_stream->p_es->p_decoder_fifo ) )
            {
                p_ogg->p_stream_audio = p_stream;
                break;
            }
        }
    }

    if( p_input->stream.p_selected_program->i_synchro_state == SYNCHRO_REINIT )
    {
        msg_Warn( p_input, "synchro reinit" );

        /* An ogg packet does only contain the starting date of the next
         * packet, not its own starting date.
         * As a quick work around, we just skip an oggpage */

        for( i_stream = 0; i_stream < p_ogg->i_streams; i_stream++ )
        {
            /* we'll trash all the data until we find the next pcr */
            p_stream->b_reinit = 1;
            p_stream->i_pcr = -1;
            p_stream->i_interpolated_pcr = -1;
        }
        p_ogg->b_reinit = 1;
    }


    /*
     * Demux ogg pages from the stream
     */
    for( i = 0; i < PAGES_READ_ONCE || p_ogg->b_reinit;  i++ )
    {
        if( Ogg_ReadPage( p_input, p_ogg, &oggpage ) != VLC_SUCCESS )
        {
            b_eos = 1;
            break;
        }

        for( i_stream = 0; i_stream < p_ogg->i_streams; i_stream++ )
        {
            if( ogg_stream_pagein( &p_stream->os, &oggpage ) != 0 )
                continue;

            while( ogg_stream_packetout( &p_stream->os, &oggpacket ) > 0 )
            {
                if( !p_stream->p_es )
                {
                    break;
                }

                if( p_stream->b_reinit )
                {
                    if( oggpacket.granulepos >= 0 )
                    {
                        p_stream->b_reinit = 0;

                        /* Convert the next granulepos into a pcr */
                        Ogg_UpdatePCR( p_stream, &oggpacket );

                        /* Call the pace control to reinitialize
                         * the system clock */
                         input_ClockManageRef( p_input,
                             p_input->stream.p_selected_program,
                             p_stream->i_pcr );

                         if( (!p_ogg->p_stream_video ||
                              !p_ogg->p_stream_video->b_reinit) &&
                             (!p_ogg->p_stream_audio ||
                              !p_ogg->p_stream_audio->b_reinit) )
                         {
                             p_ogg->b_reinit = 0;
                         }
                    }
                    continue;
                }

                Ogg_DecodePacket( p_input, p_stream, &oggpacket );

            }
        }
    }

    i_stream = 0;
    p_ogg->i_pcr = p_stream->i_interpolated_pcr;
    for( ; i_stream < p_ogg->i_streams; i_stream++ )
    {
        if( p_stream->i_cat == SPU_ES )
            continue;

        if( p_stream->i_interpolated_pcr > 0
            && p_stream->i_interpolated_pcr < p_ogg->i_pcr )
            p_ogg->i_pcr = p_stream->i_interpolated_pcr;
    }
#undef p_stream


    /* Call the pace control */
    input_ClockManageRef( p_input, p_input->stream.p_selected_program,
                          p_ogg->i_pcr );

    /* Did we reach the end of stream ? */
    return( b_eos ? 0 : 1 );
}
