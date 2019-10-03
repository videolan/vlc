/*****************************************************************************
 * a52.h
 *****************************************************************************
 * Copyright (C) 2001-2016 Laurent Aimar
 *
 * Authors: Stéphane Borel <stef@via.ecp.fr>
 *          Christophe Massiot <massiot@via.ecp.fr>
 *          Gildas Bazin <gbazin@videolan.org>
 *          Laurent Aimar <fenrir@via.ecp.fr>
 *          Thomas Guillem <thomas@gllm.fr>
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

#ifndef VLC_A52_H_
#define VLC_A52_H_

#include <vlc_bits.h>

/**
 * Minimum AC3 header size that vlc_a52_header_Parse needs.
 */
#define VLC_A52_MIN_HEADER_SIZE  (8)
#define VLC_A52_EAC3_BSI_SIZE    ((532 + 7)/8)
#define VLC_A52_EAC3_HEADER_SIZE (VLC_A52_EAC3_BSI_SIZE + 2)

#define VLC_A52_PROFILE_EAC3_DEPENDENT 1

/**
 * AC3 header information.
 */
struct vlc_a52_bitstream_info
{
    uint8_t i_fscod;
    uint8_t i_frmsizcod;
    uint8_t i_bsid;
    uint8_t i_bsmod;
    uint8_t i_acmod;
    uint8_t i_lfeon;
    union
    {
        struct {
            enum {
                EAC3_STRMTYP_INDEPENDENT    = 0,
                EAC3_STRMTYP_DEPENDENT      = 1,
                EAC3_STRMTYP_AC3_CONVERT    = 2,
                EAC3_STRMTYP_RESERVED,
            } strmtyp;
            uint16_t i_frmsiz;
            uint8_t i_fscod2;
            uint8_t i_numblkscod;
            uint8_t i_substreamid;
        } eac3;
        struct
        {
            uint8_t i_dsurmod;
        } ac3;
    };
};

typedef struct
{
    bool b_eac3;

    unsigned int i_channels;
    unsigned int i_channels_conf;
    unsigned int i_chan_mode;
    unsigned int i_rate;
    unsigned int i_bitrate;

    unsigned int i_size;
    unsigned int i_samples;

    struct vlc_a52_bitstream_info bs;

    uint8_t i_blocks_per_sync_frame;
} vlc_a52_header_t;

/**
 * It parse AC3 sync info.
 *
 * cf. AC3 spec
 */
static inline int vlc_a52_ParseAc3BitstreamInfo( struct vlc_a52_bitstream_info *bs,
                                                 const uint8_t *p_buf, size_t i_buf )
{
    bs_t s;
    bs_init( &s, p_buf, i_buf );

    /* cf. 5.3.2 */
    bs->i_fscod = bs_read( &s, 2 );
    if( bs->i_fscod == 3 )
        return VLC_EGENERIC;
    bs->i_frmsizcod = bs_read( &s, 6 );
    if( bs->i_frmsizcod >= 38 )
        return VLC_EGENERIC;
    bs->i_bsid = bs_read( &s, 5 );
    bs->i_bsmod = bs_read( &s, 3 );
    bs->i_acmod = bs_read( &s, 3 );
    if( ( bs->i_acmod & 0x1 ) && ( bs->i_acmod != 0x1 ) )
    {
        /* if 3 front channels */
        bs_skip( &s, 2 ); /* i_cmixlev */
    }
    if( bs->i_acmod & 0x4 )
    {
        /* if a surround channel exists */
        bs_skip( &s, 2 ); /* i_surmixlev */
    }
    /* if in 2/0 mode */
    bs->ac3.i_dsurmod = bs->i_acmod == 0x2 ? bs_read( &s, 2 ) : 0;
    bs->i_lfeon = bs_read( &s, 1 );

    return VLC_SUCCESS;
}

