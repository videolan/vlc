/*****************************************************************************
 * ts.c
 *****************************************************************************
 * Copyright (C) 2001, 2002 VideoLAN
 * $Id: ts.c,v 1.1 2002/12/14 21:32:41 fenrir Exp $
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

typedef struct ts_stream_s
{
    int             i_pid;
    int             i_stream_type;
    int             i_stream_id;
    int             i_continuity_counter;
} ts_stream_t;

typedef struct sout_mux_s
{
    int             i_pcr_pid;
    int             i_stream_id_mpga;
    int             i_stream_id_mpgv;
    int             i_stream_id_a52;

    int             i_audio_bound;
    int             i_video_bound;

    int             i_pid_free; // first usable pid

    int             i_pat_version_number;
    ts_stream_t     pat;

    int             i_pmt_version_number;
    ts_stream_t     pmt;        // Up to now only one program

    int             i_ts_packet;// To known when to put pat/mpt
} sout_mux_t;


/*****************************************************************************
 * Exported prototypes
 *****************************************************************************/
static int     Open   ( vlc_object_t * );
static void    Close  ( vlc_object_t * );

static int     AddStream( sout_instance_t *, sout_input_t * );
static int     DelStream( sout_instance_t *, sout_input_t * );
static int     Mux      ( sout_instance_t * );

/* Reserve a pid and return it */
static int     AllocatePID( sout_mux_t *p_mux )
{
    return( ++p_mux->i_pid_free );
}

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( _("TS muxer") );
    set_capability( "sout mux", 100 );
    add_shortcut( "ts" );
    set_callbacks( Open, Close );
vlc_module_end();

/*****************************************************************************
 * Open:
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    sout_instance_t     *p_sout = (sout_instance_t*)p_this;
    sout_mux_t          *p_mux;

    msg_Info( p_sout, "Open" );

    p_mux = malloc( sizeof( sout_mux_t ) );

    p_sout->pf_mux_addstream = AddStream;
    p_sout->pf_mux_delstream = DelStream;
    p_sout->pf_mux           = Mux;
    p_sout->p_mux_data       = (void*)p_mux;

    p_mux->i_stream_id_mpga = 0xc0;
    p_mux->i_stream_id_a52  = 0x80;
    p_mux->i_stream_id_mpgv = 0xe0;

    p_mux->i_audio_bound = 0;
    p_mux->i_video_bound = 0;

    p_mux->i_pat_version_number = 0;
    p_mux->pat.i_pid = 0;
    p_mux->pat.i_continuity_counter = 0;

    p_mux->i_pmt_version_number = 0;
    p_mux->pmt.i_pid = 0x10;
    p_mux->pmt.i_continuity_counter = 0;

    p_mux->i_pid_free = 0x11;
    p_mux->i_pcr_pid = 0x1fff;
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close:
 *****************************************************************************/

static void Close( vlc_object_t * p_this )
{
    sout_instance_t     *p_sout = (sout_instance_t*)p_this;
    sout_mux_t          *p_mux = (sout_mux_t*)p_sout->p_mux_data;

    msg_Info( p_sout, "Close" );

    free( p_mux );
    p_sout->p_mux_data = NULL;
}


