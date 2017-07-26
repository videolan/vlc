/*****************************************************************************
 * jpeg2000.h J2K definitions
 *****************************************************************************
 * Copyright (C) 2017 VideoLAN Authors
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
#ifndef VLC_JPEG2000_H
#define VLC_JPEG2000_H

#define J2K_BOX_JP2C VLC_FOURCC('j','p','2','c')

enum j2k_profiles_e
{
    J2K_PROFILE_SD = 0,
    J2K_PROFILE_HD,
    J2K_PROFILE_3G,
    J2K_PROFILE_S3D_HD,
    J2K_PROFILE_S3D_3G,
};

static inline bool j2k_is_valid_framerate( unsigned num, unsigned den )
{
    const struct
    {
        const unsigned num;
        const unsigned den;
    } numdens[] = { /* table 2-99 */
        { 24000, 1001 },
        { 24, 1 },
        { 25, 1 },
        { 30000, 1001 },
        { 30, 1 },
        { 50, 1 },
        { 60000, 1001 },
        { 60, 1 },
    };
    for( size_t i=0; i<ARRAY_SIZE(numdens); i++ )
        if( numdens[i].den == den && numdens[i].num == num )
            return true;
    return false;
}

static inline enum j2k_profiles_e j2k_get_profile( unsigned w, unsigned h,
                                                   unsigned num, unsigned den, bool p )
{
    const uint64_t s = w *(uint64_t)h;
    const uint64_t f = num / den;
    if( s <= 720*576 && f < 50 )
        return J2K_PROFILE_SD; /* VSF_TR-01_2013-04-15 */
    else if( s <= 1280*720 && f < 60 && p )
        return J2K_PROFILE_HD;
    else if( s <= 1920*1080 && f < 60 && !p )
        return J2K_PROFILE_HD;
    else
        return J2K_PROFILE_3G;
}

static const struct
{
    const uint16_t min;
    const uint16_t max;
} j2k_profiles_rates[] = {
    [J2K_PROFILE_SD]     = {  25, 200 },
    [J2K_PROFILE_HD]     = {  75, 200 },
    [J2K_PROFILE_3G]     = { 100, 400 },
    [J2K_PROFILE_S3D_HD] = { 150, 200 },
    [J2K_PROFILE_S3D_3G] = { 200, 400 },
};

enum j2k_color_specs_e
{
    J2K_COLOR_SPEC_UNKNOWN = 0,
    J2K_COLOR_SPEC_SRGB,
    J2K_COLOR_SPEC_REC_601,
    J2K_COLOR_SPEC_REC_709,
    J2K_COLOR_SPEC_CIE_LUV,
    J2K_COLOR_SPEC_CIE_XYZ,
    J2K_COLOR_SPEC_REC_2020,
    J2K_COLOR_SPEC_SMPTE_2084,
};

static const struct
{
    video_color_primaries_t primaries;
    video_transfer_func_t transfer;
    video_color_space_t space;
} j2k_color_specifications[] = {
    [J2K_COLOR_SPEC_UNKNOWN] = { COLOR_PRIMARIES_UNDEF,
                                 TRANSFER_FUNC_UNDEF,
                                 COLOR_SPACE_UNDEF },
    [J2K_COLOR_SPEC_SRGB] =    { COLOR_PRIMARIES_SRGB,
                                 TRANSFER_FUNC_SRGB,
                                 COLOR_SPACE_SRGB },
    [J2K_COLOR_SPEC_REC_601] = { COLOR_PRIMARIES_BT601_625,
                                 TRANSFER_FUNC_SMPTE_170,
                                 COLOR_SPACE_BT601 },
    [J2K_COLOR_SPEC_REC_709] = { COLOR_PRIMARIES_BT709,
                                 TRANSFER_FUNC_BT709,
                                 COLOR_SPACE_BT709 },
    [J2K_COLOR_SPEC_CIE_LUV] = { COLOR_PRIMARIES_UNDEF,
                                 TRANSFER_FUNC_UNDEF,
                                 COLOR_SPACE_UNDEF },
    [J2K_COLOR_SPEC_CIE_XYZ] = { COLOR_PRIMARIES_UNDEF,
                                 TRANSFER_FUNC_UNDEF,
                                 COLOR_SPACE_UNDEF },
    [J2K_COLOR_SPEC_REC_2020] ={ COLOR_PRIMARIES_BT2020,
                                 TRANSFER_FUNC_BT2020,
                                 COLOR_SPACE_BT2020 },
    [J2K_COLOR_SPEC_SMPTE_2084]={ COLOR_PRIMARIES_SMTPE_170,
                                 TRANSFER_FUNC_SMPTE_ST2084,
                                 COLOR_SPACE_BT2020 },
};

static inline void j2k_fill_color_profile( enum j2k_color_specs_e e,
                                           video_color_primaries_t *primaries,
                                           video_transfer_func_t *transfer,
                                           video_color_space_t *space )
{
    if( e > J2K_COLOR_SPEC_UNKNOWN && e <= J2K_COLOR_SPEC_SMPTE_2084 )
    {
        *primaries = j2k_color_specifications[e].primaries;
        *transfer = j2k_color_specifications[e].transfer;
        *space = j2k_color_specifications[e].space;
    }
}

static inline enum j2k_color_specs_e
        j2k_get_color_spec( video_color_primaries_t primaries,
                            video_transfer_func_t transfer ,
                            video_color_space_t space )
{
    enum j2k_color_specs_e e;
    for( e = J2K_COLOR_SPEC_UNKNOWN; e <= J2K_COLOR_SPEC_SMPTE_2084; e++ )
    {
        if( primaries == j2k_color_specifications[e].primaries &&
            transfer == j2k_color_specifications[e].transfer &&
            space == j2k_color_specifications[e].space )
        {
            return e;
        }
    }
    return J2K_COLOR_SPEC_UNKNOWN;
}

#endif
