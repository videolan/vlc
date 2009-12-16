/*****************************************************************************
 * lpcm.c: lpcm decoder/packetizer module
 *****************************************************************************
 * Copyright (C) 1999-2008 the VideoLAN team
 * $Id$
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
 *          Henri Fallon <henri@videolan.org>
 *          Christophe Massiot <massiot@via.ecp.fr>
 *          Gildas Bazin <gbazin@videolan.org>
 *          Lauren Aimar <fenrir _AT_ videolan _DOT_ org >
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
#include <vlc_codec.h>
#include <vlc_aout.h>
#include <assert.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  OpenDecoder   ( vlc_object_t * );
static int  OpenPacketizer( vlc_object_t * );
static void CloseCommon   ( vlc_object_t * );

vlc_module_begin ()

    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_ACODEC )
    set_description( N_("Linear PCM audio decoder") )
    set_capability( "decoder", 100 )
    set_callbacks( OpenDecoder, CloseCommon )

    add_submodule ()
    set_description( N_("Linear PCM audio packetizer") )
    set_capability( "packetizer", 100 )
    set_callbacks( OpenPacketizer, CloseCommon )

vlc_module_end ()


/*****************************************************************************
 * decoder_sys_t : lpcm decoder descriptor
 *****************************************************************************/
struct decoder_sys_t
{
    /* Module mode */
    bool b_packetizer;

    /*
     * Output properties
     */
    date_t   end_date;

    /* */
    unsigned i_header_size;
    int      i_type;
};

/*
 * LPCM DVD header :
 * - frame number (8 bits)
 * - unknown (16 bits) == 0x0003 ?
 * - unknown (4 bits)
 * - current frame (4 bits)
 * - unknown (2 bits)
 * - frequency (2 bits) 0 == 48 kHz, 1 == 32 kHz, 2 == ?, 3 == ?
 * - unknown (1 bit)
 * - number of channels - 1 (3 bits) 1 == 2 channels
 * - start code (8 bits) == 0x80
 *
 * LPCM DVD-A header (http://dvd-audio.sourceforge.net/spec/aob.shtml)
 * - continuity counter (8 bits, clipped to 0x00-0x1f)
 * - header size (16 bits)
 * - byte pointer to start of first audio frame.
 * - unknown (8bits, 0x10 for stereo, 0x00 for surround)
 * - sample size (4+4 bits)
 * - samplerate (4+4 bits)
 * - unknown (8 bits)
 * - group assignment (8 bits)
 * - unknown (8 bits)
 * - padding(variable)
 *
 * LPCM BD header :
 * - unkown (16 bits)
 * - number of channels (4 bits)
 * - frequency (4 bits)
 * - bits per sample (2 bits)
 * - unknown (6 bits)
 */

#define LPCM_VOB_HEADER_LEN (6)
#define LPCM_AOB_HEADER_LEN (11)
#define LPCM_BD_HEADER_LEN (4)

enum
{
    LPCM_VOB,
    LPCM_AOB,
    LPCM_BD,
};

typedef struct
{
    unsigned i_channels;
    bool     b_used;
    unsigned pi_position[6];
} aob_group_t;

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static void *DecodeFrame  ( decoder_t *, block_t ** );

/* */
static int VobHeader( unsigned *pi_rate,
                      unsigned *pi_channels, unsigned *pi_original_channels,
                      unsigned *pi_bits,
                      const uint8_t *p_header );
static void VobExtract( aout_buffer_t *, block_t *, unsigned i_bits );
/* */
static int AobHeader( unsigned *pi_rate,
                      unsigned *pi_channels, unsigned *pi_layout,
                      unsigned *pi_bits,
                      unsigned *pi_padding,
                      aob_group_t g[2],
                      const uint8_t *p_header );
static void AobExtract( aout_buffer_t *, block_t *, unsigned i_bits, aob_group_t p_group[2] );
/* */
static int BdHeader( unsigned *pi_rate,
                     unsigned *pi_channels, unsigned *pi_original_channels,
                     unsigned *pi_bits,
                     const uint8_t *p_header );
static void BdExtract( aout_buffer_t *, block_t * );


/*****************************************************************************
 * OpenCommon:
 *****************************************************************************/
