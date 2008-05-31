/*****************************************************************************
 * lpcm.c: lpcm decoder/packetizer module
 *****************************************************************************
 * Copyright (C) 1999-2005 the VideoLAN team
 * $Id$
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
 *          Henri Fallon <henri@videolan.org>
 *          Christophe Massiot <massiot@via.ecp.fr>
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

};

/*
 * LPCM header :
 * - PES header
 * - private stream ID (16 bits) == 0xA0 -> not in the bitstream
 *
 * - frame number (8 bits)
 * - unknown (16 bits) == 0x0003 ?
 * - unknown (4 bits)
 * - current frame (4 bits)
 * - unknown (2 bits)
 * - frequency (2 bits) 0 == 48 kHz, 1 == 32 kHz, 2 == ?, 3 == ?
 * - unknown (1 bit)
 * - number of channels - 1 (3 bits) 1 == 2 channels
 * - start code (8 bits) == 0x80
 */

#define LPCM_HEADER_LEN 6

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  OpenDecoder   ( vlc_object_t * );
static int  OpenPacketizer( vlc_object_t * );
static void CloseDecoder  ( vlc_object_t * );

static void *DecodeFrame  ( decoder_t *, block_t ** );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();

    set_category( CAT_INPUT );
    set_subcategory( SUBCAT_INPUT_ACODEC );
    set_description( N_("Linear PCM audio decoder") );
    set_capability( "decoder", 100 );
    set_callbacks( OpenDecoder, CloseDecoder );

    add_submodule();
    set_description( N_("Linear PCM audio packetizer") );
    set_capability( "packetizer", 100 );
    set_callbacks( OpenPacketizer, CloseDecoder );

vlc_module_end();

/*****************************************************************************
 * OpenDecoder: probe the decoder and return score
 *****************************************************************************/
static int OpenDecoder( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;
    decoder_sys_t *p_sys;

    if( p_dec->fmt_in.i_codec != VLC_FOURCC('l','p','c','m')
         && p_dec->fmt_in.i_codec != VLC_FOURCC('l','p','c','b') )
    {
        return VLC_EGENERIC;
    }

    /* Allocate the memory needed to store the decoder's structure */
    if( ( p_dec->p_sys = p_sys =
          (decoder_sys_t *)malloc(sizeof(decoder_sys_t)) ) == NULL )
        return VLC_ENOMEM;

    /* Misc init */
    p_sys->b_packetizer = false;
    aout_DateSet( &p_sys->end_date, 0 );

    /* Set output properties */
    p_dec->fmt_out.i_cat = AUDIO_ES;

    if( p_dec->fmt_out.audio.i_bitspersample == 24 )
    {
        p_dec->fmt_out.i_codec = VLC_FOURCC('s','2','4','b');
    }
    else
    {
        p_dec->fmt_out.i_codec = VLC_FOURCC('s','1','6','b');
        p_dec->fmt_out.audio.i_bitspersample = 16;
    }

    /* Set callback */
    p_dec->pf_decode_audio = (aout_buffer_t *(*)(decoder_t *, block_t **))
        DecodeFrame;
    p_dec->pf_packetize    = (block_t *(*)(decoder_t *, block_t **))
        DecodeFrame;

    return VLC_SUCCESS;
}

