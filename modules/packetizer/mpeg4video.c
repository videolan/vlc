/*****************************************************************************
 * mpeg4video.c
 *****************************************************************************
 * Copyright (C) 2001, 2002 VideoLAN
 * $Id: mpeg4video.c,v 1.6 2003/01/19 08:27:28 fenrir Exp $
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
    sout_input_t            *p_sout_input;
    sout_packet_format_t    output_format;

    mtime_t i_pts_start;

    int                     i_vol;
    uint8_t                 *p_vol;

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
    set_description( _("MPEG4 Video packetizer") );
    set_capability( "packetizer", 50 );
    set_callbacks( Open, NULL );
vlc_module_end();

#define VIDEO_OBJECT_MASK                       0x01f
#define VIDEO_OBJECT_LAYER_MASK                 0x00f

#define VIDEO_OBJECT_START_CODE                 0x100
#define VIDEO_OBJECT_LAYER_START_CODE           0x120
#define VISUAL_OBJECT_SEQUENCE_START_CODE       0x1b0
#define VISUAL_OBJECT_SEQUENCE_END_CODE         0x1b1
#define USER_DATA_START_CODE                    0x1b2
#define GROUP_OF_VOP_START_CODE                 0x1b3
#define VIDEO_SESSION_ERROR_CODE                0x1b4
#define VISUAL_OBJECT_START_CODE                0x1b5
#define VOP_START_CODE                          0x1b6
#define FACE_OBJECT_START_CODE                  0x1ba
#define FACE_OBJECT_PLANE_START_CODE            0x1bb
#define MESH_OBJECT_START_CODE                  0x1bc
#define MESH_OBJECT_PLANE_START_CODE            0x1bd
#define STILL_TEXTURE_OBJECT_START_CODE         0x1be
#define TEXTURE_SPATIAL_LAYER_START_CODE        0x1bf
#define TEXTURE_SNR_LAYER_START_CODE            0x1c0


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

    switch(  p_fifo->i_fourcc )
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

            return VLC_SUCCESS;
        default:
            return VLC_EGENERIC;
    }
}

/*****************************************************************************
 * RunDecoder: this function is called just after the thread is created
 *****************************************************************************/
static int Run( decoder_fifo_t *p_fifo )
{
    packetizer_thread_t *p_pack;
    int b_error;

    msg_Info( p_fifo, "Running MPEG4 Video packetizer" );
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
    BITMAPINFOHEADER *p_bih;

    p_bih = (BITMAPINFOHEADER*)p_pack->p_fifo->p_bitmapinfoheader;

    if( p_bih && p_bih->biSize > sizeof( BITMAPINFOHEADER ) )
    {
        /* We have a vol */
        p_pack->i_vol = p_bih->biSize - sizeof( BITMAPINFOHEADER );
        p_pack->p_vol = malloc( p_pack->i_vol );
        memcpy( p_pack->p_vol, &p_bih[1], p_pack->i_vol );

        /* create stream input output */
        p_pack->output_format.i_cat = VIDEO_ES;
        p_pack->output_format.i_fourcc = VLC_FOURCC( 'm', 'p', '4', 'v' );
        p_pack->output_format.p_format = malloc( p_bih->biSize );
        memcpy( p_pack->output_format.p_format, p_bih, p_bih->biSize );

        msg_Warn( p_pack->p_fifo, "opening with vol size:%d", p_pack->i_vol );
        p_pack->p_sout_input =
            sout_InputNew( p_pack->p_fifo,
                           &p_pack->output_format );
    }
    else
    {
        p_pack->i_vol = 0;
        p_pack->p_vol = 0;
        p_pack->output_format.i_cat = UNKNOWN_ES;
        p_pack->output_format.i_fourcc = VLC_FOURCC( 'n', 'u', 'l', 'l' );
        p_pack->output_format.p_format = NULL;

        p_pack->p_sout_input =
            sout_InputNew( p_pack->p_fifo,
                           &p_pack->output_format );
    }

    if( !p_pack->p_sout_input )
    {
        msg_Err( p_pack->p_fifo, "cannot add a new stream" );
        return( -1 );
    }
    p_pack->i_pts_start = -1;
    return( 0 );
}

static int m4v_FindStartCode( uint8_t **pp_data, uint8_t *p_end )
{
    for( ; *pp_data < p_end - 4; (*pp_data)++ )
    {
        if( (*pp_data)[0] == 0 && (*pp_data)[1] == 0 && (*pp_data)[2] == 1 )
        {
            return( 0 );
        }
    }
    fprintf( stderr, "\n********* cannot find startcode\n" );
    return( -1 );
}
/*****************************************************************************
 * PacketizeThread: packetize an unit (here copy a complete pes)
 *****************************************************************************/