static int OpenCommon( vlc_object_t *p_this, bool b_packetizer )
{
    decoder_t *p_dec = (decoder_t*)p_this;
    decoder_sys_t *p_sys;
    int i_type;
    int i_header_size;

    switch( p_dec->fmt_in.i_codec )
    {
    /* DVD LPCM */
    case VLC_CODEC_DVD_LPCM:
        i_type = LPCM_VOB;
        i_header_size = LPCM_VOB_HEADER_LEN;
        break;
    /* DVD-Audio LPCM */
    case VLC_CODEC_DVDA_LPCM:
        i_type = LPCM_AOB;
        i_header_size = LPCM_AOB_HEADER_LEN;
        break;
    /* BD LPCM */
    case VLC_CODEC_BD_LPCM:
        i_type = LPCM_BD;
        i_header_size = LPCM_BD_HEADER_LEN;
        break;
    default:
        return VLC_EGENERIC;
    }

    /* Allocate the memory needed to store the decoder's structure */
    if( ( p_dec->p_sys = p_sys = malloc(sizeof(decoder_sys_t)) ) == NULL )
        return VLC_ENOMEM;

    /* Misc init */
    p_sys->b_packetizer = b_packetizer;
    date_Set( &p_sys->end_date, 0 );
    p_sys->i_type = i_type;
    p_sys->i_header_size = i_header_size;

    /* Set output properties */
    p_dec->fmt_out.i_cat = AUDIO_ES;

    if( b_packetizer )
    {
        switch( i_type )
        {
        case LPCM_VOB:
            p_dec->fmt_out.i_codec = VLC_CODEC_DVD_LPCM;
            break;
        case LPCM_AOB:
            p_dec->fmt_out.i_codec = VLC_CODEC_DVDA_LPCM;
            break;
        default:
            assert(0);
        case LPCM_BD:
            p_dec->fmt_out.i_codec = VLC_CODEC_BD_LPCM;
            break;
        }
    }
    else
    {
        switch( p_dec->fmt_out.audio.i_bitspersample )
        {
        case 24:
        case 20:
            p_dec->fmt_out.i_codec = VLC_CODEC_S24B;
            p_dec->fmt_out.audio.i_bitspersample = 24;
            break;
        default:
            p_dec->fmt_out.i_codec = VLC_CODEC_S16B;
            p_dec->fmt_out.audio.i_bitspersample = 16;
            break;
        }
    }

    /* Set callback */
    p_dec->pf_decode_audio = (aout_buffer_t *(*)(decoder_t *, block_t **))
        DecodeFrame;
    p_dec->pf_packetize    = (block_t *(*)(decoder_t *, block_t **))
        DecodeFrame;

    return VLC_SUCCESS;
}
static int OpenDecoder( vlc_object_t *p_this )
{
    return OpenCommon( p_this, false );
}
static int OpenPacketizer( vlc_object_t *p_this )
{
    return OpenCommon( p_this, true );
}

/*****************************************************************************
 * DecodeFrame: decodes an lpcm frame.
 ****************************************************************************
 * Beware, this function must be fed with complete frames (PES packet).
 *****************************************************************************/
