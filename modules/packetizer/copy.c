/*****************************************************************************
 * copy.c
 *****************************************************************************
 * Copyright (C) 2001, 2002 VideoLAN
 * $Id: copy.c,v 1.12 2003/07/31 19:02:23 fenrir Exp $
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
#include <stdlib.h>                                      /* malloc(), free() */

#include <vlc/vlc.h>
#include <vlc/decoder.h>
#include <vlc/input.h>
#include <vlc/sout.h>

#include "codecs.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open    ( vlc_object_t * );

vlc_module_begin();
    set_description( _("Copy packetizer") );
    set_capability( "packetizer", 1 );
    set_callbacks( Open, NULL );
vlc_module_end();

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Run         ( decoder_fifo_t * );

typedef struct packetizer_thread_s
{
    /* Input properties */
    decoder_fifo_t          *p_fifo;

    /* Output properties */
    sout_packetizer_input_t *p_sout_input;
    sout_format_t           output_format;

    void                    (*pf_packetize)( struct packetizer_thread_s * );

} packetizer_thread_t;

static int  Init        ( packetizer_thread_t * );
static void PacketizeStd( packetizer_thread_t * );
static void PacketizeSPU( packetizer_thread_t * );
static void End         ( packetizer_thread_t * );


static void AppendPEStoSoutBuffer( sout_instance_t *,sout_buffer_t **,pes_packet_t *);
static void input_ShowPES( decoder_fifo_t *p_fifo, pes_packet_t **pp_pes );

/*****************************************************************************
 * Open: probe the packetizer and return score
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
 * Run: this function is called just after the thread is created
 *****************************************************************************/
static int Run( decoder_fifo_t *p_fifo )
{
    packetizer_thread_t *p_pack;

    msg_Dbg( p_fifo, "Running copy packetizer (fcc=%4.4s)", (char*)&p_fifo->i_fourcc );

    p_pack = malloc( sizeof( packetizer_thread_t ) );
    memset( p_pack, 0, sizeof( packetizer_thread_t ) );

    p_pack->p_fifo = p_fifo;

    if( Init( p_pack ) )
    {
        DecoderError( p_fifo );
        return VLC_EGENERIC;
    }

    while( !p_pack->p_fifo->b_die && !p_pack->p_fifo->b_error )
    {
        p_pack->pf_packetize( p_pack );
    }

    if( p_pack->p_fifo->b_error )
    {
        DecoderError( p_pack->p_fifo );
    }

    End( p_pack );

    return( p_pack->p_fifo->b_error ? VLC_EGENERIC : VLC_SUCCESS );
}

/*****************************************************************************
 * Init: initialize data before entering main loop
 *****************************************************************************/