static int AddStream( sout_instance_t *p_sout, sout_input_t *p_input )
{
    sout_mux_t          *p_mux = (sout_mux_t*)p_sout->p_mux_data;
    ts_stream_t         *p_stream;

    msg_Dbg( p_sout, "adding input" );
    p_input->p_mux_data = (void*)p_stream = malloc( sizeof( ts_stream_t ) );

    p_stream->i_pid = AllocatePID( p_mux );
    if( p_mux->i_pcr_pid == 0x1fff )
    {
        p_mux->i_pcr_pid = p_stream->i_pid;
    }
    p_stream->i_continuity_counter = 0;

    switch( p_input->input_format.i_cat )
    {
        case VIDEO_ES:
            switch( p_input->input_format.i_fourcc )
            {
                case VLC_FOURCC( 'm', 'p','g', 'v' ):
                    p_stream->i_stream_type = 0x02;
                    p_stream->i_stream_id = p_mux->i_stream_id_mpgv;
                    p_mux->i_stream_id_mpgv++;
                    break;
                case VLC_FOURCC( 'm', 'p','4', 'v' ):
                    p_stream->i_stream_type = 0x10;
                    p_stream->i_stream_id = 0xfa;
                    break;
                default:
                    return( -1 );
            }
            p_mux->i_video_bound++;

            break;
        case AUDIO_ES:
            switch( p_input->input_format.i_fourcc )
            {
                case VLC_FOURCC( 'a', '5','2', ' ' ):
                case VLC_FOURCC( 'a', '5','2', 'b' ):
                    p_stream->i_stream_type = 0x81;
                    p_stream->i_stream_id = p_mux->i_stream_id_a52;
                    p_mux->i_stream_id_a52++;
                    break;
#if 0
                case VLC_FOURCC( 'm', 'p','4', 'a' ):
                    p_stream->i_stream_type = 0x11;
                    p_stream->i_stream_id = 0xfa;
                    p_mux->i_stream_id_mp4a++;
                    break;
#endif
                case VLC_FOURCC( 'm', 'p','g', 'a' ):
                    p_stream->i_stream_type = 0x04;
                    p_stream->i_stream_id = p_mux->i_stream_id_mpga;
                    p_mux->i_stream_id_mpga++;
                    break;
                default:
                    return( -1 );
            }
            p_mux->i_audio_bound++;
            break;
        default:
            return( -1 );
    }

    p_mux->i_ts_packet = 0; // force pat/pmt recreation
    p_mux->i_pat_version_number++; p_mux->i_pat_version_number %= 32;
    p_mux->i_pmt_version_number++; p_mux->i_pmt_version_number %= 32;

    return( 0 );
}

static int DelStream( sout_instance_t *p_sout, sout_input_t *p_input )
{
    sout_mux_t          *p_mux = (sout_mux_t*)p_sout->p_mux_data;

    msg_Dbg( p_sout, "removing input" );
    p_mux->i_ts_packet = 0; // force pat/pmt recreation
    p_mux->i_pat_version_number++; p_mux->i_pat_version_number %= 32;
    p_mux->i_pmt_version_number++; p_mux->i_pmt_version_number %= 32;

    return( 0 );
}



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

static int PEStoTS( sout_instance_t *p_sout,
                    sout_buffer_t **pp_ts, sout_buffer_t *p_pes,
                    ts_stream_t *p_stream )
{
    int i_size;
    sout_buffer_t *p_last_buffer = NULL;
    uint8_t       *p_data;
    int i_first;
    mtime_t       i_dts;
    int         b_new_pes;
    /* get PES total size */
    i_size = p_pes->i_size;
    p_data = p_pes->p_buffer;

    if( p_pes->i_dts == 0 && p_pes->i_length > 0 )
    {
        i_dts = 1; // XXX <french> kludge immonde </french>
    }
    else
    {
        i_dts = p_pes->i_dts;
    }

    for( i_first = 1, b_new_pes = 1; p_pes != NULL; )
    {
        int           i_adaptation_field;
        int           i_payload;
        int           i_copy;
        bits_buffer_t bits;
        sout_buffer_t *p_ts;

        p_ts = sout_BufferNew( p_sout, 188 );

        p_ts->i_pts = 0;
        p_ts->i_dts = i_dts;


        i_payload = 184 - ( i_first && i_dts > 0 ? 8 : 0 );
        i_copy = __MIN( i_size, i_payload );

        i_adaptation_field = ( ( i_first && i_dts > 0 ) || 
                               i_size < i_payload ) ? 1 : 0;

        /* write headers */
        bits_initwrite( &bits, 188, p_ts->p_buffer );
        bits_write( &bits, 8, 0x47 ); /* sync byte */
        bits_write( &bits, 1, 0 ); /* transport_error_indicator */
        bits_write( &bits, 1, b_new_pes ? 1 : 0 ); /* payload_unit_start */
        b_new_pes = 0;
        bits_write( &bits, 1, 0 ); /* transport_priority */
        bits_write( &bits, 13, p_stream->i_pid );
        bits_write( &bits, 2, 0 ); /* transport_scrambling_control */
        bits_write( &bits, 2, ( i_adaptation_field ? 0x03 : 0x01 ) );

        bits_write( &bits, 4, /* continuity_counter */
                    p_stream->i_continuity_counter );
        p_stream->i_continuity_counter++;
        p_stream->i_continuity_counter %= 16;
        if( i_adaptation_field )
        {
            int i;
            int i_stuffing;

            if( i_first && i_dts > 0 )
            {
                i_stuffing = i_payload - i_copy;
                bits_write( &bits, 8, 7 + i_stuffing );
                bits_write( &bits,  8, 0x10 ); /* various flags */
                bits_write( &bits, 33, i_dts * 9 / 100);
                bits_write( &bits,  6, 0 );
                bits_write( &bits,  9, 0 );
                i_dts = 0; /* XXX set dts only for first ts packet */
            }
            else
            {
                i_stuffing = i_payload - i_copy;
                bits_write( &bits, 8, i_stuffing - 1);
                if( i_stuffing - 1 > 0 )
                {
                    bits_write( &bits, 8, 0 );
                }
                i_stuffing -= 2;
            }

            /* put stuffing */
            for( i = 0; i < i_stuffing; i++ )
            {
                bits_write( &bits, 8, 0xff );
            }
        }
        /* copy payload */
        memcpy( p_ts->p_buffer + bits.i_data,
                p_data,
                i_copy );
        p_data += i_copy;
        i_size -= i_copy;

        /* chain buffers */
        if( i_first )
        {
            *pp_ts        = p_ts;
            p_last_buffer = p_ts;
        }
        else
        {
            p_last_buffer->p_next = p_ts;
            p_last_buffer         = p_ts;
        }
        i_first = 0;

        if( i_size <= 0 )
        {
            sout_buffer_t *p_next;

            p_next = p_pes->p_next;
            p_pes->p_next = NULL;
            sout_BufferDelete( p_sout, p_pes );
            p_pes = p_next;
            b_new_pes = 1;
            if( p_pes )
            {
                i_size = p_pes->i_size;
                p_data = p_pes->p_buffer;
            }
            else
            {
                break;
            }
        }
    }

    return 0;
}

