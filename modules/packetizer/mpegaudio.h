/*****************************************************************************
 * mpegaudio.h:
 *****************************************************************************
 * Copyright (C) 2001-2016 VLC authors and VideoLAN
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Eric Petit <titer@videolan.org>
 *          Christophe Massiot <massiot@via.ecp.fr>
 *          Gildas Bazin <gbazin@videolan.org>
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

#include <stdbool.h>
#include <stdint.h>

/*****************************************************************************
 * SyncInfo: parse MPEG audio sync info
 *****************************************************************************/
static int SyncInfo(uint32_t i_header, unsigned int *restrict pi_channels,
                    unsigned int *restrict pi_channels_conf,
                    unsigned int *restrict pi_chan_mode,
                    unsigned int *restrict pi_sample_rate,
                    unsigned int *restrict pi_bit_rate,
                    unsigned int *restrict pi_frame_length,
                    unsigned int *restrict pi_max_frame_size,
                    unsigned int *restrict pi_layer)
{
    static const unsigned short ppi_bitrate[2][3][16] =
    {
        {
            /* v1 l1 */
            { 0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384,
              416, 448, 0},
            /* v1 l2 */
            { 0, 32, 48, 56,  64,  80,  96, 112, 128, 160, 192, 224, 256,
              320, 384, 0},
            /* v1 l3 */
            { 0, 32, 40, 48,  56,  64,  80,  96, 112, 128, 160, 192, 224,
              256, 320, 0}
        },

        {
            /* v2 l1 */
            { 0, 32, 48, 56,  64,  80,  96, 112, 128, 144, 160, 176, 192,
              224, 256, 0},
            /* v2 l2 */
            { 0,  8, 16, 24,  32,  40,  48,  56,  64,  80,  96, 112, 128,
              144, 160, 0},
            /* v2 l3 */
            { 0,  8, 16, 24,  32,  40,  48,  56,  64,  80,  96, 112, 128,
              144, 160, 0}
        }
    };

    static const unsigned short ppi_samplerate[2][4] = /* version 1 then 2 */
    {
        { 44100, 48000, 32000, 0 },
        { 22050, 24000, 16000, 0 }
    };

    int i_frame_size;

    bool b_mpeg_2_5 = 1 - ((i_header & 0x100000) >> 20);
    int i_version = 1 - ((i_header & 0x80000) >> 19);
    *pi_layer   = 4 - ((i_header & 0x60000) >> 17);
    //bool b_crc = !((i_header >> 16) & 0x01);
    int i_bitrate_index = (i_header & 0xf000) >> 12;
    int i_samplerate_index = (i_header & 0xc00) >> 10;
    bool b_padding = (i_header & 0x200) >> 9;
    /* Extension */
    int i_mode = (i_header & 0xc0) >> 6;
    /* Modeext, copyright & original */
    int i_emphasis = i_header & 0x3;

    *pi_chan_mode = 0;

    if (*pi_layer == 4
     || i_bitrate_index == 0x0f
     || i_samplerate_index == 0x03
     || i_emphasis == 0x02)
        return -1;

    switch (i_mode)
    {
        case 2: /* dual-mono */
            *pi_chan_mode = AOUT_CHANMODE_DUALMONO;
            /* fall through */
        case 0: /* stereo */
        case 1: /* joint stereo */
            *pi_channels = 2;
            *pi_channels_conf = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT;
            break;
        case 3: /* mono */
            *pi_channels = 1;
            *pi_channels_conf = AOUT_CHAN_CENTER;
            break;
    }

    int i_max_bit_rate = ppi_bitrate[i_version][*pi_layer-1][14];
    *pi_bit_rate = ppi_bitrate[i_version][*pi_layer-1][i_bitrate_index];
    *pi_sample_rate = ppi_samplerate[i_version][i_samplerate_index];

    if (b_mpeg_2_5)
        *pi_sample_rate /= 2;

    switch (*pi_layer)
    {
        case 1:
            i_frame_size = (12000 * *pi_bit_rate / *pi_sample_rate +
                            b_padding) * 4;
            *pi_max_frame_size = (12000 * i_max_bit_rate /
                                  *pi_sample_rate + 1) * 4;
            *pi_frame_length = 384;
            break;

        case 2:
            i_frame_size = 144000 * *pi_bit_rate / *pi_sample_rate + b_padding;
            *pi_max_frame_size = 144000 * i_max_bit_rate / *pi_sample_rate + 1;
            *pi_frame_length = 1152;
            break;

        case 3:
            i_frame_size = (i_version ? 72000 : 144000) *
                            *pi_bit_rate / *pi_sample_rate + b_padding;
            *pi_max_frame_size = (i_version ? 72000 : 144000) *
                                  i_max_bit_rate / *pi_sample_rate + 1;
            *pi_frame_length = i_version ? 576 : 1152;
            break;

        default:
            vlc_assert_unreachable();
    }

    /* Free bitrate mode can support higher bitrates */
    if (*pi_bit_rate == 0)
        *pi_max_frame_size *= 2;

    return i_frame_size;
}