static int Init( packetizer_thread_t *p_pack )
{

    p_pack->pf_packetize = PacketizeStd;

    switch( p_pack->p_fifo->i_fourcc )
    {
        /* video */
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
        case VLC_FOURCC( 'S', 'V', 'Q', '1' ):
            p_pack->output_format.i_fourcc = VLC_FOURCC( 'S', 'V', 'Q', '1' );
            p_pack->output_format.i_cat = VIDEO_ES;
            break;
        case VLC_FOURCC( 'S', 'V', 'Q', '3' ):
            p_pack->output_format.i_fourcc = VLC_FOURCC( 'S', 'V', 'Q', '3' );
            p_pack->output_format.i_cat = VIDEO_ES;
            break;

        case VLC_FOURCC( 'I', '4', '2', '0' ):
            p_pack->output_format.i_fourcc = VLC_FOURCC( 'I', '4', '2', '0' );
            p_pack->output_format.i_cat = VIDEO_ES;
            break;
        case VLC_FOURCC( 'I', '4', '2', '2' ):
            p_pack->output_format.i_fourcc = VLC_FOURCC( 'I', '4', '2', '2' );
            p_pack->output_format.i_cat = VIDEO_ES;
            break;
        case VLC_FOURCC( 'R', 'V', '1', '5' ):
            p_pack->output_format.i_fourcc = VLC_FOURCC( 'R', 'V', '1', '5' );
            p_pack->output_format.i_cat = VIDEO_ES;
            break;
        case VLC_FOURCC( 'R', 'V', '1', '6' ):
            p_pack->output_format.i_fourcc = VLC_FOURCC( 'R', 'V', '1', '6' );
            p_pack->output_format.i_cat = VIDEO_ES;
            break;
        case VLC_FOURCC( 'R', 'V', '2', '4' ):
            p_pack->output_format.i_fourcc = VLC_FOURCC( 'R', 'V', '2', '4' );
            p_pack->output_format.i_cat = VIDEO_ES;
            break;
        case VLC_FOURCC( 'R', 'V', '3', '2' ):
            p_pack->output_format.i_fourcc = VLC_FOURCC( 'R', 'V', '3', '2' );
            p_pack->output_format.i_cat = VIDEO_ES;
            break;
        case VLC_FOURCC( 'G', 'R', 'E', 'Y' ):
            p_pack->output_format.i_fourcc = VLC_FOURCC( 'G', 'R', 'E', 'Y' );
            p_pack->output_format.i_cat = VIDEO_ES;
            break;

        /* audio */
        case VLC_FOURCC( 'm', 'p', 'g', 'a' ):
            p_pack->output_format.i_fourcc = VLC_FOURCC( 'm', 'p', 'g', 'a' );
            p_pack->output_format.i_cat = AUDIO_ES;
            break;
        case VLC_FOURCC( 'w', 'm', 'a', '1' ):
            p_pack->output_format.i_fourcc = VLC_FOURCC( 'w', 'm', 'a', '1' );
            p_pack->output_format.i_cat = AUDIO_ES;
            break;
        case VLC_FOURCC( 'w', 'm', 'a', '2' ):
            p_pack->output_format.i_fourcc = VLC_FOURCC( 'w', 'm', 'a', '2' );
            p_pack->output_format.i_cat = AUDIO_ES;
            break;
        case VLC_FOURCC( 'a', 'r', 'a', 'w' ):
        {
            WAVEFORMATEX *p_wf = (WAVEFORMATEX*)p_pack->p_fifo->p_waveformatex;
            if( p_wf )
            {
                switch( ( p_wf->wBitsPerSample + 7 ) / 8 )
                {
                    case 1:
                        p_pack->output_format.i_fourcc = VLC_FOURCC('u','8',' ',' ');
                        break;
                    case 2:
                        p_pack->output_format.i_fourcc = VLC_FOURCC('s','1','6','l');
                        break;
                    case 3:
                        p_pack->output_format.i_fourcc = VLC_FOURCC('s','2','4','l');
                        break;
                    case 4:
                        p_pack->output_format.i_fourcc = VLC_FOURCC('s','3','2','l');
                        break;
                    default:
                        msg_Err( p_pack->p_fifo, "unknown raw audio sample size !!" );
                        return VLC_EGENERIC;
                }
            }
            else
            {
                msg_Err( p_pack->p_fifo, "unknown raw audio sample size !!" );
                return VLC_EGENERIC;
            }
            p_pack->output_format.i_cat = AUDIO_ES;
            break;
        }
        case VLC_FOURCC( 't', 'w', 'o', 's' ):
        {
            WAVEFORMATEX *p_wf = (WAVEFORMATEX*)p_pack->p_fifo->p_waveformatex;
            if( p_wf )
            {
                switch( ( p_wf->wBitsPerSample + 7 ) / 8 )
                {
                    case 1:
                        p_pack->output_format.i_fourcc = VLC_FOURCC('s','8',' ',' ');
                        break;
                    case 2:
                        p_pack->output_format.i_fourcc = VLC_FOURCC('s','1','6','b');
                        break;
                    case 3:
                        p_pack->output_format.i_fourcc = VLC_FOURCC('s','2','4','b');
                        break;
                    case 4:
                        p_pack->output_format.i_fourcc = VLC_FOURCC('s','3','2','b');
                        break;
                    default:
                        msg_Err( p_pack->p_fifo, "unknown raw audio sample size !!" );
                        return VLC_EGENERIC;
                }
            }
            else
            {
                msg_Err( p_pack->p_fifo, "unknown raw audio sample size !!" );
                return VLC_EGENERIC;
            }
            p_pack->output_format.i_cat = AUDIO_ES;
            break;
        }
        case VLC_FOURCC( 's', 'o', 'w', 't' ):
        {
            WAVEFORMATEX *p_wf = (WAVEFORMATEX*)p_pack->p_fifo->p_waveformatex;
            if( p_wf )
            {
                switch( ( p_wf->wBitsPerSample + 7 ) / 8 )
                {
                    case 1:
                        p_pack->output_format.i_fourcc = VLC_FOURCC('s','8',' ',' ');
                        break;
                    case 2:
                        p_pack->output_format.i_fourcc = VLC_FOURCC('s','1','6','l');
                        break;
                    case 3:
                        p_pack->output_format.i_fourcc = VLC_FOURCC('s','2','4','l');
                        break;
                    case 4:
                        p_pack->output_format.i_fourcc = VLC_FOURCC('s','3','2','l');
                        break;
                    default:
                        msg_Err( p_pack->p_fifo, "unknown raw audio sample size !!" );
                        return VLC_EGENERIC;
                }
            }
            else
            {
                msg_Err( p_pack->p_fifo, "unknown raw audio sample size !!" );
                return VLC_EGENERIC;
            }
            p_pack->output_format.i_cat = AUDIO_ES;
            break;
        }

        /* subtitles */
        case VLC_FOURCC( 's', 'p', 'u', ' ' ):  /* DVD */
        case VLC_FOURCC( 's', 'p', 'u', 'b' ):
            p_pack->output_format.i_fourcc = VLC_FOURCC( 's', 'p', 'u', ' ' );
            p_pack->output_format.i_cat = SPU_ES;
            p_pack->pf_packetize = PacketizeSPU;
            break;
        default:
            msg_Err( p_pack->p_fifo, "unknown es type !!" );
            return VLC_EGENERIC;
    }

    switch( p_pack->output_format.i_cat )
    {
        case AUDIO_ES:
            {
                WAVEFORMATEX *p_wf = (WAVEFORMATEX*)p_pack->p_fifo->p_waveformatex;
                if( p_wf )
                {
                    p_pack->output_format.i_sample_rate = p_wf->nSamplesPerSec;
                    p_pack->output_format.i_channels    = p_wf->nChannels;
                    p_pack->output_format.i_block_align = p_wf->nBlockAlign;
                    p_pack->output_format.i_bitrate     = p_wf->nAvgBytesPerSec * 8;
                    p_pack->output_format.i_extra_data  = p_wf->cbSize;
                    if( p_wf->cbSize  > 0 )
                    {
                        p_pack->output_format.p_extra_data =
                            malloc( p_pack->output_format.i_extra_data );
                        memcpy( p_pack->output_format.p_extra_data,
                                &p_wf[1],
                                p_pack->output_format.i_extra_data );
                    }
                    else
                    {
                        p_pack->output_format.p_extra_data = NULL;
                    }
                }
                else
                {
                    p_pack->output_format.i_sample_rate = 0;
                    p_pack->output_format.i_channels    = 0;
                    p_pack->output_format.i_block_align = 0;
                    p_pack->output_format.i_bitrate     = 0;
                    p_pack->output_format.i_extra_data  = 0;
                    p_pack->output_format.p_extra_data  = NULL;
                }
            }
            break;

        case VIDEO_ES:
            {
                BITMAPINFOHEADER *p_bih = (BITMAPINFOHEADER*)p_pack->p_fifo->p_bitmapinfoheader;

                p_pack->output_format.i_bitrate = 0;
                if( p_bih )
                {
                    p_pack->output_format.i_width  = p_bih->biWidth;
                    p_pack->output_format.i_height = p_bih->biHeight;
                    p_pack->output_format.i_extra_data  = p_bih->biSize - sizeof( BITMAPINFOHEADER );
                    if( p_pack->output_format.i_extra_data > 0 )
                    {
                        p_pack->output_format.p_extra_data =
                            malloc( p_pack->output_format.i_extra_data );
                        memcpy( p_pack->output_format.p_extra_data,
                                &p_bih[1],
                                p_pack->output_format.i_extra_data );
                    }
                }
                else
                {
                    p_pack->output_format.i_width  = 0;
                    p_pack->output_format.i_height = 0;
                    p_pack->output_format.i_extra_data  = 0;
                    p_pack->output_format.p_extra_data  = NULL;
                }
            }
            break;

        case SPU_ES:
            p_pack->output_format.i_extra_data  = 0;
            p_pack->output_format.p_extra_data  = NULL;
            break;

        default:
            return VLC_EGENERIC;
    }

    p_pack->p_sout_input =
        sout_InputNew( p_pack->p_fifo,
                       &p_pack->output_format );

    if( !p_pack->p_sout_input )
    {
        msg_Err( p_pack->p_fifo, "cannot add a new stream" );
        return VLC_EGENERIC;
    }

    return( VLC_SUCCESS );
}

