/*****************************************************************************
 * ps.c
 *****************************************************************************
 * Copyright (C) 2001, 2002 VideoLAN
 * $Id: ps.c,v 1.9 2003/02/24 14:14:43 fenrir Exp $
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Eric Petit <titer@videolan.org>
 *
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
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#include <vlc/vlc.h>
#include <vlc/input.h>
#include <vlc/sout.h>

#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#elif defined( _MSC_VER ) && defined( _WIN32 ) && !defined( UNDER_CE )
#   include <io.h>
#endif

#include "codecs.h"
#include "bits.h"
#include "pes.h"

/*****************************************************************************
 * Exported prototypes
 *****************************************************************************/
static int     Open   ( vlc_object_t * );
static void    Close  ( vlc_object_t * );

static int Capability(sout_instance_t *, int, void *, void * );
static int AddStream( sout_instance_t *, sout_input_t * );
static int DelStream( sout_instance_t *, sout_input_t * );
static int Mux      ( sout_instance_t * );

static void SetWBE ( uint8_t *p, uint16_t v )
{
    p[0] = ( v >> 8 )&0xff;
    p[1] = v&0xff;
}
static void SetDWBE( uint8_t *p, uint32_t v )
{
    SetWBE( p,    ( v >> 16 )&0xffff );
    SetWBE( p + 2,  v & 0xffff );
}
#define ADD_DWBE( p_buff, v ) \
    SetDWBE( (p_buff)->p_buffer + i_buffer, (v) ); \
    i_buffer +=4;

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( _("PS muxer") );
    set_capability( "sout mux", 50 );
    add_shortcut( "ps" );
    set_callbacks( Open, Close );
vlc_module_end();

typedef struct ps_stream_s
{
    int             i_ok;

    int             i_stream_id;

} ps_stream_t;

typedef struct sout_mux_s
{

    int         i_stream_id_mpga;
    int         i_stream_id_mpgv;
    int         i_stream_id_a52;

    int         i_audio_bound;
    int         i_video_bound;

    int         i_pes_count;

    int         i_system_header;
} sout_mux_t;

/*****************************************************************************
 * Open:
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    sout_instance_t     *p_sout = (sout_instance_t*)p_this;
    sout_mux_t          *p_mux;

    msg_Info( p_sout, "Open" );

    p_mux = malloc( sizeof( sout_mux_t ) );

    p_sout->pf_mux_capacity  = Capability;
    p_sout->pf_mux_addstream = AddStream;
    p_sout->pf_mux_delstream = DelStream;
    p_sout->pf_mux           = Mux;
    p_sout->p_mux_data       = (void*)p_mux;
    p_sout->i_mux_preheader  = 30; // really enough for a pes header

    p_mux->i_stream_id_mpga = 0xc0;
    p_mux->i_stream_id_a52  = 0x80;
    p_mux->i_stream_id_mpgv = 0xe0;
    p_mux->i_audio_bound = 0;
    p_mux->i_video_bound = 0;
    p_mux->i_system_header = 0;
    p_mux->i_pes_count = 0;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close:
 *****************************************************************************/

static void Close( vlc_object_t * p_this )
{
    sout_instance_t     *p_sout = (sout_instance_t*)p_this;
    sout_mux_t          *p_mux = (sout_mux_t*)p_sout->p_mux_data;
    sout_buffer_t       *p_end;

    msg_Info( p_sout, "Close" );

    p_end = sout_BufferNew( p_sout, 4 );
    SetDWBE( p_end->p_buffer, 0x01b9 );

    sout_AccessOutWrite( p_sout->p_access, p_end );

    free( p_mux );

    p_sout->p_mux_data = NULL;
}

static int Capability( sout_instance_t *p_sout, int i_query, void *p_args, void *p_answer )
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

static int AddStream( sout_instance_t *p_sout, sout_input_t *p_input )
{
    sout_mux_t          *p_mux = (sout_mux_t*)p_sout->p_mux_data;
    ps_stream_t         *p_stream;

    msg_Dbg( p_sout, "adding input" );
    p_input->p_mux_data = (void*)p_stream = malloc( sizeof( ps_stream_t ) );
    p_stream->i_ok = 0;
    switch( p_input->input_format.i_cat )
    {
        case VIDEO_ES:

            switch( p_input->input_format.i_fourcc )
            {
                case VLC_FOURCC( 'm', 'p', 'g', 'v' ):
                    p_stream->i_stream_id = p_mux->i_stream_id_mpgv;
                    p_mux->i_stream_id_mpgv++;
                    p_mux->i_video_bound++;
                    break;
                default:
                    return( -1 );
            }
            break;
        case AUDIO_ES:
            switch( p_input->input_format.i_fourcc )
            {
                case VLC_FOURCC( 'a', '5', '2', ' ' ):
                case VLC_FOURCC( 'a', '5', '2', 'b' ):
                    p_stream->i_stream_id = p_mux->i_stream_id_a52 | ( 0xbd << 8 );
                    p_mux->i_stream_id_a52++;
                    p_mux->i_audio_bound++;
                    break;
                case VLC_FOURCC( 'm', 'p', 'g', 'a' ):
                    p_stream->i_stream_id = p_mux->i_stream_id_mpga;
                    p_mux->i_stream_id_mpga++;
                    p_mux->i_audio_bound++;
                    break;
                default:
                    return( -1 );
            }
            break;
        default:
            return( -1 );
    }

    p_stream->i_ok = 1;
    msg_Dbg( p_sout, "adding input stream_id:0x%x [OK]", p_stream->i_stream_id );
    return( 0 );
}