static int OpenPacketizer( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;

    int i_ret = OpenDecoder( p_this );

    if( i_ret != VLC_SUCCESS ) return i_ret;

    p_dec->p_sys->b_packetizer = true;

    p_dec->fmt_out.i_codec = VLC_FOURCC('l','p','c','m');

    return i_ret;
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
    unsigned int  i_rate = 0, i_original_channels = 0, i_channels = 0;
    int           i_frame_length, i_bitspersample;
    uint8_t       i_header;

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

    if( p_block->i_buffer <= LPCM_HEADER_LEN )
    {
        msg_Err(p_dec, "frame is too short");
        block_Release( p_block );
        return NULL;
    }

    i_header = p_block->p_buffer[4];
    switch ( (i_header >> 4) & 0x3 )
    {
    case 0:
        i_rate = 48000;
        break;
    case 1:
        i_rate = 96000;
        break;
    case 2:
        i_rate = 44100;
        break;
    case 3:
        i_rate = 32000;
        break;
    }

    i_channels = (i_header & 0x7);
    switch ( i_channels )
    {
    case 0:
        i_original_channels = AOUT_CHAN_CENTER;
        break;
    case 1:
        i_original_channels = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT;
        break;
    case 2:
        /* This is unsure. */
        i_original_channels = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_LFE;
        break;
    case 3:
        i_original_channels = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT
                               | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT;
        break;
    case 4:
        /* This is unsure. */
        i_original_channels = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT
                               | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT
                               | AOUT_CHAN_LFE;
        break;
    case 5:
        i_original_channels = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT
                               | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT
                               | AOUT_CHAN_CENTER | AOUT_CHAN_LFE;
        break;
    case 6:
        i_original_channels = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT
                               | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT
                               | AOUT_CHAN_CENTER | AOUT_CHAN_MIDDLELEFT
                               | AOUT_CHAN_MIDDLERIGHT;
        break;
    case 7:
        i_original_channels = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT
                               | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT
                               | AOUT_CHAN_CENTER | AOUT_CHAN_MIDDLELEFT
                               | AOUT_CHAN_MIDDLERIGHT | AOUT_CHAN_LFE;
        break;
    }

    switch ( (i_header >> 6) & 0x3 )
    {
    case 2:
        p_dec->fmt_out.i_codec = VLC_FOURCC('s','2','4','b');
        p_dec->fmt_out.audio.i_bitspersample = 24;
        i_bitspersample = 24;
        break;
    case 1:
        p_dec->fmt_out.i_codec = VLC_FOURCC('s','2','4','b');
        p_dec->fmt_out.audio.i_bitspersample = 24;
        i_bitspersample = 20;
        break;
    case 0:
    default:
        p_dec->fmt_out.i_codec = VLC_FOURCC('s','1','6','b');
        p_dec->fmt_out.audio.i_bitspersample = 16;
        i_bitspersample = 16;
        break;
    }

    /* Check frame sync and drop it. */
    if( p_block->p_buffer[5] != 0x80 )
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
    p_dec->fmt_out.audio.i_channels = i_channels + 1;
    p_dec->fmt_out.audio.i_original_channels = i_original_channels;
    p_dec->fmt_out.audio.i_physical_channels
        = i_original_channels & AOUT_CHAN_PHYSMASK;

    i_frame_length = (p_block->i_buffer - LPCM_HEADER_LEN) /
        p_dec->fmt_out.audio.i_channels * 8 / i_bitspersample;

    if( p_sys->b_packetizer )
    {
        p_dec->fmt_out.i_codec = VLC_FOURCC('l','p','c','m');
        p_block->i_pts = p_block->i_dts = aout_DateGet( &p_sys->end_date );
        p_block->i_length =
            aout_DateIncrement( &p_sys->end_date, i_frame_length ) -
            p_block->i_pts;

        /* Just pass on the incoming frame */
        return p_block;
    }
    else
    {
        aout_buffer_t *p_aout_buffer;
        p_aout_buffer = p_dec->pf_aout_buffer_new( p_dec, i_frame_length );
        if( p_aout_buffer == NULL ) return NULL;

        p_aout_buffer->start_date = aout_DateGet( &p_sys->end_date );
        p_aout_buffer->end_date =
            aout_DateIncrement( &p_sys->end_date, i_frame_length );

        p_block->p_buffer += LPCM_HEADER_LEN;
        p_block->i_buffer -= LPCM_HEADER_LEN;

        /* 20/24 bits LPCM use special packing */
        if( i_bitspersample == 24 )
        {
            uint8_t *p_out = p_aout_buffer->p_buffer;

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
        else if( i_bitspersample == 20 )
        {
            uint8_t *p_out = p_aout_buffer->p_buffer;

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
            memcpy( p_aout_buffer->p_buffer,
                    p_block->p_buffer, p_block->i_buffer );
        }

        block_Release( p_block );
        return p_aout_buffer;
    }
}

/*****************************************************************************
 * CloseDecoder : lpcm decoder destruction
 *****************************************************************************/
static void CloseDecoder( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;
    free( p_dec->p_sys );
}