/*****************************************************************************
 * PacketizeStd: packetize an unit (here copy a complete pes)
 *****************************************************************************/
static void PacketizeStd( packetizer_thread_t *p_pack )
{
    sout_buffer_t   *p_out = NULL;
    pes_packet_t    *p_pes;

    input_ExtractPES( p_pack->p_fifo, &p_pes );
    if( !p_pes )
    {
        p_pack->p_fifo->b_error = 1;
        return;
    }

    msg_Dbg( p_pack->p_fifo,
             "pes size:%d dts=%lld pts=%lld",
             p_pes->i_pes_size, p_pes->i_dts, p_pes->i_pts );


    if( p_pes->i_pts <= 0 )
    {
        msg_Dbg( p_pack->p_fifo, "need pts != 0" );
        input_DeletePES( p_pack->p_fifo->p_packets_mgt, p_pes );
        return;
    }

    if( p_pes->i_pes_size > 0 )
    {
        pes_packet_t    *p_next;

        AppendPEStoSoutBuffer( p_pack->p_sout_input->p_sout, &p_out, p_pes );

        input_ShowPES( p_pack->p_fifo, &p_next );
        if( p_next && p_next->i_pts > 0 )
        {
            p_out->i_length = p_next->i_pts - p_pes->i_pts;
        }

        sout_InputSendBuffer( p_pack->p_sout_input,
                               p_out );
    }

    input_DeletePES( p_pack->p_fifo->p_packets_mgt, p_pes );
}

