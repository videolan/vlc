/*****************************************************************************
 * ps.c: MPEG PS (ISO/IEC 13818-1) / MPEG SYSTEM (ISO/IEC 1172-1)
 *       multiplexer module for vlc
 *****************************************************************************
 * Copyright (C) 2001, 2002 VideoLAN
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Eric Petit <titer@videolan.org>
 *          Gildas Bazin <gbazin@videolan.org>
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

#include <vlc/vlc.h>
#include <vlc/input.h>
#include <vlc/sout.h>

#include "codecs.h"
#include "bits.h"
#include "pes.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define DTS_TEXT N_("DTS delay (ms)")
#define DTS_LONGTEXT N_("This option will delay the DTS (decoding time " \
  "stamps) and PTS (presentation timestamps) of the data in the " \
  "stream, compared to the SCRs. This allows for some buffering inside " \
  "the client decoder.")

static int     Open   ( vlc_object_t * );
static void    Close  ( vlc_object_t * );

#define SOUT_CFG_PREFIX "sout-ps-"

vlc_module_begin();
    set_description( _("PS muxer") );
    set_shortname( "MPEG-PS" );
    set_category( CAT_SOUT );
    set_subcategory( SUBCAT_SOUT_MUX );
    set_capability( "sout mux", 50 );
    add_shortcut( "ps" );
    add_shortcut( "mpeg1" );
    add_shortcut( "dvd" );
    set_callbacks( Open, Close );

    add_integer( SOUT_CFG_PREFIX "dts-delay", 200, NULL, DTS_TEXT,
                 DTS_LONGTEXT, VLC_TRUE );
vlc_module_end();

/*****************************************************************************
 * Exported prototypes
 *****************************************************************************/
static int Control  ( sout_mux_t *, int, va_list );
static int AddStream( sout_mux_t *, sout_input_t * );
static int DelStream( sout_mux_t *, sout_input_t * );
static int Mux      ( sout_mux_t * );

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  MuxGetStream        ( sout_mux_t *, int *, mtime_t * );

static void MuxWritePackHeader  ( sout_mux_t *, block_t **, mtime_t );
static void MuxWriteSystemHeader( sout_mux_t *, block_t **, mtime_t );

static void StreamIdInit        ( vlc_bool_t *id, int i_range );
static int  StreamIdGet         ( vlc_bool_t *id, int i_id_min, int i_id_max );
static void StreamIdRelease     ( vlc_bool_t *id, int i_id_min, int i_id );

typedef struct ps_stream_s
{
    int i_stream_id;

} ps_stream_t;

struct sout_mux_sys_t
{
    /* Which id are unused */
    vlc_bool_t  stream_id_mpga[16]; /* 0xc0 -> 0xcf */
    vlc_bool_t  stream_id_mpgv[16]; /* 0xe0 -> 0xef */
    vlc_bool_t  stream_id_a52[8];   /* 0x80 -> 0x87 <- FIXME I'm not sure */
    vlc_bool_t  stream_id_spu[32];  /* 0x20 -> 0x3f */
    vlc_bool_t  stream_id_dts[8];   /* 0x88 -> 0x8f */
    vlc_bool_t  stream_id_lpcm[16]; /* 0xa0 -> 0xaf */

    int i_audio_bound;
    int i_video_bound;
    int i_pes_count;
    int i_system_header;
    int i_dts_delay;

    int64_t i_instant_bitrate;
    int64_t i_instant_size;
    int64_t i_instant_dts;

    vlc_bool_t b_mpeg2;
};

static const char *ppsz_sout_options[] = {
    "dts-delay", NULL
};

