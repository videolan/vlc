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
#include <vlc_aout.h>

#include "dts_header.h"

#include <assert.h>

static void SyncInfo16be( const uint8_t *p_buf, uint8_t *pi_nblks,
                          uint16_t *pi_fsize, uint8_t *pi_amode,
                          uint8_t *pi_sfreq, uint8_t *pi_rate, bool *pb_lfe )
{
    *pi_nblks = (p_buf[4] & 0x01) << 6 | (p_buf[5] >> 2);
    *pi_fsize = (p_buf[5] & 0x03) << 12 | (p_buf[6] << 4) | (p_buf[7] >> 4);

    *pi_amode = (p_buf[7] & 0x0f) << 2 | (p_buf[8] >> 6);
    *pi_sfreq = (p_buf[8] >> 2) & 0x0f;
    *pi_rate = (p_buf[8] & 0x03) << 3 | ((p_buf[9] >> 5) & 0x07);

    *pb_lfe = (p_buf[10] >> 1) & 0x03;
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

bool vlc_dts_header_IsSync( const void *p_buffer, size_t i_buffer )
{
    if( i_buffer < 6 )
        return false;

    const uint8_t *p_buf = p_buffer;
    /* 14 bits, little endian version of the bitstream */
    if( p_buf[0] == 0xff && p_buf[1] == 0x1f &&
        p_buf[2] == 0x00 && p_buf[3] == 0xe8 &&
        (p_buf[4] & 0xf0) == 0xf0 && p_buf[5] == 0x07 )
        return true;
    /* 14 bits, big endian version of the bitstream */
    else if( p_buf[0] == 0x1f && p_buf[1] == 0xff &&
             p_buf[2] == 0xe8 && p_buf[3] == 0x00 &&
             p_buf[4] == 0x07 && (p_buf[5] & 0xf0) == 0xf0 )
        return true;
    /* 16 bits, big endian version of the bitstream */
    else if( p_buf[0] == 0x7f && p_buf[1] == 0xfe &&
             p_buf[2] == 0x80 && p_buf[3] == 0x01 )
        return true;
    /* 16 bits, little endian version of the bitstream */
    else if( p_buf[0] == 0xfe && p_buf[1] == 0x7f &&
             p_buf[2] == 0x01 && p_buf[3] == 0x80 )
        return true;
    /* DTS-HD */
    else if( p_buf[0] == 0x64 && p_buf[1] ==  0x58 &&
             p_buf[2] == 0x20 && p_buf[3] ==  0x25 )
        return true;
    else
        return false;
}

static unsigned int dca_get_samplerate( uint8_t i_sfreq )
{
    /* See ETSI TS 102 114, table 5-5 */
    const unsigned int p_dca_samplerates[16] = {
        0, 8000, 16000, 32000, 0, 0, 11025, 22050, 44100, 0, 0,
        12000, 24000, 48000, 96000, 192000
    };

    if( i_sfreq >= 16 )
        return 0;
    return p_dca_samplerates[i_sfreq];
}

static unsigned int dca_get_bitrate( uint8_t i_rate )
{
    /* See ETSI TS 102 114, table 5-7 */
    const unsigned int p_dca_bitrates[32] = {
        32000,   56000,   64000,   96000,  112000,
        128000, 192000,  224000,  256000,  320000,
        384000, 448000,  512000,  576000,  640000,
        768000, 896000, 1024000, 1152000, 1280000,
        1344000, 1408000, 1411200, 1472000, 1536000,
        1920000, 2048000, 3072000, 3840000,
        /* FIXME: The following can't be put in a VLC audio_format_t:
         * 1: open, 2: variable, 3: lossless */
        0, 0, 0
    };

    if( i_rate >= 32 )
        return 0;
    return p_dca_bitrates[i_rate];
}

static uint32_t dca_get_channels( uint8_t i_amode, bool b_lfe )
{
    /* See ETSI TS 102 114, table 5-4
     * 00: A
     * 01: A + B (dual mono)
     * 02: L + R (stereo)
     * 03: (L+R) + (L-R) (sum and difference)
     * 04: LT + RT (left and right total)
     * 05: C + L + R
     * 06: L + R + S
     * 07: C + L + R + S
     * 08: L + R + SL + SR
     * 09: C + L + R + SL + SR
     * 0A: CL + CR + L + R + SL + SR
     * 0B: C + L + R + LR + RR + OV
     * 0C: CF + CR + LF + RF + LR + RR
     * 0D: CL + C + CR + L + R + SL + SR
     * 0E: CL + CR + L + R + SL1 + SL2 + SR1 + SR2
     * 0F: CL + C + CR + L + R + SL + S + SR
     * 10-3F: user defined */

    uint32_t i_original_channels;

    switch( i_amode )
    {
        case 0x0:
            i_original_channels = AOUT_CHAN_CENTER;
            break;
        case 0x1:
            i_original_channels = AOUT_CHANS_FRONT | AOUT_CHAN_DUALMONO;
            break;
        case 0x2:
        case 0x3:
        case 0x4:
            i_original_channels = AOUT_CHANS_FRONT;
            break;
        case 0x5:
            i_original_channels = AOUT_CHANS_3_0;
            break;
        case 0x6:
            i_original_channels = AOUT_CHANS_FRONT | AOUT_CHAN_REARCENTER;
            break;
        case 0x7:
            i_original_channels = AOUT_CHANS_4_CENTER_REAR;
            break;
        case 0x8:
            i_original_channels = AOUT_CHANS_4_0;
            break;
        case 0x9:
            i_original_channels = AOUT_CHANS_5_0;
            break;
        case 0xA:
        case 0xB:
            i_original_channels = AOUT_CHANS_6_0;
            break;
        case 0xC:
            i_original_channels = AOUT_CHANS_CENTER | AOUT_CHANS_FRONT
                                | AOUT_CHANS_REAR;
            break;
        case 0xD:
            i_original_channels = AOUT_CHANS_7_0;
            break;
        case 0xE:
        case 0xF:
            /* FIXME: AOUT_CHANS_8_0 */
            i_original_channels = AOUT_CHANS_7_0;
            break;
        default:
            return 0;
    }
    if (b_lfe)
        i_original_channels |= AOUT_CHAN_LFE;

    return i_original_channels;
}

int vlc_dts_header_Parse( vlc_dts_header_t *p_header,
                          const void *p_buffer, size_t i_buffer)
{
    const uint8_t *p_buf = p_buffer;
    unsigned int i_frame_size;
    bool b_lfe;
    uint16_t i_fsize;
    uint8_t i_nblks, i_rate, i_sfreq, i_amode;

    if( i_buffer < 11 )
        return VLC_EGENERIC;

    /* 14 bits, little endian version of the bitstream */
    if( p_buf[0] == 0xff && p_buf[1] == 0x1f &&
        p_buf[2] == 0x00 && p_buf[3] == 0xe8 &&
        (p_buf[4] & 0xf0) == 0xf0 && p_buf[5] == 0x07 )
    {
        if( i_buffer < VLC_DTS_HEADER_SIZE )
            return VLC_EGENERIC;

        uint8_t conv_buf[VLC_DTS_HEADER_SIZE];
        Buf14To16( conv_buf, p_buf, VLC_DTS_HEADER_SIZE, 1 );
        SyncInfo16be( conv_buf, &i_nblks, &i_fsize, &i_amode, &i_sfreq,
                      &i_rate, &b_lfe );
        i_frame_size = (i_fsize + 1) * 8 / 14 * 2;
    }
    /* 14 bits, big endian version of the bitstream */
    else if( p_buf[0] == 0x1f && p_buf[1] == 0xff &&
             p_buf[2] == 0xe8 && p_buf[3] == 0x00 &&
             p_buf[4] == 0x07 && (p_buf[5] & 0xf0) == 0xf0 )
    {
        if( i_buffer < VLC_DTS_HEADER_SIZE )
            return VLC_EGENERIC;

        uint8_t conv_buf[VLC_DTS_HEADER_SIZE];
        Buf14To16( conv_buf, p_buf, VLC_DTS_HEADER_SIZE, 0 );
        SyncInfo16be( conv_buf, &i_nblks, &i_fsize, &i_amode, &i_sfreq,
                      &i_rate, &b_lfe );
        i_frame_size = (i_fsize + 1) * 8 / 14 * 2;
    }
    /* 16 bits, big endian version of the bitstream */
    else if( p_buf[0] == 0x7f && p_buf[1] == 0xfe &&
             p_buf[2] == 0x80 && p_buf[3] == 0x01 )
    {
        SyncInfo16be( p_buf, &i_nblks, &i_fsize, &i_amode, &i_sfreq,
                      &i_rate, &b_lfe );
        i_frame_size = i_fsize + 1;
    }
    /* 16 bits, little endian version of the bitstream */
    else if( p_buf[0] == 0xfe && p_buf[1] == 0x7f &&
             p_buf[2] == 0x01 && p_buf[3] == 0x80 )
    {
        if( i_buffer < VLC_DTS_HEADER_SIZE )
            return VLC_EGENERIC;

        uint8_t conv_buf[VLC_DTS_HEADER_SIZE];
        BufLeToBe( conv_buf, p_buf, VLC_DTS_HEADER_SIZE );
        SyncInfo16be( conv_buf, &i_nblks, &i_fsize, &i_amode, &i_sfreq,
                      &i_rate, &b_lfe );
        i_frame_size = i_fsize + 1;
    }
    /* DTS-HD */
    else if( p_buf[0] == 0x64 && p_buf[1] ==  0x58 &&
                p_buf[2] == 0x20 && p_buf[3] ==  0x25 )
    {
        if( i_buffer < VLC_DTS_HEADER_SIZE )
            return VLC_EGENERIC;

        int i_dts_hd_size;
        bs_t s;
        bs_init( &s, &p_buf[4], VLC_DTS_HEADER_SIZE - 4 );

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

        /* As we ignore the stream, do not modify those variables: */
        memset(p_header, 0, sizeof(*p_header));
        p_header->b_dts_hd = true;
        p_header->i_frame_size = i_dts_hd_size;
        return VLC_SUCCESS;
    }
    else
        return VLC_EGENERIC;

    p_header->b_dts_hd = false;
    p_header->i_rate = dca_get_samplerate( i_sfreq );
    p_header->i_bitrate = dca_get_bitrate( i_rate );
    p_header->i_frame_size = i_frame_size;
    /* See ETSI TS 102 114, table 5-2 */
    p_header->i_frame_length = (i_nblks + 1) * 32;
    p_header->i_original_channels = dca_get_channels( i_amode, b_lfe );

    return VLC_SUCCESS;
}
