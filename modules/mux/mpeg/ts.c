/*****************************************************************************
 * ts.c
 *****************************************************************************
 * Copyright (C) 2001, 2002 VideoLAN
 * $Id: ts.c,v 1.4 2003/01/08 10:34:58 fenrir Exp $
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

#if defined MODULE_NAME_IS_mux_ts_dvbpsi
#       include <dvbpsi/dvbpsi.h>
#       include <dvbpsi/descriptor.h>
#       include <dvbpsi/pat.h>
#       include <dvbpsi/pmt.h>
#       include <dvbpsi/psi.h>
#       include <dvbpsi/dr.h>
#endif

typedef struct ts_stream_s
{
    int             i_pid;
    int             i_stream_type;
    int             i_stream_id;
    int             i_continuity_counter;

    /* Specific to mpeg4 in mpeg2ts */
    int             i_es_id;
    int             i_sl_predefined;

    int             i_decoder_specific_info_len;
    uint8_t         *p_decoder_specific_info;
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

    int             i_mpeg4_streams;

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

static int GetPAT( sout_instance_t *p_sout, sout_buffer_t **pp_ts );
static int GetPMT( sout_instance_t *p_sout, sout_buffer_t **pp_ts );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
#if defined MODULE_NAME_IS_mux_ts
    set_description( _("TS muxer") );
    set_capability( "sout mux", 100 );
    add_shortcut( "ts" );
#elif defined MODULE_NAME_IS_mux_ts_dvbpsi
    set_description( _("TS muxer (libdvbpsi)") );
    set_capability( "sout mux", 120 );
    add_shortcut( "ts_dvbpsi" );
#endif
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

    srand( (uint32_t)mdate() );

    p_mux->i_stream_id_mpga = 0xc0;
    p_mux->i_stream_id_a52  = 0x80;
    p_mux->i_stream_id_mpgv = 0xe0;

    p_mux->i_audio_bound = 0;
    p_mux->i_video_bound = 0;

    p_mux->i_pat_version_number = rand() % 32;
    p_mux->pat.i_pid = 0;
    p_mux->pat.i_continuity_counter = 0;

    p_mux->i_pmt_version_number = rand() % 32;
    p_mux->pmt.i_pid = 0x10;
    p_mux->pmt.i_continuity_counter = 0;

    p_mux->i_pid_free = 0x11;
    p_mux->i_pcr_pid = 0x1fff;

    p_mux->i_mpeg4_streams = 0;

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
    BITMAPINFOHEADER    *p_bih;
    WAVEFORMATEX        *p_wf;

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
                    p_mux->i_mpeg4_streams++;
                    p_stream->i_es_id = p_stream->i_pid;
                    p_stream->i_sl_predefined = 0x01;   // NULL SL header
                    break;
                default:
                    return( -1 );
            }
            p_mux->i_video_bound++;
            p_bih = (BITMAPINFOHEADER*)p_input->input_format.p_format;
            if( p_bih && p_bih->biSize > sizeof( BITMAPINFOHEADER ) )
            {
                p_stream->i_decoder_specific_info_len =
                    p_bih->biSize - sizeof( BITMAPINFOHEADER );
                p_stream->p_decoder_specific_info =
                    malloc( p_stream->i_decoder_specific_info_len );
                memcpy( p_stream->p_decoder_specific_info,
                        &p_bih[1],
                        p_stream->i_decoder_specific_info_len );
            }
            else
            {
                p_stream->p_decoder_specific_info = NULL;
                p_stream->i_decoder_specific_info_len = 0;
            }
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
                case VLC_FOURCC( 'm', 'p','4', 'a' ):
                    p_stream->i_stream_type = 0x11;
                    p_stream->i_stream_id = 0xfa;
                    p_mux->i_mpeg4_streams++;
                    p_stream->i_es_id = p_stream->i_pid;
                    p_stream->i_sl_predefined = 0x01;   // NULL SL header
                    break;
                case VLC_FOURCC( 'm', 'p','g', 'a' ):
                    p_stream->i_stream_type = 0x04;
                    p_stream->i_stream_id = p_mux->i_stream_id_mpga;
                    p_mux->i_stream_id_mpga++;
                    break;
                default:
                    return( -1 );
            }
            p_mux->i_audio_bound++;
            p_wf = (WAVEFORMATEX*)p_input->input_format.p_format;
            if( p_wf && p_wf->cbSize > 0 )
            {
                p_stream->i_decoder_specific_info_len = p_wf->cbSize;
                p_stream->p_decoder_specific_info =
                    malloc( p_stream->i_decoder_specific_info_len );
                memcpy( p_stream->p_decoder_specific_info,
                        &p_wf[1],
                        p_stream->i_decoder_specific_info_len );
            }
            else
            {
                p_stream->p_decoder_specific_info = NULL;
                p_stream->i_decoder_specific_info_len = 0;
            }
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
    ts_stream_t         *p_stream;

    msg_Dbg( p_sout, "removing input" );
    p_stream = (ts_stream_t*)p_input->p_mux_data;

    if( p_stream->p_decoder_specific_info )
    {
        free( p_stream->p_decoder_specific_info );
    }
    if( p_stream->i_stream_id == 0xfa || p_stream->i_stream_id == 0xfb )
    {
        p_mux->i_mpeg4_streams--;
    }
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
    uint8_t       *p_data;
    int i_first;
    mtime_t       i_dts;
    int         b_new_pes;

    *pp_ts = NULL;

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

        sout_BufferChain( pp_ts, p_ts );

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

        E_( EStoPES )( p_sout, &p_data, p_data, p_stream->i_stream_id, 1);
        PEStoTS( p_sout, &p_data, p_data, p_stream );

        if( p_mux->i_ts_packet % 30 == 0 )
        {
            /* create pat/pmt */
            GetPAT( p_sout, &p_pat );
            GetPMT( p_sout, &p_pmt );

            p_ts = p_pat;
            sout_BufferChain( &p_ts, p_pmt );
            sout_BufferChain( &p_ts, p_data );
        }
        else
        {
            p_ts = p_data;
        }

        p_mux->i_ts_packet++;
        SetTSDate( p_ts, i_dts, i_length );

        p_sout->pf_write( p_sout, p_ts );
    }

    return( 0 );
}