static int GetPAT( sout_instance_t *p_sout,
                   sout_buffer_t **pp_ts )
{
    sout_mux_t          *p_mux = (sout_mux_t*)p_sout->p_mux_data;
    sout_buffer_t       *p_pat;
    bits_buffer_t bits;

    p_pat = sout_BufferNew( p_sout, 1024 );

    p_pat->i_pts = 0;
    p_pat->i_dts = 0;
    p_pat->i_length = 0;

    bits_initwrite( &bits, 1024, p_pat->p_buffer );

    bits_write( &bits, 8, 0 );      // pointer
    bits_write( &bits, 8, 0x00 );   // table id
    bits_write( &bits, 1,  1 );     // section_syntax_indicator
    bits_write( &bits, 1,  0 );     // 0
    bits_write( &bits, 2,  0 );     // reserved FIXME
    bits_write( &bits, 12, 13 );    // XXX for one program only XXX 
    bits_write( &bits, 16, 0x01 );  // FIXME stream id
    bits_write( &bits, 2,  0 );     //  FIXME
    bits_write( &bits, 5,  p_mux->i_pat_version_number );
    bits_write( &bits, 1,  1 );     // current_next_indicator
    bits_write( &bits, 8,  0 );     // section number
    bits_write( &bits, 8,  0 );     // last section number

    bits_write( &bits, 16, 1 );     // program number
    bits_write( &bits,  3, 0 );     // reserved
    bits_write( &bits, 13, p_mux->pmt.i_pid );  // program map pid

    bits_write( &bits, 32, 0 );     // FIXME FIXME FIXME

    p_pat->i_size = bits.i_data;

    return( PEStoTS( p_sout, pp_ts, p_pat, &p_mux->pat ) );
}