static int DelStream( sout_instance_t *p_sout, sout_input_t *p_input )
{
    ps_stream_t         *p_stream =(ps_stream_t*)p_input->p_mux_data;

    msg_Dbg( p_sout, "removing input" );
    if( p_stream )
    {
        free( p_stream );
    }
    return( 0 );
}

static int MuxWritePackHeader( sout_instance_t *p_sout,
                               mtime_t         i_dts )
{
    sout_buffer_t   *p_hdr;
    bits_buffer_t   bits;
    mtime_t         i_src;

    i_src = i_dts * 9 / 100;

    p_hdr = sout_BufferNew( p_sout, 18 );
    bits_initwrite( &bits, 14, p_hdr->p_buffer );
    bits_write( &bits, 32, 0x01ba );
    bits_write( &bits, 2, 0x01 );       // FIXME ??
    bits_write( &bits, 3, ( i_src >> 30 )&0x07 );
    bits_write( &bits, 1,  1 );
    bits_write( &bits, 15, ( i_src >> 15 )&0x7fff );
    bits_write( &bits, 1,  1 );
    bits_write( &bits, 15, i_src&0x7fff );
    bits_write( &bits, 1,  1 );

    bits_write( &bits, 9,  0 ); // src extention
    bits_write( &bits, 1,  1 );

    bits_write( &bits, 22,  0/8/50); // FIXME
    bits_write( &bits, 1,  1 );
    bits_write( &bits, 1,  1 );
    bits_write( &bits, 5,  0x1f );  // FIXME reserved
    bits_write( &bits, 3,  0 );     // stuffing bytes
    p_hdr->i_size = 14;
    sout_AccessOutWrite( p_sout->p_access, p_hdr );

    return( 0 );
}

static int MuxWriteSystemHeader( sout_instance_t *p_sout )
{
    sout_mux_t          *p_mux = (sout_mux_t*)p_sout->p_mux_data;
    sout_buffer_t   *p_hdr;
    bits_buffer_t   bits;

    p_hdr = sout_BufferNew( p_sout, 12 );

    bits_initwrite( &bits, 12, p_hdr->p_buffer );
    bits_write( &bits, 32, 0x01bb );
    bits_write( &bits, 16, 12 - 6);
    bits_write( &bits, 1,  1 );
    bits_write( &bits, 22, 0 ); // FIXME rate bound
    bits_write( &bits, 1,  1 );

    bits_write( &bits, 6,  p_mux->i_audio_bound );
    bits_write( &bits, 1,  0 ); // fixed flag
    bits_write( &bits, 1,  0 ); // CSPS flag
    bits_write( &bits, 1,  0 ); // system audio lock flag
    bits_write( &bits, 1,  0 ); // system video lock flag

    bits_write( &bits, 1,  1 ); // marker bit

    bits_write( &bits, 5,  p_mux->i_video_bound );
    bits_write( &bits, 1,  0 ); // packet rate restriction flag
    bits_write( &bits, 7,  0x7f ); // reserved bits

    /* FIXME missing stream_id ... */

    sout_AccessOutWrite( p_sout->p_access, p_hdr );

    return( 0 );
}

/* return stream number to be muxed */
static int MuxGetStream( sout_instance_t *p_sout, 
                         int        *pi_stream, 
                         mtime_t    *pi_dts )
{
    mtime_t i_dts;
    int     i_stream;
    int     i;

    for( i = 0, i_dts = 0, i_stream = -1; i < p_sout->i_nb_inputs; i++ )
    {
        sout_fifo_t  *p_fifo;

        p_fifo = p_sout->pp_inputs[i]->p_fifo;

        if( p_fifo->i_depth > 1 )
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
            return( -1 ); // wait that all fifo have at least 2 packets
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


static int Mux      ( sout_instance_t *p_sout )
{
    sout_mux_t          *p_mux = (sout_mux_t*)p_sout->p_mux_data;
    mtime_t i_dts;
    int     i_stream;

    for( ;; )
    {
        sout_input_t *p_input;
        ps_stream_t *p_stream;
        sout_fifo_t  *p_fifo;
        sout_buffer_t *p_data;

        if( MuxGetStream( p_sout, &i_stream, &i_dts ) < 0 )
        {
            return( 0 );
        }

        p_input = p_sout->pp_inputs[i_stream];
        p_fifo = p_input->p_fifo;
        p_stream = (ps_stream_t*)p_input->p_mux_data;

        if( p_mux->i_pes_count % 30 == 0)
        {
            MuxWritePackHeader( p_sout, i_dts );
        }

        if( p_mux->i_pes_count % 300 == 0 )
        {
//            MuxWriteSystemHeader( p_sout );
        }

        p_data = sout_FifoGet( p_fifo );
        E_( EStoPES )( p_sout, &p_data, p_data, p_stream->i_stream_id, 1);
        sout_AccessOutWrite( p_sout->p_access, p_data );

        p_mux->i_pes_count++;

    }
    return( 0 );
}