static uint32_t CalculateCRC( uint8_t *p_begin, int i_count )
{
    static uint32_t CRC32[256] =
    {
        0x00000000, 0x04c11db7, 0x09823b6e, 0x0d4326d9,
        0x130476dc, 0x17c56b6b, 0x1a864db2, 0x1e475005,
        0x2608edb8, 0x22c9f00f, 0x2f8ad6d6, 0x2b4bcb61,
        0x350c9b64, 0x31cd86d3, 0x3c8ea00a, 0x384fbdbd,
        0x4c11db70, 0x48d0c6c7, 0x4593e01e, 0x4152fda9,
        0x5f15adac, 0x5bd4b01b, 0x569796c2, 0x52568b75,
        0x6a1936c8, 0x6ed82b7f, 0x639b0da6, 0x675a1011,
        0x791d4014, 0x7ddc5da3, 0x709f7b7a, 0x745e66cd,
        0x9823b6e0, 0x9ce2ab57, 0x91a18d8e, 0x95609039,
        0x8b27c03c, 0x8fe6dd8b, 0x82a5fb52, 0x8664e6e5,
        0xbe2b5b58, 0xbaea46ef, 0xb7a96036, 0xb3687d81,
        0xad2f2d84, 0xa9ee3033, 0xa4ad16ea, 0xa06c0b5d,
        0xd4326d90, 0xd0f37027, 0xddb056fe, 0xd9714b49,
        0xc7361b4c, 0xc3f706fb, 0xceb42022, 0xca753d95,
        0xf23a8028, 0xf6fb9d9f, 0xfbb8bb46, 0xff79a6f1,
        0xe13ef6f4, 0xe5ffeb43, 0xe8bccd9a, 0xec7dd02d,
        0x34867077, 0x30476dc0, 0x3d044b19, 0x39c556ae,
        0x278206ab, 0x23431b1c, 0x2e003dc5, 0x2ac12072,
        0x128e9dcf, 0x164f8078, 0x1b0ca6a1, 0x1fcdbb16,
        0x018aeb13, 0x054bf6a4, 0x0808d07d, 0x0cc9cdca,
        0x7897ab07, 0x7c56b6b0, 0x71159069, 0x75d48dde,
        0x6b93dddb, 0x6f52c06c, 0x6211e6b5, 0x66d0fb02,
        0x5e9f46bf, 0x5a5e5b08, 0x571d7dd1, 0x53dc6066,
        0x4d9b3063, 0x495a2dd4, 0x44190b0d, 0x40d816ba,
        0xaca5c697, 0xa864db20, 0xa527fdf9, 0xa1e6e04e,
        0xbfa1b04b, 0xbb60adfc, 0xb6238b25, 0xb2e29692,
        0x8aad2b2f, 0x8e6c3698, 0x832f1041, 0x87ee0df6,
        0x99a95df3, 0x9d684044, 0x902b669d, 0x94ea7b2a,
        0xe0b41de7, 0xe4750050, 0xe9362689, 0xedf73b3e,
        0xf3b06b3b, 0xf771768c, 0xfa325055, 0xfef34de2,
        0xc6bcf05f, 0xc27dede8, 0xcf3ecb31, 0xcbffd686,
        0xd5b88683, 0xd1799b34, 0xdc3abded, 0xd8fba05a,
        0x690ce0ee, 0x6dcdfd59, 0x608edb80, 0x644fc637,
        0x7a089632, 0x7ec98b85, 0x738aad5c, 0x774bb0eb,
        0x4f040d56, 0x4bc510e1, 0x46863638, 0x42472b8f,
        0x5c007b8a, 0x58c1663d, 0x558240e4, 0x51435d53,
        0x251d3b9e, 0x21dc2629, 0x2c9f00f0, 0x285e1d47,
        0x36194d42, 0x32d850f5, 0x3f9b762c, 0x3b5a6b9b,
        0x0315d626, 0x07d4cb91, 0x0a97ed48, 0x0e56f0ff,
        0x1011a0fa, 0x14d0bd4d, 0x19939b94, 0x1d528623,
        0xf12f560e, 0xf5ee4bb9, 0xf8ad6d60, 0xfc6c70d7,
        0xe22b20d2, 0xe6ea3d65, 0xeba91bbc, 0xef68060b,
        0xd727bbb6, 0xd3e6a601, 0xdea580d8, 0xda649d6f,
        0xc423cd6a, 0xc0e2d0dd, 0xcda1f604, 0xc960ebb3,
        0xbd3e8d7e, 0xb9ff90c9, 0xb4bcb610, 0xb07daba7,
        0xae3afba2, 0xaafbe615, 0xa7b8c0cc, 0xa379dd7b,
        0x9b3660c6, 0x9ff77d71, 0x92b45ba8, 0x9675461f,
        0x8832161a, 0x8cf30bad, 0x81b02d74, 0x857130c3,
        0x5d8a9099, 0x594b8d2e, 0x5408abf7, 0x50c9b640,
        0x4e8ee645, 0x4a4ffbf2, 0x470cdd2b, 0x43cdc09c,
        0x7b827d21, 0x7f436096, 0x7200464f, 0x76c15bf8,
        0x68860bfd, 0x6c47164a, 0x61043093, 0x65c52d24,
        0x119b4be9, 0x155a565e, 0x18197087, 0x1cd86d30,
        0x029f3d35, 0x065e2082, 0x0b1d065b, 0x0fdc1bec,
        0x3793a651, 0x3352bbe6, 0x3e119d3f, 0x3ad08088,
        0x2497d08d, 0x2056cd3a, 0x2d15ebe3, 0x29d4f654,
        0xc5a92679, 0xc1683bce, 0xcc2b1d17, 0xc8ea00a0,
        0xd6ad50a5, 0xd26c4d12, 0xdf2f6bcb, 0xdbee767c,
        0xe3a1cbc1, 0xe760d676, 0xea23f0af, 0xeee2ed18,
        0xf0a5bd1d, 0xf464a0aa, 0xf9278673, 0xfde69bc4,
        0x89b8fd09, 0x8d79e0be, 0x803ac667, 0x84fbdbd0,
        0x9abc8bd5, 0x9e7d9662, 0x933eb0bb, 0x97ffad0c,
        0xafb010b1, 0xab710d06, 0xa6322bdf, 0xa2f33668,
        0xbcb4666d, 0xb8757bda, 0xb5365d03, 0xb1f740b4
    };

    uint32_t i_crc = 0xffffffff;

    /* Calculate the CRC */
    while( i_count > 0 )
    {
        i_crc = (i_crc<<8) ^ CRC32[ (i_crc>>24) ^ ((uint32_t)*p_begin) ];
        p_begin++;
        i_count--;
    }

    return( i_crc );
}

