/*****************************************************************************
 * copy.c
 *****************************************************************************
 * Copyright (C) 2001, 2002 VideoLAN
 * $Id: copy.c,v 1.5 2003/03/11 19:02:30 fenrir Exp $
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Eric Petit <titer@videolan.org>
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
#include <vlc/aout.h>
#include <vlc/decoder.h>
#include <vlc/input.h>
#include <vlc/sout.h>

#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>                                              /* strdup() */

#include "codecs.h"
/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
typedef struct packetizer_thread_s
{
    /* Input properties */
    decoder_fifo_t          *p_fifo;

    /* Output properties */
    sout_packetizer_input_t *p_sout_input;
    sout_packet_format_t    output_format;

    mtime_t i_pts_start;

} packetizer_thread_t;

static int  Open    ( vlc_object_t * );
static int  Run     ( decoder_fifo_t * );

static int  InitThread     ( packetizer_thread_t * );
static void PacketizeThread   ( packetizer_thread_t * );
static void EndThread      ( packetizer_thread_t * );


static void input_ShowPES( decoder_fifo_t *p_fifo, pes_packet_t **pp_pes );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

vlc_module_begin();
    set_description( _("Copy packetizer") );
    set_capability( "packetizer", 1 );
    set_callbacks( Open, NULL );
vlc_module_end();


/*****************************************************************************
 * OpenDecoder: probe the packetizer and return score
 *****************************************************************************
 * Tries to launch a decoder and return score so that the interface is able
 * to choose.
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    decoder_fifo_t *p_fifo = (decoder_fifo_t*) p_this;

    p_fifo->pf_run = Run;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * RunDecoder: this function is called just after the thread is created
 *****************************************************************************/
static int Run( decoder_fifo_t *p_fifo )
{
    packetizer_thread_t *p_pack;
    int b_error;

    msg_Info( p_fifo, "Running copy packetizer" );
    if( !( p_pack = malloc( sizeof( packetizer_thread_t ) ) ) )
    {
        msg_Err( p_fifo, "out of memory" );
        DecoderError( p_fifo );
        return( -1 );
    }
    memset( p_pack, 0, sizeof( packetizer_thread_t ) );

    p_pack->p_fifo = p_fifo;

    if( InitThread( p_pack ) != 0 )
    {
        DecoderError( p_fifo );
        return( -1 );
    }

    while( ( !p_pack->p_fifo->b_die )&&( !p_pack->p_fifo->b_error ) )
    {
        PacketizeThread( p_pack );
    }


    if( ( b_error = p_pack->p_fifo->b_error ) )
    {
        DecoderError( p_pack->p_fifo );
    }

    EndThread( p_pack );
    if( b_error )
    {
        return( -1 );
    }

    return( 0 );
}


#define FREE( p ) if( p ) free( p ); p = NULL

/*****************************************************************************
 * InitThread: initialize data before entering main loop
 *****************************************************************************/

