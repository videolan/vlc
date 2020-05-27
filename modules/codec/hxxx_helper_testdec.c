/*****************************************************************************
 * hxxx_helper_testdec.c: test decoder for hxxx_helper API
 *****************************************************************************
 * Copyright © 2020 VideoLAN, VLC authors and libbluray AUTHORS
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
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_codec.h>
#include "hxxx_helper.h"

struct decoder_sys {
    struct hxxx_helper hh;
};

static void
Flush(decoder_t *dec)
{
    (void) dec;
}

static int
DecodeBlock(decoder_t *dec, block_t *block)
{
    struct decoder_sys *sys = dec->p_sys;

    if (block == NULL)
        return VLCDEC_SUCCESS;

    bool config_changed;
    block = sys->hh.pf_process_block(&sys->hh, block, &config_changed);

    if (block == NULL)
        return VLCDEC_SUCCESS;

    if (config_changed)
    {
        int ret;
        video_color_primaries_t primaries;
        video_transfer_func_t transfer;
        video_color_space_t colorspace;
        video_color_range_t full_range;

        ret = hxxx_helper_get_colorimetry(&sys->hh, &primaries, &transfer,
                                          &colorspace, &full_range);
        if (ret == VLC_SUCCESS)
        {
            dec->fmt_out.video.primaries = primaries;
            dec->fmt_out.video.transfer = transfer;
            dec->fmt_out.video.space = colorspace;
            dec->fmt_out.video.color_range = full_range;
        }

        unsigned width, height, vis_width, vis_height;
        ret = hxxx_helper_get_current_picture_size(&sys->hh,
                                                   &width, &height,
                                                   &vis_width, &vis_height);
        if (ret == VLC_SUCCESS)
        {
            dec->fmt_out.video.i_width =
            dec->fmt_out.video.i_visible_width = vis_width;
            dec->fmt_out.video.i_height =
            dec->fmt_out.video.i_visible_height = vis_height;
        }

        int sar_num, sar_den;
        ret = hxxx_helper_get_current_sar(&sys->hh, &sar_num, &sar_den);
        if (ret == VLC_SUCCESS)
        {
            dec->fmt_out.video.i_sar_num = sar_num;
            dec->fmt_out.video.i_sar_den = sar_den;
        }

        ret = decoder_UpdateVideoOutput(dec, NULL);
        if (ret != 0)
            return VLCDEC_ECRITICAL;
    }

    block_Release(block);

    return VLCDEC_SUCCESS;
}

static void
CloseDecoder(vlc_object_t *this)
{
    decoder_t *dec = (void *)this;
    struct decoder_sys *sys = dec->p_sys;

    hxxx_helper_clean(&sys->hh);
    free(sys);
}

static int
OpenDecoder(vlc_object_t *this)
{
    decoder_t *dec = (void *)this;

    switch (dec->fmt_in.i_codec)
    {
        case VLC_CODEC_H264:
        case VLC_CODEC_HEVC:
            break;
        default:
            return VLC_EGENERIC;
    }

    struct decoder_sys *sys = malloc(sizeof(*sys));
    if (sys == NULL)
        return VLC_EGENERIC;

    hxxx_helper_init(&sys->hh, this, dec->fmt_in.i_codec,
                     var_InheritBool(this, "hxxx-helper-testdec-xvcC"));

    int ret = hxxx_helper_set_extra(&sys->hh, dec->fmt_in.p_extra,
                                    dec->fmt_in.i_extra);
    if (ret != VLC_SUCCESS)
    {
        hxxx_helper_clean(&sys->hh);
        free(sys);
        return ret;
    }

    dec->p_sys = sys;
    dec->pf_decode = DecodeBlock;
    dec->pf_flush  = Flush;

    dec->fmt_out.video = dec->fmt_in.video;
    dec->fmt_out.video.p_palette = NULL;

    return VLC_SUCCESS;
}

vlc_module_begin()
    set_category(CAT_INPUT)
    set_subcategory(SUBCAT_INPUT_VCODEC)
    set_description(N_("hxxx test decoder"))
    add_shortcut("hxxxhelper")
    set_capability("video decoder", 0)
    add_bool("hxxx-helper-testdec-xvcC", false, NULL, NULL, true)
    set_callbacks(OpenDecoder, CloseDecoder)
vlc_module_end()