#if defined MODULE_NAME_IS_mux_ts
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
    bits_write( &bits, 2,  0x03 );     // reserved FIXME
    bits_write( &bits, 12, 13 );    // XXX for one program only XXX 
    bits_write( &bits, 16, 0x01 );  // FIXME stream id
    bits_write( &bits, 2,  0x03 );     //  FIXME
    bits_write( &bits, 5,  p_mux->i_pat_version_number );
    bits_write( &bits, 1,  1 );     // current_next_indicator
    bits_write( &bits, 8,  0 );     // section number
    bits_write( &bits, 8,  0 );     // last section number

    bits_write( &bits, 16, 1 );     // program number
    bits_write( &bits,  3, 0x07 );     // reserved
    bits_write( &bits, 13, p_mux->pmt.i_pid );  // program map pid

    bits_write( &bits, 32, CalculateCRC( bits.p_data + 1, bits.i_data - 1) );

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
    bits_write( &bits, 1,  0 );     // current_next_indicator
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

    bits_write( &bits, 32, CalculateCRC( bits.p_data + 1, bits.i_data - 1) );

    p_pmt->i_size = bits.i_data;

    return( PEStoTS( p_sout, pp_ts, p_pmt, &p_mux->pmt ) );

}
#elif defined MODULE_NAME_IS_mux_ts_dvbpsi

