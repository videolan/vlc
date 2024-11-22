/*****************************************************************************
 * mpeg4systems.h: MPEG4 ISO-14496-1 definitions
 *****************************************************************************
 * Copyright (C) 2001-2024 VLC authors, VideoLAN and VideoLabs
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
#ifndef MP4_MPEG4SYSTEMS_H
#define MP4_MPEG4SYSTEMS_H

#include <vlc_codec.h>
#include "dts_header.h"

/* See 14496-1 and http://mp4ra.org/#/object_types */

enum MPEG4_objectTypeIndication
{
    MPEG4_OT_FORBIDDEN                              = 0x00,
    MPEG4_OT_SYSTEMS_14496_1                        = 0x01,
    MPEG4_OT_SYSTEMS_14496_1_BIFS_V2                = 0x02,
    MPEG4_OT_INTERACTION_STREAM                     = 0x03,
    MPEG4_OT_EXTENDED_BIFS_CONFIGURATION            = 0x04,
    MPEG4_OT_AFX                                    = 0x05,
    MPEG4_OT_FONT_DATA_STREAM                       = 0x06,
    MPEG4_OT_SYNTHESIZED_TEXTURE_STREAM             = 0x07,
    MPEG4_OT_STREAMING_TEXT_STREAM                  = 0x08,
    MPEG4_OT_LASER_STREAM                           = 0x09,
    MPEG4_OT_SAF_STREAM                             = 0x0A,
    // RESERVED_FOR_ISO                               0x0B-0x1F
    MPEG4_OT_VISUAL_ISO_IEC_14496_2                 = 0x20,
    MPEG4_OT_VISUAL_ISO_IEC_14496_10_H264           = 0x21,
    MPEG4_OT_ISO_IEC_14496_10_H264_PARAMETER_SETS   = 0x22,
    MPEG4_OT_VISUAL_ISO_IEC_23008_2_H265            = 0x23,
    // RESERVED_FOR_ISO                               0x24-0x3F
    MPEG4_OT_AUDIO_ISO_IEC_14496_3                  = 0x40,
    // RESERVED_FOR_ISO                               0x41-0x5F
    MPEG4_OT_VISUAL_ISO_IEC_13818_2_SIMPLE_PROFILE  = 0x60,
    MPEG4_OT_VISUAL_ISO_IEC_13818_2_MAIN_PROFILE    = 0x61,
    MPEG4_OT_VISUAL_ISO_IEC_13818_2_SNR_PROFILE     = 0x62,
    MPEG4_OT_VISUAL_ISO_IEC_13818_2_SPATIAL_PROFILE = 0x63,
    MPEG4_OT_VISUAL_ISO_IEC_13818_2_HIGH_PROFILE    = 0x64,
    MPEG4_OT_VISUAL_ISO_IEC_13818_2_422_PROFILE     = 0x65,
    MPEG4_OT_AUDIO_ISO_IEC_13818_7_MAIN_PROFILE     = 0x66,
    MPEG4_OT_AUDIO_ISO_IEC_13818_7_LC_PROFILE       = 0x67,
    MPEG4_OT_AUDIO_ISO_IEC_13818_7_SSR_PROFILE      = 0x68,
    MPEG4_OT_AUDIO_ISO_IEC_13818_3                  = 0x69,
    MPEG4_OT_VISUAL_ISO_IEC_11172_2                 = 0x6A,
    MPEG4_OT_AUDIO_ISO_IEC_11172_3                  = 0x6B,
    MPEG4_OT_VISUAL_ISO_IEC_10918_1_JPEG            = 0x6C,
    MPEG4_OT_PORTABLE_NETWORK_GRAPHICS              = 0x6D,
    MPEG4_OT_VISUAL_ISO_IEC_15444_1_JPEG2000        = 0x6E,
    // RESERVED_FOR_ISO                               0x6F-0x9F
    MPEG4_OT_3GPP2_EVRC_VOICE                       = 0xA0,
    MPEG4_OT_3GPP2_SMV_VOICE                        = 0xA1,
    MPEG4_OT_3GPP2_COMPACT_MULTIMEDIA_FORMAT        = 0xA2,
    MPEG4_OT_SMPTE_VC1_VIDEO                        = 0xA3,
    MPEG4_OT_DIRAC_VIDEO_CODER                      = 0xA4,
    MPEG4_OT_DEPRECATED_AC3                         = 0xA5,
    MPEG4_OT_DEPRECATED_EAC3                        = 0xA6,
    MPEG4_OT_DRA_AUDIO                              = 0xA7,
    MPEG4_OT_ITU_G719_AUDIO                         = 0xA8,
    MPEG4_OT_DTSHD_CORE_SUBSTREAM                   = 0xA9,
    MPEG4_OT_DTSHD_CORE_SUBSTREAM_PLUS_EXTENSION    = 0xAA,
    MPEG4_OT_DTSHD_EXTENSION_SUBSTREAM_ONLY_XLL     = 0xAB,
    MPEG4_OT_DTSHD_EXTENSION_SUBSTREAM_ONLY_LBR     = 0xAC,
    MPEG4_OT_OPUS_AUDIO                             = 0xAD,
    MPEG4_OT_DEPRECATED_AC4                         = 0xAE,
    MPEG4_OT_AURO_CX_D3_AUDIO                       = 0xAF,
    MPEG4_OT_REALVIDEO_CODEC_11                     = 0xB0,
    MPEG4_OT_VP9_VIDEO                              = 0xB1,
    MPEG4_OT_DTSUHD_PROFILE_2                       = 0xB2,
    MPEG4_OT_DTSUHD_PROFILE_3_OR_HIGHER             = 0xB3,
    // RESERVED_FOR_REGISTRATION                      0xB4-0xBF
    // USER_PRIVATE                                   0xC0-0xE0
    MPEG4_OT_3GPP2_13K_VOICE                        = 0xE1,
    // USER_PRIVATE                                   0xE2-0xFE
    MPEG4_OT_NO_OBJECT_TYPE_SPECIFIED               = 0xFF,
};