static void *DecodeFrame( decoder_t *p_dec, block_t **pp_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    block_t       *p_block;
    unsigned int  i_rate = 0, i_original_channels = 0, i_channels = 0, i_bits = 0;
    int           i_frame_length;

    if( !pp_block || !*pp_block ) return NULL;

    p_block = *pp_block;
    *pp_block = NULL; /* So the packet doesn't get re-sent */

    /* Date management */
    if( p_block->i_pts > VLC_TS_INVALID &&
        p_block->i_pts != date_Get( &p_sys->end_date ) )
    {
        date_Set( &p_sys->end_date, p_block->i_pts );
    }

    if( !date_Get( &p_sys->end_date ) )
    {
        /* We've just started the stream, wait for the first PTS. */
        block_Release( p_block );
        return NULL;
    }

    if( p_block->i_buffer <= p_sys->i_header_size )
    {
        msg_Err(p_dec, "frame is too short");
        block_Release( p_block );
        return NULL;
    }

    int i_ret;
    unsigned i_padding = 0;
    aob_group_t p_aob_group[2];
    switch( p_sys->i_type )
    {
    case LPCM_VOB:
        i_ret = VobHeader( &i_rate, &i_channels, &i_original_channels, &i_bits,
                           p_block->p_buffer );
        break;
    case LPCM_AOB:
        i_ret = AobHeader( &i_rate, &i_channels, &i_original_channels, &i_bits, &i_padding,
                           p_aob_group,
                           p_block->p_buffer );
        break;
    case LPCM_BD:
        i_ret = BdHeader( &i_rate, &i_channels, &i_original_channels, &i_bits,
                          p_block->p_buffer );
        break;
    default:
        abort();
    }

    if( i_ret || p_block->i_buffer <= p_sys->i_header_size + i_padding )
    {
        msg_Warn( p_dec, "no frame sync or too small frame" );
        block_Release( p_block );
        return NULL;
    }

    /* Set output properties */
    if( p_dec->fmt_out.audio.i_rate != i_rate )
    {
        date_Init( &p_sys->end_date, i_rate, 1 );
        date_Set( &p_sys->end_date, p_block->i_pts );
    }
    p_dec->fmt_out.audio.i_rate = i_rate;
    p_dec->fmt_out.audio.i_channels = i_channels;
    p_dec->fmt_out.audio.i_original_channels = i_original_channels;
    p_dec->fmt_out.audio.i_physical_channels = i_original_channels & AOUT_CHAN_PHYSMASK;

    i_frame_length = (p_block->i_buffer - p_sys->i_header_size - i_padding) / i_channels * 8 / i_bits;

    if( p_sys->b_packetizer )
    {
        p_block->i_pts = p_block->i_dts = date_Get( &p_sys->end_date );
        p_block->i_length =
            date_Increment( &p_sys->end_date, i_frame_length ) -
            p_block->i_pts;

        /* Just pass on the incoming frame */
        return p_block;
    }
    else
    {
        /* */
        if( i_bits == 16 )
        {
            p_dec->fmt_out.i_codec = VLC_CODEC_S16B;
            p_dec->fmt_out.audio.i_bitspersample = 16;
        }
        else
        {
            p_dec->fmt_out.i_codec = VLC_CODEC_S24B;
            p_dec->fmt_out.audio.i_bitspersample = 24;
        }

        /* */
        aout_buffer_t *p_aout_buffer;
        p_aout_buffer = decoder_NewAudioBuffer( p_dec, i_frame_length );
        if( !p_aout_buffer )
            return NULL;

        p_aout_buffer->i_pts = date_Get( &p_sys->end_date );
        p_aout_buffer->i_length =
            date_Increment( &p_sys->end_date, i_frame_length )
            - p_aout_buffer->i_pts;

        p_block->p_buffer += p_sys->i_header_size + i_padding;
        p_block->i_buffer -= p_sys->i_header_size + i_padding;

        switch( p_sys->i_type )
        {
        case LPCM_VOB:
            VobExtract( p_aout_buffer, p_block, i_bits );
            break;
        case LPCM_AOB:
            AobExtract( p_aout_buffer, p_block, i_bits, p_aob_group );
            break;
        default:
            assert(0);
        case LPCM_BD:
            BdExtract( p_aout_buffer, p_block );
            break;
        }

        block_Release( p_block );
        return p_aout_buffer;
    }
}

/*****************************************************************************
 * CloseCommon : lpcm decoder destruction
 *****************************************************************************/
static void CloseCommon( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;
    free( p_dec->p_sys );
}

/*****************************************************************************
 *
 *****************************************************************************/