static int GetPMT( sout_instance_t *p_sout,
                   sout_buffer_t **pp_ts )
{
    sout_mux_t          *p_mux = (sout_mux_t*)p_sout->p_mux_data;
    sout_buffer_t       *p_pmt;
    bits_buffer_t bits;
    int           i_stream;

    p_pmt = sout_BufferNew( p_sout, 1024 );

    p_pmt->i_pts = 0;
    p_pmt->i_dts = 0;
    p_pmt->i_length = 0;

    bits_initwrite( &bits, 1024, p_pmt->p_buffer );

    bits_write( &bits, 8, 0 );      // pointer
    bits_write( &bits, 8, 0x02 );   // table id
    bits_write( &bits, 1,  1 );     // section_syntax_indicator
    bits_write( &bits, 1,  0 );     // 0
    bits_write( &bits, 2,  0 );     // reserved FIXME
    bits_write( &bits, 12, 13 + 5 * p_sout->i_nb_inputs );
    bits_write( &bits, 16, 1 );     // FIXME program number
    bits_write( &bits, 2,  0 );     //  FIXME
    bits_write( &bits, 5,  p_mux->i_pmt_version_number );
    bits_write( &bits, 1,  1 );     // current_next_indicator
    bits_write( &bits, 8,  0 );     // section number
    bits_write( &bits, 8,  0 );     // last section number

    bits_write( &bits,  3, 0 );     // reserved

    bits_write( &bits, 13, p_mux->i_pcr_pid );     //  FIXME FXIME PCR_PID FIXME
    bits_write( &bits,  4, 0 );     // reserved FIXME

    bits_write( &bits, 12, 0 );    // program info len FIXME

    for( i_stream = 0; i_stream < p_sout->i_nb_inputs; i_stream++ )
    {
        ts_stream_t *p_stream;

        p_stream = (ts_stream_t*)p_sout->pp_inputs[i_stream]->p_mux_data;

        bits_write( &bits,  8, p_stream->i_stream_type ); // stream_type
        bits_write( &bits,  3, 0 );                 // reserved
        bits_write( &bits, 13, p_stream->i_pid );   // es pid
        bits_write( &bits,  4, 0 );                 //reserved
        bits_write( &bits, 12, 0 );                 // es info len FIXME
    }

    bits_write( &bits, 32, 0 );     // FIXME FIXME FIXME

    p_pmt->i_size = bits.i_data;

    return( PEStoTS( p_sout, pp_ts, p_pmt, &p_mux->pmt ) );

}

static void SetTSDate( sout_buffer_t *p_ts, mtime_t i_dts, mtime_t i_length )
{
    int i_count;
    sout_buffer_t *p_tmp;
    mtime_t i_delta;

    for( p_tmp = p_ts, i_count = 0; p_tmp != NULL; p_tmp = p_tmp->p_next )
    {
        i_count++;
    }
    i_delta = i_length / i_count;

    for( p_tmp = p_ts; p_tmp != NULL; p_tmp = p_tmp->p_next )
    {
        p_tmp->i_dts    = i_dts;
        p_tmp->i_length = i_delta;

        i_dts += i_delta;
    }
}

static int Mux( sout_instance_t *p_sout )
{
    sout_mux_t          *p_mux = (sout_mux_t*)p_sout->p_mux_data;
    mtime_t i_dts;
    int     i_stream;

    sout_buffer_t *p_pat, *p_pmt, *p_ts;

    for( ;; )
    {
        mtime_t i_dts, i_length;

        sout_input_t *p_input;
        ts_stream_t *p_stream;
        sout_fifo_t  *p_fifo;
        sout_buffer_t *p_data;

        if( MuxGetStream( p_sout, &i_stream, &i_dts ) < 0 )
        {
            return( 0 );
        }

        p_input = p_sout->pp_inputs[i_stream];
        p_fifo = p_input->p_fifo;
        p_stream = (ts_stream_t*)p_input->p_mux_data;

        p_data   = sout_FifoGet( p_fifo );
        i_dts    = p_data->i_dts;
        i_length = p_data->i_length;

        EStoPES( p_sout, &p_data, p_data, p_stream->i_stream_id, 1);
        PEStoTS( p_sout, &p_data, p_data, p_stream );

        if( p_mux->i_ts_packet % 30 == 0 )
        {
            /* create pat/pmt */
            GetPAT( p_sout, &p_pat );
            GetPMT( p_sout, &p_pmt );

            p_ts = p_pat;
            p_pat->p_next = p_pmt;
            p_pmt->p_next = p_data;
        }
        else
        {
            p_pat = NULL;
            p_pmt = NULL;
            p_ts = p_data;
        }

        p_mux->i_ts_packet++;

        SetTSDate( p_ts, i_dts, i_length );

        p_sout->pf_write( p_sout, p_ts );
    }

    return( 0 );
}