static sout_buffer_t *WritePSISection( sout_instance_t *p_sout,
                                       dvbpsi_psi_section_t* p_section )
{
    sout_buffer_t   *p_psi, *p_first = NULL;


    while( p_section )
    {
        int             i_size;

        i_size =  p_section->p_payload_end - p_section->p_data +
                  ( p_section->b_syntax_indicator ? 4 : 0 );

        p_psi = sout_BufferNew( p_sout, i_size + 1 );
        p_psi->i_pts = 0;
        p_psi->i_dts = 0;
        p_psi->i_length = 0;
        p_psi->i_size = i_size + 1;

        p_psi->p_buffer[0] = 0; // pointer
        memcpy( p_psi->p_buffer + 1,
                p_section->p_data,
                i_size );

        sout_BufferChain( &p_first, p_psi );

        p_section = p_section->p_next;
    }

    return( p_first );
}

static int GetPAT( sout_instance_t *p_sout,
                   sout_buffer_t **pp_ts )
{
    sout_mux_t          *p_mux = (sout_mux_t*)p_sout->p_mux_data;
    sout_buffer_t        *p_pat;
    dvbpsi_pat_t         pat;
    dvbpsi_psi_section_t *p_section;

    dvbpsi_InitPAT( &pat,
                    0x01,    // i_ts_id
                    p_mux->i_pat_version_number,
                    0);      // b_current_next
    /* add all program (only one) */
    dvbpsi_PATAddProgram( &pat,
                          1,                    // i_number
                          p_mux->pmt.i_pid );   // i_pid

    p_section = dvbpsi_GenPATSections( &pat,
                                       0 );     // max program per section

    p_pat = WritePSISection( p_sout, p_section );

    PEStoTS( p_sout, pp_ts, p_pat, &p_mux->pat );

    dvbpsi_DeletePSISections( p_section );
    dvbpsi_EmptyPAT( &pat );
    return( 0 );
}

