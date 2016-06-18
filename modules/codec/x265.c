/*****************************************************************************
 * x265.c: HEVC/H.265 video encoder
 *****************************************************************************
 * Copyright (C) 2013 Rafaël Carré
 *
 * Authors: Rafaël Carré <funman@videolanorg>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#define VLC_MODULE_LICENSE VLC_LICENSE_GPL_2_PLUS
#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_threads.h>
#include <vlc_sout.h>
#include <vlc_codec.h>

#include <x265.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open (vlc_object_t *);
static void Close(vlc_object_t *);

vlc_module_begin ()
    set_description(N_("H.265/HEVC encoder (x265)"))
    set_capability("encoder", 200)
    set_callbacks(Open, Close)
    set_category(CAT_INPUT)
    set_subcategory(SUBCAT_INPUT_VCODEC)
vlc_module_end ()

struct encoder_sys_t
{
    x265_encoder    *h;
    x265_param      param;

    mtime_t         i_initial_delay;

    mtime_t         dts;
    mtime_t         initial_date;
#ifndef NDEBUG
    mtime_t         start;
#endif
};

static block_t *Encode(encoder_t *p_enc, picture_t *p_pict)
{
    encoder_sys_t *p_sys = p_enc->p_sys;
    x265_picture pic;

    x265_picture_init(&p_sys->param, &pic);

    if (likely(p_pict)) {
        pic.pts = p_pict->date;
        if (unlikely(p_sys->initial_date == 0)) {
            p_sys->initial_date = p_pict->date;
#ifndef NDEBUG
            p_sys->start = mdate();
#endif
        }

        for (int i = 0; i < p_pict->i_planes; i++) {
            pic.planes[i] = p_pict->p[i].p_pixels;
            pic.stride[i] = p_pict->p[i].i_pitch;
        }
    }

    x265_nal *nal;
    uint32_t i_nal = 0;
    x265_encoder_encode(p_sys->h, &nal, &i_nal,
            likely(p_pict) ? &pic : NULL, &pic);

    if (!i_nal)
        return NULL;

    int i_out = 0;
    for (uint32_t i = 0; i < i_nal; i++)
        i_out += nal[i].sizeBytes;

    block_t *p_block = block_Alloc(i_out);
    if (!p_block)
        return NULL;

    /* all payloads are sequentially laid out in memory */
    memcpy(p_block->p_buffer, nal[0].payload, i_out);

    /* This isn't really valid for streams with B-frames */
    p_block->i_length = CLOCK_FREQ *
        p_enc->fmt_in.video.i_frame_rate_base /
            p_enc->fmt_in.video.i_frame_rate;

    p_block->i_pts = p_sys->initial_date + pic.poc * p_block->i_length;
    p_block->i_dts = p_sys->initial_date + p_sys->dts++ * p_block->i_length;

    switch (pic.sliceType)
    {
    case X265_TYPE_I:
    case X265_TYPE_IDR:
        p_block->i_flags |= BLOCK_FLAG_TYPE_I;
        break;
    case X265_TYPE_P:
        p_block->i_flags |= BLOCK_FLAG_TYPE_P;
        break;
    case X265_TYPE_B:
    case X265_TYPE_BREF:
        p_block->i_flags |= BLOCK_FLAG_TYPE_B;
        break;
    }

#ifndef NDEBUG
    msg_Dbg(p_enc, "%zu bytes (frame %"PRId64", %.2ffps)", p_block->i_buffer,
        p_sys->dts, (float)p_sys->dts * CLOCK_FREQ / (mdate() - p_sys->start));
#endif

    return p_block;
}

static int  Open (vlc_object_t *p_this)
{
    encoder_t     *p_enc = (encoder_t *)p_this;
    encoder_sys_t *p_sys;

    if (p_enc->fmt_out.i_codec != VLC_CODEC_HEVC && !p_enc->obj.force)
        return VLC_EGENERIC;

    p_enc->fmt_out.i_cat = VIDEO_ES;
    p_enc->fmt_out.i_codec = VLC_CODEC_HEVC;
    p_enc->p_sys = p_sys = malloc(sizeof(encoder_sys_t));
    if (!p_sys)
        return VLC_ENOMEM;

    p_enc->fmt_in.i_codec = VLC_CODEC_I420;

    x265_param *param = &p_sys->param;
    x265_param_default(param);

    param->frameNumThreads = vlc_GetCPUCount();
    param->bEnableWavefront = 0; // buggy in x265, use frame threading for now
    param->maxCUSize = 16; /* use smaller macroblock */

#if X265_BUILD >= 6
    param->fpsNum = p_enc->fmt_in.video.i_frame_rate;
    param->fpsDenom = p_enc->fmt_in.video.i_frame_rate_base;
    if (!param->fpsNum) {
        param->fpsNum = 25;
        param->fpsDenom = 1;
    }
#else
    if (p_enc->fmt_in.video.i_frame_rate_base) {
        param->frameRate = p_enc->fmt_in.video.i_frame_rate /
            p_enc->fmt_in.video.i_frame_rate_base;
    } else {
        param->frameRate = 25;
    }
#endif
    param->sourceWidth = p_enc->fmt_in.video.i_visible_width;
    param->sourceHeight = p_enc->fmt_in.video.i_visible_height;

    if (param->sourceWidth & (param->maxCUSize - 1)) {
        msg_Err(p_enc, "Width (%d) must be a multiple of %d",
            param->sourceWidth, param->maxCUSize);
        free(p_sys);
        return VLC_EGENERIC;
    }
    if (param->sourceHeight & 7) {
        msg_Err(p_enc, "Height (%d) must be a multiple of 8", param->sourceHeight);
        free(p_sys);
        return VLC_EGENERIC;
    }

    if (p_enc->fmt_out.i_bitrate > 0) {
        param->rc.bitrate = p_enc->fmt_out.i_bitrate / 1000;
        param->rc.rateControlMode = X265_RC_ABR;
    }

    p_sys->h = x265_encoder_open(param);
    if (p_sys->h == NULL) {
        msg_Err(p_enc, "cannot open x265 encoder");
        free(p_sys);
        return VLC_EGENERIC;
    }

    x265_nal *nal;
    uint32_t i_nal;
    if (x265_encoder_headers(p_sys->h, &nal, &i_nal) < 0) {
        msg_Err(p_enc, "cannot get x265 headers");
        Close(VLC_OBJECT(p_enc));
        return VLC_EGENERIC;
    }

    size_t i_extra = 0;
    for (uint32_t i = 0; i < i_nal; i++)
        i_extra += nal[i].sizeBytes;

    p_enc->fmt_out.i_extra = i_extra;

    uint8_t *p_extra = p_enc->fmt_out.p_extra = malloc(i_extra);
    if (!p_extra) {
        Close(VLC_OBJECT(p_enc));
        return VLC_ENOMEM;
    }

    for (uint32_t i = 0; i < i_nal; i++) {
        memcpy(p_extra, nal[i].payload, nal[i].sizeBytes);
        p_extra += nal[i].sizeBytes;
    }

    p_sys->dts = 0;
    p_sys->initial_date = 0;
    p_sys->i_initial_delay = 0;

    p_enc->pf_encode_video = Encode;
    p_enc->pf_encode_audio = NULL;

    return VLC_SUCCESS;
}

static void Close(vlc_object_t *p_this)
{
    encoder_t     *p_enc = (encoder_t *)p_this;
    encoder_sys_t *p_sys = p_enc->p_sys;

    x265_encoder_close(p_sys->h);

    free(p_sys);
}
