/*****************************************************************************
 * dts_header.c: parse DTS audio headers info
 *****************************************************************************
 * Copyright (C) 2004-2009 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Gildas Bazin <gbazin@netcourrier.com>
 *          Laurent Aimar
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_bits.h>

#include "dts_header.h"

#include <assert.h>


static int SyncInfo16be( const uint8_t *p_buf,
                         unsigned int *pi_audio_mode,
                         unsigned int *pi_sample_rate,
                         unsigned int *pi_bit_rate,
                         unsigned int *pi_frame_length )
{
    unsigned int frame_size;
    unsigned int i_lfe;

    *pi_frame_length = (p_buf[4] & 0x01) << 6 | (p_buf[5] >> 2);
    frame_size = (p_buf[5] & 0x03) << 12 | (p_buf[6] << 4) | (p_buf[7] >> 4);

    *pi_audio_mode = (p_buf[7] & 0x0f) << 2 | (p_buf[8] >> 6);
    *pi_sample_rate = (p_buf[8] >> 2) & 0x0f;
    *pi_bit_rate = (p_buf[8] & 0x03) << 3 | ((p_buf[9] >> 5) & 0x07);

    i_lfe = (p_buf[10] >> 1) & 0x03;
    if( i_lfe ) *pi_audio_mode |= 0x10000;

    return frame_size + 1;
}

static void BufLeToBe( uint8_t *p_out, const uint8_t *p_in, int i_in )
{
    int i;

    for( i = 0; i < i_in/2; i++  )
    {
        p_out[i*2] = p_in[i*2+1];
        p_out[i*2+1] = p_in[i*2];
    }
}

static int Buf14To16( uint8_t *p_out, const uint8_t *p_in, int i_in, int i_le )
{
    unsigned char tmp, cur = 0;
    int bits_in, bits_out = 0;
    int i, i_out = 0;

    for( i = 0; i < i_in; i++  )
    {
        if( i%2 )
        {
            tmp = p_in[i-i_le];
            bits_in = 8;
        }
        else
        {
            tmp = p_in[i+i_le] & 0x3F;
            bits_in = 8 - 2;
        }

        if( bits_out < 8 )
        {
            int need = __MIN( 8 - bits_out, bits_in );
            cur <<= need;
            cur |= ( tmp >> (bits_in - need) );
            tmp <<= (8 - bits_in + need);
            tmp >>= (8 - bits_in + need);
            bits_in -= need;
            bits_out += need;
        }

        if( bits_out == 8 )
        {
            p_out[i_out] = cur;
            cur = 0;
            bits_out = 0;
            i_out++;
        }

        bits_out += bits_in;
        cur <<= bits_in;
        cur |= tmp;
    }

    return i_out;
}

int SyncCode( const uint8_t *p_buf )
{
    /* 14 bits, little endian version of the bitstream */
    if( p_buf[0] == 0xff && p_buf[1] == 0x1f &&
        p_buf[2] == 0x00 && p_buf[3] == 0xe8 &&
        (p_buf[4] & 0xf0) == 0xf0 && p_buf[5] == 0x07 )
    {
        return VLC_SUCCESS;
    }
    /* 14 bits, big endian version of the bitstream */
    else if( p_buf[0] == 0x1f && p_buf[1] == 0xff &&
             p_buf[2] == 0xe8 && p_buf[3] == 0x00 &&
             p_buf[4] == 0x07 && (p_buf[5] & 0xf0) == 0xf0 )
    {
        return VLC_SUCCESS;
    }
    /* 16 bits, big endian version of the bitstream */
    else if( p_buf[0] == 0x7f && p_buf[1] == 0xfe &&
             p_buf[2] == 0x80 && p_buf[3] == 0x01 )
    {
        return VLC_SUCCESS;
    }
    /* 16 bits, little endian version of the bitstream */
    else if( p_buf[0] == 0xfe && p_buf[1] == 0x7f &&
             p_buf[2] == 0x01 && p_buf[3] == 0x80 )
    {
        return VLC_SUCCESS;
    }
    /* DTS-HD */
    else if( p_buf[0] == 0x64 && p_buf[1] ==  0x58 &&
             p_buf[2] == 0x20 && p_buf[3] ==  0x25 )
    {
        return VLC_SUCCESS;
    }

    return VLC_EGENERIC;
}