static int VobHeader( unsigned *pi_rate,
                      unsigned *pi_channels, unsigned *pi_original_channels,
                      unsigned *pi_bits,
                      const uint8_t *p_header )
{
    const uint8_t i_header = p_header[4];

    switch( (i_header >> 4) & 0x3 )
    {
    case 0:
        *pi_rate = 48000;
        break;
    case 1:
        *pi_rate = 96000;
        break;
    case 2:
        *pi_rate = 44100;
        break;
    case 3:
        *pi_rate = 32000;
        break;
    }

    *pi_channels = (i_header & 0x7) + 1;
    switch( *pi_channels - 1 )
    {
    case 0:
        *pi_original_channels = AOUT_CHAN_CENTER;
        break;
    case 1:
        *pi_original_channels = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT;
        break;
    case 2:
        /* This is unsure. */
        *pi_original_channels = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_LFE;
        break;
    case 3:
        *pi_original_channels = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT
                               | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT;
        break;
    case 4:
        /* This is unsure. */
        *pi_original_channels = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT
                               | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT
                               | AOUT_CHAN_LFE;
        break;
    case 5:
        *pi_original_channels = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT
                               | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT
                               | AOUT_CHAN_CENTER | AOUT_CHAN_LFE;
        break;
    case 6:
        *pi_original_channels = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT
                               | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT
                               | AOUT_CHAN_CENTER | AOUT_CHAN_MIDDLELEFT
                               | AOUT_CHAN_MIDDLERIGHT;
        break;
    case 7:
        *pi_original_channels = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT
                               | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT
                               | AOUT_CHAN_CENTER | AOUT_CHAN_MIDDLELEFT
                               | AOUT_CHAN_MIDDLERIGHT | AOUT_CHAN_LFE;
        break;
    }

    switch( (i_header >> 6) & 0x3 )
    {
    case 2:
        *pi_bits = 24;
        break;
    case 1:
        *pi_bits = 20;
        break;
    case 0:
    default:
        *pi_bits = 16;
        break;
    }

    /* Check frame sync and drop it. */
    if( p_header[5] != 0x80 )
        return -1;
    return 0;
}

static const unsigned p_aob_group1[21][6] = {
    { AOUT_CHAN_CENTER, 0 },
    { AOUT_CHAN_LEFT,   AOUT_CHAN_RIGHT, 0 },
    { AOUT_CHAN_LEFT,   AOUT_CHAN_RIGHT, 0 },
    { AOUT_CHAN_LEFT,   AOUT_CHAN_RIGHT, 0 },
    { AOUT_CHAN_LEFT,   AOUT_CHAN_RIGHT, 0 },
    { AOUT_CHAN_LEFT,   AOUT_CHAN_RIGHT, 0 },
    { AOUT_CHAN_LEFT,   AOUT_CHAN_RIGHT, 0 },
    { AOUT_CHAN_LEFT,   AOUT_CHAN_RIGHT, 0 },
    { AOUT_CHAN_LEFT,   AOUT_CHAN_RIGHT, 0 },
    { AOUT_CHAN_LEFT,   AOUT_CHAN_RIGHT, 0 },
    { AOUT_CHAN_LEFT,   AOUT_CHAN_RIGHT, 0 },
    { AOUT_CHAN_LEFT,   AOUT_CHAN_RIGHT, 0 },
    { AOUT_CHAN_LEFT,   AOUT_CHAN_RIGHT, 0 },
    { AOUT_CHAN_LEFT,   AOUT_CHAN_RIGHT, AOUT_CHAN_CENTER,   0 },
    { AOUT_CHAN_LEFT,   AOUT_CHAN_RIGHT, AOUT_CHAN_CENTER,   0 },
    { AOUT_CHAN_LEFT,   AOUT_CHAN_RIGHT, AOUT_CHAN_CENTER,   0 },
    { AOUT_CHAN_LEFT,   AOUT_CHAN_RIGHT, AOUT_CHAN_CENTER,   0 },
    { AOUT_CHAN_LEFT,   AOUT_CHAN_RIGHT, AOUT_CHAN_CENTER,   0 },
    { AOUT_CHAN_LEFT,   AOUT_CHAN_RIGHT, AOUT_CHAN_REARLEFT, AOUT_CHAN_REARRIGHT, 0  },
    { AOUT_CHAN_LEFT,   AOUT_CHAN_RIGHT, AOUT_CHAN_REARLEFT, AOUT_CHAN_REARRIGHT, 0  },
    { AOUT_CHAN_LEFT,   AOUT_CHAN_RIGHT, AOUT_CHAN_REARLEFT, AOUT_CHAN_REARRIGHT, 0  },
};
static const unsigned p_aob_group2[21][6] = {
    { 0 },
    { 0 },
    { AOUT_CHAN_REARCENTER, 0 },
    { AOUT_CHAN_REARLEFT,   AOUT_CHAN_REARRIGHT,    0 },
    { AOUT_CHAN_LFE,        0 },
    { AOUT_CHAN_LFE,        AOUT_CHAN_REARCENTER,   0 },
    { AOUT_CHAN_LFE,        AOUT_CHAN_REARLEFT,     AOUT_CHAN_REARRIGHT,    0 },
    { AOUT_CHAN_CENTER,     0 },
    { AOUT_CHAN_CENTER,     AOUT_CHAN_REARCENTER, 0 },
    { AOUT_CHAN_CENTER,     AOUT_CHAN_REARLEFT,   AOUT_CHAN_REARRIGHT,    0 },
    { AOUT_CHAN_CENTER,     AOUT_CHAN_LFE,        0 },
    { AOUT_CHAN_CENTER,     AOUT_CHAN_LFE,        AOUT_CHAN_REARCENTER,   0 },
    { AOUT_CHAN_CENTER,     AOUT_CHAN_LFE,        AOUT_CHAN_REARLEFT,     AOUT_CHAN_REARRIGHT,    0 },
    { AOUT_CHAN_REARCENTER, 0 },
    { AOUT_CHAN_REARLEFT,   AOUT_CHAN_REARRIGHT,    0 },
    { AOUT_CHAN_LFE,        0 },
    { AOUT_CHAN_LFE,        AOUT_CHAN_REARCENTER,   0 },
    { AOUT_CHAN_LFE,        AOUT_CHAN_REARLEFT,     AOUT_CHAN_REARRIGHT,    0 },
    { AOUT_CHAN_LFE,        0 },
    { AOUT_CHAN_CENTER,     0 },
    { AOUT_CHAN_CENTER,     AOUT_CHAN_LFE,          0 },
};