static inline int vlc_a52_header_ParseAc3( vlc_a52_header_t *p_header,
                                           const uint8_t *p_buf,
                                           const uint32_t *p_acmod,
                                           const unsigned *pi_fscod_samplerates )
{
    if( vlc_a52_ParseAc3BitstreamInfo( &p_header->bs,
                                       &p_buf[4], /* start code + CRC */
                                       VLC_A52_MIN_HEADER_SIZE - 4 ) != VLC_SUCCESS )
        return VLC_EGENERIC;

    /* cf. Table 5.18 Frame Size Code Table */
    static const uint16_t ppi_frmsizcod_fscod_sizes[][3] = {
        /* 32K, 44.1K, 48K */
        { 96, 69, 64 },
        { 96, 70, 64 },
        { 120, 87, 80 },
        { 120, 88, 80 },
        { 144, 104, 96 },
        { 144, 105, 96 },
        { 168, 121, 112 },
        { 168, 122, 112 },
        { 192, 139, 128 },
        { 192, 140, 128 },
        { 240, 174, 160 },
        { 240, 175, 160 },
        { 288, 208, 192 },
        { 288, 209, 192 },
        { 336, 243, 224 },
        { 336, 244, 224 },
        { 384, 278, 256 },
        { 384, 279, 256 },
        { 480, 348, 320 },
        { 480, 349, 320 },
        { 576, 417, 384 },
        { 576, 418, 384 },
        { 672, 487, 448 },
        { 672, 488, 448 },
        { 768, 557, 512 },
        { 768, 558, 512 },
        { 960, 696, 640 },
        { 960, 697, 640 },
        { 1152, 835, 768 },
        { 1152, 836, 768 },
        { 1344, 975, 896 },
        { 1344, 976, 896 },
        { 1536, 1114, 1024 },
        { 1536, 1115, 1024 },
        { 1728, 1253, 1152 },
        { 1728, 1254, 1152 },
        { 1920, 1393, 1280 },
        { 1920, 1394, 1280 }
    };
    static const uint16_t pi_frmsizcod_bitrates[] = {
        32,  40,  48,  56,
        64,  80,  96, 112,
        128, 160, 192, 224,
        256, 320, 384, 448,
        512, 576, 640
    };

    const struct vlc_a52_bitstream_info *bs = &p_header->bs;

    p_header->i_channels_conf = p_acmod[bs->i_acmod];
    p_header->i_chan_mode = 0;
    if( bs->ac3.i_dsurmod == 2 )
        p_header->i_chan_mode |= AOUT_CHANMODE_DOLBYSTEREO;
    if( bs->i_acmod == 0 )
        p_header->i_chan_mode |= AOUT_CHANMODE_DUALMONO;

    if( bs->i_lfeon )
        p_header->i_channels_conf |= AOUT_CHAN_LFE;

    p_header->i_channels = vlc_popcount(p_header->i_channels_conf);

    const unsigned i_rate_shift = VLC_CLIP(bs->i_bsid, 8, 11) - 8;
    p_header->i_bitrate = (pi_frmsizcod_bitrates[bs->i_frmsizcod >> 1] * 1000)
                        >> i_rate_shift;
    p_header->i_rate = pi_fscod_samplerates[bs->i_fscod] >> i_rate_shift;

    p_header->i_size = ppi_frmsizcod_fscod_sizes[bs->i_frmsizcod]
                                                [2 - bs->i_fscod] * 2;
    p_header->i_blocks_per_sync_frame = 6;
    p_header->i_samples = p_header->i_blocks_per_sync_frame * 256;

    p_header->b_eac3 = false;
    return VLC_SUCCESS;
}

/**
 * It parse E-AC3 sync info
 */
static inline int vlc_a52_ParseEac3BitstreamInfo( struct vlc_a52_bitstream_info *bs,
                                                  const uint8_t *p_buf, size_t i_buf )
{
    bs_t s;
    bs_init( &s, p_buf, i_buf );
    bs->eac3.strmtyp = bs_read( &s, 2 );      /* Stream Type */
    bs->eac3.i_substreamid = bs_read( &s, 3 );/* Substream Identification */

    bs->eac3.i_frmsiz = bs_read( &s, 11 );
    if( bs->eac3.i_frmsiz < 2 )
        return VLC_EGENERIC;

    bs->i_fscod = bs_read( &s, 2 );
    if( bs->i_fscod == 0x03 )
    {
        bs->eac3.i_fscod2 = bs_read( &s, 2 );
        if( bs->eac3.i_fscod2 == 0x03 )
            return VLC_EGENERIC;
        bs->eac3.i_numblkscod = 0x03;
    }
    else bs->eac3.i_numblkscod = bs_read( &s, 2 );

    bs->i_acmod = bs_read( &s, 3 );
    bs->i_lfeon = bs_read1( &s );
    bs->i_bsid = bs_read( &s, 5 );

    if( i_buf <= VLC_A52_MIN_HEADER_SIZE )
    {
        bs->i_bsmod = 0;
        return VLC_SUCCESS;
    }

    bs_skip( &s, 5 ); /* dialnorm */
    if(bs_read1( &s ))
        bs_skip( &s, 8 ); /* compr */

    if( bs->i_acmod == 0x00 )
    {
        bs_skip( &s, 5 );
        if(bs_read1( &s ))
            bs_skip( &s, 8 ); /* compr2 */
    }

    if( bs->eac3.strmtyp == 0x01 && bs_read1( &s ) )
        bs_skip( &s, 16 ); /* chanmap */

    if( bs_read1( &s ) ) /* mixmdate */
    {
        if( bs->i_acmod > 0x02 )
        {
            bs_skip( &s, 2 ); /* dmixmod */
            if( bs->i_acmod & 0x01 )
                bs_skip( &s, 6 ); /* ltrtcmixlev + lorocmixlev */
            if( bs->i_acmod & 0x04 )
                bs_skip( &s, 6 ); /* ltrtsurmixlev + lorosurmixlev */
        }

        if( bs->i_lfeon && bs_read1( &s ) )
            bs_skip( &s, 5 ); /* (lfemixlevcode) */

        if( bs->eac3.strmtyp == 0x00 )
        {
            if( bs_read1( &s ) )
                bs_skip( &s, 6 ); /* pgmscl */
            if( bs->i_acmod == 0x00 && bs_read1( &s ) )
                bs_skip( &s, 6 ); /* pgmscl2 */
            if(bs_read1( &s ))
                bs_skip( &s, 6 ); /* extpgmscl */
            const uint8_t i_mixdef = bs_read( &s, 2 );
            if( i_mixdef == 0x01 )
                bs_skip( &s, 5 ); /* premixcmpsel + drcsrc + premixcmpscl */
            else if( i_mixdef == 0x02 )
                bs_skip( &s, 12 ); /* mixdata */
            else if( i_mixdef == 0x03 )
            {
                const unsigned mixdeflen = bs_read( &s, 5 ) + 2;
                for(size_t i=0; i<mixdeflen; i++)
                    bs_skip( &s, 8 );
                bs_align( &s );
            }
            if( bs->i_acmod < 0x02 )
            {
                if( bs_read1( &s ) )
                    bs_skip( &s, 14 ); /* panmean + paninfo */
                if( bs->i_acmod == 0x00 && bs_read1( &s ) )
                    bs_skip( &s, 14 ); /* panmean2 + paninfo2 */
            }
            if( bs_read1( &s ) )
            {
                const uint8_t blkspersyncframe[] = { 0+1, 1, 2, 6 };
                const size_t nb = blkspersyncframe[bs->eac3.i_numblkscod];
                for(size_t i=0; i<nb; i++)
                {
                    if( bs->eac3.i_numblkscod == 0x00 )
                        bs_skip( &s, 5 ); /* blkmixcfginfo[N] */
                }
            }
        }
    }

    if( bs_read1( &s ) ) /* infomdate */
    {
        bs->i_bsmod = bs_read( &s, 3 );
        // ...
    }
    else bs->i_bsmod = 0;

    return VLC_SUCCESS;
}

