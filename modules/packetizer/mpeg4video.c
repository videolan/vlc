/*****************************************************************************
 * mpeg4video.c:
 *****************************************************************************
 * Copyright (C) 2001, 2002 VideoLAN
 * $Id: mpeg4video.c,v 1.2 2002/12/18 16:33:09 fenrir Exp $
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

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
typedef struct packetizer_thread_s
{
    /* Input properties */
    decoder_fifo_t          *p_fifo;
    bit_stream_t            bit_stream;

    mtime_t                 i_dts;

    /* Output properties */
    sout_input_t            *p_sout_input;
    sout_packet_format_t    output_format;


    sout_buffer_t           *p_vol;
    int                     i_vop_since_vol;
} packetizer_thread_t;

static int  Open    ( vlc_object_t * );
static int  Run     ( decoder_fifo_t * );

static int  InitThread     ( packetizer_thread_t * );
static void PacketizeThread   ( packetizer_thread_t * );
static void EndThread      ( packetizer_thread_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

vlc_module_begin();
    set_description( _("MPEG-4 packetizer") );
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
        case VLC_FOURCC( 'm', 'p', '4', 'v'):
        case VLC_FOURCC( 'D', 'I', 'V', 'X'):
        case VLC_FOURCC( 'd', 'i', 'v', 'x'):
        case VLC_FOURCC( 'X', 'V', 'I', 'D'):
        case VLC_FOURCC( 'X', 'v', 'i', 'D'):
        case VLC_FOURCC( 'x', 'v', 'i', 'd'):
        case VLC_FOURCC( 'D', 'X', '5', '0'):
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

    msg_Info( p_fifo, "Running MPEG-4 packetizer" );
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

static int CopyUntilNextStartCode( packetizer_thread_t   *p_pack,
                                   sout_buffer_t  *p_sout_buffer,
                                   int            *pi_pos )
{
    int i_copy = 0;

    do
    {
        p_sout_buffer->p_buffer[(*pi_pos)++] =
        GetBits( &p_pack->bit_stream, 8 );
        i_copy++;

        if( *pi_pos + 2048 > p_sout_buffer->i_allocated_size )
        {
            sout_BufferRealloc( p_pack->p_sout_input->p_sout,
                                p_sout_buffer,
                                p_sout_buffer->i_allocated_size + 50 * 1024);
        }

    } while( ShowBits( &p_pack->bit_stream, 24 ) != 0x01 &&
             !p_pack->p_fifo->b_die && !p_pack->p_fifo->b_error );

    return( i_copy );
}

static int sout_BufferAddMem( sout_instance_t *p_sout,
                              sout_buffer_t   *p_buffer,
                              int             i_mem,
                              uint8_t         *p_mem )
{
    if( p_buffer->i_size + i_mem >= p_buffer->i_allocated_size )
    {
        sout_BufferRealloc( p_sout,
                            p_buffer,
                            p_buffer->i_size + i_mem + 1024 );
    }
    memcpy( p_buffer->p_buffer + p_buffer->i_size, p_mem, i_mem );
    p_buffer->i_size += i_mem;

    return( i_mem );
}

/*****************************************************************************
 * InitThread: initialize data before entering main loop
 *****************************************************************************/

static int InitThread( packetizer_thread_t *p_pack )
{
    p_pack->i_dts = 0;
    p_pack->p_vol = NULL;
    p_pack->i_vop_since_vol = 0;
    p_pack->output_format.i_cat = VIDEO_ES;
    p_pack->output_format.i_fourcc = VLC_FOURCC( 'm', 'p', '4', 'v' );

    if( InitBitstream( &p_pack->bit_stream, p_pack->p_fifo,
                       NULL, NULL ) != VLC_SUCCESS )
    {
        msg_Err( p_pack->p_fifo, "cannot initialize bitstream" );
        return -1;
    }

    p_pack->p_sout_input =
        sout_InputNew( p_pack->p_fifo,
                       &p_pack->output_format );

    if( !p_pack->p_sout_input )
    {
        msg_Err( p_pack->p_fifo,
                 "cannot add a new stream" );
        return( -1 );
    }

    return( 0 );
}

/*****************************************************************************
 * PacketizeThread: packetize an unit (here copy a complete pes)
 *****************************************************************************/
static void PacketizeThread( packetizer_thread_t *p_pack )
{
    sout_instance_t *p_sout = p_pack->p_sout_input->p_sout;
    sout_buffer_t   *p_frame;

    uint32_t        i_startcode;

    /* Idea: Copy until a vop has been found
     *       Once a videoobject & videoobjectlayer has been found we save it
     */

    p_frame = sout_BufferNew( p_sout, 20*1024 );    // FIXME
    p_frame->i_size = 0;

    for( ;; )
    {
        while( ( ( i_startcode = ShowBits( &p_pack->bit_stream, 32 ) )&0xffffff00 ) != 0x00000100 )
        {
            RemoveBits( &p_pack->bit_stream, 8 );
        }

        if( i_startcode == VISUAL_OBJECT_SEQUENCE_START_CODE )
        {
            msg_Dbg( p_pack->p_fifo, "<visuel_object_sequence>" );
            RemoveBits32( &p_pack->bit_stream );
        }
        else if( i_startcode == VISUAL_OBJECT_SEQUENCE_END_CODE )
        {
            msg_Dbg( p_pack->p_fifo, "</visuel_object_sequence>" );
            RemoveBits32( &p_pack->bit_stream );
        }
        else
        {
            msg_Dbg( p_pack->p_fifo, "start code:0x%8.8x", i_startcode );

            if( ( i_startcode & ~VIDEO_OBJECT_MASK ) == VIDEO_OBJECT_START_CODE )
            {
                msg_Dbg( p_pack->p_fifo, "<video_object>" );
                CopyUntilNextStartCode( p_pack, p_frame, &p_frame->i_size );
            }
            else if( ( i_startcode & ~VIDEO_OBJECT_LAYER_MASK ) == VIDEO_OBJECT_LAYER_START_CODE )
            {
                /* first: save it */
                if( p_pack->p_vol == NULL )
                {
                    p_pack->p_vol = sout_BufferNew( p_sout, 1024 );
                }
                p_pack->p_vol->i_size = 0;
                CopyUntilNextStartCode( p_pack, p_pack->p_vol, &p_pack->p_vol->i_size );
                p_pack->i_vop_since_vol = 0;

                /* then: add it to p_frame */
                sout_BufferAddMem( p_sout, p_frame,
                                   p_pack->p_vol->i_size,
                                   p_pack->p_vol->p_buffer );
            }
            else if( i_startcode == GROUP_OF_VOP_START_CODE )
            {
                msg_Dbg( p_pack->p_fifo, "<group_of_vop>" );
#if 0
                if( p_pack->p_vol && p_pack->i_vop_since_vol > 100 ) // FIXME
                {
                    sout_BufferAddMem( p_sout, p_frame,
                                       p_pack->p_vol->i_size,
                                       p_pack->p_vol->p_buffer );
                    p_pack->i_vop_since_vol = 0;
                }
#endif
                CopyUntilNextStartCode( p_pack, p_frame, &p_frame->i_size );
            }
            else if( i_startcode == VOP_START_CODE )
            {
                msg_Dbg( p_pack->p_fifo, "<vop>" );
#if 1
                if( p_pack->p_vol && p_pack->i_vop_since_vol > 30 ) // FIXME
                {
                    sout_BufferAddMem( p_sout, p_frame,
                                       p_pack->p_vol->i_size,
                                       p_pack->p_vol->p_buffer );
                    p_pack->i_vop_since_vol = 0;
                }
#endif
                CopyUntilNextStartCode( p_pack, p_frame, &p_frame->i_size );
                p_pack->i_vop_since_vol++;
                break;
            }
            else
            {
                msg_Dbg( p_pack->p_fifo, "unknown start code" );
                CopyUntilNextStartCode( p_pack, p_frame, &p_frame->i_size );
            }

        }
    }

    p_frame->i_length = 1000000 / 25;
    p_frame->i_bitrate= 0;
    p_frame->i_dts = p_pack->i_dts;
    p_frame->i_pts = p_pack->i_dts;

    p_pack->i_dts += 1000000 / 25;
    sout_InputSendBuffer( p_pack->p_sout_input, p_frame );
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