static int InitThread( packetizer_thread_t *p_pack )
{

    switch( p_pack->p_fifo->i_fourcc )
    {
        case VLC_FOURCC( 'm', '4', 's', '2'):
        case VLC_FOURCC( 'M', '4', 'S', '2'):
        case VLC_FOURCC( 'm', 'p', '4', 's'):
        case VLC_FOURCC( 'M', 'P', '4', 'S'):
        case VLC_FOURCC( 'm', 'p', '4', 'v'):
        case VLC_FOURCC( 'D', 'I', 'V', 'X'):
        case VLC_FOURCC( 'd', 'i', 'v', 'x'):
        case VLC_FOURCC( 'X', 'V', 'I', 'D'):
        case VLC_FOURCC( 'X', 'v', 'i', 'D'):
        case VLC_FOURCC( 'x', 'v', 'i', 'd'):
        case VLC_FOURCC( 'D', 'X', '5', '0'):
        case VLC_FOURCC( 0x04, 0,   0,   0):
        case VLC_FOURCC( '3', 'I', 'V', '2'):
            p_pack->output_format.i_fourcc = VLC_FOURCC( 'm', 'p', '4', 'v');
            p_pack->output_format.i_cat = VIDEO_ES;
            break;
        case VLC_FOURCC( 'm', 'p', 'g', 'v' ):
        case VLC_FOURCC( 'm', 'p', 'g', '1' ):
        case VLC_FOURCC( 'm', 'p', 'g', '2' ):
        case VLC_FOURCC( 'm', 'p', '1', 'v' ):
        case VLC_FOURCC( 'm', 'p', '2', 'v' ):
            p_pack->output_format.i_fourcc = VLC_FOURCC( 'm', 'p', 'g', 'v' );
            p_pack->output_format.i_cat = VIDEO_ES;
            break;
        case VLC_FOURCC( 'm', 'p', 'g', 'a' ):
            p_pack->output_format.i_fourcc = VLC_FOURCC( 'm', 'p', 'g', 'a' );
            p_pack->output_format.i_cat = AUDIO_ES;
            break;

        case VLC_FOURCC( 'd', 'i', 'v', '1' ):
        case VLC_FOURCC( 'D', 'I', 'V', '1' ):
        case VLC_FOURCC( 'M', 'P', 'G', '4' ):
        case VLC_FOURCC( 'm', 'p', 'g', '4' ):
            p_pack->output_format.i_fourcc = VLC_FOURCC( 'D', 'I', 'V', '1' );
            p_pack->output_format.i_cat = VIDEO_ES;
            break;
        case VLC_FOURCC( 'd', 'i', 'v', '2' ):
        case VLC_FOURCC( 'D', 'I', 'V', '2' ):
        case VLC_FOURCC( 'M', 'P', '4', '2' ):
        case VLC_FOURCC( 'm', 'p', '4', '2' ):
            p_pack->output_format.i_fourcc = VLC_FOURCC( 'D', 'I', 'V', '2' );
            p_pack->output_format.i_cat = VIDEO_ES;
            break;
        case VLC_FOURCC( 'd', 'i', 'v', '3' ):
        case VLC_FOURCC( 'D', 'I', 'V', '3' ):
        case VLC_FOURCC( 'd', 'i', 'v', '4' ):
        case VLC_FOURCC( 'D', 'I', 'V', '4' ):
        case VLC_FOURCC( 'd', 'i', 'v', '5' ):
        case VLC_FOURCC( 'D', 'I', 'V', '5' ):
        case VLC_FOURCC( 'd', 'i', 'v', '6' ):
        case VLC_FOURCC( 'D', 'I', 'V', '6' ):
        case VLC_FOURCC( 'M', 'P', '4', '3' ):
        case VLC_FOURCC( 'm', 'p', '4', '3' ):
        case VLC_FOURCC( 'm', 'p', 'g', '3' ):
        case VLC_FOURCC( 'M', 'P', 'G', '3' ):
        case VLC_FOURCC( 'A', 'P', '4', '1' ):
            p_pack->output_format.i_fourcc = VLC_FOURCC( 'D', 'I', 'V', '3' );
            p_pack->output_format.i_cat = VIDEO_ES;
            break;
        case VLC_FOURCC( 'H', '2', '6', '3' ):
        case VLC_FOURCC( 'h', '2', '6', '3' ):
        case VLC_FOURCC( 'U', '2', '6', '3' ):
        case VLC_FOURCC( 'u', '2', '6', '3' ):
            p_pack->output_format.i_fourcc = VLC_FOURCC( 'H', '2', '6', '3' );
            p_pack->output_format.i_cat = VIDEO_ES;
            break;
        case VLC_FOURCC( 'I', '2', '6', '3' ):
        case VLC_FOURCC( 'i', '2', '6', '3' ):
            p_pack->output_format.i_fourcc = VLC_FOURCC( 'I', '2', '6', '3' );
            p_pack->output_format.i_cat = VIDEO_ES;
            break;
        case VLC_FOURCC( 'W', 'M', 'V', '1' ):
            p_pack->output_format.i_fourcc = VLC_FOURCC( 'W', 'M', 'V', '1' );
            p_pack->output_format.i_cat = VIDEO_ES;
            break;
        case VLC_FOURCC( 'W', 'M', 'V', '2' ):
            p_pack->output_format.i_fourcc = VLC_FOURCC( 'W', 'M', 'V', '2' );
            p_pack->output_format.i_cat = VIDEO_ES;
            break;
        case VLC_FOURCC( 'M', 'J', 'P', 'G' ):
        case VLC_FOURCC( 'm', 'j', 'p', 'g' ):
        case VLC_FOURCC( 'm', 'j', 'p', 'a' ):
        case VLC_FOURCC( 'j', 'p', 'e', 'g' ):
        case VLC_FOURCC( 'J', 'P', 'E', 'G' ):
        case VLC_FOURCC( 'J', 'F', 'I', 'F' ):
            p_pack->output_format.i_fourcc = VLC_FOURCC( 'M', 'J', 'P', 'G' );
            p_pack->output_format.i_cat = VIDEO_ES;
            break;
        case VLC_FOURCC( 'm', 'j', 'p', 'b' ):
            p_pack->output_format.i_fourcc = VLC_FOURCC( 'm', 'j', 'p', 'b' );
            p_pack->output_format.i_cat = VIDEO_ES;
            break;
        case VLC_FOURCC( 'd', 'v', 's', 'l' ):
        case VLC_FOURCC( 'd', 'v', 's', 'd' ):
        case VLC_FOURCC( 'D', 'V', 'S', 'D' ):
        case VLC_FOURCC( 'd', 'v', 'h', 'd' ):
            p_pack->output_format.i_fourcc = VLC_FOURCC( 'd', 'v', 's', 'l' );
            p_pack->output_format.i_cat = VIDEO_ES;
            break;

        default:
            p_pack->output_format.i_fourcc = p_pack->p_fifo->i_fourcc;
            p_pack->output_format.i_cat = UNKNOWN_ES;
            break;
    }

    switch( p_pack->output_format.i_cat )
    {
        case AUDIO_ES:
            {
                WAVEFORMATEX *p_wf = (WAVEFORMATEX*)p_pack->p_fifo->p_waveformatex;
                if( p_wf )
                {
                    p_pack->output_format.p_format = malloc( sizeof( WAVEFORMATEX ) + p_wf->cbSize );
                    memcpy( p_pack->output_format.p_format, p_wf, sizeof( WAVEFORMATEX ) + p_wf->cbSize );
                }
                else
                {
                    p_pack->output_format.p_format = NULL;
                }
            }
            break;

        case VIDEO_ES:
            {
                BITMAPINFOHEADER *p_bih = (BITMAPINFOHEADER*)p_pack->p_fifo->p_bitmapinfoheader;
                if( p_bih )
                {
                    p_pack->output_format.p_format = malloc( p_bih->biSize );
                    memcpy( p_pack->output_format.p_format, p_bih, p_bih->biSize );
                    if( p_pack->output_format.i_fourcc == VLC_FOURCC( 'm', 'p', '4', 'v' ) )
                    {
                        p_bih->biCompression = VLC_FOURCC( 'd', 'i', 'v', 'x' );
                    }
                    else
                    {
                        p_bih->biCompression = p_pack->output_format.i_fourcc;
                    }

                }
                else
                {
                    p_pack->output_format.p_format = NULL;
                }
            }
            break;
        default:
            p_pack->output_format.p_format = NULL;
            break;
    }

    p_pack->p_sout_input =
        sout_InputNew( p_pack->p_fifo,
                       &p_pack->output_format );

    if( !p_pack->p_sout_input )
    {
        msg_Err( p_pack->p_fifo, "cannot add a new stream" );
        return( -1 );
    }
    p_pack->i_pts_start = -1;
    return( 0 );
}

