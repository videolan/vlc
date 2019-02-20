/*****************************************************************************
 * mpeg4audio.h: MPEG 4 audio definitions
 *****************************************************************************
 * Copyright (C) 2001-2017 VLC authors and VideoLAN
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
enum mpeg4_audioObjectType /* ISO/IEC 14496-3:2009 1.5.1 */
{
    AOT_AAC_MAIN        = 1,
    AOT_AAC_LC          = 2,
    AOT_AAC_SSR         = 3,
    AOT_AAC_LTP         = 4,
    AOT_AAC_SBR         = 5,
    AOT_AAC_SC          = 6,
    AOT_TWINVQ          = 7,
    AOT_CELP            = 8,
    AOT_HVXC            = 9,
    AOT_RESERVED10      = 10,
    AOT_RESERVED11      = 11,
    AOT_TTSI            = 12,
    AOT_MAIN_SYNTHETIC  = 13,
    AOT_WAVETABLES      = 14,
    AOT_GENERAL_MIDI    = 15,
    AOT_ALGORITHMIC     = 16,
    AOT_ER_AAC_LC       = 17,
    AOT_RESERVED18      = 18,
    AOT_ER_AAC_LTP      = 19,
    AOT_ER_AAC_SC       = 20,
    AOT_ER_TWINVQ       = 21,
    AOT_ER_BSAC         = 22,
    AOT_ER_AAC_LD       = 23,
    AOT_ER_CELP         = 24,
    AOT_ER_HXVC         = 25,
    AOT_ER_HILN         = 26,
    AOT_ER_Parametric   = 27,
    AOT_SSC             = 28,
    AOT_AAC_PS          = 29,
    AOT_MPEG_SURROUND   = 30,
    AOT_ESCAPE          = 31,
    AOT_LAYER1          = 32,
    AOT_LAYER2          = 33,
    AOT_LAYER3          = 34,
    AOT_DST             = 35,
    AOT_ALS             = 36,
    AOT_SLS             = 37,
    AOT_SLS_NON_CORE    = 38,
    AOT_ER_AAC_ELD      = 39,
    AOT_SMR_SIMPLE      = 40,
    AOT_SMR_MAIN        = 41,
};

enum
{
    AAC_PROFILE_MAIN = AOT_AAC_MAIN - 1,
    AAC_PROFILE_LC,
    AAC_PROFILE_SSR,
    AAC_PROFILE_LTP,
    AAC_PROFILE_HE,
    AAC_PROFILE_LD   = AOT_ER_AAC_LD - 1,
    AAC_PROFILE_HEv2 = AOT_AAC_PS - 1,
    AAC_PROFILE_ELD  = AOT_ER_AAC_ELD - 1,
    /* Similar shift signaling as avcodec, as signaling should have been
       done in ADTS header. Values defaults to MPEG4 */
    AAC_PROFILE_MPEG2_LC = AAC_PROFILE_LC + 128,
    AAC_PROFILE_MPEG2_HE = AAC_PROFILE_HE + 128,
};

#define MPEG4_ASC_MAX_INDEXEDPOS 9

static const uint32_t mpeg4_asc_channelsbyindex[MPEG4_ASC_MAX_INDEXEDPOS] =
{
    0, /* Set later */

    AOUT_CHAN_CENTER, AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT,

    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_LFE,

    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT,

    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT |
    AOUT_CHAN_CENTER,

    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT |
    AOUT_CHAN_CENTER | AOUT_CHAN_LFE,

    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_MIDDLELEFT |
    AOUT_CHAN_MIDDLERIGHT | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT |
    AOUT_CHAN_CENTER,

    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_MIDDLELEFT |
    AOUT_CHAN_MIDDLERIGHT | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT |
    AOUT_CHAN_CENTER | AOUT_CHAN_LFE
};