/*****************************************************************************
 * Open:
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    sout_mux_t *p_mux = (sout_mux_t*)p_this;
    sout_mux_sys_t *p_sys;
    vlc_value_t val;

    msg_Info( p_mux, "Open" );
    sout_CfgParse( p_mux, SOUT_CFG_PREFIX, ppsz_sout_options, p_mux->p_cfg );

    p_mux->pf_control   = Control;
    p_mux->pf_addstream = AddStream;
    p_mux->pf_delstream = DelStream;
    p_mux->pf_mux       = Mux;
    p_mux->p_sys        = p_sys = malloc( sizeof( sout_mux_sys_t ) );

    /* Init free stream id */
    StreamIdInit( p_sys->stream_id_a52,  8  );
    StreamIdInit( p_sys->stream_id_dts,  8  );
    StreamIdInit( p_sys->stream_id_mpga, 16 );
    StreamIdInit( p_sys->stream_id_mpgv, 16 );
    StreamIdInit( p_sys->stream_id_lpcm, 16 );
    StreamIdInit( p_sys->stream_id_spu,  32 );

    p_sys->i_audio_bound   = 0;
    p_sys->i_video_bound   = 0;
    p_sys->i_system_header = 0;
    p_sys->i_pes_count     = 0;

    p_sys->i_instant_bitrate  = 0;
    p_sys->i_instant_size     = 0;
    p_sys->i_instant_dts      = 0;

    p_sys->b_mpeg2 = !(p_mux->psz_mux && !strcmp( p_mux->psz_mux, "mpeg1" ));

    var_Get( p_mux, SOUT_CFG_PREFIX "dts-delay", &val );
    p_sys->i_dts_delay = (int64_t)val.i_int * 1000;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close:
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    sout_mux_t      *p_mux = (sout_mux_t*)p_this;
    sout_mux_sys_t  *p_sys = p_mux->p_sys;

    block_t   *p_end;

    msg_Info( p_mux, "Close" );

    p_end = block_New( p_mux, 4 );
    p_end->p_buffer[0] = 0x00; p_end->p_buffer[1] = 0x00;
    p_end->p_buffer[2] = 0x01; p_end->p_buffer[3] = 0xb9;

    sout_AccessOutWrite( p_mux->p_access, p_end );

    free( p_sys );
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( sout_mux_t *p_mux, int i_query, va_list args )
{
    vlc_bool_t *pb_bool;
    char **ppsz;

   switch( i_query )
   {
       case MUX_CAN_ADD_STREAM_WHILE_MUXING:
           pb_bool = (vlc_bool_t*)va_arg( args, vlc_bool_t * );
           *pb_bool = VLC_TRUE;
           return VLC_SUCCESS;

       case MUX_GET_ADD_STREAM_WAIT:
           pb_bool = (vlc_bool_t*)va_arg( args, vlc_bool_t * );
           *pb_bool = VLC_FALSE;
           return VLC_SUCCESS;

       case MUX_GET_MIME:
           ppsz = (char**)va_arg( args, char ** );
           *ppsz = strdup( "video/mpeg" );
           return VLC_SUCCESS;

        default:
            return VLC_EGENERIC;
   }
}

/*****************************************************************************
 * AddStream:
 *****************************************************************************/
static int AddStream( sout_mux_t *p_mux, sout_input_t *p_input )
{
    sout_mux_sys_t  *p_sys = p_mux->p_sys;
    ps_stream_t *p_stream;

    msg_Dbg( p_mux, "adding input codec=%4.4s",
             (char*)&p_input->p_fmt->i_codec );

    p_input->p_sys = p_stream = malloc( sizeof( ps_stream_t ) );

    /* Init this new stream */
    switch( p_input->p_fmt->i_codec )
    {
        case VLC_FOURCC( 'm', 'p', 'g', 'v' ):
            p_stream->i_stream_id =
                StreamIdGet( p_sys->stream_id_mpgv, 0xe0, 0xef );
            break;
        case VLC_FOURCC( 'l', 'p', 'c', 'm' ):
            p_stream->i_stream_id =
                0xbd00 | StreamIdGet( p_sys->stream_id_lpcm, 0xa0, 0xaf );
            break;
        case VLC_FOURCC( 'd', 't', 's', ' ' ):
            p_stream->i_stream_id =
                0xbd00 | StreamIdGet( p_sys->stream_id_dts, 0x88, 0x8f );
            break;
        case VLC_FOURCC( 'a', '5', '2', ' ' ):
            p_stream->i_stream_id =
                0xbd00 | StreamIdGet( p_sys->stream_id_a52, 0x80, 0x87 );
            break;
        case VLC_FOURCC( 'm', 'p', 'g', 'a' ):
            p_stream->i_stream_id =
                StreamIdGet( p_sys->stream_id_mpga, 0xc0, 0xcf );
            break;
        case VLC_FOURCC( 's', 'p', 'u', ' ' ):
            p_stream->i_stream_id =
                0xbd00 | StreamIdGet( p_sys->stream_id_spu, 0x20, 0x3f );
            break;
        default:
            goto error;
    }

    if( p_stream->i_stream_id < 0 )
    {
        goto error;
    }

    if( p_input->p_fmt->i_cat == AUDIO_ES )
    {
        p_sys->i_audio_bound++;
    }
    else if( p_input->p_fmt->i_cat == VIDEO_ES )
    {
        p_sys->i_video_bound++;
    }

    /* Try to set a sensible default value for the instant bitrate */
    p_sys->i_instant_bitrate += p_input->p_fmt->i_bitrate + 1000/* overhead */;

    return VLC_SUCCESS;

error:
    free( p_stream );
    return VLC_EGENERIC;
}