enum MPEG4_streamType
{
    MPEG4_ST_FORBIDDEN                              = 0x00,
    MPEG4_ST_OBJECT_DESCRIPTOR_STREAM               = 0X01,
    MPEG4_ST_CLOCK_REFERENCE_STREAM                 = 0X02,
    MPEG4_ST_SCENE_DESCRIPTION_STREAM               = 0X03,
    MPEG4_ST_VISUAL_STREAM                          = 0X04,
    MPEG4_ST_AUDIO_STREAM                           = 0X05,
    MPEG4_ST_MPEG7_STREAM                           = 0X06,
    MPEG4_ST_IPMP_STREAM                            = 0X07,
    MPEG4_ST_OBJECT_CONTENT_INFO_STREAM             = 0X08,
    MPEG4_ST_MPEG_J_STREAM                          = 0X09,
    MPEG4_ST_INTERACTION_STREAM                     = 0X0A,
    MPEG4_ST_IPMP_TOOL_STREAM                       = 0X0B,
    MPEG4_ST_FONT_DATA_STREAM                       = 0X0C,
    MPEG4_ST_STREAMING_TEXT                         = 0X0D,
    // USER_PRIVATE                                   0x20-0x3F
};

static inline bool MPEG4_get_codec_by_ObjectType(uint8_t oti,
                                                 const uint8_t *p_dsi,
                                                 size_t i_dsi,
                                                 vlc_fourcc_t *pi_codec,
                                                 int *pi_profile)
{
    switch(oti)
    {
    case MPEG4_OT_VISUAL_ISO_IEC_14496_2: /* MPEG4 VIDEO */
        *pi_codec = VLC_CODEC_MP4V;
        break;
    case MPEG4_OT_VISUAL_ISO_IEC_14496_10_H264: /* H.264 */
        *pi_codec = VLC_CODEC_H264;
        break;
    case MPEG4_OT_VISUAL_ISO_IEC_23008_2_H265: /* H.265 */
        *pi_codec = VLC_CODEC_HEVC;
        break;
    case 0x33: /* H.266 */
        *pi_codec = VLC_CODEC_VVC;
        break;
    case MPEG4_OT_AUDIO_ISO_IEC_14496_3:
    case 0x41:
        *pi_codec = VLC_CODEC_MP4A;
        if(i_dsi >= 2 && p_dsi[0] == 0xF8 && (p_dsi[1]&0xE0)== 0x80)
            *pi_codec = VLC_CODEC_ALS;
        break;
         /* MPEG2 video */
    case MPEG4_OT_VISUAL_ISO_IEC_13818_2_SIMPLE_PROFILE:
    case MPEG4_OT_VISUAL_ISO_IEC_13818_2_MAIN_PROFILE:
    case MPEG4_OT_VISUAL_ISO_IEC_13818_2_SNR_PROFILE:
    case MPEG4_OT_VISUAL_ISO_IEC_13818_2_SPATIAL_PROFILE:
    case MPEG4_OT_VISUAL_ISO_IEC_13818_2_HIGH_PROFILE:
    case MPEG4_OT_VISUAL_ISO_IEC_13818_2_422_PROFILE:
        *pi_codec = VLC_CODEC_MPGV;
        break;
        /* Theses are MPEG2-AAC */
    case MPEG4_OT_AUDIO_ISO_IEC_13818_7_MAIN_PROFILE: /* main profile */
    case MPEG4_OT_AUDIO_ISO_IEC_13818_7_LC_PROFILE: /* Low complexity profile */
    case MPEG4_OT_AUDIO_ISO_IEC_13818_7_SSR_PROFILE: /* Scalable Sampling rate profile */
        *pi_codec = VLC_CODEC_MP4A;
        break;
        /* True MPEG 2 audio */
    case MPEG4_OT_AUDIO_ISO_IEC_13818_3:
        *pi_codec = VLC_CODEC_MPGA;
        break;
    case MPEG4_OT_VISUAL_ISO_IEC_11172_2: /* MPEG1 video */
        *pi_codec = VLC_CODEC_MPGV;
        break;
    case MPEG4_OT_AUDIO_ISO_IEC_11172_3: /* MPEG1 audio */
        *pi_codec = VLC_CODEC_MPGA;
        break;
    case MPEG4_OT_VISUAL_ISO_IEC_10918_1_JPEG: /* jpeg */
        *pi_codec = VLC_CODEC_JPEG;
        break;
    case MPEG4_OT_PORTABLE_NETWORK_GRAPHICS: /* png */
        *pi_codec = VLC_CODEC_PNG;
        break;
    case MPEG4_OT_VISUAL_ISO_IEC_15444_1_JPEG2000: /* jpeg2000 */
        *pi_codec = VLC_FOURCC('M','J','2','C');
        break;
    case MPEG4_OT_SMPTE_VC1_VIDEO: /* vc-1 */
        *pi_codec = VLC_CODEC_VC1;
        break;
    case MPEG4_OT_DIRAC_VIDEO_CODER:
        *pi_codec = VLC_CODEC_DIRAC;
        break;
    case MPEG4_OT_DEPRECATED_AC3:
        *pi_codec = VLC_CODEC_A52;
        break;
    case MPEG4_OT_DEPRECATED_EAC3:
        *pi_codec = VLC_CODEC_EAC3;
        break;
    case MPEG4_OT_DTSHD_CORE_SUBSTREAM: /* DTS */
        *pi_codec = VLC_CODEC_DTS;
        break;
    case MPEG4_OT_DTSHD_CORE_SUBSTREAM_PLUS_EXTENSION: /* DTS-HD HRA */
    case MPEG4_OT_DTSHD_EXTENSION_SUBSTREAM_ONLY_XLL:  /* DTS-HD Master Audio */
        *pi_profile = PROFILE_DTS_HD;
        *pi_codec = VLC_CODEC_DTS;
        break;
    case MPEG4_OT_DTSHD_EXTENSION_SUBSTREAM_ONLY_LBR:
        *pi_profile = PROFILE_DTS_EXPRESS;
        *pi_codec = VLC_CODEC_DTS;
        break;
    case MPEG4_OT_OPUS_AUDIO:
        *pi_codec = VLC_CODEC_OPUS;
        break;
    case MPEG4_OT_VP9_VIDEO:
        *pi_codec = VLC_CODEC_VP9;
        break;
    case 0xDD:
        *pi_codec = VLC_CODEC_VORBIS;
        break;
    case MPEG4_OT_3GPP2_13K_VOICE:
        *pi_codec = VLC_CODEC_QCELP;
        break;
    default:
        return false;
    }
    return true;
}

#endif
