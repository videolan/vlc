/*****************************************************************************
 * transrate.h: MPEG2 video transrating module
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * Copyright (C) 2003 Antoine Missout
 * Copyright (C) 2000-2003 Michel Lespinasse <walken@zoy.org>
 * Copyright (C) 1999-2000 Aaron Holtzman <aholtzma@ess.engr.uvic.ca>
 * $Id$
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
 *          Laurent Aimar <fenrir@via.ecp.fr>
 *          Antoine Missout
 *          Michel Lespinasse <walken@zoy.org>
 *          Aaron Holtzman <aholtzma@ess.engr.uvic.ca>
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

/*****************************************************************************
 * sout_stream_id_t:
 *****************************************************************************/

typedef struct
{
    uint8_t run;
    short level;
} RunLevel;

typedef struct
{
    uint8_t *p_c;
    uint8_t *p_r;
    uint8_t *p_w;
    uint8_t *p_ow;
    uint8_t *p_rw;

    int i_bit_in;
    int i_bit_out;
    uint32_t i_bit_in_cache;
    uint32_t i_bit_out_cache;

    uint32_t i_byte_in;
    uint32_t i_byte_out;
} bs_transrate_t;

typedef struct
{
    bs_transrate_t bs;

    /* MPEG2 state */

    // seq header
    unsigned int horizontal_size_value;
    unsigned int vertical_size_value;

    // pic header
    unsigned int picture_coding_type;

    // pic code ext
    unsigned int f_code[2][2];
    /* unsigned int intra_dc_precision; */
    unsigned int picture_structure;
    unsigned int frame_pred_frame_dct;
    unsigned int concealment_motion_vectors;
    unsigned int q_scale_type;
    unsigned int intra_vlc_format;
    /* unsigned int alternate_scan; */

    // slice or mb
    // quantizer_scale_code
    unsigned int quantizer_scale;
    unsigned int new_quantizer_scale;
    unsigned int last_coded_scale;
    int   h_offset, v_offset;

    // mb
    double quant_corr, fact_x, current_fact_x;
    int level_i, level_p;

    ssize_t i_current_gop_size, i_wanted_gop_size, i_new_gop_size;
} transrate_t;


struct sout_stream_id_t
{
    void            *id;
    vlc_bool_t      b_transrate;

    block_t   *p_current_buffer;
    block_t   *p_next_gop;
    mtime_t         i_next_gop_duration;
    size_t          i_next_gop_size;

    transrate_t     tr;
};


