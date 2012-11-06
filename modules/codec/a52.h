/*****************************************************************************
 * a52.h
 *****************************************************************************
 * Copyright (C) 2001-2009 Laurent Aimar
 * $Id$
 *
 * Authors: Stéphane Borel <stef@via.ecp.fr>
 *          Christophe Massiot <massiot@via.ecp.fr>
 *          Gildas Bazin <gbazin@videolan.org>
 *          Laurent Aimar <fenrir@via.ecp.fr>
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

#ifndef _VLC_A52_H
#define _VLC_A52_H 1

#include <vlc_bits.h>

/**
 * Minimum AC3 header size that vlc_a52_header_Parse needs.
 */
#define VLC_A52_HEADER_SIZE (8)

/**
 * AC3 header information.
 */
typedef struct
{
    bool b_eac3;

    unsigned int i_channels;
    unsigned int i_channels_conf;
    unsigned int i_rate;
    unsigned int i_bitrate;

    unsigned int i_size;
    unsigned int i_samples;

} vlc_a52_header_t;

/**
 * It parse AC3 sync info.
 *
 * This code is borrowed from liba52 by Aaron Holtzman & Michel Lespinasse,
 * since we don't want to oblige S/PDIF people to use liba52 just to get
 * their SyncInfo...
 */
static inline int vlc_a52_header_ParseAc3( vlc_a52_header_t *p_header,
                                           const uint8_t *p_buf,
                                           const uint32_t *p_acmod )
{
    static const uint8_t pi_halfrate[12] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3 };
    static const unsigned int pi_bitrate[] = { 32,  40,  48,  56,  64,  80,  96, 112,
                                128, 160, 192, 224, 256, 320, 384, 448,
                                512, 576, 640 };
    static const uint8_t pi_lfeon[8] = { 0x10, 0x10, 0x04, 0x04,
                                      0x04, 0x01, 0x04, 0x01 };

    /* */
    const unsigned i_rate_shift = pi_halfrate[p_buf[5] >> 3];

    /* acmod, dsurmod and lfeon */
    const unsigned i_acmod = p_buf[6] >> 5;
    if( (p_buf[6] & 0xf8) == 0x50 )
        /* Dolby surround = stereo + Dolby */
        p_header->i_channels_conf = AOUT_CHANS_STEREO | AOUT_CHAN_DOLBYSTEREO;
    else
        p_header->i_channels_conf = p_acmod[i_acmod];
    if( p_buf[6] & pi_lfeon[i_acmod] )
        p_header->i_channels_conf |= AOUT_CHAN_LFE;

    p_header->i_channels = popcount(p_header->i_channels_conf
                                                         & AOUT_CHAN_PHYSMASK);

    const unsigned i_frmsizecod = p_buf[4] & 63;
    if( i_frmsizecod >= 38 )
        return VLC_EGENERIC;
    const unsigned i_bitrate_base = pi_bitrate[i_frmsizecod >> 1];
    p_header->i_bitrate = (i_bitrate_base * 1000) >> i_rate_shift;

    switch( p_buf[4] & 0xc0 )
    {
    case 0:
        p_header->i_rate = 48000 >> i_rate_shift;
        p_header->i_size = 4 * i_bitrate_base;
        break;
    case 0x40:
        p_header->i_rate = 44100 >> i_rate_shift;
        p_header->i_size = 2 * (320 * i_bitrate_base / 147 + (i_frmsizecod & 1));
        break;
    case 0x80:
        p_header->i_rate = 32000 >> i_rate_shift;
        p_header->i_size = 6 * i_bitrate_base;
        break;
    default:
        return VLC_EGENERIC;
    }
    p_header->i_samples = 6*256;

    p_header->b_eac3 = false;
    return VLC_SUCCESS;
}

/**
 * It parse E-AC3 sync info
 */
static inline int vlc_a52_header_ParseEac3( vlc_a52_header_t *p_header,
                                            const uint8_t *p_buf,
                                            const uint32_t *p_acmod )
{
    static const unsigned pi_samplerate[3] = { 48000, 44100, 32000 };
    unsigned i_numblkscod;
    bs_t s;


    bs_init( &s, (void*)p_buf, VLC_A52_HEADER_SIZE );
    bs_skip( &s, 16 +   /* start code */
                 2 +    /* stream type */
                 3 );   /* substream id */
    const unsigned i_frame_size = bs_read( &s, 11 );
    if( i_frame_size < 2 )
        return VLC_EGENERIC;
    p_header->i_size = 2 * ( i_frame_size + 1 );

    const unsigned i_fscod = bs_read( &s, 2 );
    if( i_fscod == 0x03 )
    {
        const unsigned i_fscod2 = bs_read( &s, 2 );
        if( i_fscod2 == 0X03 )
            return VLC_EGENERIC;
        p_header->i_rate = pi_samplerate[i_fscod2] / 2;
        i_numblkscod = 6;
    }
    else
    {
        static const int pi_blocks[4] = { 1, 2, 3, 6 };

        p_header->i_rate = pi_samplerate[i_fscod];
        i_numblkscod = pi_blocks[bs_read( &s, 2 )];
    }

    const unsigned i_acmod = bs_read( &s, 3 );
    const unsigned i_lfeon = bs_read1( &s );

    p_header->i_channels_conf = p_acmod[i_acmod];
    if( i_lfeon )
        p_header->i_channels_conf |= AOUT_CHAN_LFE;
    p_header->i_channels = popcount(p_header->i_channels_conf
                                                         & AOUT_CHAN_PHYSMASK);
    p_header->i_bitrate = 8 * p_header->i_size * (p_header->i_rate)
                                               / (i_numblkscod * 256);
    p_header->i_samples = i_numblkscod * 256;

    p_header->b_eac3 = true;
    return VLC_SUCCESS;
}

/**
 * It will parse the header AC3 frame and fill vlc_a52_header_t* if
 * it is valid or return VLC_EGENERIC.
 *
 * XXX It will only recognize big endian bitstream ie starting with 0x0b, 0x77
 */
static inline int vlc_a52_header_Parse( vlc_a52_header_t *p_header,
                                        const uint8_t *p_buffer, int i_buffer )
{
    static const uint32_t p_acmod[8] = {
        AOUT_CHANS_2_0 | AOUT_CHAN_DUALMONO,
        AOUT_CHAN_CENTER,
        AOUT_CHANS_2_0,
        AOUT_CHANS_3_0,
        AOUT_CHANS_FRONT | AOUT_CHAN_REARCENTER, /* 2F1R */
        AOUT_CHANS_FRONT | AOUT_CHANS_CENTER,    /* 3F1R */
        AOUT_CHANS_4_0,
        AOUT_CHANS_5_0,
    };

    if( i_buffer < VLC_A52_HEADER_SIZE )
        return VLC_EGENERIC;

    /* Check synword */
    if( p_buffer[0] != 0x0b || p_buffer[1] != 0x77 )
        return VLC_EGENERIC;

    /* Check bsid */
    const int bsid = p_buffer[5] >> 3;
    if( bsid > 16 )
        return VLC_EGENERIC;

    if( bsid <= 10 )
    {
        if( vlc_a52_header_ParseAc3( p_header, p_buffer, p_acmod ) )
            return VLC_EGENERIC;
    }
    else
    {
        if( vlc_a52_header_ParseEac3( p_header, p_buffer, p_acmod ) )
            return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

#endif
