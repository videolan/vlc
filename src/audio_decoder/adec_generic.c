/*****************************************************************************
 * adec_generic.c: MPEG audio decoder
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: adec_generic.c,v 1.4 2001/03/21 13:42:34 sam Exp $
 *
 * Authors: Michel Kaempf <maxx@via.ecp.fr>
 *          Michel Lespinasse <walken@via.ecp.fr>
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

#include "defs.h"

#include "config.h"
#include "common.h"
#include "threads.h"
#include "mtime.h"
#include "stream_control.h"
#include "input_ext-dec.h"

#include "adec_generic.h"
#include "audio_decoder.h"
#include "adec_math.h"                                     /* DCT32(), PCM() */
#include "adec_layer1.h"
#include "adec_layer2.h"

int adec_Init( adec_thread_t * p_adec )
{
    p_adec->bank_0.actual = p_adec->bank_0.v1;
    p_adec->bank_0.pos = 0;
    p_adec->bank_1.actual = p_adec->bank_1.v1;
    p_adec->bank_1.pos = 0;
    return 0;
}

int adec_SyncFrame( adec_thread_t * p_adec, adec_sync_info_t * p_sync_info )
{
    static int mpeg1_sample_rate[3] = {44100, 48000, 32000};
    static int mpeg1_layer1_bit_rate[15] =
    {
        0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448
    };
    static int mpeg1_layer2_bit_rate[15] =
    {
        0, 32, 48, 56,  64,  80,  96, 112, 128, 160, 192, 224, 256, 320, 384
    };
    static int mpeg2_layer1_bit_rate[15] =
    {
        0, 32, 48, 56,  64,  80,  96, 112, 128, 144, 160, 176, 192, 224, 256
    };
    static int mpeg2_layer2_bit_rate[15] =
    {
        0,  8, 16, 24,  32,  40,  48,  56,  64,  80,  96, 112, 128, 144, 160
    };
    static int * bit_rate_table[8] =
    {
        NULL, NULL, mpeg2_layer2_bit_rate, mpeg2_layer1_bit_rate,
        NULL, NULL, mpeg1_layer2_bit_rate, mpeg1_layer1_bit_rate
    };

    u32 header;
    int index;
    int * bit_rates;
    int sample_rate;
    int bit_rate;
    int frame_size;

    /* We read the whole header, but only really take 8 bits */
    header = GetBits( &p_adec->bit_stream, 8 ) << 24;
    header |= ShowBits( &p_adec->bit_stream, 24 );

    p_adec->header = header;

    /* basic header check : sync word, no emphasis */
    if( (header & 0xfff00003) != 0xfff00000 )
    {
        return 1;
    }

    /* calculate bit rate */
    index = ( header >> 17 ) & 7; /* mpeg ID + layer */
    bit_rates = bit_rate_table[ index ];
    if( bit_rate_table == NULL )
    {
        return 1; /* invalid layer */
    }

    index = ( header >> 12 ) & 15; /* bit rate index */
    if (index > 14)
    {
        return 1;
    }
    bit_rate = bit_rates[ index ];

    /* mpeg 1 layer 2 : check that bitrate per channel is valid */

    if( bit_rates == mpeg1_layer2_bit_rate )
    {
        if( (header & 0xc0) == 0xc0 )
        {   /* mono */
            if( index > 10 )
            {
                return 1; /* invalid bitrate per channel */
            }
        }
        else
        {   /* stereo */
            if( (1 << index) & 0x2e )
            {
                return 1; /* invalid bitrate per channel */
            }
        }
    }

    /* calculate sample rate */

    index = ( header >> 10 ) & 3; /* sample rate index */
    if( index > 2 )
    {
        return 1;
    }

    sample_rate = mpeg1_sample_rate[ index ];

    if( ! (header & 0x80000) )
    {
        sample_rate >>= 1; /* half sample rate for mpeg2 */
    }

    /* calculate frame length */

    if( (header & 0x60000) == 0x60000 )
    {
        /* layer 1 */
        frame_size = 48000 * bit_rate / sample_rate;

        /* padding */
        if( header & 0x200 )
        {
            frame_size += 4;
        }
    }
    else
    {
        /* layer >1 */
        frame_size = 144000 * bit_rate / sample_rate;

        /* padding */
        if( header & 0x200 )
        {
            frame_size ++;
        }
    }

    /* Now we are sure we want this header, read it */
    RemoveBits( &p_adec->bit_stream, 24 );
    p_adec->i_read_bits = 32;

    p_sync_info->sample_rate = sample_rate;
    p_sync_info->bit_rate = bit_rate;
    p_sync_info->frame_size = frame_size;
    p_adec->frame_size = frame_size;

    return 0;
}

int adec_DecodeFrame( adec_thread_t * p_adec, s16 * buffer )
{
    int i_total_bytes_read;

    if( ! (p_adec->header & 0x10000) )
    {
        /* Error check, skip it */
        RemoveBits( &p_adec->bit_stream, 16 );
        p_adec->i_read_bits += 16;
    }

    /* parse audio data */

    switch( (p_adec->header >> 17) & 3 )
    {
    case 2:
        /* layer 2 */
        if( (p_adec->header & 0xc0) == 0xc0 )
        {
            if( adec_layer2_mono (p_adec, buffer) )
            {
                return 1;
            }
        }
        else
        {
            if( adec_layer2_stereo (p_adec, buffer) )
            {
                return 1;
            }
        }
        break;

    case 3:
        /* layer 1 */
        if( (p_adec->header & 0xc0) == 0xc0 )
        {
            if( adec_layer1_mono (p_adec, buffer) )
            {
                return 1;
            }
        }
        else
        {
            if( adec_layer1_stereo (p_adec, buffer) )
            {
                return 1;
            }
        }
        break;
    }

    /* Skip ancillary data */

    if( (p_adec->header & 0xf000) == 0 ) /* free bitrate format */
    {
        return 0;
    }

    RealignBits( &p_adec->bit_stream );
    i_total_bytes_read = ( p_adec->i_read_bits + 7 ) / 8;

    if( i_total_bytes_read > p_adec->frame_size )
    {
        return 1; /* overrun */
    }

    while( i_total_bytes_read++ < p_adec->frame_size )
    {
        RemoveBits( &p_adec->bit_stream, 8 ); /* skip ancillary data */
    }

    return 0;
}