/*****************************************************************************
 * PacketizeSPU: packetize an SPU unit (so gather all PES of one subtitle)
 *****************************************************************************/
static void PacketizeSPU( packetizer_thread_t *p_pack )
{
    sout_buffer_t   *p_out = NULL;
    pes_packet_t    *p_pes;

    int     i_spu_size = 0;

    for( ;; )
    {
        input_ExtractPES( p_pack->p_fifo, &p_pes );
        if( !p_pes )
        {
            p_pack->p_fifo->b_error = 1;
            return;
        }

        msg_Dbg( p_pack->p_fifo,
                 "pes size:%d dts=%lld pts=%lld",
                 p_pes->i_pes_size, p_pes->i_dts, p_pes->i_pts );

        if( p_out == NULL &&
            ( p_pes->i_pts <= 0 || p_pes->i_pes_size < 4 ) )
        {
            msg_Dbg( p_pack->p_fifo, "invalid starting packet (size < 4 or pts <=0)" );
            input_DeletePES( p_pack->p_fifo->p_packets_mgt, p_pes );
            return;
        }

        if( p_pes->i_pes_size > 0 )
        {
            AppendPEStoSoutBuffer( p_pack->p_sout_input->p_sout, &p_out, p_pes );

            if( i_spu_size <= 0 )
            {
                int i_rle;
                i_spu_size = ( p_out->p_buffer[0] << 8 )| p_out->p_buffer[1];
                i_rle      = ( ( p_out->p_buffer[2] << 8 )| p_out->p_buffer[3] ) - 4;

                msg_Dbg( p_pack->p_fifo, "i_spu_size=%d i_rle=%d", i_spu_size, i_rle );
                if( i_spu_size == 0 || i_rle >= i_spu_size )
                {
                    sout_BufferDelete( p_pack->p_sout_input->p_sout, p_out );
                    input_DeletePES( p_pack->p_fifo->p_packets_mgt, p_pes );
                    return;
                }
            }
        }

        input_DeletePES( p_pack->p_fifo->p_packets_mgt, p_pes );

        if( (int)p_out->i_size >= i_spu_size )
        {
            break;
        }
    }
    msg_Dbg( p_pack->p_fifo,
             "SPU packets size=%d should be %d",
             p_out->i_size, i_spu_size );

    sout_InputSendBuffer( p_pack->p_sout_input, p_out );
}


