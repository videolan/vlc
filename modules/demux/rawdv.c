/*****************************************************************************
 * rawdv.c : raw DV input module for vlc
 *****************************************************************************
 * Copyright (C) 2001-2007 the VideoLAN team
 * $Id$
 *
 * Authors: Gildas Bazin <gbazin@netcourrier.com>
 *          Paul Corke <paul dot corke at datatote dot co dot uk>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_demux.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define HURRYUP_TEXT N_( "Hurry up" )
#define HURRYUP_LONGTEXT N_( "The demuxer will advance timestamps if the " \
                "input can't keep up with the rate." )

static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

vlc_module_begin();
    set_shortname( "DV" );
    set_description( N_("DV (Digital Video) demuxer") );
    set_capability( "demux", 3 );
    set_category( CAT_INPUT );
    set_subcategory( SUBCAT_INPUT_DEMUX );
    add_bool( "rawdv-hurry-up", 0, NULL, HURRYUP_TEXT, HURRYUP_LONGTEXT, false );
    set_callbacks( Open, Close );
    add_shortcut( "rawdv" );
vlc_module_end();


/*****************************************************************************
 A little bit of background information (copied over from libdv glossary).

 - DIF block: A block of 80 bytes. This is the basic data framing unit of the
       DVC tape format, analogous to sectors of hard disc.

 - Video Section: Each DIF sequence contains a video section, consisting of
       135 DIF blocks, which are further subdivided into Video Segments.

 - Video Segment: A video segment consists of 5 DIF blocks, each corresponding
       to a single compressed macroblock.

*****************************************************************************/


/*****************************************************************************
 * Constants
 *****************************************************************************/
#define DV_PAL_FRAME_SIZE  144000
#define DV_NTSC_FRAME_SIZE 122000

/*****************************************************************************
 * Definitions of structures used by this plugin
 *****************************************************************************/
typedef struct {
    int8_t sct;      /* Section type (header,subcode,aux,audio,video) */
    int8_t dsn;      /* DIF sequence number (0-12) */
    int    fsc;      /* First (0)/Second channel (1) */
    int8_t dbn;      /* DIF block number (0-134) */
} dv_id_t;

typedef struct {
    int    dsf;      /* DIF sequence flag: 525/60 (0) or 625,50 (1) */
    int8_t apt;
    int    tf1;
    int8_t ap1;
    int    tf2;
    int8_t ap2;
    int    tf3;
    int8_t ap3;
} dv_header_t;

struct demux_sys_t
{
    int    frame_size;

    es_out_id_t *p_es_video;
    es_format_t  fmt_video;

    es_out_id_t *p_es_audio;
    es_format_t  fmt_audio;

    int    i_dsf;
    double f_rate;
    int    i_bitrate;