/*****************************************************************************
 * DelStream:
 *****************************************************************************/
static int DelStream( sout_mux_t *p_mux, sout_input_t *p_input )
{
    sout_mux_sys_t *p_sys = p_mux->p_sys;
    ps_stream_t *p_stream =(ps_stream_t*)p_input->p_sys;

    msg_Dbg( p_mux, "removing input" );
    switch( p_input->p_fmt->i_codec )
    {
        case VLC_FOURCC( 'm', 'p', 'g', 'v' ):
            StreamIdRelease( p_sys->stream_id_mpgv, 0xe0,
                             p_stream->i_stream_id );
            break;
        case VLC_FOURCC( 'l', 'p', 'c', 'm' ):
            StreamIdRelease( p_sys->stream_id_lpcm, 0xa0,
                             p_stream->i_stream_id&0xff );
            break;
        case VLC_FOURCC( 'd', 't', 's', ' ' ):
            StreamIdRelease( p_sys->stream_id_dts, 0x88,
                             p_stream->i_stream_id&0xff );
            break;
        case VLC_FOURCC( 'a', '5', '2', ' ' ):
            StreamIdRelease( p_sys->stream_id_a52, 0x80,
                             p_stream->i_stream_id&0xff );
            break;
        case VLC_FOURCC( 'm', 'p', 'g', 'a' ):
            StreamIdRelease( p_sys->stream_id_mpga, 0xc0,
                             p_stream->i_stream_id  );
            break;
        case VLC_FOURCC( 's', 'p', 'u', ' ' ):
            StreamIdRelease( p_sys->stream_id_spu, 0x20,
                             p_stream->i_stream_id&0xff );
            break;
        default:
            /* Never reached */
            break;
    }

    if( p_input->p_fmt->i_cat == AUDIO_ES )
    {
        p_sys->i_audio_bound--;
    }
    else if( p_input->p_fmt->i_cat == VIDEO_ES )
    {
        p_sys->i_video_bound--;
    }

    free( p_stream );
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Mux: Call each time there is new data for at least one stream
 *****************************************************************************/
static int Mux( sout_mux_t *p_mux )
{
    sout_mux_sys_t *p_sys = p_mux->p_sys;

    for( ;; )
    {
        sout_input_t *p_input;
        ps_stream_t *p_stream;

        block_t *p_ps, *p_data;

        mtime_t        i_dts;
        int            i_stream;

        /* Choose which stream to mux */
        if( MuxGetStream( p_mux, &i_stream, &i_dts ) )
        {
            return VLC_SUCCESS;
        }

        p_input  = p_mux->pp_inputs[i_stream];
        p_stream = (ps_stream_t*)p_input->p_sys;
        p_ps     = NULL;

        /* Write regulary PackHeader */
        if( p_sys->i_pes_count % 30 == 0)
        {
            /* Update the instant bitrate every second or so */
            if( p_sys->i_instant_size &&
                i_dts - p_sys->i_instant_dts > 1000000 )
            {
                int64_t i_instant_bitrate = p_sys->i_instant_size * 8000000 /
                    ( i_dts - p_sys->i_instant_dts );

                p_sys->i_instant_bitrate += i_instant_bitrate;
                p_sys->i_instant_bitrate /= 2;

                p_sys->i_instant_size = 0;
                p_sys->i_instant_dts = i_dts;
            }
            else if( !p_sys->i_instant_size )
            {
                p_sys->i_instant_dts = i_dts;
            }

            MuxWritePackHeader( p_mux, &p_ps, i_dts );
        }

        /* Write regulary SystemHeader */
        if( p_sys->i_pes_count % 300 == 0 )
        {
            block_t *p_pk;

            MuxWriteSystemHeader( p_mux, &p_ps, i_dts );

            /* For MPEG1 streaming, set HEADER flag */
            for( p_pk = p_ps; p_pk != NULL; p_pk = p_pk->p_next )
            {
                p_pk->i_flags |= BLOCK_FLAG_HEADER;
            }
        }

        /* Get and mux a packet */
        p_data = block_FifoGet( p_input->p_fifo );
        E_( EStoPES )( p_mux->p_sout, &p_data, p_data,
                       p_input->p_fmt, p_stream->i_stream_id,
                       p_sys->b_mpeg2, 0, 0 );

        block_ChainAppend( &p_ps, p_data );

        /* Get size of output data so we can calculate the instant bitrate */
        for( p_data = p_ps; p_data; p_data = p_data->p_next )
        {
            p_sys->i_instant_size += p_data->i_buffer;
        }

        sout_AccessOutWrite( p_mux->p_access, p_ps );

        /* Increase counter */
        p_sys->i_pes_count++;
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 *
 *****************************************************************************/
static void StreamIdInit( vlc_bool_t *id, int i_range )
{
    int i;

    for( i = 0; i < i_range; i++ )
    {
        id[i] = VLC_TRUE;
    }
}
static int StreamIdGet( vlc_bool_t *id, int i_id_min, int i_id_max )
{
    int i;

    for( i = 0; i <= i_id_max - i_id_min; i++ )
    {
        if( id[i] )
        {
            id[i] = VLC_FALSE;

            return i_id_min + i;
        }
    }
    return -1;
}
static void StreamIdRelease( vlc_bool_t *id, int i_id_min, int i_id )
{
    id[i_id - i_id_min] = VLC_TRUE;
}

static void MuxWritePackHeader( sout_mux_t *p_mux, block_t **p_buf,
                                mtime_t i_dts )
{
    sout_mux_sys_t *p_sys = p_mux->p_sys;
    bits_buffer_t bits;
    block_t *p_hdr;
    mtime_t i_scr;
    int i_mux_rate;

    i_scr = (i_dts - p_sys->i_dts_delay) * 9 / 100;

    p_hdr = block_New( p_mux, 18 );
    p_hdr->i_pts = p_hdr->i_dts = i_dts;
    bits_initwrite( &bits, 14, p_hdr->p_buffer );
    bits_write( &bits, 32, 0x01ba );

    /* The spec specifies that the mux rate must be rounded upwards */
    i_mux_rate = (p_sys->i_instant_bitrate + 8 * 50 - 1 ) / (8 * 50);

    if( p_sys->b_mpeg2 )
    {
        bits_write( &bits, 2, 0x01 );
    }
    else
    {
        bits_write( &bits, 4, 0x02 );
    }

    bits_write( &bits, 3, ( i_scr >> 30 )&0x07 );
    bits_write( &bits, 1,  1 );
    bits_write( &bits, 15, ( i_scr >> 15 )&0x7fff );
    bits_write( &bits, 1,  1 );
    bits_write( &bits, 15, i_scr&0x7fff );
    bits_write( &bits, 1,  1 );

    if( p_sys->b_mpeg2 )
    {
        bits_write( &bits, 9,  0 ); // src extention
    }
    bits_write( &bits, 1,  1 );

    bits_write( &bits, 22,  i_mux_rate);  // FIXME mux rate
    bits_write( &bits, 1,  1 );

    if( p_sys->b_mpeg2 )
    {
        bits_write( &bits, 1,  1 );
        bits_write( &bits, 5,  0x1f );  // FIXME reserved
        bits_write( &bits, 3,  0 );     // stuffing bytes
    }

    p_hdr->i_buffer = p_sys->b_mpeg2 ? 14: 12;

    block_ChainAppend( p_buf, p_hdr );
}

static void MuxWriteSystemHeader( sout_mux_t *p_mux, block_t **p_buf,
                                  mtime_t i_dts )
{
    sout_mux_sys_t  *p_sys = p_mux->p_sys;
    block_t   *p_hdr;
    bits_buffer_t   bits;
    vlc_bool_t      b_private;
    int i_mux_rate;

    int             i_nb_private, i_nb_stream;
    int i;

    /* Count the number of private stream */
    for( i = 0, i_nb_private = 0; i < p_mux->i_nb_inputs; i++ )
    {
        ps_stream_t *p_stream;

        p_stream = (ps_stream_t*)p_mux->pp_inputs[i]->p_sys;

        if( ( p_stream->i_stream_id&0xff00 ) == 0xbd00 )
        {
            i_nb_private++;
        }
    }

    /* Private stream are declared only one time */
    i_nb_stream = p_mux->i_nb_inputs -
        ( i_nb_private > 0 ? i_nb_private - 1 : 0 );

    p_hdr = block_New( p_mux, 12 + i_nb_stream * 3 );
    p_hdr->i_dts = p_hdr->i_pts = i_dts;

    /* The spec specifies that the mux rate must be rounded upwards */
    i_mux_rate = (p_sys->i_instant_bitrate + 8 * 50 - 1 ) / (8 * 50);

    bits_initwrite( &bits, 12 + i_nb_stream * 3, p_hdr->p_buffer );
    bits_write( &bits, 32, 0x01bb );
    bits_write( &bits, 16, 12 - 6 + i_nb_stream * 3 );
    bits_write( &bits, 1,  1 );
    bits_write( &bits, 22, i_mux_rate); // FIXME rate bound
    bits_write( &bits, 1,  1 );

    bits_write( &bits, 6,  p_sys->i_audio_bound );
    bits_write( &bits, 1,  0 ); // fixed flag
    bits_write( &bits, 1,  0 ); // CSPS flag
    bits_write( &bits, 1,  0 ); // system audio lock flag
    bits_write( &bits, 1,  0 ); // system video lock flag

    bits_write( &bits, 1,  1 ); // marker bit

    bits_write( &bits, 5,  p_sys->i_video_bound );
    bits_write( &bits, 1,  1 ); // packet rate restriction flag (1 for mpeg1)
    bits_write( &bits, 7,  0xff ); // reserved bits

    /* stream_id table */
    for( i = 0, b_private = VLC_FALSE; i < p_mux->i_nb_inputs; i++ )
    {
        sout_input_t *p_input;
        ps_stream_t *p_stream;

        p_input = p_mux->pp_inputs[i];
        p_stream = (ps_stream_t *)p_input->p_sys;

        if( ( p_stream->i_stream_id&0xff00 ) == 0xbd00 )
        {
            if( b_private )
            {
                continue;
            }
            b_private = VLC_TRUE;
            /* Write stream id */
            bits_write( &bits, 8, 0xbd );
        }
        else
        {
            /* Write stream id */
            bits_write( &bits, 8, p_stream->i_stream_id&0xff );
        }
        bits_write( &bits, 2, 0x03 );
        if( p_input->p_fmt->i_cat == AUDIO_ES )
        {
            bits_write( &bits, 1, 0 );
            bits_write( &bits, 13, /* stream->max_buffer_size */ 0 / 128 );
        }
        else if( p_input->p_fmt->i_cat == VIDEO_ES )
        {
            bits_write( &bits, 1, 1 );
            bits_write( &bits, 13, /* stream->max_buffer_size */ 0 / 1024);
        }
        else
        {
            /* FIXME */
            bits_write( &bits, 1, 0 );
            bits_write( &bits, 13, /* stream->max_buffer_size */ 0 );
        }
    }

    block_ChainAppend( p_buf, p_hdr );
}

/*
 * Find stream to be muxed.
 */
static int MuxGetStream( sout_mux_t *p_mux, int *pi_stream, mtime_t *pi_dts )
{
    mtime_t i_dts;
    int     i_stream;
    int     i;

    for( i = 0, i_dts = 0, i_stream = -1; i < p_mux->i_nb_inputs; i++ )
    {
        sout_input_t *p_input = p_mux->pp_inputs[i];
        block_t *p_data;

        if( p_input->p_fifo->i_depth <= 0 )
        {
            if( p_input->p_fmt->i_cat == AUDIO_ES ||
                p_input->p_fmt->i_cat == VIDEO_ES )
            {
                /* We need that audio+video fifo contain at least 1 packet */
                return VLC_EGENERIC;
            }

            /* SPU */
            continue;
        }

        p_data = block_FifoShow( p_input->p_fifo );
        if( i_stream == -1 || p_data->i_dts < i_dts )
        {
            i_stream = i;
            i_dts    = p_data->i_dts;
        }
    }

    *pi_stream = i_stream;
    *pi_dts = i_dts;

    return VLC_SUCCESS;
}