/*****************************************************************************
 * PacketizeThread: packetize an unit (here copy a complete pes)
 *****************************************************************************/
static void PacketizeThread( packetizer_thread_t *p_pack )
{
    sout_buffer_t   *p_sout_buffer;
    pes_packet_t    *p_pes;
    ssize_t          i_size;

    /* **** get samples count **** */
    input_ExtractPES( p_pack->p_fifo, &p_pes );
    if( !p_pes )
    {
        p_pack->p_fifo->b_error = 1;
        return;
    }
    if( p_pack->i_pts_start < 0 && p_pes->i_pts > 0 )
    {
        p_pack->i_pts_start = p_pes->i_pts;
    }
    i_size = p_pes->i_pes_size;
//    msg_Dbg( p_pack->p_fifo, "pes size:%d", i_size );
    if( i_size > 0 )
    {
        pes_packet_t    *p_pes_next;
        data_packet_t   *p_data;
        ssize_t          i_buffer;

        p_sout_buffer = 
            sout_BufferNew( p_pack->p_sout_input->p_sout, i_size );
        if( !p_sout_buffer )
        {
            p_pack->p_fifo->b_error = 1;
            return;
        }
        /* TODO: memcpy of the pes packet */
        for( i_buffer = 0, p_data = p_pes->p_first;
             p_data != NULL && i_buffer < i_size;
             p_data = p_data->p_next)
        {
            ssize_t i_copy;

            i_copy = __MIN( p_data->p_payload_end - p_data->p_payload_start, 
                            i_size - i_buffer );
            if( i_copy > 0 )
            {
                p_pack->p_fifo->p_vlc->pf_memcpy( p_sout_buffer->p_buffer + i_buffer,
                                                  p_data->p_payload_start,
                                                  i_copy );
            }
            i_buffer += i_copy;
        }
        p_sout_buffer->i_length = 0;
        p_sout_buffer->i_dts = p_pes->i_pts - p_pack->i_pts_start;
        p_sout_buffer->i_pts = p_pes->i_pts - p_pack->i_pts_start;
        p_sout_buffer->i_bitrate = 0;

        input_ShowPES( p_pack->p_fifo, &p_pes_next );
        if( p_pes_next )
        {
            p_sout_buffer->i_length = p_pes_next->i_pts - p_pes->i_pts;
        }
        sout_InputSendBuffer( p_pack->p_sout_input,
                               p_sout_buffer );
    }

    input_DeletePES( p_pack->p_fifo->p_packets_mgt, p_pes );
}


/*****************************************************************************
 * EndThread : packetizer thread destruction
 *****************************************************************************/
static void EndThread ( packetizer_thread_t *p_pack)
{
    if( p_pack->p_sout_input )
    {
        sout_InputDelete( p_pack->p_sout_input );
    }
}

static void input_ShowPES( decoder_fifo_t *p_fifo, pes_packet_t **pp_pes )
{
    pes_packet_t *p_pes;

    vlc_mutex_lock( &p_fifo->data_lock );

    if( p_fifo->p_first == NULL )
    {
        if( p_fifo->b_die )
        {
            vlc_mutex_unlock( &p_fifo->data_lock );
            if( pp_pes ) *pp_pes = NULL;
            return;
        }

        /* Signal the input thread we're waiting. This is only
         * needed in case of slave clock (ES plug-in) but it won't
         * harm. */
        vlc_cond_signal( &p_fifo->data_wait );

        /* Wait for the input to tell us when we received a packet. */
        vlc_cond_wait( &p_fifo->data_wait, &p_fifo->data_lock );
    }
    p_pes = p_fifo->p_first;
    vlc_mutex_unlock( &p_fifo->data_lock );

    if( pp_pes )
    {
        *pp_pes = p_pes;
    }
}

