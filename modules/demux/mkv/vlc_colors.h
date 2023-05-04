// SPDX-License-Identifier: LGPL-2.1-or-later
/*****************************************************************************
 * mkv/vlc_colors.h: ISO to VLC colorimetry conversion for C++
 *****************************************************************************
 * Copyright © 2023 Videolabs, VLC authors and VideoLAN
 *
 * Authors: Steve Lhomme <robux4@videolabs.io>
 *****************************************************************************/

#pragma once

#include <vlc_es.h>

#ifdef __cplusplus
extern "C" {
#endif

video_transfer_func_t iso_23001_transfer_to_vlc(uint8_t iso_transfer);
video_color_primaries_t iso_23001_primaries_to_vlc(uint8_t iso_primaries);
video_color_space_t iso_23001_matrix_coeffs_to_vlc(uint8_t iso_coefs);

#ifdef __cplusplus
};
#endif