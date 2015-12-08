/*****************************************************************************
 * Copyright © 2010-2014 VideoLAN
 *
 * Authors: Thomas Guillem <thomas.guillem@gmail.com>
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

#ifndef HEVC_NAL_H
# define HEVC_NAL_H

# ifdef HAVE_CONFIG_H
#  include "config.h"
# endif

# include <vlc_common.h>
# include <vlc_codec.h>

/* NAL types from https://www.itu.int/rec/dologin_pub.asp?lang=e&id=T-REC-H.265-201304-I!!PDF-E&type=items */
enum hevc_nal_unit_type_e
{
    HEVC_NAL_TRAIL_N    = 0,
    HEVC_NAL_TRAIL_R    = 1,
    HEVC_NAL_TSA_N      = 2,
    HEVC_NAL_TSA_R      = 3,
    HEVC_NAL_STSA_N     = 4,
    HEVC_NAL_STSA_R     = 5,
    HEVC_NAL_RADL_N     = 6,
    HEVC_NAL_RADL_R     = 7,
    HEVC_NAL_RASL_N     = 8,
    HEVC_NAL_RASL_R     = 9,
    /* 10 to 15 reserved */
    /* Key frames */
    HEVC_NAL_BLA_W_LP   = 16,
    HEVC_NAL_BLA_W_RADL = 17,
    HEVC_NAL_BLA_N_LP   = 18,
    HEVC_NAL_IDR_W_RADL = 19,
    HEVC_NAL_IDR_N_LP   = 20,
    HEVC_NAL_CRA        = 21,
    /* 22 to 31 reserved */
    /* Non VCL NAL*/
    HEVC_NAL_VPS        = 32,
    HEVC_NAL_SPS        = 33,
    HEVC_NAL_PPS        = 34,
    HEVC_NAL_AUD        = 35, /* Access unit delimiter */
    HEVC_NAL_EOS        = 36, /* End of sequence */
    HEVC_NAL_EOB        = 37, /* End of bitstream */
    HEVC_NAL_FD         = 38, /* Filler data*/
    HEVC_NAL_PREF_SEI   = 39, /* Prefix SEI */
    HEVC_NAL_SUFF_SEI   = 40, /* Suffix SEI */
    HEVC_NAL_UNKNOWN
};

/* Parse the hvcC Metadata and convert it to annex b format */
int convert_hevc_nal_units( decoder_t *p_dec, const uint8_t *p_buf,
                            uint32_t i_buf_size, uint8_t *p_out_buf,
                            uint32_t i_out_buf_size, uint32_t *p_sps_pps_size,
                            uint8_t *p_nal_size);


#endif /* HEVC_NAL_H */