int GetSyncInfo( const uint8_t *p_buf,
                        bool *pb_dts_hd,
                        unsigned int *pi_sample_rate,
                        unsigned int *pi_bit_rate,
                        unsigned int *pi_frame_length,
                        unsigned int *pi_audio_mode )
{
    unsigned int i_frame_size;

    /* 14 bits, little endian version of the bitstream */
    if( p_buf[0] == 0xff && p_buf[1] == 0x1f &&
        p_buf[2] == 0x00 && p_buf[3] == 0xe8 &&
        (p_buf[4] & 0xf0) == 0xf0 && p_buf[5] == 0x07 )
    {
        uint8_t conv_buf[DTS_HEADER_SIZE];
        Buf14To16( conv_buf, p_buf, DTS_HEADER_SIZE, 1 );
        i_frame_size = SyncInfo16be( conv_buf, pi_audio_mode, pi_sample_rate,
                                     pi_bit_rate, pi_frame_length );
        i_frame_size = i_frame_size * 8 / 14 * 2;
    }
    /* 14 bits, big endian version of the bitstream */
    else if( p_buf[0] == 0x1f && p_buf[1] == 0xff &&
             p_buf[2] == 0xe8 && p_buf[3] == 0x00 &&
             p_buf[4] == 0x07 && (p_buf[5] & 0xf0) == 0xf0 )
    {
        uint8_t conv_buf[DTS_HEADER_SIZE];
        Buf14To16( conv_buf, p_buf, DTS_HEADER_SIZE, 0 );
        i_frame_size = SyncInfo16be( conv_buf, pi_audio_mode, pi_sample_rate,
                                     pi_bit_rate, pi_frame_length );
        i_frame_size = i_frame_size * 8 / 14 * 2;
    }
    /* 16 bits, big endian version of the bitstream */
    else if( p_buf[0] == 0x7f && p_buf[1] == 0xfe &&
             p_buf[2] == 0x80 && p_buf[3] == 0x01 )
    {
        i_frame_size = SyncInfo16be( p_buf, pi_audio_mode, pi_sample_rate,
                                     pi_bit_rate, pi_frame_length );
    }
    /* 16 bits, little endian version of the bitstream */
    else if( p_buf[0] == 0xfe && p_buf[1] == 0x7f &&
             p_buf[2] == 0x01 && p_buf[3] == 0x80 )
    {
        uint8_t conv_buf[DTS_HEADER_SIZE];
        BufLeToBe( conv_buf, p_buf, DTS_HEADER_SIZE );
        i_frame_size = SyncInfo16be( p_buf, pi_audio_mode, pi_sample_rate,
                                     pi_bit_rate, pi_frame_length );
    }
    /* DTS-HD */
    else if( p_buf[0] == 0x64 && p_buf[1] ==  0x58 &&
                p_buf[2] == 0x20 && p_buf[3] ==  0x25 )
    {
        int i_dts_hd_size;
        bs_t s;
        bs_init( &s, &p_buf[4], DTS_HEADER_SIZE - 4 );

        bs_skip( &s, 8 + 2 );

        if( bs_read1( &s ) )
        {
            bs_skip( &s, 12 );
            i_dts_hd_size = bs_read( &s, 20 ) + 1;
        }
        else
        {
            bs_skip( &s, 8 );
            i_dts_hd_size = bs_read( &s, 16 ) + 1;
        }
        //uint16_t s0 = bs_read( &s, 16 );
        //uint16_t s1 = bs_read( &s, 16 );
        //fprintf( stderr, "DTS HD=%d : %x %x\n", i_dts_hd_size, s0, s1 );

        *pb_dts_hd = true;
        /* As we ignore the stream, do not modify those variables:
        *pi_channels = ;
        *pi_channels_conf = ;
        *pi_sample_rate = ;
        *pi_bit_rate = ;
        *pi_frame_length = ;
        */
        return i_dts_hd_size;
    }
    else
    {
        return VLC_EGENERIC;
    }

    *pb_dts_hd = false;
    return i_frame_size;
}

