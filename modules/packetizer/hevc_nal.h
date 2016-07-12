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

# include <vlc_common.h>
# include <vlc_es.h>

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

/* NAL types from https://www.itu.int/rec/T-REC-H.265-201504-I */
enum hevc_nal_unit_type_e
{
    HEVC_NAL_TRAIL_N    = 0, /* Trailing */
    HEVC_NAL_TRAIL_R    = 1, /* Trailing Reference */
    HEVC_NAL_TSA_N      = 2, /* Temporal Sublayer Access */
    HEVC_NAL_TSA_R      = 3, /* Temporal Sublayer Access Reference */
    HEVC_NAL_STSA_N     = 4, /* Stepwise Temporal Sublayer Access */
    HEVC_NAL_STSA_R     = 5, /* Stepwise Temporal Sublayer Access Reference */
    HEVC_NAL_RADL_N     = 6, /* Random Access Decodable Leading (display order) */
    HEVC_NAL_RADL_R     = 7, /* Random Access Decodable Leading (display order) Reference */
    HEVC_NAL_RASL_N     = 8, /* Random Access Skipped Leading (display order) */
    HEVC_NAL_RASL_R     = 9, /* Random Access Skipped Leading (display order) Reference */
    /* 10 to 15 reserved */
    HEVC_NAL_RSV_VCL_N10= 10,
    HEVC_NAL_RSV_VCL_N12= 12,
    HEVC_NAL_RSV_VCL_N14= 14,
    /* Key frames */
    HEVC_NAL_BLA_W_LP   = 16, /* Broken Link Access with Associated RASL */
    HEVC_NAL_BLA_W_RADL = 17, /* Broken Link Access with Associated RADL */
    HEVC_NAL_BLA_N_LP   = 18, /* Broken Link Access */
    HEVC_NAL_IDR_W_RADL = 19, /* Instantaneous Decoder Refresh with Associated RADL */
    HEVC_NAL_IDR_N_LP   = 20, /* Instantaneous Decoder Refresh */
    HEVC_NAL_CRA        = 21, /* Clean Random Access */
    /* 22 to 31 reserved */
    HEVC_NAL_IRAP_VCL22 = 22, /* Intra Random Access Point */
    HEVC_NAL_IRAP_VCL23 = 23,
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
    /* 41 to 47 reserved */
    HEVC_NAL_RSV_NVCL41 = 41, /* Reserved Non VCL */
    HEVC_NAL_RSV_NVCL44 = 44,
    HEVC_NAL_RSV_NVCL45 = 45,
    HEVC_NAL_RSV_NVCL47 = 47,
    HEVC_NAL_UNSPEC48   = 48, /* Unspecified (custom) */
    HEVC_NAL_UNSPEC55   = 55,
    HEVC_NAL_UNSPEC56   = 56,
    HEVC_NAL_UNSPEC63   = 63,
    HEVC_NAL_UNKNOWN
};

enum hevc_slice_type_e
{
    HEVC_SLICE_TYPE_B = 0,
    HEVC_SLICE_TYPE_P,
    HEVC_SLICE_TYPE_I
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

static inline uint8_t hevc_getNALLengthSize( const uint8_t *p_hvcC )
{
    return (p_hvcC[21] & 0x03) + 1;
}

static inline uint8_t hevc_getNALType( const uint8_t *p_buf )
{
    return ((p_buf[0] & 0x7E) >> 1);
}

static inline uint8_t hevc_getNALLayer( const uint8_t *p_buf )
{
    return ((p_buf[0] & 0x01) << 6) | (p_buf[1] >> 3);
}

/* NAL decoding */
typedef struct hevc_video_parameter_set_t hevc_video_parameter_set_t;
typedef struct hevc_sequence_parameter_set_t hevc_sequence_parameter_set_t;
typedef struct hevc_picture_parameter_set_t hevc_picture_parameter_set_t;
typedef struct hevc_slice_segment_header_t hevc_slice_segment_header_t;

/* Decodes from three bytes emulation prevented or rbsp stream */
hevc_video_parameter_set_t *    hevc_decode_vps( const uint8_t *, size_t, bool );
hevc_sequence_parameter_set_t * hevc_decode_sps( const uint8_t *, size_t, bool );
hevc_picture_parameter_set_t *  hevc_decode_pps( const uint8_t *, size_t, bool );
hevc_slice_segment_header_t *   hevc_decode_slice_header( const uint8_t *, size_t, bool,
                                                  hevc_sequence_parameter_set_t **pp_sps/* HEVC_MAX_SPS */,
                                                  hevc_picture_parameter_set_t **pp_pps /* HEVC_MAX_PPS */);

void hevc_rbsp_release_vps( hevc_video_parameter_set_t * );
void hevc_rbsp_release_sps( hevc_sequence_parameter_set_t * );
void hevc_rbsp_release_pps( hevc_picture_parameter_set_t * );
void hevc_rbsp_release_slice_header( hevc_slice_segment_header_t * );

/* Converts HEVCDecoderConfigurationRecord to Annex B format */
uint8_t * hevc_hvcC_to_AnnexB_NAL( const uint8_t *p_buf, size_t i_buf,
                                   size_t *pi_res, uint8_t *pi_nal_length_size );

bool hevc_get_xps_id(const uint8_t *p_buf, size_t i_buf, uint8_t *pi_id);
bool hevc_get_picture_size( const hevc_sequence_parameter_set_t *, unsigned *p_w, unsigned *p_h,
                            unsigned *p_vw, unsigned *p_vh );
bool hevc_get_frame_rate( const hevc_sequence_parameter_set_t *,
                          hevc_video_parameter_set_t ** /* HEVC_MAX_VPS || NULL */,
                          unsigned *pi_num, unsigned *pi_den );
bool hevc_get_colorimetry( const hevc_sequence_parameter_set_t *p_sps,
                           video_color_primaries_t *p_primaries,
                           video_transfer_func_t *p_transfer,
                           video_color_space_t *p_colorspace,
                           bool *p_full_range );
bool hevc_get_slice_type( const hevc_slice_segment_header_t *, enum hevc_slice_type_e * );

/* Get level and Profile from DecoderConfigurationRecord */
bool hevc_get_profile_level(const es_format_t *p_fmt, uint8_t *pi_profile,
                            uint8_t *pi_level, uint8_t *pi_nal_length_size);

#endif /* HEVC_NAL_H */
