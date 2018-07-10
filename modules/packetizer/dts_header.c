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

enum dts_bitsteam_type {
    DTS_SYNC_CORE_BE,
    DTS_SYNC_CORE_LE,
    DTS_SYNC_CORE_14BITS_BE,
    DTS_SYNC_CORE_14BITS_LE,
    DTS_SYNC_SUBSTREAM,
};

static bool dts_header_IsSync( const uint8_t *p_buf,
                               enum dts_bitsteam_type *p_bitstream_type )
{
    if( memcmp( p_buf, "\x7F\xFE\x80\x01", 4 ) == 0 )
        *p_bitstream_type = DTS_SYNC_CORE_BE;
    else
    if( memcmp( p_buf, "\xFE\x7F\x01\x80", 4 ) == 0 )
        *p_bitstream_type = DTS_SYNC_CORE_LE;
    else
    if( memcmp( p_buf, "\x64\x58\x20\x25", 4 ) == 0 )
        *p_bitstream_type = DTS_SYNC_SUBSTREAM;
    else
    if( memcmp( p_buf, "\x1F\xFF\xE8\x00", 4 ) == 0
     && p_buf[4] == 0x07 && (p_buf[5] & 0xf0) == 0xf0 )
        *p_bitstream_type = DTS_SYNC_CORE_14BITS_BE;
    else
    if( memcmp( p_buf, "\xFF\x1F\x00\xE8", 4 ) == 0
     && (p_buf[4] & 0xf0) == 0xf0 && p_buf[5] == 0x07 )
        *p_bitstream_type = DTS_SYNC_CORE_14BITS_LE;
    else
        return false;
    return true;
}

bool vlc_dts_header_IsSync( const void *p_buf, size_t i_buf )
{
    return i_buf >= 6
        && dts_header_IsSync( p_buf, &(enum dts_bitsteam_type) { 0 } );
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

static uint16_t dca_get_channels( uint8_t i_amode, bool b_lfe,
                                  uint16_t *p_chan_mode )
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

    uint16_t i_physical_channels;

    switch( i_amode )
    {
        case 0x0:
            i_physical_channels = AOUT_CHAN_CENTER;
            break;
        case 0x1:
            i_physical_channels = AOUT_CHANS_FRONT;
            *p_chan_mode = AOUT_CHANMODE_DUALMONO;
            break;
        case 0x2:
        case 0x3:
        case 0x4:
            i_physical_channels = AOUT_CHANS_FRONT;
            break;
        case 0x5:
            i_physical_channels = AOUT_CHANS_3_0;
            break;
        case 0x6:
            i_physical_channels = AOUT_CHANS_FRONT | AOUT_CHAN_REARCENTER;
            break;
        case 0x7:
            i_physical_channels = AOUT_CHANS_4_CENTER_REAR;
            break;
        case 0x8:
            i_physical_channels = AOUT_CHANS_4_0;
            break;
        case 0x9:
            i_physical_channels = AOUT_CHANS_5_0;
            break;
        case 0xA:
        case 0xB:
            i_physical_channels = AOUT_CHANS_6_0;
            break;
        case 0xC:
            i_physical_channels = AOUT_CHANS_CENTER | AOUT_CHANS_FRONT
                                | AOUT_CHANS_REAR;
            break;
        case 0xD:
            i_physical_channels = AOUT_CHANS_7_0;
            break;
        case 0xE:
        case 0xF:
            /* FIXME: AOUT_CHANS_8_0 */
            i_physical_channels = AOUT_CHANS_7_0;
            break;
        default:
            return 0;
    }
    if (b_lfe)
        i_physical_channels |= AOUT_CHAN_LFE;

    return i_physical_channels;
}

static int dts_header_ParseSubstream( vlc_dts_header_t *p_header,
                                      const void *p_buffer )
{
    bs_t s;
    bs_init( &s, p_buffer, VLC_DTS_HEADER_SIZE );
    bs_skip( &s, 32 /*SYNCEXTSSH*/ + 8 /*UserDefinedBits*/ + 2 /*nExtSSIndex*/ );
    uint8_t bHeaderSizeType = bs_read1( &s );
    uint32_t nuBits4ExSSFsize;
    if( bHeaderSizeType == 0 )
    {
        bs_skip( &s, 8 /*nuBits4Header*/ );
        nuBits4ExSSFsize = bs_read( &s, 16 );
    }
    else
    {
        bs_skip( &s, 12 /*nuBits4Header*/ );
        nuBits4ExSSFsize = bs_read( &s, 20 );
    }
    memset( p_header, 0, sizeof(*p_header) );
    p_header->b_substream = true;
    p_header->i_frame_size = nuBits4ExSSFsize + 1;
    return VLC_SUCCESS;
}

