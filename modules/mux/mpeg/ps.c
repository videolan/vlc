/*****************************************************************************
 * ps.c: MPEG PS (ISO/IEC 13818-1) / MPEG SYSTEM (ISO/IEC 1172-1)
 *       multiplexer module for vlc
 *****************************************************************************
 * Copyright (C) 2001, 2002 VLC authors and VideoLAN
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Eric Petit <titer@videolan.org>
 *          Gildas Bazin <gbazin@videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_sout.h>
#include <vlc_block.h>

#include "bits.h"
#include "pes.h"

#include "../../demux/mpeg/timestamps.h"

#include <vlc_iso_lang.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define DTS_TEXT N_("DTS delay (ms)")
#define DTS_LONGTEXT N_("Delay the DTS (decoding time " \
  "stamps) and PTS (presentation timestamps) of the data in the " \
  "stream, compared to the SCRs. This allows for some buffering inside " \
  "the client decoder.")

#define PES_SIZE_TEXT N_("PES maximum size")
#define PES_SIZE_LONGTEXT N_("Set the maximum allowed PES "\
  "size when producing the MPEG PS streams.")

static int     Open   ( vlc_object_t * );
static void    Close  ( vlc_object_t * );

#define SOUT_CFG_PREFIX "sout-ps-"

vlc_module_begin ()
    set_description( N_("PS muxer") )
    set_shortname( "MPEG-PS" )
    set_category( CAT_SOUT )
    set_subcategory( SUBCAT_SOUT_MUX )
    set_capability( "sout mux", 50 )
    add_shortcut( "ps", "mpeg1", "dvd" )
    set_callbacks( Open, Close )

    add_integer( SOUT_CFG_PREFIX "dts-delay", 200, DTS_TEXT,
                 DTS_LONGTEXT, true )
    add_integer( SOUT_CFG_PREFIX "pes-max-size", PES_PAYLOAD_SIZE_MAX,
                 PES_SIZE_TEXT, PES_SIZE_LONGTEXT, true )
vlc_module_end ()

/*****************************************************************************
 * Exported prototypes
 *****************************************************************************/
static int Control  ( sout_mux_t *, int, va_list );
static int AddStream( sout_mux_t *, sout_input_t * );
static void DelStream( sout_mux_t *, sout_input_t * );
static int Mux      ( sout_mux_t * );

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

static void MuxWritePackHeader  ( sout_mux_t *, block_t **, vlc_tick_t );
static void MuxWriteSystemHeader( sout_mux_t *, block_t **, vlc_tick_t );
static void MuxWritePSM         ( sout_mux_t *, block_t **, vlc_tick_t );

static void StreamIdInit        ( bool *id, int i_range );
static int  StreamIdGet         ( bool *id, int i_id_min, int i_id_max );
static void StreamIdRelease     ( bool *id, int i_id_min, int i_id );

typedef struct ps_stream_s
{
    int i_stream_id;
    int i_stream_type;
    int i_max_buff_size; /* used in system header */

    /* Language is iso639-2T */
    uint8_t lang[3];
    vlc_tick_t i_dts;

} ps_stream_t;

typedef struct
{
    /* Which id are unused */
    bool  stream_id_mpga[16]; /* 0xc0 -> 0xcf */
    bool  stream_id_mpgv[16]; /* 0xe0 -> 0xef */
    bool  stream_id_a52[8];   /* 0x80 -> 0x87 <- FIXME I'm not sure */
    bool  stream_id_spu[32];  /* 0x20 -> 0x3f */
    bool  stream_id_dts[8];   /* 0x88 -> 0x8f */
    bool  stream_id_lpcm[16]; /* 0xa0 -> 0xaf */

    int i_audio_bound;
    int i_video_bound;
    int i_pes_count;
    int i_system_header;
    vlc_tick_t i_dts_delay;
    int i_rate_bound; /* units of 50 bytes/second */
 
    int64_t i_instant_bitrate;
    int64_t i_instant_size;
    vlc_tick_t i_instant_dts;

    bool b_mpeg2;

    int i_pes_max_size;

    int i_psm_version;
    uint32_t crc32_table[256];
} sout_mux_sys_t;

