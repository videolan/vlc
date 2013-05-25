/*****************************************************************************
 * rawdv.h : raw DV helpers
 *****************************************************************************
 * Copyright (C) 2001-2011 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Gildas Bazin <gbazin@netcourrier.com>
 *          Paul Corke <paul dot corke at datatote dot co dot uk>
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

#define DV_PAL_FRAME_SIZE  (12 * 150 * 80)
#define DV_NTSC_FRAME_SIZE (10 * 150 * 80)


static const uint16_t dv_audio_shuffle525[10][9] = {
  {  0, 30, 60, 20, 50, 80, 10, 40, 70 }, /* 1st channel */
  {  6, 36, 66, 26, 56, 86, 16, 46, 76 },
  { 12, 42, 72,  2, 32, 62, 22, 52, 82 },
  { 18, 48, 78,  8, 38, 68, 28, 58, 88 },
  { 24, 54, 84, 14, 44, 74,  4, 34, 64 },

  {  1, 31, 61, 21, 51, 81, 11, 41, 71 }, /* 2nd channel */
  {  7, 37, 67, 27, 57, 87, 17, 47, 77 },
  { 13, 43, 73,  3, 33, 63, 23, 53, 83 },
  { 19, 49, 79,  9, 39, 69, 29, 59, 89 },
  { 25, 55, 85, 15, 45, 75,  5, 35, 65 },
};

static const uint16_t dv_audio_shuffle625[12][9] = {
  {   0,  36,  72,  26,  62,  98,  16,  52,  88}, /* 1st channel */
  {   6,  42,  78,  32,  68, 104,  22,  58,  94},
  {  12,  48,  84,   2,  38,  74,  28,  64, 100},
  {  18,  54,  90,   8,  44,  80,  34,  70, 106},
  {  24,  60,  96,  14,  50,  86,   4,  40,  76},
  {  30,  66, 102,  20,  56,  92,  10,  46,  82},

  {   1,  37,  73,  27,  63,  99,  17,  53,  89}, /* 2nd channel */
  {   7,  43,  79,  33,  69, 105,  23,  59,  95},
  {  13,  49,  85,   3,  39,  75,  29,  65, 101},
  {  19,  55,  91,   9,  45,  81,  35,  71, 107},
  {  25,  61,  97,  15,  51,  87,   5,  41,  77},
  {  31,  67, 103,  21,  57,  93,  11,  47,  83},
};

static inline uint16_t dv_audio_12to16( uint16_t sample )
{
    uint16_t shift, result;

    sample = (sample < 0x800) ? sample : sample | 0xf000;
    shift = (sample & 0xf00) >> 8;

    if (shift < 0x2 || shift > 0xd) {
        result = sample;
    } else if (shift < 0x8) {
        shift--;
        result = (sample - (256 * shift)) << shift;
    } else {
        shift = 0xe - shift;
        result = ((sample + ((256 * shift) + 1)) << shift) - 1;
    }

    return result;
}

static inline void dv_get_audio_format( es_format_t *p_fmt,
                                        const uint8_t *p_aaux_src )
{
    /* 12 bits non-linear will be converted to 16 bits linear */
    es_format_Init( p_fmt, AUDIO_ES, VLC_CODEC_S16L );

    p_fmt->audio.i_bitspersample = 16;
    p_fmt->audio.i_channels = 2;
    switch( (p_aaux_src[4-1] >> 3) & 0x07 )
    {
    case 0:
        p_fmt->audio.i_rate = 48000;
        break;
    case 1:
        p_fmt->audio.i_rate = 44100;
        break;
    case 2:
    default:
        p_fmt->audio.i_rate = 32000;
        break;
    }
}

static inline int dv_get_audio_sample_count( const uint8_t *p_buffer, int i_dsf )
{
    int i_samples = p_buffer[0] & 0x3f; /* samples in this frame - min samples */
    switch( (p_buffer[3] >> 3) & 0x07 )
    {
    case 0:
        return i_samples + (i_dsf ? 1896 : 1580);
    case 1:
        return i_samples + (i_dsf ? 1742 : 1452);
    case 2:
    default:
        return i_samples + (i_dsf ? 1264 : 1053);
    }
}

