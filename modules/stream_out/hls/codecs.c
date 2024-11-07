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

#include "../../packetizer/h264_nal.h"
#include "../../packetizer/hevc_nal.h"
#include "../../packetizer/hxxx_ep3b.h"
#include "../../packetizer/hxxx_nal.h"
#include "codecs.h"

static int FormatAVC(struct vlc_memstream *ms, const es_format_t *fmt)
{
    /* Parse the h264 constraint flag. */
    hxxx_iterator_ctx_t it;
    hxxx_iterator_init(&it, fmt->p_extra, fmt->i_extra, 0);
    const uint8_t *nal = NULL;
    size_t nal_size;
    if (hxxx_annexb_iterate_next(&it, &nal, &nal_size))
    {
        switch (h264_getNALType(nal))
        {
        case H264_NAL_SPS:
            break;
        default:
            nal = NULL;
            break;
        }
    }

    /* NAL units aren't provisioned. Codec description will be minimal. */
    if (unlikely(nal == NULL || nal_size<4))
        return VLC_SUCCESS;

    uint8_t profile = nal[1];
    uint8_t constraints = nal[2];
    uint8_t level = nal[3];

    const int written = vlc_memstream_printf(
        ms, "avc1.%02X%02X%02X", profile, constraints, level);
    return (written < 0) ? VLC_ENOMEM : VLC_SUCCESS;
}

static uint32_t ReverseBits(uint32_t in)
{
    uint32_t out = 0;
    for (int i = 0; i < 32; ++i)
    {
        out |= in & 1;
        if (i == 31)
            break;
        out <<= 1;
        in >>= 1;
    }
    return out;
}

static int FormatHEVC(struct vlc_memstream *ms, const es_format_t *fmt)
{
    if (vlc_memstream_puts(ms, "hvc1") < 0)
        return VLC_ENOMEM;

    uint8_t to_skip = 8;
    hxxx_iterator_ctx_t it;
    hxxx_iterator_init(&it, fmt->p_extra, fmt->i_extra, 0);
    const uint8_t *nal = NULL;
    size_t nal_size;
    if (hxxx_annexb_iterate_next(&it, &nal, &nal_size))
    {
        switch (hevc_getNALType(nal))
        {
            case HEVC_NAL_SPS:
                break;
            case HEVC_NAL_VPS:
                to_skip += 24;
                break;
            default:
                nal = NULL;
                break;
        }
    }

    /* NAL units aren't provisioned. Codec description will be minimal. */
    if (unlikely(nal == NULL))
        return VLC_SUCCESS;

    const void *profile_space = nal + 2;
    struct hxxx_bsfw_ep3b_ctx_s ep3b;
    hxxx_bsfw_ep3b_ctx_init(&ep3b);
    bs_t bs;
    bs_init_custom(
        &bs, profile_space, nal_size - 2, &hxxx_bsfw_ep3b_callbacks, &ep3b);

    /* Jump to profile_tier_level @profile_space. */
    bs_skip(&bs, to_skip);

    const char *profile_space_ids[4] = {"", "A", "B", "C"};
    const char *profile_space_id = profile_space_ids[bs_read(&bs, 2)];
    const char tier_flag = (bs_read1(&bs) != 0) ? 'H' : 'L';
    const uint8_t profile_idc = bs_read(&bs, 5);
    const uint32_t profile_compatibility_flag = ReverseBits(bs_read(&bs, 32));
    /* Read constraints. */
    uint8_t constraints[6];
    uint8_t cnstrflags = 0;
    for (unsigned i = 0; i < ARRAY_SIZE(constraints); ++i)
    {
        constraints[i] = bs_read(&bs, 8);
        cnstrflags |= (!!constraints[i] << i);
    }
    const uint8_t level_idc = bs_read(&bs, 8);

    if (vlc_memstream_printf(ms,
                             "%s.%u.%X.%c%u",
                             profile_space_id,
                             profile_idc,
                             profile_compatibility_flag,
                             tier_flag,
                             level_idc) < 0)
        return VLC_ENOMEM;

    /* Append constraints bytes. */
    for (unsigned i = 0; cnstrflags; cnstrflags >>= 1)
    {
        if (vlc_memstream_printf(ms, ".%2.2x", constraints[i++]) < 0)
            return VLC_ENOMEM;
    }

    return VLC_SUCCESS;
}

static int FormatMP4A(struct vlc_memstream *ms, const es_format_t *fmt)
{
    const int written = vlc_memstream_printf(
        ms, "mp4a.40.%02x", (fmt->i_profile == -1) ? 2u : fmt->i_profile + 1u);
    return (written < 0) ? VLC_ENOMEM : VLC_SUCCESS;
}

static int FormatWebVTT(struct vlc_memstream *ms)
{
    const int written = vlc_memstream_puts(ms, "wvtt");
    return (written <= 0) ? VLC_ENOMEM : VLC_SUCCESS;
}

int hls_codec_Format(struct vlc_memstream *ms, const es_format_t *fmt)
{
    switch (fmt->i_codec)
    {
        case VLC_CODEC_H264:
            return FormatAVC(ms, fmt);
        case VLC_CODEC_HEVC:
            return FormatHEVC(ms, fmt);
        case VLC_CODEC_MP4A:
            return FormatMP4A(ms, fmt);
        case VLC_CODEC_TEXT:
        case VLC_CODEC_WEBVTT:
            return FormatWebVTT(ms);
        default:
            return VLC_ENOTSUP;
    }
}

bool hls_codec_IsSupported(const es_format_t *fmt)
{
    return fmt->i_codec == VLC_CODEC_H264 || fmt->i_codec == VLC_CODEC_HEVC ||
           fmt->i_codec == VLC_CODEC_MP4A || fmt->i_codec == VLC_CODEC_TEXT ||
           fmt->i_codec == VLC_CODEC_WEBVTT;
}