static int AobHeader( unsigned *pi_rate,
                      unsigned *pi_channels, unsigned *pi_layout,
                      unsigned *pi_bits,
                      unsigned *pi_padding,
                      aob_group_t g[2],
                      const uint8_t *p_header )
{
    const unsigned i_header_size = GetWBE( &p_header[1] );
    if( i_header_size + 3 < LPCM_AOB_HEADER_LEN )
        return VLC_EGENERIC;

    *pi_padding = 3+i_header_size - LPCM_AOB_HEADER_LEN;

    const int i_index_size_g1 = (p_header[6] >> 4) & 0x0f;
    const int i_index_size_g2 = (p_header[6]     ) & 0x0f;
    const int i_index_rate_g1 = (p_header[7] >> 4) & 0x0f;
    const int i_index_rate_g2 = (p_header[7]     ) & 0x0f;
    const int i_assignment     = p_header[9];

    /* Validate */
    if( i_index_size_g1 > 0x02 ||
        ( i_index_size_g2 != 0x0f && i_index_size_g2 > 0x02 ) )
        return VLC_EGENERIC;
    if( (i_index_rate_g1 & 0x07) > 0x02 ||
        ( i_index_rate_g2 != 0x0f && (i_index_rate_g1 & 0x07) > 0x02 ) )
        return VLC_EGENERIC;
    if( i_assignment > 20 )
        return VLC_EGENERIC;

    /* */
    *pi_bits = 16 + 4 * i_index_size_g1;
    if( i_index_rate_g1 & 0x08 )
        *pi_rate = 44100 << (i_index_rate_g1 & 0x07);
    else
        *pi_rate = 48000 << (i_index_rate_g1 & 0x07);


    /* Group1 */
    unsigned i_channels1 = 0;
    unsigned i_layout1 = 0;
    for( int i = 0; p_aob_group1[i_assignment][i] != 0; i++ )
    {
        i_channels1++;
        i_layout1 |= p_aob_group1[i_assignment][i];
    }
    /* Group2 */
    unsigned i_channels2 = 0;
    unsigned i_layout2 = 0;
    if( i_index_size_g2 != 0x0f && i_index_rate_g2 != 0x0f )
    {
        for( int i = 0; p_aob_group2[i_assignment][i] != 0; i++ )
        {
            i_channels2++;
            i_layout2 |= p_aob_group2[i_assignment][i];
        }
        assert( (i_layout1 & i_layout2) == 0 );
    }
    /* It is enabled only when presents and compatible wih group1 */
    const bool b_group2_used = i_index_size_g1 == i_index_size_g2 &&
                               i_index_rate_g1 == i_index_rate_g2;

    /* */
    *pi_channels = i_channels1 + ( b_group2_used ? i_channels2 : 0 );
    *pi_layout   = i_layout1   | ( b_group2_used ? i_layout2   : 0 );

    /* */
    for( unsigned i = 0; i < 2; i++ )
    {
        const unsigned *p_aob = i == 0 ? p_aob_group1[i_assignment] :
                                         p_aob_group2[i_assignment];
        g[i].i_channels = i == 0 ? i_channels1 :
                                   i_channels2;

        g[i].b_used = i == 0 || b_group2_used;
        if( !g[i].b_used )
            continue;
        for( unsigned j = 0; j < g[i].i_channels; j++ )
        {
            g[i].pi_position[j] = 0;
            for( int k = 0; pi_vlc_chan_order_wg4[k] != 0; k++ )
            {
                const unsigned i_channel = pi_vlc_chan_order_wg4[k];
                if( i_channel == p_aob[j] )
                    break;
                if( (*pi_layout) & i_channel )
                    g[i].pi_position[j]++;
            }
        }
    }
    return VLC_SUCCESS;
}