/*****************************************************************************
 * End : packetizer thread destruction
 *****************************************************************************/
static void End ( packetizer_thread_t *p_pack)
{
    if( p_pack->p_sout_input )
    {
        sout_InputDelete( p_pack->p_sout_input );
    }
    free( p_pack );
}

/*****************************************************************************
 * AppendPEStoSoutBuffer: copy/cat one pes into a sout_buffer_t.
 *****************************************************************************/
static void AppendPEStoSoutBuffer( sout_instance_t *p_sout,
                                   sout_buffer_t **pp_out,
                                   pes_packet_t *p_pes )
{
    sout_buffer_t *p_out = *pp_out;
    unsigned int  i_out;

    data_packet_t   *p_data;

    if( p_out == NULL )
    {
        i_out = 0;
        p_out = *pp_out = sout_BufferNew( p_sout, p_pes->i_pes_size );
        p_out->i_dts = p_pes->i_pts;
        p_out->i_pts = p_pes->i_pts;
    }
    else
    {
        i_out = p_out->i_size;
        sout_BufferRealloc( p_sout, p_out, i_out + p_pes->i_pes_size );
    }
    p_out->i_size = i_out + p_pes->i_pes_size;

    for( p_data = p_pes->p_first; p_data != NULL; p_data = p_data->p_next)
    {
        int i_copy;

        i_copy = __MIN( p_data->p_payload_end - p_data->p_payload_start,
                        p_out->i_size - i_out );
        if( i_copy > 0 )
        {
            memcpy( &p_out->p_buffer[i_out],
                    p_data->p_payload_start,
                    i_copy );
        }
        i_out += i_copy;
    }
    p_out->i_size = i_out;
}

/*****************************************************************************
 * input_ShowPES: Show the next PES in the fifo
 *****************************************************************************/
static void input_ShowPES( decoder_fifo_t *p_fifo, pes_packet_t **pp_pes )
{
    vlc_mutex_lock( &p_fifo->data_lock );

    if( p_fifo->p_first == NULL )
    {
        if( p_fifo->b_die )
        {
            vlc_mutex_unlock( &p_fifo->data_lock );
            *pp_pes = NULL;
            return;
        }

        /* Signal the input thread we're waiting. This is only
         * needed in case of slave clock (ES plug-in) but it won't
         * harm. */
        vlc_cond_signal( &p_fifo->data_wait );

        /* Wait for the input to tell us when we received a packet. */
        vlc_cond_wait( &p_fifo->data_wait, &p_fifo->data_lock );
    }
    *pp_pes = p_fifo->p_first;
    vlc_mutex_unlock( &p_fifo->data_lock );
}