    /* program clock reference (in units of 90kHz) */
    mtime_t i_pcr;
    bool b_hurry_up;
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int Demux( demux_t * );
static int Control( demux_t *, int i_query, va_list args );

static block_t *dv_extract_audio( demux_t *p_demux,
                                  block_t* p_frame_block );

/*****************************************************************************
 * Open: initializes raw DV demux structures
 *****************************************************************************/
static int Open( vlc_object_t * p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys;

    const uint8_t *p_peek, *p_peek_backup;

    uint32_t    i_dword;
    dv_header_t dv_header;
    dv_id_t     dv_id;

    /* It isn't easy to recognize a raw DV stream. The chances that we'll
     * mistake a stream from another type for a raw DV stream are too high, so
     * we'll rely on the file extension to trigger this demux. Alternatively,
     * it is possible to force this demux. */

    /* Check for DV file extension */
    if( !demux_IsPathExtension( p_demux, ".dv" ) && !p_demux->b_force )
        return VLC_EGENERIC;

    if( stream_Peek( p_demux->s, &p_peek, DV_PAL_FRAME_SIZE ) <
        DV_NTSC_FRAME_SIZE )
    {
        /* Stream too short ... */
        msg_Err( p_demux, "cannot peek()" );
        return VLC_EGENERIC;
    }
    p_peek_backup = p_peek;

    /* fill in the dv_id_t structure */
    i_dword = GetDWBE( p_peek ); p_peek += 4;
    dv_id.sct = i_dword >> (32 - 3);
    i_dword <<= 8;
    dv_id.dsn = i_dword >> (32 - 4);
    i_dword <<= 4;
    dv_id.fsc = i_dword >> (32 - 1);
    i_dword <<= 4;
    dv_id.dbn = i_dword >> (32 - 8);
    i_dword <<= 8;

    if( dv_id.sct != 0 )
    {
        msg_Warn( p_demux, "not a raw DV stream header" );
        return VLC_EGENERIC;
    }

    /* fill in the dv_header_t structure */
    dv_header.dsf = i_dword >> (32 - 1);
    i_dword <<= 1;
    if( i_dword >> (32 - 1) ) /* incorrect bit */
    {
        msg_Warn( p_demux, "incorrect bit" );
        return VLC_EGENERIC;
    }

    i_dword = GetDWBE( p_peek ); p_peek += 4;
    i_dword <<= 5;
    dv_header.apt = i_dword >> (32 - 3);
    i_dword <<= 3;
    dv_header.tf1 = i_dword >> (32 - 1);
    i_dword <<= 5;
    dv_header.ap1 = i_dword >> (32 - 3);
    i_dword <<= 3;
    dv_header.tf2 = i_dword >> (32 - 1);
    i_dword <<= 5;
    dv_header.ap2 = i_dword >> (32 - 3);
    i_dword <<= 3;
    dv_header.tf3 = i_dword >> (32 - 1);
    i_dword <<= 5;
    dv_header.ap3 = i_dword >> (32 - 3);

    p_peek += 72;                                  /* skip rest of DIF block */

    /* Set p_input field */
    p_demux->pf_demux   = Demux;
    p_demux->pf_control = Control;
    p_demux->p_sys      = p_sys = malloc( sizeof( demux_sys_t ) );
    if( !p_sys )
        return VLC_ENOMEM;

    p_sys->b_hurry_up = var_CreateGetBool( p_demux, "rawdv-hurry-up" );
    msg_Dbg( p_demux, "Realtime DV Source: %s", (p_sys->b_hurry_up)?"Yes":"No" );

    p_sys->i_dsf = dv_header.dsf;
    p_sys->frame_size = dv_header.dsf ? 12 * 150 * 80 : 10 * 150 * 80;
    p_sys->f_rate = dv_header.dsf ? 25 : 29.97;

    p_sys->i_pcr = 1;
    p_sys->p_es_video = NULL;
    p_sys->p_es_audio = NULL;

    p_sys->i_bitrate = 0;

    es_format_Init( &p_sys->fmt_video, VIDEO_ES, VLC_FOURCC('d','v','s','d') );
    p_sys->fmt_video.video.i_width = 720;
    p_sys->fmt_video.video.i_height= dv_header.dsf ? 576 : 480;;

    /* FIXME FIXME */
#if 0
    /* necessary because input_SplitBuffer() will only get
     * INPUT_DEFAULT_BUFSIZE bytes at a time. */
    p_input->i_bufsize = p_sys->frame_size;
#endif

    p_sys->p_es_video = es_out_Add( p_demux->out, &p_sys->fmt_video );

    /* Audio stuff */
    p_peek = p_peek_backup + 80*6+80*16*3 + 3; /* beginning of AAUX pack */
    if( *p_peek == 0x50 )
    {
        es_format_Init( &p_sys->fmt_audio, AUDIO_ES,
                        VLC_FOURCC('a','r','a','w') );

        p_sys->fmt_audio.audio.i_channels = 2;
        switch( (p_peek[4] >> 3) & 0x07 )
        {
        case 0:
            p_sys->fmt_audio.audio.i_rate = 48000;
            break;
        case 1:
            p_sys->fmt_audio.audio.i_rate = 44100;
            break;
        case 2:
        default:
            p_sys->fmt_audio.audio.i_rate = 32000;
            break;
        }

        /* 12 bits non-linear will be converted to 16 bits linear */
        p_sys->fmt_audio.audio.i_bitspersample = 16;

        p_sys->p_es_audio = es_out_Add( p_demux->out, &p_sys->fmt_audio );
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: frees unused data
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys  = p_demux->p_sys;

    var_Destroy( p_demux, "rawdv-hurry-up");
    free( p_sys );
}

/*****************************************************************************
 * Demux: reads and demuxes data packets
 *****************************************************************************
 * Returns -1 in case of error, 0 in case of EOF, 1 otherwise
 *****************************************************************************/
static int Demux( demux_t *p_demux )
{
    demux_sys_t *p_sys  = p_demux->p_sys;
    block_t     *p_block;
    bool  b_audio = false;

    if( p_sys->b_hurry_up )
    {
         /* 3 frames */
        p_sys->i_pcr = mdate() + (p_sys->i_dsf ? 120000 : 90000);
    }

    /* Call the pace control */
    es_out_Control( p_demux->out, ES_OUT_SET_PCR, p_sys->i_pcr );
    p_block = stream_Block( p_demux->s, p_sys->frame_size );
    if( p_block == NULL )
    {
        /* EOF */
        return 0;
    }

    if( p_sys->p_es_audio )
    {
        es_out_Control( p_demux->out, ES_OUT_GET_ES_STATE,
                        p_sys->p_es_audio, &b_audio );
    }

    p_block->i_dts =
    p_block->i_pts = p_sys->i_pcr;

    if( b_audio )
    {
        block_t *p_audio_block = dv_extract_audio( p_demux, p_block );
        if( p_audio_block )
        {
            p_audio_block->i_pts =
            p_audio_block->i_dts = p_sys->i_pcr;
            es_out_Send( p_demux->out, p_sys->p_es_audio, p_audio_block );
        }
    }

    es_out_Send( p_demux->out, p_sys->p_es_video, p_block );

    if( !p_sys->b_hurry_up )
    {
        p_sys->i_pcr += ( INT64_C(1000000) / p_sys->f_rate );
    }

    return 1;
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( demux_t *p_demux, int i_query, va_list args )
{
    demux_sys_t *p_sys  = p_demux->p_sys;

    /* XXX: DEMUX_SET_TIME is precise here */
    return demux_vaControlHelper( p_demux->s,
                                   0, -1,
                                   p_sys->frame_size * p_sys->f_rate * 8,
                                   p_sys->frame_size, i_query, args );
}

static const uint16_t dv_audio_shuffle525[10][9] = {
  {  0, 30, 60, 20, 50, 80, 10, 40, 70 }, /* 1st channel */
  {  6, 36, 66, 26, 56, 86, 16, 46, 76 },
  { 12, 42, 72,  2, 32, 62, 22, 52, 82 },
  { 18, 48, 78,  8, 38, 68, 28, 58, 88 },
  { 24, 54, 84, 14, 44, 74,  4, 34, 64 },

  {  1, 31, 61, 21, 51, 81, 11, 41, 71 }, /* 2nd channel */
  {  7, 37, 67, 27, 57, 87, 17, 47, 77 },
  { 13, 43, 73,  3, 33, 63, 23, 53, 83 },
  { 19, 49, 79,  9, 39, 69, 29, 59, 89 },
  { 25, 55, 85, 15, 45, 75,  5, 35, 65 },
};

static const uint16_t dv_audio_shuffle625[12][9] = {
  {   0,  36,  72,  26,  62,  98,  16,  52,  88}, /* 1st channel */
  {   6,  42,  78,  32,  68, 104,  22,  58,  94},
  {  12,  48,  84,   2,  38,  74,  28,  64, 100},
  {  18,  54,  90,   8,  44,  80,  34,  70, 106},
  {  24,  60,  96,  14,  50,  86,   4,  40,  76},
  {  30,  66, 102,  20,  56,  92,  10,  46,  82},

  {   1,  37,  73,  27,  63,  99,  17,  53,  89}, /* 2nd channel */
  {   7,  43,  79,  33,  69, 105,  23,  59,  95},
  {  13,  49,  85,   3,  39,  75,  29,  65, 101},
  {  19,  55,  91,   9,  45,  81,  35,  71, 107},
  {  25,  61,  97,  15,  51,  87,   5,  41,  77},
  {  31,  67, 103,  21,  57,  93,  11,  47,  83},
};

static inline uint16_t dv_audio_12to16( uint16_t sample )
{
    uint16_t shift, result;

    sample = (sample < 0x800) ? sample : sample | 0xf000;
    shift = (sample & 0xf00) >> 8;

    if (shift < 0x2 || shift > 0xd) {
        result = sample;
    } else if (shift < 0x8) {
        shift--;
        result = (sample - (256 * shift)) << shift;
    } else {
        shift = 0xe - shift;
        result = ((sample + ((256 * shift) + 1)) << shift) - 1;
    }

    return result;
}

static block_t *dv_extract_audio( demux_t *p_demux,
                                  block_t* p_frame_block )
{
    demux_sys_t *p_sys  = p_demux->p_sys;
    block_t *p_block;
    uint8_t *p_frame, *p_buf;
    int i_audio_quant, i_samples, i_size, i_half_ch;
    const uint16_t (*audio_shuffle)[9];
    int i, j, d, of;
    uint16_t lc;

    /* Beginning of AAUX pack */
    p_buf = p_frame_block->p_buffer + 80*6+80*16*3 + 3;
    if( *p_buf != 0x50 ) return NULL;

    i_audio_quant = p_buf[4] & 0x07; /* 0 - 16bit, 1 - 12bit */
    if( i_audio_quant > 1 )
    {
        msg_Dbg( p_demux, "unsupported quantization for DV audio");
        return NULL;
    }

    i_samples = p_buf[1] & 0x3f; /* samples in this frame - min samples */
    switch( (p_buf[4] >> 3) & 0x07 )
    {
    case 0:
        i_size = p_sys->i_dsf ? 1896 : 1580;
        break;
    case 1:
        i_size = p_sys->i_dsf ? 1742 : 1452;
        break;
    case 2:
    default:
        i_size = p_sys->i_dsf ? 1264 : 1053;
        break;
    }
    i_size = (i_size + i_samples) * 4; /* 2ch, 2bytes */

    p_block = block_New( p_demux, i_size );

    /* for each DIF segment */
    p_frame = p_frame_block->p_buffer;
    audio_shuffle = p_sys->i_dsf ? dv_audio_shuffle625 : dv_audio_shuffle525;
    i_half_ch = (p_sys->i_dsf ? 12 : 10)/2;
    for( i = 0; i < (p_sys->i_dsf ? 12 : 10); i++ )
    {
        p_frame += 6 * 80; /* skip DIF segment header */

        if( i_audio_quant == 1 && i == i_half_ch ) break;

        for( j = 0; j < 9; j++ )
        {
            for( d = 8; d < 80; d += 2 )
            {
                if( i_audio_quant == 0 )
                {
                    /* 16bit quantization */
                    of = audio_shuffle[i][j] + (d - 8) / 2 *
                           (p_sys->i_dsf ? 108 : 90);

                    if( of * 2 >= i_size ) continue;

                    /* big endian */
                    p_block->p_buffer[of*2] = p_frame[d+1];
                    p_block->p_buffer[of*2+1] = p_frame[d];

                    if( p_block->p_buffer[of*2+1] == 0x80 &&
                        p_block->p_buffer[of*2] == 0x00 )
                        p_block->p_buffer[of*2+1] = 0;
                }
                else
                {
                    /* 12bit quantization */
                    lc = ((uint16_t)p_frame[d] << 4) |
                         ((uint16_t)p_frame[d+2] >> 4);
                    lc = (lc == 0x800 ? 0 : dv_audio_12to16(lc));

                    of = audio_shuffle[i][j] + (d - 8) / 3 *
                         (p_sys->i_dsf ? 108 : 90);
                    if( of*2 >= i_size ) continue;

                    /* big endian */
                    p_block->p_buffer[of*2] = lc & 0xff;
                    p_block->p_buffer[of*2+1] = lc >> 8;
                    ++d;
                }
            }

            p_frame += 16 * 80; /* 15 Video DIFs + 1 Audio DIF */
        }
    }

    return p_block;
}