static inline int vlc_a52_header_ParseEac3( vlc_a52_header_t *p_header,
                                            const uint8_t *p_buf,
                                            const uint32_t *p_acmod,
                                            const unsigned *pi_fscod_samplerates )
{
    if( vlc_a52_ParseEac3BitstreamInfo( &p_header->bs,
                                        &p_buf[2], /* start code */
                                        VLC_A52_MIN_HEADER_SIZE - 2 ) != VLC_SUCCESS )
        return VLC_EGENERIC;

    const struct vlc_a52_bitstream_info *bs = &p_header->bs;

    p_header->i_size = 2 * (bs->eac3.i_frmsiz + 1 );

    if( bs->i_fscod == 0x03 )
    {
        p_header->i_rate = pi_fscod_samplerates[bs->eac3.i_fscod2] / 2;
        p_header->i_blocks_per_sync_frame = 6;
    }
    else
    {
        static const int pi_numblkscod [4] = { 1, 2, 3, 6 };
        p_header->i_rate = pi_fscod_samplerates[bs->i_fscod];
        p_header->i_blocks_per_sync_frame = pi_numblkscod[bs->eac3.i_numblkscod];
    }

    p_header->i_channels_conf = p_acmod[bs->i_acmod];
    p_header->i_chan_mode = 0;
    if( bs->i_acmod == 0 )
        p_header->i_chan_mode |= AOUT_CHANMODE_DUALMONO;
    if( bs->i_lfeon )
        p_header->i_channels_conf |= AOUT_CHAN_LFE;
    p_header->i_channels = vlc_popcount( p_header->i_channels_conf );
    p_header->i_bitrate = 8 * p_header->i_size * p_header->i_rate
                        / (p_header->i_blocks_per_sync_frame * 256);
    p_header->i_samples = p_header->i_blocks_per_sync_frame * 256;

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
        AOUT_CHANS_2_0,
        AOUT_CHAN_CENTER,
        AOUT_CHANS_2_0,
        AOUT_CHANS_3_0,
        AOUT_CHANS_FRONT | AOUT_CHAN_REARCENTER, /* 2F1R */
        AOUT_CHANS_FRONT | AOUT_CHANS_CENTER,    /* 3F1R */
        AOUT_CHANS_4_0,
        AOUT_CHANS_5_0,
    };
    static const unsigned pi_fscod_samplerates[] = {
        48000, 44100, 32000
    };

    if( i_buffer < VLC_A52_MIN_HEADER_SIZE )
        return VLC_EGENERIC;

    /* Check synword */
    if( p_buffer[0] != 0x0b || p_buffer[1] != 0x77 )
        return VLC_EGENERIC;

    /* Check bsid */
    const int bsid = p_buffer[5] >> 3;

    /* cf. Annex E 2.3.1.6 of AC3 spec */
    if( bsid <= 10 )
        return vlc_a52_header_ParseAc3( p_header, p_buffer,
                                        p_acmod, pi_fscod_samplerates );
    else if( bsid <= 16 )
        return vlc_a52_header_ParseEac3( p_header, p_buffer,
                                         p_acmod, pi_fscod_samplerates );
    else
        return VLC_EGENERIC;
}

#endif