static int BdHeader( unsigned *pi_rate,
                     unsigned *pi_channels, unsigned *pi_original_channels,
                     unsigned *pi_bits,
                     const uint8_t *p_header )
{
    const uint32_t h = GetDWBE( p_header );
    switch( ( h & 0xf000) >> 12 )
    {
    case 1:
        *pi_channels = 1;
        *pi_original_channels = AOUT_CHAN_CENTER;
        break;
    case 3:
        *pi_channels = 2;
        *pi_original_channels = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT;
        break;
    case 4:
        *pi_channels = 3;
        *pi_original_channels = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER;
        break;
    case 5:
        *pi_channels = 3;
        *pi_original_channels = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_REARCENTER;
        break;
    case 6:
        *pi_channels = 4;
        *pi_original_channels = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER |
                                AOUT_CHAN_REARCENTER;
        break;
    case 7:
        *pi_channels = 4;
        *pi_original_channels = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT |
                                AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT;
        break;
    case 8:
        *pi_channels = 5;
        *pi_original_channels = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER |
                                AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT;
        break;
    case 9:
        *pi_channels = 6;
        *pi_original_channels = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER |
                                AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT |
                                AOUT_CHAN_LFE;
        break;
    case 10:
        *pi_channels = 7;
        *pi_original_channels = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER |
                                AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT |
                                AOUT_CHAN_MIDDLELEFT | AOUT_CHAN_MIDDLERIGHT;
        break;
    case 11:
        *pi_channels = 8;
        *pi_original_channels = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER |
                                AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT |
                                AOUT_CHAN_MIDDLELEFT | AOUT_CHAN_MIDDLERIGHT |
                                AOUT_CHAN_LFE;
        break;

    default:
        return -1;
    }
    switch( (h >> 6) & 0x03 )
    {
    case 1:
        *pi_bits = 16;
        break;
    case 2: /* 20 bits but samples are stored on 24 bits */
    case 3: /* 24 bits */
        *pi_bits = 24;
        break;
    default:
        return -1;
    }
    switch( (h >> 8) & 0x0f ) 
    {
    case 1:
        *pi_rate = 48000;
        break;
    case 4:
        *pi_rate = 96000;
        break;
    case 5:
        *pi_rate = 192000;
        break;
    default:
        return -1;
    }
    return 0;
}

