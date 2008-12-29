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
    audio_date_t end_date;

    /* */
    unsigned i_header_size;
    bool b_dvd;
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
 * LPCM BD header :
 * - unkown (16 bits)
 * - number of channels (4 bits)
 * - frequency (4 bits)
 * - bits per sample (2 bits)
 * - unknown (6 bits)
 */

#define LPCM_DVD_HEADER_LEN (6)
#define LPCM_BD_HEADER_LEN (4)

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static void *DecodeFrame  ( decoder_t *, block_t ** );
static int DvdHeader( unsigned *pi_rate,
                      unsigned *pi_channels, unsigned *pi_original_channels,
                      unsigned *pi_bits,
                      const uint8_t *p_header );
static void DvdExtract( aout_buffer_t *, block_t *, unsigned i_bits );

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
    bool b_dvd;

    switch( p_dec->fmt_in.i_codec )
    {
    /* DVD LPCM */
    case VLC_FOURCC('l','p','c','m'):
    case VLC_FOURCC('l','p','c','b'):
        b_dvd = true;
        break;
    /* BD LPCM */
    case VLC_FOURCC('b','p','c','m'):
        b_dvd = false;
        break;
    default:
        return VLC_EGENERIC;
    }

    /* Allocate the memory needed to store the decoder's structure */
    if( ( p_dec->p_sys = p_sys = malloc(sizeof(decoder_sys_t)) ) == NULL )
        return VLC_ENOMEM;

    /* Misc init */
    p_sys->b_packetizer = b_packetizer;
    aout_DateSet( &p_sys->end_date, 0 );
    p_sys->i_header_size = b_dvd ? LPCM_DVD_HEADER_LEN : LPCM_BD_HEADER_LEN;
    p_sys->b_dvd = b_dvd;

    /* Set output properties */
    p_dec->fmt_out.i_cat = AUDIO_ES;

    if( b_packetizer )
    {
        p_dec->fmt_out.i_codec = b_dvd ? VLC_FOURCC('l','p','c','m') : VLC_FOURCC('b','p','c','m');
    }
    else
    {
        switch( p_dec->fmt_out.audio.i_bitspersample )
        {
        case 24:
        case 20:
            p_dec->fmt_out.i_codec = VLC_FOURCC('s','2','4','b');
            p_dec->fmt_out.audio.i_bitspersample = 24;
            break;
        default:
            p_dec->fmt_out.i_codec = VLC_FOURCC('s','1','6','b');
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
    if( p_block->i_pts > 0 &&
        p_block->i_pts != aout_DateGet( &p_sys->end_date ) )
    {
        aout_DateSet( &p_sys->end_date, p_block->i_pts );
    }

    if( !aout_DateGet( &p_sys->end_date ) )
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
    if( p_sys->b_dvd )
        i_ret = DvdHeader( &i_rate, &i_channels, &i_original_channels, &i_bits,
                           p_block->p_buffer );
    else
        i_ret = BdHeader( &i_rate, &i_channels, &i_original_channels, &i_bits,
                          p_block->p_buffer );

    if( i_ret )
    {
        msg_Warn( p_dec, "no frame sync" );
        block_Release( p_block );
        return NULL;
    }

    /* Set output properties */
    if( p_dec->fmt_out.audio.i_rate != i_rate )
    {
        aout_DateInit( &p_sys->end_date, i_rate );
        aout_DateSet( &p_sys->end_date, p_block->i_pts );
    }
    p_dec->fmt_out.audio.i_rate = i_rate;
    p_dec->fmt_out.audio.i_channels = i_channels;
    p_dec->fmt_out.audio.i_original_channels = i_original_channels;
    p_dec->fmt_out.audio.i_physical_channels = i_original_channels & AOUT_CHAN_PHYSMASK;

    i_frame_length = (p_block->i_buffer - p_sys->i_header_size) / i_channels * 8 / i_bits;

    if( p_sys->b_packetizer )
    {
        p_block->i_pts = p_block->i_dts = aout_DateGet( &p_sys->end_date );
        p_block->i_length =
            aout_DateIncrement( &p_sys->end_date, i_frame_length ) -
            p_block->i_pts;

        /* Just pass on the incoming frame */
        return p_block;
    }
    else
    {
        /* */
        if( i_bits == 16 )
        {
            p_dec->fmt_out.i_codec = VLC_FOURCC('s','1','6','b');
            p_dec->fmt_out.audio.i_bitspersample = 16;
        }
        else
        {
            p_dec->fmt_out.i_codec = VLC_FOURCC('s','2','4','b');
            p_dec->fmt_out.audio.i_bitspersample = 24;
        }

        /* */
        aout_buffer_t *p_aout_buffer;
        p_aout_buffer = decoder_NewAudioBuffer( p_dec, i_frame_length );
        if( !p_aout_buffer )
            return NULL;

        p_aout_buffer->start_date = aout_DateGet( &p_sys->end_date );
        p_aout_buffer->end_date =
            aout_DateIncrement( &p_sys->end_date, i_frame_length );

        p_block->p_buffer += p_sys->i_header_size;
        p_block->i_buffer -= p_sys->i_header_size;

        if( p_sys->b_dvd )
            DvdExtract( p_aout_buffer, p_block, i_bits );
        else
            BdExtract( p_aout_buffer, p_block );

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
static int DvdHeader( unsigned *pi_rate,
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

static void DvdExtract( aout_buffer_t *p_aout_buffer, block_t *p_block,
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
static void BdExtract( aout_buffer_t *p_aout_buffer, block_t *p_block )
{
    memcpy( p_aout_buffer->p_buffer, p_block->p_buffer, p_block->i_buffer );
}