static inline block_t *dv_extract_audio( block_t *p_frame_block )
{
    block_t *p_block;
    uint8_t *p_frame, *p_buf;
    int i_audio_quant, i_samples, i_half_ch;
    const uint16_t (*audio_shuffle)[9];
    int i, j, d, of;

    if( p_frame_block->i_buffer < 4 )
        return NULL;
    const int i_dsf = (p_frame_block->p_buffer[3] & 0x80) >> 7;
    if( p_frame_block->i_buffer < (i_dsf ? DV_PAL_FRAME_SIZE
                                         : DV_NTSC_FRAME_SIZE ) )
        return NULL;

    /* Beginning of AAUX pack */
    p_buf = p_frame_block->p_buffer + 80*6+80*16*3 + 3;
    if( *p_buf != 0x50 ) return NULL;

    i_audio_quant = p_buf[4] & 0x07; /* 0 - 16bit, 1 - 12bit */
    if( i_audio_quant > 1 )
        return NULL;

    i_samples = dv_get_audio_sample_count( &p_buf[1], i_dsf );

    p_block = block_Alloc( 4 * i_samples );

    /* for each DIF segment */
    p_frame = p_frame_block->p_buffer;
    audio_shuffle = i_dsf ? dv_audio_shuffle625 : dv_audio_shuffle525;
    i_half_ch = (i_dsf ? 12 : 10)/2;
    for( i = 0; i < (i_dsf ? 12 : 10); i++ )
    {
        p_frame += 6 * 80; /* skip DIF segment header */

        if( i_audio_quant == 1 && i == i_half_ch ) break;

        for( j = 0; j < 9; j++ )
        {
            for( d = 8; d < 80; d += 2 )
            {
                if( i_audio_quant == 0 )
                {
                    /* 16bit quantization */
                    of = audio_shuffle[i][j] + (d - 8) / 2 *
                           (i_dsf ? 108 : 90);

                    if( of * 2 >= 4 * i_samples ) continue;

                    /* big endian */
                    p_block->p_buffer[of*2] = p_frame[d+1];
                    p_block->p_buffer[of*2+1] = p_frame[d];

                    if( p_block->p_buffer[of*2+1] == 0x80 &&
                        p_block->p_buffer[of*2] == 0x00 )
                        p_block->p_buffer[of*2+1] = 0;
                }
                else
                {
                    /* 12bit quantization */
                    uint16_t lc = (p_frame[d+0] << 4) | (p_frame[d+2] >> 4);
                    uint16_t rc = (p_frame[d+1] << 4) | (p_frame[d+2] & 0x0f);

                    lc = lc == 0x800 ? 0 : dv_audio_12to16(lc);
                    rc = rc == 0x800 ? 0 : dv_audio_12to16(rc);

                    of = audio_shuffle[i][j] + (d - 8) / 3 * (i_dsf ? 108 : 90);
                    if( of*2 >= 4 * i_samples )
                        continue;
                    p_block->p_buffer[of*2+0] = lc & 0xff;
                    p_block->p_buffer[of*2+1] = lc >> 8;

                    of = audio_shuffle[i + i_half_ch][j] + (d - 8) / 3 * (i_dsf ? 108 : 90);
                    if( of*2 >= 4 * i_samples )
                        continue;
                    p_block->p_buffer[of*2+0] = rc & 0xff;
                    p_block->p_buffer[of*2+1] = rc >> 8;

                    ++d;
                }
            }

            p_frame += 16 * 80; /* 15 Video DIFs + 1 Audio DIF */
        }
    }

    p_block->i_pts = p_frame_block->i_pts > VLC_TS_INVALID ? p_frame_block->i_pts
                                                           : p_frame_block->i_dts;
    p_block->i_dts = p_frame_block->i_dts;
    return p_block;
}

