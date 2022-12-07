/*****************************************************************************
 * gst_mem.c: GStreamer Memory to picture converter
 *****************************************************************************
 * Copyright (C) 2022 VLC authors and VideoLAN
 *
 * Author: Yann Lochet <yann@l0chet.fr>
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

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/video/video-format.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_filter.h>
#include <vlc_picture.h>

#include "../codec/gstreamer/gstcopypicture.h"
#include "../codec/gstreamer/gst_mem.h"

static picture_t * Filter(filter_t *p_filter, picture_t *src)
{
    struct gst_mem_pic_context *pctx = container_of(src->context,
                                                    struct gst_mem_pic_context, s);
    GstBuffer *p_buf = pctx->p_buf;

    picture_t *dst = filter_NewPicture(p_filter);
    if (!dst)
        return NULL;
    picture_CopyProperties(dst, src);

    GstVideoFrame frame;
    if (unlikely(!gst_video_frame_map(&frame, pctx->p_vinfo, p_buf, GST_MAP_READ)))
    {
        msg_Err(p_filter, "failed to map gst video frame");
        return NULL;
    }

    gst_CopyPicture(dst, &frame);
    gst_video_frame_unmap(&frame);

    picture_Release(src);
    return dst;
}

static picture_t * Filter_chain( filter_t *p_filter, picture_t *src )
{
    filter_chain_t *p_chain = p_filter->p_sys;
    return filter_chain_VideoFilter(p_chain, src);
}

static const struct vlc_filter_operations filter_ops = {
    .filter_video = Filter,
};

static const struct vlc_filter_operations chain_ops = {
    .filter_video = Filter_chain,
};

static int Open(filter_t *p_filter)
{
    if(p_filter->fmt_in.video.i_chroma != VLC_CODEC_GST_MEM_OPAQUE)
        return VLC_EGENERIC;

    if(p_filter->fmt_out.video.i_chroma == VLC_CODEC_NV12)
    {
        p_filter->ops = &filter_ops;
        return VLC_SUCCESS;
    }

    es_format_t fmt_intermediate;
    es_format_Copy(&fmt_intermediate, &p_filter->fmt_out);
    fmt_intermediate.video.i_chroma = fmt_intermediate.i_codec = VLC_CODEC_NV12;

    filter_chain_t *p_chain = filter_chain_NewVideo(p_filter, false, &p_filter->owner);
    if (p_chain == NULL)
        return VLC_ENOMEM;
    filter_chain_Reset(p_chain, &p_filter->fmt_in, p_filter->vctx_in, &p_filter->fmt_out);

    int ret;
    ret = filter_chain_AppendConverter(p_chain, &fmt_intermediate);
    if (ret != 0)
        return VLC_EGENERIC;

    ret = filter_chain_AppendConverter(p_chain, NULL);
    if (ret != 0)
        return VLC_EGENERIC;

    p_filter->p_sys = p_chain;
    p_filter->ops = &chain_ops;

    return VLC_SUCCESS;
}

vlc_module_begin()
    set_shortname(N_("GST_MEM converter"))
    set_description(N_("GST_MEM Chroma Converter filter"))
    set_subcategory(SUBCAT_VIDEO_VFILTER)
    set_callback_video_converter(Open, 10)
vlc_module_end()