static uint32_t GetDescriptorLength24b( int i_length )
{
    uint32_t    i_l1, i_l2, i_l3;

    i_l1 = i_length&0x7f;
    i_l2 = ( i_length >> 7 )&0x7f;
    i_l3 = ( i_length >> 14 )&0x7f;

    return( 0x808000 | ( i_l3 << 16 ) | ( i_l2 << 8 ) | i_l1 );
}

static int GetPMT( sout_instance_t *p_sout,
                   sout_buffer_t **pp_ts )
{
    sout_mux_t          *p_mux = (sout_mux_t*)p_sout->p_mux_data;
    sout_buffer_t       *p_pmt;

    dvbpsi_pmt_t        pmt;
    dvbpsi_pmt_es_t* p_es;
    dvbpsi_psi_section_t *p_section;

    int                 i_stream;

    dvbpsi_InitPMT( &pmt,
                    0x01,   // program number
                    p_mux->i_pmt_version_number,
                    1,      // b_current_next
                    p_mux->i_pcr_pid );

    if( p_mux->i_mpeg4_streams > 0 )
    {
        uint8_t iod[4096];
        bits_buffer_t bits;
        bits_buffer_t bits_fix_IOD;

        bits_initwrite( &bits, 4096, iod );
        // IOD_label
        bits_write( &bits, 8,   0x01 );
        // InitialObjectDescriptor
        bits_align( &bits );
        bits_write( &bits, 8,   0x02 );     // tag
        bits_fix_IOD = bits;    // save states to fix length later
        bits_write( &bits, 24,  GetDescriptorLength24b( 0 ) ); // variable length (fixed later)
        bits_write( &bits, 10,  0x01 );     // ObjectDescriptorID
        bits_write( &bits, 1,   0x00 );     // URL Flag
        bits_write( &bits, 1,   0x00 );     // includeInlineProfileLevelFlag
        bits_write( &bits, 4,   0x0f );     // reserved
        bits_write( &bits, 8,   0xff );     // ODProfile (no ODcapability )
        bits_write( &bits, 8,   0xff );     // sceneProfile
        bits_write( &bits, 8,   0xfe );     // audioProfile (unspecified)
        bits_write( &bits, 8,   0xfe );     // visualProfile( // )
        bits_write( &bits, 8,   0xff );     // graphicProfile (no )
        for( i_stream = 0; i_stream < p_sout->i_nb_inputs; i_stream++ )
        {
            ts_stream_t *p_stream;
            p_stream = (ts_stream_t*)p_sout->pp_inputs[i_stream]->p_mux_data;

            if( p_stream->i_stream_id == 0xfa || p_stream->i_stream_id == 0xfb )
            {
                bits_buffer_t bits_fix_ESDescr, bits_fix_Decoder;
                /* ES descriptor */
                bits_align( &bits );
                bits_write( &bits, 8,   0x03 );     // ES_DescrTag
                bits_fix_ESDescr = bits;
                bits_write( &bits, 24,  GetDescriptorLength24b( 0 ) ); // variable size
                bits_write( &bits, 16,  p_stream->i_es_id );
                bits_write( &bits, 1,   0x00 );     // streamDependency
                bits_write( &bits, 1,   0x00 );     // URL Flag
                bits_write( &bits, 1,   0x00 );     // OCRStreamFlag
                bits_write( &bits, 5,   0x1f );     // streamPriority

                    // DecoderConfigDesciptor
                bits_align( &bits );
                bits_write( &bits, 8,   0x04 ); // DecoderConfigDescrTag
                bits_fix_Decoder = bits;
                bits_write( &bits, 24,  GetDescriptorLength24b( 0 ) );
                if( p_stream->i_stream_type == 0x10 )
                {
                    bits_write( &bits, 8, 0x20 );   // Visual 14496-2
                    bits_write( &bits, 6, 0x04 );   // VisualStream
                }
                else if( p_stream->i_stream_type == 0x11 )
                {
                    bits_write( &bits, 8, 0x40 );   // Audio 14496-3
                    bits_write( &bits, 6, 0x05 );   // AudioStream
                }
                else
                {
                    bits_write( &bits, 8, 0x00 );
                    bits_write( &bits, 6, 0x00 );

                    msg_Err( p_sout,"Unsupported stream_type => broken IOD");
                }
                bits_write( &bits, 1,   0x00 );     // UpStream
                bits_write( &bits, 1,   0x01 );     // reserved
                bits_write( &bits, 24,  1024 * 1024 );  // bufferSizeDB
                bits_write( &bits, 32,  0x7fffffff );   // maxBitrate
                bits_write( &bits, 32,  0 );            // avgBitrate

                if( p_stream->i_decoder_specific_info_len > 0 )
                {
                    int i;
                    // DecoderSpecificInfo
                    bits_align( &bits );
                    bits_write( &bits, 8,   0x05 ); // tag
                    bits_write( &bits, 24,
                                GetDescriptorLength24b( p_stream->i_decoder_specific_info_len ) );
                    for( i = 0; i < p_stream->i_decoder_specific_info_len; i++ )
                    {
                        bits_write( &bits, 8,   ((uint8_t*)p_stream->p_decoder_specific_info)[i] );
                    }
                }
                /* fix Decoder length */
                bits_write( &bits_fix_Decoder, 24,
                            GetDescriptorLength24b( bits.i_data - bits_fix_Decoder.i_data - 3 ) );

                    // SLConfigDescriptor
                switch( p_stream->i_sl_predefined )
                {
                    case 0x01:
                        // FIXME
                        bits_align( &bits );
                        bits_write( &bits, 8,   0x06 ); // tag
                        bits_write( &bits, 24,  GetDescriptorLength24b( 8 ) );
                        bits_write( &bits, 8,   0x01 ); // predefined
                        bits_write( &bits, 1,   0 );   // durationFlag
                        bits_write( &bits, 32,  0 );   // OCRResolution
                        bits_write( &bits, 8,   0 );   // OCRLength
                        bits_write( &bits, 8,   0 );   // InstantBitrateLength
                        bits_align( &bits );
                        break;
                    default:
                        msg_Err( p_sout,"Unsupported SL profile => broken IOD");
                        break;
                }
                /* fix ESDescr length */
                bits_write( &bits_fix_ESDescr, 24,
                            GetDescriptorLength24b( bits.i_data - bits_fix_ESDescr.i_data - 3 ) );
            }
        }
        bits_align( &bits );
        /* fix IOD length */
        bits_write( &bits_fix_IOD, 24,
                    GetDescriptorLength24b( bits.i_data - bits_fix_IOD.i_data - 3 ) );
        dvbpsi_PMTAddDescriptor( &pmt,
                                 0x1d,
                                 bits.i_data,
                                 bits.p_data );
    }

    for( i_stream = 0; i_stream < p_sout->i_nb_inputs; i_stream++ )
    {
        ts_stream_t *p_stream;

        p_stream = (ts_stream_t*)p_sout->pp_inputs[i_stream]->p_mux_data;

        p_es = dvbpsi_PMTAddES( &pmt,
                                p_stream->i_stream_type,
                                p_stream->i_pid );
        if( p_stream->i_stream_id == 0xfa || p_stream->i_stream_id == 0xfb )
        {
            uint8_t     data[512];
            bits_buffer_t bits;

            /* SL descriptor */
            bits_initwrite( &bits, 512, data );
            bits_write( &bits, 16, p_stream->i_es_id );

            dvbpsi_PMTESAddDescriptor( p_es,
                                       0x1f,
                                       bits.i_data,
                                       bits.p_data );
        }
    }

    p_section = dvbpsi_GenPMTSections( &pmt );

    p_pmt = WritePSISection( p_sout, p_section );

    PEStoTS( p_sout, pp_ts, p_pmt, &p_mux->pmt );

    dvbpsi_DeletePSISections( p_section );
    dvbpsi_EmptyPMT( &pmt );
    return( 0 );
}

#endif