static const char *const ppsz_sout_options[] = {
    "dts-delay", "pes-max-size", NULL
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
    config_ChainParse( p_mux, SOUT_CFG_PREFIX, ppsz_sout_options, p_mux->p_cfg );

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

    p_sys->i_psm_version   = 0;

    p_sys->i_instant_bitrate  = 0;
    p_sys->i_instant_size     = 0;
    p_sys->i_instant_dts      = 0;
    p_sys->i_rate_bound      = 0;
    p_sys->b_mpeg2 = !(p_mux->psz_mux && !strcmp( p_mux->psz_mux, "mpeg1" ));

    var_Get( p_mux, SOUT_CFG_PREFIX "dts-delay", &val );
    p_sys->i_dts_delay = VLC_TICK_FROM_MS(val.i_int);

    var_Get( p_mux, SOUT_CFG_PREFIX "pes-max-size", &val );
    p_sys->i_pes_max_size = (int64_t)val.i_int;

    /* Initialise CRC32 table */
    if( p_sys->b_mpeg2 )
    {
        uint32_t i, j, k;

        for( i = 0; i < 256; i++ )
        {
            k = 0;
            for( j = (i << 24) | 0x800000; j != 0x80000000; j <<= 1 )
                k = (k << 1) ^ (((k ^ j) & 0x80000000) ? 0x04c11db7 : 0);

            p_sys->crc32_table[i] = k;
        }
    }

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

    p_end = block_Alloc( 4 );
    if( p_end )
    {
        p_end->p_buffer[0] = 0x00; p_end->p_buffer[1] = 0x00;
        p_end->p_buffer[2] = 0x01; p_end->p_buffer[3] = 0xb9;

        sout_AccessOutWrite( p_mux->p_access, p_end );
    }

    free( p_sys );
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( sout_mux_t *p_mux, int i_query, va_list args )
{
    VLC_UNUSED(p_mux);
    bool *pb_bool;
    char **ppsz;

    switch( i_query )
    {
        case MUX_CAN_ADD_STREAM_WHILE_MUXING:
            pb_bool = va_arg( args, bool * );
            *pb_bool = true;
            return VLC_SUCCESS;

        case MUX_GET_MIME:
            ppsz = va_arg( args, char ** );
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
    if( unlikely(p_input->p_sys == NULL) )
        return VLC_ENOMEM;
    p_stream->i_stream_type = 0x81;
    p_stream->i_dts = -1;

    /* Init this new stream */
    switch( p_input->p_fmt->i_codec )
    {
        case VLC_CODEC_MPGV:
        case VLC_CODEC_MP2V:
        case VLC_CODEC_MP1V:
            p_stream->i_stream_id =
                StreamIdGet( p_sys->stream_id_mpgv, 0xe0, 0xef );
            p_stream->i_stream_type = 0x02; /* ISO/IEC 13818 Video */
            break;
        case VLC_CODEC_MP4V:
            p_stream->i_stream_id =
                StreamIdGet( p_sys->stream_id_mpgv, 0xe0, 0xef );
            p_stream->i_stream_type = 0x10;
            break;
        case VLC_CODEC_H264:
            p_stream->i_stream_id =
                StreamIdGet( p_sys->stream_id_mpgv, 0xe0, 0xef );
            p_stream->i_stream_type = 0x1b;
            break;
        case VLC_CODEC_DVD_LPCM:
            p_stream->i_stream_id =
                0xbd00 | StreamIdGet( p_sys->stream_id_lpcm, 0xa0, 0xaf );
            break;
        case VLC_CODEC_DTS:
            p_stream->i_stream_id =
                0xbd00 | StreamIdGet( p_sys->stream_id_dts, 0x88, 0x8f );
            break;
        case VLC_CODEC_A52:
            p_stream->i_stream_id =
                0xbd00 | StreamIdGet( p_sys->stream_id_a52, 0x80, 0x87 );
            break;
        case VLC_CODEC_MPGA:
            p_stream->i_stream_id =
                StreamIdGet( p_sys->stream_id_mpga, 0xc0, 0xcf );
            p_stream->i_stream_type = 0x03; /* ISO/IEC 11172 Audio */
            break;
        case VLC_CODEC_MP4A:
            p_stream->i_stream_id =
                StreamIdGet( p_sys->stream_id_mpga, 0xc0, 0xcf );
            p_stream->i_stream_type = 0x0f;
            break;
        case VLC_CODEC_SPU:
            p_stream->i_stream_id =
                0xbd00 | StreamIdGet( p_sys->stream_id_spu, 0x20, 0x3f );
            break;
        default:
            goto error;
    }

    if( p_stream->i_stream_id < 0 ) goto error;

    if( p_input->p_fmt->i_cat == AUDIO_ES )
    {
        p_sys->i_audio_bound++;
        p_stream->i_max_buff_size = 4 * 1024;
    }
    else if( p_input->p_fmt->i_cat == VIDEO_ES )
    {
        p_sys->i_video_bound++;
        p_stream->i_max_buff_size = 400 * 1024; /* FIXME -- VCD uses 46, SVCD
                        uses 230, ffmpeg has 230 with a note that it is small */
    }
    else
    {   /* FIXME -- what's valid for not audio or video? */
        p_stream->i_max_buff_size = 4 * 1024;
    }

    /* Try to set a sensible default value for the instant bitrate */
    p_sys->i_instant_bitrate += p_input->p_fmt->i_bitrate + 1000/* overhead */;

    /* FIXME -- spec requires  an upper limit rate boundary in the system header;
       our codecs are VBR; using 2x nominal rate, convert to 50 bytes/sec */
    p_sys->i_rate_bound += p_input->p_fmt->i_bitrate * 2 / (8 * 50);
    p_sys->i_psm_version++;

    p_stream->lang[0] = p_stream->lang[1] = p_stream->lang[2] = 0;
    if( p_input->p_fmt->psz_language )
    {
        char *psz = p_input->p_fmt->psz_language;
        const iso639_lang_t *pl = NULL;

        if( strlen( psz ) == 2 )
        {
            pl = GetLang_1( psz );
        }
        else if( strlen( psz ) == 3 )
        {
            pl = GetLang_2B( psz );
            if( !strcmp( pl->psz_iso639_1, "??" ) )
            {
                pl = GetLang_2T( psz );
            }
        }
        if( pl && strcmp( pl->psz_iso639_1, "??" ) )
        {
            p_stream->lang[0] = pl->psz_iso639_2T[0];
            p_stream->lang[1] = pl->psz_iso639_2T[1];
            p_stream->lang[2] = pl->psz_iso639_2T[2];

            msg_Dbg( p_mux, "    - lang=%c%c%c",
                     p_stream->lang[0], p_stream->lang[1], p_stream->lang[2] );
        }
    }
    return VLC_SUCCESS;

error:
    free( p_stream );
    return VLC_EGENERIC;
}

/*****************************************************************************
 * DelStream:
 *****************************************************************************/
static void DelStream( sout_mux_t *p_mux, sout_input_t *p_input )
{
    sout_mux_sys_t *p_sys = p_mux->p_sys;
    ps_stream_t *p_stream =(ps_stream_t*)p_input->p_sys;

    msg_Dbg( p_mux, "removing input" );
    switch( p_input->p_fmt->i_codec )
    {
        case VLC_CODEC_MPGV:
            StreamIdRelease( p_sys->stream_id_mpgv, 0xe0,
                             p_stream->i_stream_id );
            break;
        case VLC_CODEC_DVD_LPCM:
            StreamIdRelease( p_sys->stream_id_lpcm, 0xa0,
                             p_stream->i_stream_id&0xff );
            break;
        case VLC_CODEC_DTS:
            StreamIdRelease( p_sys->stream_id_dts, 0x88,
                             p_stream->i_stream_id&0xff );
            break;
        case VLC_CODEC_A52:
            StreamIdRelease( p_sys->stream_id_a52, 0x80,
                             p_stream->i_stream_id&0xff );
            break;
        case VLC_CODEC_MPGA:
            StreamIdRelease( p_sys->stream_id_mpga, 0xc0,
                             p_stream->i_stream_id  );
            break;
        case VLC_CODEC_SPU:
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

    /* Try to set a sensible default value for the instant bitrate */
    p_sys->i_instant_bitrate -= (p_input->p_fmt->i_bitrate + 1000);
    /* rate_bound is in units of 50 bytes/second */
    p_sys->i_rate_bound -= (p_input->p_fmt->i_bitrate * 2)/(8 * 50);

    p_sys->i_psm_version++;

    free( p_stream );
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

        vlc_tick_t     i_dts;

        /* Choose which stream to mux */
        int i_stream = sout_MuxGetStream( p_mux, 1, &i_dts );
        if( i_stream < 0 )
        {
            return VLC_SUCCESS;
        }

        p_input  = p_mux->pp_inputs[i_stream];
        p_stream = (ps_stream_t*)p_input->p_sys;
        p_ps     = NULL;

        p_stream->i_dts = i_dts;

        /* Write regulary PackHeader */
        if( p_sys->i_pes_count % 30 == 0)
        {
            vlc_tick_t i_mindts = INT64_MAX;
            for( int i=0; i < p_mux->i_nb_inputs; i++ )
            {
                ps_stream_t *p_s = (ps_stream_t*)p_input->p_sys;
                if( p_input->p_fmt->i_cat == SPU_ES && p_mux->i_nb_inputs > 1 )
                    continue;
                if( p_s->i_dts >= 0 && i_mindts > p_s->i_dts )
                    i_mindts = p_s->i_dts;
            }

            if( i_mindts > p_sys->i_instant_dts )
            {
                /* Update the instant bitrate every second or so */
                if( p_sys->i_instant_size &&
                    i_dts - p_sys->i_instant_dts > VLC_TICK_FROM_SEC(1))
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

        /* Write regulary ProgramStreamMap */
        if( p_sys->b_mpeg2 && p_sys->i_pes_count % 300 == 0 )
        {
            MuxWritePSM( p_mux, &p_ps, i_dts );
        }

        /* Get and mux a packet */
        p_data = block_FifoGet( p_input->p_fifo );
        EStoPES ( &p_data, p_input->p_fmt, p_stream->i_stream_id,
                       p_sys->b_mpeg2, 0, 0, p_sys->i_pes_max_size, 0 );

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
static void StreamIdInit( bool *id, int i_range )
{
    int i;

    for( i = 0; i < i_range; i++ )
    {
        id[i] = true;
    }
}
static int StreamIdGet( bool *id, int i_id_min, int i_id_max )
{
    int i;

    for( i = 0; i <= i_id_max - i_id_min; i++ )
    {
        if( id[i] )
        {
            id[i] = false;

            return i_id_min + i;
        }
    }
    return -1;
}
static void StreamIdRelease( bool *id, int i_id_min, int i_id )
{
    id[i_id - i_id_min] = true;
}

static void MuxWritePackHeader( sout_mux_t *p_mux, block_t **p_buf,
                                vlc_tick_t i_dts )
{
    sout_mux_sys_t *p_sys = p_mux->p_sys;
    bits_buffer_t bits;
    block_t *p_hdr;
    int64_t i_scr;
    int i_mux_rate;

    i_scr = TO_SCALE_NZ(i_dts - p_sys->i_dts_delay);

    p_hdr = block_Alloc( 18 );
    if( !p_hdr )
        return;
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
    bits_write( &bits, 1,  1 ); // marker
    bits_write( &bits, 15, ( i_scr >> 15 )&0x7fff );
    bits_write( &bits, 1,  1 ); // marker
    bits_write( &bits, 15, i_scr&0x7fff );
    bits_write( &bits, 1,  1 ); // marker

    if( p_sys->b_mpeg2 )
    {
        bits_write( &bits, 9,  0 ); // src extension
    }
    bits_write( &bits, 1,  1 );     // marker

    bits_write( &bits, 22, i_mux_rate);
    bits_write( &bits, 1,  1 );     // marker

    if( p_sys->b_mpeg2 )
    {
        bits_write( &bits, 1,  1 );     // marker
        bits_write( &bits, 5,  0x1f );  // reserved
        bits_write( &bits, 3,  0 );     // stuffing bytes
    }

    p_hdr->i_buffer = p_sys->b_mpeg2 ? 14: 12;

    block_ChainAppend( p_buf, p_hdr );
}

static void MuxWriteSystemHeader( sout_mux_t *p_mux, block_t **p_buf,
                                  vlc_tick_t i_dts )
{
    sout_mux_sys_t  *p_sys = p_mux->p_sys;
    block_t   *p_hdr;
    bits_buffer_t   bits;
    bool      b_private;
    int i_rate_bound;

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

    p_hdr = block_Alloc(  12 + i_nb_stream * 3 );
    if( !p_hdr )
        return;
    p_hdr->i_dts = p_hdr->i_pts = i_dts;

    /* The spec specifies that the reported rate_bound must be upper limit */
    i_rate_bound = (p_sys->i_rate_bound);

    bits_initwrite( &bits, 12 + i_nb_stream * 3, p_hdr->p_buffer );
    bits_write( &bits, 32, 0x01bb );
    bits_write( &bits, 16, 12 - 6 + i_nb_stream * 3 );
    bits_write( &bits, 1,  1 ); // marker bit
    bits_write( &bits, 22, i_rate_bound);
    bits_write( &bits, 1,  1 ); // marker bit

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
    for( i = 0, b_private = false; i < p_mux->i_nb_inputs; i++ )
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
            b_private = true;
            /* Write stream id */
            bits_write( &bits, 8, 0xbd );
        }
        else
        {
            /* Write stream id */
            bits_write( &bits, 8, p_stream->i_stream_id&0xff );
        }

        bits_write( &bits, 2, 0x03 ); /* reserved */
        if( p_input->p_fmt->i_cat == AUDIO_ES )
        {
            bits_write( &bits, 1, 0 );
            bits_write( &bits, 13, p_stream->i_max_buff_size / 128 );
        }
        else if( p_input->p_fmt->i_cat == VIDEO_ES )
        {
            bits_write( &bits, 1, 1 );
            bits_write( &bits, 13, p_stream->i_max_buff_size / 1024);
        }
        else
        {
            /* FIXME -- the scale of 0 means do a /128 */
            bits_write( &bits, 1, 0 );
            bits_write( &bits, 13, p_stream->i_max_buff_size / 128 );
        }
    }

    block_ChainAppend( p_buf, p_hdr );
}

static void MuxWritePSM( sout_mux_t *p_mux, block_t **p_buf, vlc_tick_t i_dts )
{
    sout_mux_sys_t *p_sys = p_mux->p_sys;
    block_t *p_hdr;
    bits_buffer_t bits;
    int i, i_psm_size = 16, i_es_map_size = 0;

    for( i = 0; i < p_mux->i_nb_inputs; i++ )
    {
        sout_input_t *p_input = p_mux->pp_inputs[i];
        ps_stream_t *p_stream = p_input->p_sys;

        i_es_map_size += 4;
        if( p_stream->lang[0] != 0 ) i_es_map_size += 6;
    }

    i_psm_size += i_es_map_size;

    p_hdr = block_Alloc( i_psm_size );
    if( !p_hdr )
        return;
    p_hdr->i_dts = p_hdr->i_pts = i_dts;

    memset( p_hdr->p_buffer, 0, p_hdr->i_buffer );
    bits_initwrite( &bits, i_psm_size, p_hdr->p_buffer );
    bits_write( &bits, 32, 0x01bc );
    bits_write( &bits, 16, i_psm_size - 6 );
    bits_write( &bits, 1, 1 ); /* current_next_indicator */
    bits_write( &bits, 2, 0xF ); /* reserved */
    bits_write( &bits, 5, p_sys->i_psm_version );
    bits_write( &bits, 7, 0xFF ); /* reserved */
    bits_write( &bits, 1, 1 ); /* marker */

    bits_write( &bits, 16, 0 ); /* program_stream_info_length */
    /* empty */

    bits_write( &bits, 16, i_es_map_size ); /* elementary_stream_map_length */
    for( i = 0; i < p_mux->i_nb_inputs; i++ )
    {
        sout_input_t *p_input = p_mux->pp_inputs[i];
        ps_stream_t *p_stream = p_input->p_sys;

        bits_write( &bits, 8, p_stream->i_stream_type ); /* stream_type */
        bits_write( &bits, 8, p_stream->i_stream_id ); /* elementary_stream_id */

        /* ISO639 language descriptor */
        if( p_stream->lang[0] != 0 )
        {
            bits_write( &bits, 16, 6 ); /* elementary_stream_info_length */

            bits_write( &bits, 8, 0x0a ); /* descriptor_tag */
            bits_write( &bits, 8, 4 ); /* descriptor_length */

            bits_write( &bits, 8, p_stream->lang[0] );
            bits_write( &bits, 8, p_stream->lang[1] );
            bits_write( &bits, 8, p_stream->lang[2] );
            bits_write( &bits, 8, 0 ); /* audio type: 0x00 undefined */
        }
        else
        {
            bits_write( &bits, 16, 0 ); /* elementary_stream_info_length */
        }
    }

    /* CRC32 */
    {
        uint32_t i_crc = 0xffffffff;
        for( i = 0; (size_t)i < p_hdr->i_buffer; i++ )
        i_crc = (i_crc << 8) ^
            p_sys->crc32_table[((i_crc >> 24) ^ p_hdr->p_buffer[i]) & 0xff];

        bits_write( &bits, 32, i_crc );
    }

    block_ChainAppend( p_buf, p_hdr );
}