static void PacketizeThread( packetizer_thread_t *p_pack )
{
    sout_buffer_t   *p_sout_buffer;
    pes_packet_t    *p_pes;
    size_t          i_size;

    /* **** get samples count **** */
    input_ExtractPES( p_pack->p_fifo, &p_pes );
    if( !p_pes )
    {
        p_pack->p_fifo->b_error = 1;
        return;
    }
    if( p_pack->i_pts_start < 0 )
    {
        p_pack->i_pts_start = p_pes->i_pts;
    }

    i_size = p_pes->i_pes_size;
    if( i_size > 0 )
    {
        pes_packet_t    *p_pes_next;
        data_packet_t   *p_data;
        size_t          i_buffer;

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
            size_t          i_copy;

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

        if( p_pack->p_vol == NULL )
        {
            uint8_t *p_vol_begin, *p_vol_end, *p_end;
            /* search if p_sout_buffer contains with a vol */
            p_vol_begin = p_sout_buffer->p_buffer;
            p_vol_end   = NULL;
            p_end       = p_sout_buffer->p_buffer + p_sout_buffer->i_size;

            for( ;; )
            {
                if( m4v_FindStartCode( &p_vol_begin, p_end ) )
                {
                    break;
                }
                msg_Dbg( p_pack->p_fifo,
                          "starcode 0x%2.2x%2.2x%2.2x%2.2x",
                          p_vol_begin[0], p_vol_begin[1], p_vol_begin[2], p_vol_begin[3] );

                if( ( p_vol_begin[3] & ~VIDEO_OBJECT_MASK ) == ( VIDEO_OBJECT_START_CODE&0xff ) )
                {
                    p_vol_end = p_vol_begin + 4;
                    if( m4v_FindStartCode( &p_vol_end, p_end ) )
                    {
                        break;
                    }
                    if( ( p_vol_end[3] & ~VIDEO_OBJECT_LAYER_MASK ) == ( VIDEO_OBJECT_LAYER_START_CODE&0xff ) )
                    {
                        p_vol_end += 4;
                        if( m4v_FindStartCode( &p_vol_end, p_end ) )
                        {
                            p_vol_end = p_end;
                        }
                    }
                    else
                    {
                        p_vol_end = NULL;
                    }
                }
                else if( ( p_vol_begin[3] & ~VIDEO_OBJECT_LAYER_MASK ) == ( VIDEO_OBJECT_LAYER_START_CODE&0xff) )
                {
                    p_vol_end = p_vol_begin + 4;
                    if( m4v_FindStartCode( &p_vol_end, p_end ) )
                    {
                        p_vol_end = p_end;
                    }
                }

                if( p_vol_end != NULL && p_vol_begin < p_vol_end )
                {
                    BITMAPINFOHEADER *p_bih;

                    p_pack->i_vol = p_vol_end - p_vol_begin;
                    msg_Dbg( p_pack->p_fifo, "Reopening output" );

                    p_pack->p_vol = malloc( p_pack->i_vol );
                    memcpy( p_pack->p_vol, p_vol_begin, p_pack->i_vol );

                    sout_InputDelete( p_pack->p_sout_input );

                    p_pack->output_format.i_cat = VIDEO_ES;
                    p_pack->output_format.i_fourcc = VLC_FOURCC( 'm', 'p', '4', 'v' );
                    p_pack->output_format.p_format =
                        (void*)p_bih = malloc( sizeof( BITMAPINFOHEADER ) + p_pack->i_vol);

                    p_bih->biSize = sizeof( BITMAPINFOHEADER ) + p_pack->i_vol;
                    p_bih->biWidth  = 0;
                    p_bih->biHeight = 0;
                    p_bih->biPlanes = 1;
                    p_bih->biBitCount = 24;
                    p_bih->biCompression = VLC_FOURCC( 'd', 'i', 'v', 'x' );
                    p_bih->biSizeImage = 0;
                    p_bih->biXPelsPerMeter = 0;
                    p_bih->biYPelsPerMeter = 0;
                    p_bih->biClrUsed = 0;
                    p_bih->biClrImportant = 0;
                    memcpy( &p_bih[1], p_pack->p_vol, p_pack->i_vol );

                    p_pack->p_sout_input =
                        sout_InputNew( p_pack->p_fifo,
                                       &p_pack->output_format );
                    if( !p_pack->p_sout_input )
                    {
                        p_pack->p_fifo->b_error = 1;
                        return;
                    }

                    break;
                }
                else
                {
                    p_vol_begin += 4;
                }
            }
        }

        input_ShowPES( p_pack->p_fifo, &p_pes_next );
        if( p_pes_next )
        {
            mtime_t i_gap;

            i_gap = p_pes_next->i_pts - p_pes->i_pts;
#if 0
            if( i_gap > 1000000 / 4 )  // too littl fps < 4 is no sense
            {
                i_gap = 1000000 / 25;
                p_pack->i_pts_start =
                    - ( p_pes->i_pts - p_pack->i_pts_start ) + p_pes_next->i_pts - i_gap;

            }
            else if( i_gap < 0 )
            {
                p_pack->i_pts_start =
                    ( p_pes->i_pts - p_pack->i_pts_start ) + p_pes_next->i_pts;
                i_gap = 0;
            }
            if( i_gap < 0 )
            {
                msg_Dbg( p_pack->p_fifo, "pts:%lld next_pts:%lld", p_pes->i_pts, p_pes_next->i_pts );
                /* work around for seek */
                p_pack->i_pts_start -= i_gap;
            }

//            msg_Dbg( p_pack->p_fifo, "gap %lld date %lld next diff %lld", i_gap, p_pes->i_pts,  p_pes_next->i_pts-p_pack->i_pts_start );
#endif
            p_sout_buffer->i_length = i_gap;
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

