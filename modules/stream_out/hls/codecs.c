/*****************************************************************************
 * codecs.c:
 *****************************************************************************
 * Copyright (C) 2023 VLC authors and VideoLAN
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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <vlc_common.h>

#include <vlc_memstream.h>

#include "../codec/hxxx_helper.h"
#include "codecs.h"

static int FormatAVC1(struct vlc_memstream *ms, const es_format_t *fmt)
{
    /* Parse the h264 constraint flag. */
    uint8_t constraints = 0;
    uint8_t profile = 0;
    uint8_t level = 0;
    struct hxxx_helper hh;
    hxxx_helper_init(&hh, NULL, fmt->i_codec, 0, 0);
    if (hxxx_helper_set_extra(&hh, fmt->p_extra, fmt->i_extra) == VLC_SUCCESS)
    {
        h264_helper_get_constraint_flag(&hh, &constraints);
        hxxx_helper_get_current_profile_level(&hh, &profile, &level);
    }
    hxxx_helper_clean(&hh);

    const int wrote = vlc_memstream_printf(
        ms, "avc1.%02X%02X%02X", profile, constraints, level);
    return (wrote == -1) ? VLC_ENOMEM : VLC_SUCCESS;
}

static int FormatMP4A(struct vlc_memstream *ms, const es_format_t *fmt)
{
    const int wrote = vlc_memstream_printf(
        ms, "mp4a.40.%02x", (fmt->i_profile == -1) ? 2u : fmt->i_profile + 1u);
    return (wrote == -1) ? VLC_ENOMEM : VLC_SUCCESS;
}

int hls_codec_Format(struct vlc_memstream *ms, const es_format_t *fmt)
{
    switch (fmt->i_codec)
    {
        case VLC_CODEC_H264:
            return FormatAVC1(ms, fmt);
        case VLC_CODEC_MP4A:
            return FormatMP4A(ms, fmt);
        default:
            return VLC_ENOTSUP;
    }
}
