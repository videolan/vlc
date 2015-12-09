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

#define HEVC_VPS_MAX 16
#define HEVC_SPS_MAX 16
#define HEVC_PPS_MAX 64

enum hevc_general_profile_idc_e
{
    HEVC_PROFILE_NONE               = 0,
    HEVC_PROFILE_MAIN               = 1,
    HEVC_PROFILE_MAIN_10            = 2,
    HEVC_PROFILE_MAIN_STILL_PICTURE = 3,
    HEVC_PROFILE_REXT               = 4, /* range extensions */
};


/* Values built from 9 bits mapping of the A-2 bitstream indications for conformance */
#define HEVC_EXT_PROFILE_MONOCHROME                 0x1F9
#define HEVC_EXT_PROFILE_MONOCHROME_12              0x139
#define HEVC_EXT_PROFILE_MONOCHROME_16              0x039
#define HEVC_EXT_PROFILE_MAIN_12                    0x131
#define HEVC_EXT_PROFILE_MAIN_422_10                0x1A1
#define HEVC_EXT_PROFILE_MAIN_422_12                0x121
#define HEVC_EXT_PROFILE_MAIN_444                   0x1C1
#define HEVC_EXT_PROFILE_MAIN_444_10                0x181
#define HEVC_EXT_PROFILE_MAIN_444_12                0x101
#define HEVC_EXT_PROFILE_MAIN_INTRA                 0x1F4 /* From this one, lowest bit is insignifiant */
#define HEVC_EXT_PROFILE_MAIN_10_INTRA              0x1B4
#define HEVC_EXT_PROFILE_MAIN_12_INTRA              0x134
#define HEVC_EXT_PROFILE_MAIN_422_10_INTRA          0x1A4
#define HEVC_EXT_PROFILE_MAIN_422_12_INTRA          0x124
#define HEVC_EXT_PROFILE_MAIN_444_INTRA             0x1C4
#define HEVC_EXT_PROFILE_MAIN_444_10_INTRA          0x184
#define HEVC_EXT_PROFILE_MAIN_444_12_INTRA          0x104
#define HEVC_EXT_PROFILE_MAIN_444_16_INTRA          0x004
#define HEVC_EXT_PROFILE_MAIN_444_STILL_PICTURE     0x1C6
#define HEVC_EXT_PROFILE_MAIN_444_16_STILL_PICTURE  0x006

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

#define HEVC_MIN_HVCC_SIZE 23

/* checks if data is an HEVCDecoderConfigurationRecord */
static inline bool hevc_ishvcC( const uint8_t *p_buf, size_t i_buf )
{
    return ( i_buf >= HEVC_MIN_HVCC_SIZE &&
             p_buf[0] == 0x01 &&
            (p_buf[13] & 0xF0) == 0xF0 && /* Match all reserved bits */
            (p_buf[15] & 0xFC) == 0xFC &&
            (p_buf[16] & 0xFC) == 0xFC &&
            (p_buf[17] & 0xF8) == 0xF8 &&
            (p_buf[18] & 0xF8) == 0xF8 &&
            (p_buf[21] & 0x03) != 0x02
           );
}

/* Converts HEVCDecoderConfigurationRecord to Annex B format */
uint8_t * hevc_hvcC_to_AnnexB_NAL( const uint8_t *p_buf, size_t i_buf,
                                   size_t *pi_res, uint8_t *pi_nal_length_size );


#endif /* HEVC_NAL_H */