static void VobExtract( aout_buffer_t *p_aout_buffer, block_t *p_block,
                        unsigned i_bits )
{
    uint8_t *p_out = p_aout_buffer->p_buffer;

    /* 20/24 bits LPCM use special packing */
    if( i_bits == 24 )
    {
        while( p_block->i_buffer / 12 )
        {
            /* Sample 1 */
            p_out[0] = p_block->p_buffer[0];
            p_out[1] = p_block->p_buffer[1];
            p_out[2] = p_block->p_buffer[8];
            /* Sample 2 */
            p_out[3] = p_block->p_buffer[2];
            p_out[4] = p_block->p_buffer[3];
            p_out[5] = p_block->p_buffer[9];
            /* Sample 3 */
            p_out[6] = p_block->p_buffer[4];
            p_out[7] = p_block->p_buffer[5];
            p_out[8] = p_block->p_buffer[10];
            /* Sample 4 */
            p_out[9] = p_block->p_buffer[6];
            p_out[10] = p_block->p_buffer[7];
            p_out[11] = p_block->p_buffer[11];

            p_block->i_buffer -= 12;
            p_block->p_buffer += 12;
            p_out += 12;
        }
    }
    else if( i_bits == 20 )
    {
        while( p_block->i_buffer / 10 )
        {
            /* Sample 1 */
            p_out[0] = p_block->p_buffer[0];
            p_out[1] = p_block->p_buffer[1];
            p_out[2] = p_block->p_buffer[8] & 0xF0;
            /* Sample 2 */
            p_out[3] = p_block->p_buffer[2];
            p_out[4] = p_block->p_buffer[3];
            p_out[5] = p_block->p_buffer[8] << 4;
            /* Sample 3 */
            p_out[6] = p_block->p_buffer[4];
            p_out[7] = p_block->p_buffer[5];
            p_out[8] = p_block->p_buffer[9] & 0xF0;
            /* Sample 4 */
            p_out[9] = p_block->p_buffer[6];
            p_out[10] = p_block->p_buffer[7];
            p_out[11] = p_block->p_buffer[9] << 4;

            p_block->i_buffer -= 10;
            p_block->p_buffer += 10;
            p_out += 12;
        }
    }
    else
    {
        assert( i_bits == 16 );
        memcpy( p_out, p_block->p_buffer, p_block->i_buffer );
    }
}
static void AobExtract( aout_buffer_t *p_aout_buffer,
                        block_t *p_block, unsigned i_bits, aob_group_t p_group[2] )
{
    const unsigned i_channels = p_group[0].i_channels +
                                ( p_group[1].b_used ? p_group[1].i_channels : 0 );
    uint8_t *p_out = p_aout_buffer->p_buffer;

    while( p_block->i_buffer > 0 )
    {
        for( int i = 0; i < 2; i++ )
        {
            const aob_group_t *g = &p_group[1-i];
            const unsigned int i_group_size = 2 * g->i_channels * i_bits / 8;

            if( p_block->i_buffer < i_group_size )
            {
                p_block->i_buffer = 0;
                break;
            }
            for( unsigned n = 0; n < 2; n++ )
            {
                for( unsigned j = 0; j < g->i_channels && g->b_used; j++ )
                {
                    const int i_src = n * g->i_channels + j;
                    const int i_dst = n * i_channels + g->pi_position[j];

                    if( i_bits == 24 )
                    {
                        p_out[3*i_dst+0] = p_block->p_buffer[2*i_src+0];
                        p_out[3*i_dst+1] = p_block->p_buffer[2*i_src+1];
                        p_out[3*i_dst+2] = p_block->p_buffer[4*g->i_channels+i_src];
                    }
                    else if( i_bits == 20 )
                    {
                        p_out[3*i_dst+0] = p_block->p_buffer[2*i_src+0];
                        p_out[3*i_dst+1] = p_block->p_buffer[2*i_src+1];
                        if( n == 0 )
                            p_out[3*i_dst+2] = (p_block->p_buffer[4*g->i_channels+i_src]     ) & 0xf0;
                        else
                            p_out[3*i_dst+2] = (p_block->p_buffer[4*g->i_channels+i_src] << 4) & 0xf0;
                    }
                    else
                    {
                        assert( i_bits == 16 );
                        p_out[2*i_dst+0] = p_block->p_buffer[2*i_src+0];
                        p_out[2*i_dst+1] = p_block->p_buffer[2*i_src+1];
                    }
                }
            }

            p_block->i_buffer -= i_group_size;
            p_block->p_buffer += i_group_size;
        }
        /* */
        p_out += (i_bits == 16 ? 2 : 3) * i_channels * 2;

    }
}
static void BdExtract( aout_buffer_t *p_aout_buffer, block_t *p_block )
{
    memcpy( p_aout_buffer->p_buffer, p_block->p_buffer, p_block->i_buffer );
}