static int dts_header_ParseCore( vlc_dts_header_t *p_header,
                                 const void *p_buffer, bool b_14b )
{
    bs_t s;
    bs_init( &s, p_buffer, VLC_DTS_HEADER_SIZE );
    bs_skip( &s, 32 /*SYNC*/ + 1 /*FTYPE*/ + 5 /*SHORT*/ + 1 /*CPF*/ );
    uint8_t i_nblks = bs_read( &s, 7 );
    if( i_nblks < 5 )
        return VLC_EGENERIC;
    uint16_t i_fsize = bs_read( &s, 14 );
    if( i_fsize < 95 )
        return VLC_EGENERIC;
    uint8_t i_amode = bs_read( &s, 6 );
    uint8_t i_sfreq = bs_read( &s, 4 );
    uint8_t i_rate = bs_read( &s, 5 );
    bs_skip( &s, 1 /*FixedBit*/ + 1 /*DYNF*/ + 1 /*TIMEF*/ + 1 /*AUXF*/ +
             1 /*HDCD*/ + 3 /*EXT_AUDIO_ID*/ + 1 /*EXT_AUDIO */ + 1 /*ASPF*/ );
    uint8_t i_lff = bs_read( &s, 2 );

    bool b_lfe = i_lff == 1 || i_lff == 2;

    p_header->b_substream = false;
    p_header->b_14b = b_14b;
    p_header->i_rate = dca_get_samplerate( i_sfreq );
    p_header->i_bitrate = dca_get_bitrate( i_rate );
    p_header->i_frame_size = !b_14b ? ( i_fsize + 1 )
                                    : ( i_fsize + 1 ) * 16 / 14;
    /* See ETSI TS 102 114, table 5-2 */
    p_header->i_frame_length = (i_nblks + 1) * 32;
    p_header->i_chan_mode = 0;
    p_header->i_physical_channels =
        dca_get_channels( i_amode, b_lfe, &p_header->i_chan_mode );

    if( !p_header->i_rate || !p_header->i_frame_size ||
        !p_header->i_frame_length || !p_header->i_physical_channels )
        return VLC_EGENERIC;

    return VLC_SUCCESS;
}

ssize_t vlc_dts_header_Convert14b16b( void *p_dst, size_t i_dst,
                                      const void *p_src, size_t i_src,
                                      bool b_out_le )
{
    size_t i_size = i_src * 14 / 16;
    if( i_src <= VLC_DTS_HEADER_SIZE || i_size > i_dst )
        return -1;

    enum dts_bitsteam_type bitstream_type;
    if( !dts_header_IsSync( p_src, &bitstream_type ) )
        return -1;

    if( bitstream_type != DTS_SYNC_CORE_14BITS_BE
     && bitstream_type != DTS_SYNC_CORE_14BITS_LE )
        return -1;

    int i_ret = Buf14To16( p_dst, p_src, i_src,
                           bitstream_type == DTS_SYNC_CORE_14BITS_LE );
    if( b_out_le ) /* since Buf14To16 convert to BE */
        swab( p_dst, p_dst, i_ret );
    return i_ret;
}

int vlc_dts_header_Parse( vlc_dts_header_t *p_header,
                          const void *p_buffer, size_t i_buffer)
{
    enum dts_bitsteam_type bitstream_type;

    if( i_buffer < VLC_DTS_HEADER_SIZE )
        return VLC_EGENERIC;

    if( !dts_header_IsSync( p_buffer, &bitstream_type ) )
        return VLC_EGENERIC;

    switch( bitstream_type )
    {
        case DTS_SYNC_CORE_LE:
        {
            uint8_t conv_buf[VLC_DTS_HEADER_SIZE];
            BufLeToBe( conv_buf, p_buffer, VLC_DTS_HEADER_SIZE );
            return dts_header_ParseCore( p_header, conv_buf, false );
        }
        case DTS_SYNC_CORE_BE:
            return dts_header_ParseCore( p_header, p_buffer, false );
        case DTS_SYNC_CORE_14BITS_BE:
        case DTS_SYNC_CORE_14BITS_LE:
        {
            uint8_t conv_buf[VLC_DTS_HEADER_SIZE];
            Buf14To16( conv_buf, p_buffer, VLC_DTS_HEADER_SIZE,
                       bitstream_type == DTS_SYNC_CORE_14BITS_LE );
            return dts_header_ParseCore( p_header, conv_buf, true );
        }
        case DTS_SYNC_SUBSTREAM:
            return dts_header_ParseSubstream( p_header, p_buffer );
        default:
            vlc_assert_unreachable();
    }
}
