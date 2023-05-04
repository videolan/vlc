// SPDX-License-Identifier: LGPL-2.1-or-later
/*****************************************************************************
 * mkv/vlc_colors.c: ISO to VLC colorimetry conversion for C++
 *****************************************************************************
 * Copyright © 2023 Videolabs, VLC authors and VideoLAN
 *
 * Authors: Steve Lhomme <robux4@videolabs.io>
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "vlc_colors.h"
#include "../../packetizer/iso_color_tables.h"

video_transfer_func_t iso_23001_transfer_to_vlc(uint8_t iso_transfer)
{
    return iso_23001_8_tc_to_vlc_xfer(iso_transfer);
}

video_color_primaries_t iso_23001_primaries_to_vlc(uint8_t iso_primaries)
{
    return iso_23001_8_cp_to_vlc_primaries(iso_primaries);
}

video_color_space_t iso_23001_matrix_coeffs_to_vlc(uint8_t iso_coefs)
{
    return iso_23001_8_mc_to_vlc_coeffs(iso_coefs);
}
