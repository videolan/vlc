/*****************************************************************************
 * ogg.c : ogg stream input module for vlc
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: ogg.c,v 1.2 2002/10/27 16:59:30 gbazin Exp $
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

#define OGG_BLOCK_SIZE 4096
#define PAGES_READ_ONCE 1

/*****************************************************************************
 * Definitions of structures and functions used by this plugins 
 *****************************************************************************/
typedef struct logical_stream_s
{
    ogg_stream_state os;                        /* logical stream of packets */

    int              i_serial_no;
    int              i_pages_read;
    int              i_cat;                            /* AUDIO_ES, VIDEO_ES */
    int              i_activated;
    vlc_fourcc_t     i_fourcc;
    vlc_fourcc_t     i_codec;

    es_descriptor_t  *p_es;   
    int              b_selected;                           /* newly selected */

    /* info for vorbis logical streams */
    int i_rate;
    int i_channels;
    int i_bitrate;

} logical_stream_t;

struct demux_sys_t
{
    ogg_sync_state oy;        /* sync and verify incoming physical bitstream */

    int i_streams;                           /* number of logical bitstreams */
    logical_stream_t **pp_stream;  /* pointer to an array of logical streams */

    /* current audio and video es */
    logical_stream_t *p_stream_video;
    logical_stream_t *p_stream_audio;

    mtime_t i_time;
    mtime_t i_length;
    mtime_t i_pcr; 
    int     b_seekable;
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Activate  ( vlc_object_t * );
static void Deactivate( vlc_object_t * );
static int  Demux     ( input_thread_t * );

/* Stream managment */
static int  Ogg_StreamStart  ( input_thread_t *, demux_sys_t *, int );
static int  Ogg_StreamSeek   ( input_thread_t *, demux_sys_t *, int, mtime_t );
static void Ogg_StreamStop   ( input_thread_t *, demux_sys_t *, int );

/* Bitstream manipulation */
static int  Ogg_Check        ( input_thread_t *p_input );
static int  Ogg_ReadPage     ( input_thread_t *, demux_sys_t *, ogg_page * );
static void Ogg_DecodePacket ( input_thread_t *p_input,
                               logical_stream_t *p_stream,
                               ogg_packet *p_oggpacket );
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

    //Ogg_StreamSeek( p_input, p_ogg, i_stream, p_ogg->i_time );

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

static int Ogg_StreamSeek( input_thread_t *p_input, demux_sys_t  *p_ogg,
                           int i_stream, mtime_t i_date )
{
#define p_stream    p_ogg->pp_stream[i_stream]

    /* FIXME: todo */

    return 1;
#undef p_stream
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
 * Ogg_DecodePacket: Decode an Ogg packet.
 ****************************************************************************/
static void Ogg_DecodePacket( input_thread_t *p_input,
                              logical_stream_t *p_stream,
                              ogg_packet *p_oggpacket )
{
    pes_packet_t  *p_pes;
    data_packet_t *p_data;

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

    p_pes->p_first = p_pes->p_last = p_data;
    p_pes->i_nb_data = 1;
    p_pes->i_pes_size = p_oggpacket->bytes;
    p_pes->i_dts = p_oggpacket->granulepos;

    /* Convert the granule into a pts */
    p_pes->i_pts = (p_oggpacket->granulepos < 0) ? 0 :
        p_oggpacket->granulepos * 90000 / p_stream->i_rate;
    p_pes->i_pts = (p_oggpacket->granulepos < 0) ? 0 :
        input_ClockGetTS( p_input, p_input->stream.p_selected_program,
                          p_pes->i_pts );

    memcpy( p_data->p_payload_start, p_oggpacket->packet, p_oggpacket->bytes );

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
                p_stream->i_pages_read++;
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

                    /* Cheat and get additionnal info ;) */
                    oggpack_readinit( &opb, oggpacket.packet, oggpacket.bytes);
                    oggpack_adv( &opb, 88 );
                    p_stream->i_channels = oggpack_read( &opb, 8 );
                    p_stream->i_rate = oggpack_read( &opb, 32 );
                    oggpack_adv( &opb, 32 );
                    p_stream->i_bitrate = oggpack_read( &opb, 32 );
                }
                else
                {
                    msg_Dbg( p_input, "found unknown codec" );
                    ogg_stream_destroy( &p_stream->os );
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
                    p_ogg->pp_stream[i_stream]->i_pages_read++;
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

    p_ogg->i_time = 0;
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
        p_stream->p_es->i_stream_id = i_stream;
        p_stream->p_es->i_fourcc = p_stream->i_fourcc;
        p_stream->p_es->i_cat = p_stream->i_cat;
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
                    p_ogg->p_stream_audio = p_stream;
                    Ogg_StreamStart( p_input, p_ogg, i_stream );
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
    int i;

    if( p_ogg )
    {
        /* Cleanup the bitstream parser */
        ogg_sync_clear( &p_ogg->oy );

        for( i = 0; i < p_ogg->i_streams; i++ )
        {
            ogg_stream_clear( &p_ogg->pp_stream[i]->os );
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
    int i, i_stream, b_eos;
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

    /* search new video and audio stream selected
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

    /* Update program clock reference */
    p_ogg->i_pcr = p_ogg->i_time * 9 / 100;

    if( (p_input->stream.p_selected_program->i_synchro_state == SYNCHRO_REINIT)
         || (input_ClockManageControl( p_input,
				       p_input->stream.p_selected_program,
				       (mtime_t)0 ) == PAUSE_S) )
    {
        msg_Warn( p_input, "synchro reinit" );
        p_input->stream.p_selected_program->i_synchro_state = SYNCHRO_OK;
    }

    /* Call the pace control. */
    input_ClockManageRef( p_input,
                          p_input->stream.p_selected_program,
                          p_ogg->i_pcr );

    /* Demux ogg pages from the stream */
    b_eos = 0;
    for( i = 0; i < PAGES_READ_ONCE; i++ )
    {
        if( Ogg_ReadPage(p_input, p_ogg, &oggpage ) != VLC_SUCCESS )
        {
            b_eos = 1;
            break;
        }

        for( i_stream = 0; i_stream < p_ogg->i_streams; i_stream++ )
        {
            /* FIXME: we already read the header */

            if( ogg_stream_pagein( &p_stream->os, &oggpage ) != 0 )
                continue;

            while( ogg_stream_packetout( &p_stream->os, &oggpacket ) > 0 )
            {
                /* FIXME: handle discontinuity */

                if( !p_stream->p_es ||
                    !p_stream->p_es->p_decoder_fifo )
                {
                    break;
                }

                if( oggpacket.granulepos >= 0 )
                    p_ogg->i_time = oggpacket.granulepos * 1000000
                        / p_stream->i_rate;
                else
                    p_ogg->i_time += (oggpacket.bytes * 1000000
                        / p_stream->i_bitrate);

                Ogg_DecodePacket( p_input, p_stream, &oggpacket );
            }
        }
#undef p_stream
    }

    /* Did we reach the end of stream ? */
    return( b_eos ? 0 : 1 );
}
