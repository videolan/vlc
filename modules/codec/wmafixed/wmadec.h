/*
 * WMA compatible decoder
 * Copyright (c) 2002 The FFmpeg Project.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _WMADEC_H
#define _WMADEC_H

#include <inttypes.h>

#include "asf.h"
#include "bitstream.h" /* For GetBitContext */
#include "mdct.h"

#undef TRACE

/* size of blocks */
#define BLOCK_MIN_BITS 7
#define BLOCK_MAX_BITS 11
#define BLOCK_MAX_SIZE (1 << BLOCK_MAX_BITS)

#define BLOCK_NB_SIZES (BLOCK_MAX_BITS - BLOCK_MIN_BITS + 1)

/* XXX: find exact max size */
#define HIGH_BAND_MAX_SIZE 16

#define NB_LSP_COEFS 10

/* XXX: is it a suitable value ? */
#define MAX_CODED_SUPERFRAME_SIZE 16384

#define M_PI    3.14159265358979323846

#define M_PI_F  0x3243f // in fixed 32 format
#define TWO_M_PI_F  0x6487f   //in fixed 32

#define MAX_CHANNELS 2

#define NOISE_TAB_SIZE 8192

#define LSP_POW_BITS 7

typedef struct WMADecodeContext
{
    GetBitContext gb;

    int nb_block_sizes;  /* number of block sizes */

    int sample_rate;
    int nb_channels;
    int bit_rate;
    int version; /* 1 = 0x160 (WMAV1), 2 = 0x161 (WMAV2) */
    int block_align;
    int use_bit_reservoir;
    int use_variable_block_len;
    int use_exp_vlc;  /* exponent coding: 0 = lsp, 1 = vlc + delta */
    int use_noise_coding; /* true if perceptual noise is added */
    int byte_offset_bits;
    VLC exp_vlc;
    int exponent_sizes[BLOCK_NB_SIZES];
    uint16_t exponent_bands[BLOCK_NB_SIZES][25];
    int high_band_start[BLOCK_NB_SIZES]; /* index of first coef in high band */
    int coefs_start;               /* first coded coef */
    int coefs_end[BLOCK_NB_SIZES]; /* max number of coded coefficients */
    int exponent_high_sizes[BLOCK_NB_SIZES];
    int exponent_high_bands[BLOCK_NB_SIZES][HIGH_BAND_MAX_SIZE];
    VLC hgain_vlc;

    /* coded values in high bands */
    int high_band_coded[MAX_CHANNELS][HIGH_BAND_MAX_SIZE];
    int high_band_values[MAX_CHANNELS][HIGH_BAND_MAX_SIZE];

    /* there are two possible tables for spectral coefficients */
    VLC coef_vlc[2];
    uint16_t *run_table[2];
    uint16_t *level_table[2];
    /* frame info */
    int frame_len;       /* frame length in samples */
    int frame_len_bits;  /* frame_len = 1 << frame_len_bits */

    /* block info */
    int reset_block_lengths;
    int block_len_bits; /* log2 of current block length */
    int next_block_len_bits; /* log2 of next block length */
    int prev_block_len_bits; /* log2 of prev block length */
    int block_len; /* block length in samples */
    int block_num; /* block number in current frame */
    int block_pos; /* current position in frame */
    uint8_t ms_stereo; /* true if mid/side stereo mode */
    uint8_t channel_coded[MAX_CHANNELS]; /* true if channel is coded */
    int exponents_bsize[MAX_CHANNELS];      // log2 ratio frame/exp. length
    int32_t exponents[MAX_CHANNELS][BLOCK_MAX_SIZE];
    int32_t max_exponent[MAX_CHANNELS];
    int16_t coefs1[MAX_CHANNELS][BLOCK_MAX_SIZE];
    int32_t (*coefs)[MAX_CHANNELS][BLOCK_MAX_SIZE];
    MDCTContext mdct_ctx[BLOCK_NB_SIZES];
    int32_t *windows[BLOCK_NB_SIZES];
    /* output buffer for one frame and the last for IMDCT windowing */
    int32_t frame_out[MAX_CHANNELS][BLOCK_MAX_SIZE * 2];
    /* last frame info */
    uint8_t last_superframe[MAX_CODED_SUPERFRAME_SIZE + 4]; /* padding added */
    int last_bitoffset;
    int last_superframe_len;
    int32_t *noise_table;
    int noise_index;
    int32_t noise_mult; /* XXX: suppress that and integrate it in the noise array */
    /* lsp_to_curve tables */
    int32_t lsp_cos_table[BLOCK_MAX_SIZE];
    int64_t lsp_pow_e_table[256];
    int32_t lsp_pow_m_table1[(1 << LSP_POW_BITS)];
    int32_t lsp_pow_m_table2[(1 << LSP_POW_BITS)];

    /* State of current superframe decoding */
    int bit_offset;
    int nb_frames;
    int current_frame;

#ifdef TRACE

    int frame_count;
#endif
}
WMADecodeContext;

int wma_decode_init(WMADecodeContext* s, asf_waveformatex_t *wfx);
int wma_decode_superframe_init(WMADecodeContext* s,
                               uint8_t *buf, int buf_size);
int wma_decode_superframe_frame(WMADecodeContext* s,
                                int32_t *samples,
                                uint8_t *buf, int buf_size);
#endif
