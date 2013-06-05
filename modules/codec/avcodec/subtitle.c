/*****************************************************************************
 * subtitle.c: subtitle decoder using libavcodec library
 *****************************************************************************
 * Copyright (C) 2009 Laurent Aimar
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <assert.h>

#include <vlc_common.h>
#include <vlc_codec.h>
#include <vlc_avcodec.h>

#include <libavcodec/avcodec.h>
#include <libavutil/mem.h>

#include "avcodec.h"

struct decoder_sys_t {
    AVCODEC_COMMON_MEMBERS
};

static subpicture_t *ConvertSubtitle(decoder_t *, AVSubtitle *, mtime_t pts);

/**
 * Initialize subtitle decoder
 */
int InitSubtitleDec(decoder_t *dec, AVCodecContext *context,
                    AVCodec *codec, int codec_id, const char *namecodec)
{
    decoder_sys_t *sys;

    /* */
    switch (codec_id) {
    case AV_CODEC_ID_HDMV_PGS_SUBTITLE:
    case AV_CODEC_ID_XSUB:
        break;
    default:
        msg_Warn(dec, "refusing to decode non validated subtitle codec");
        return VLC_EGENERIC;
    }

    /* */
    dec->p_sys = sys = malloc(sizeof(*sys));
    if (!sys)
        return VLC_ENOMEM;

    codec->type = AVMEDIA_TYPE_SUBTITLE;
    context->codec_type = AVMEDIA_TYPE_SUBTITLE;
    context->codec_id = codec_id;
    sys->p_context = context;
    sys->p_codec = codec;
    sys->i_codec_id = codec_id;
    sys->psz_namecodec = namecodec;
    sys->b_delayed_open = false;

    /* */
    context->extradata_size = 0;
    context->extradata = NULL;

    /* */
    int ret;
    char *psz_opts = var_InheritString(dec, "avcodec-options");
    AVDictionary *options = NULL;
    if (psz_opts && *psz_opts)
        options = vlc_av_get_options(psz_opts);
    free(psz_opts);

    vlc_avcodec_lock();
    ret = avcodec_open2(context, codec, options ? &options : NULL);
    vlc_avcodec_unlock();

    AVDictionaryEntry *t = NULL;
    while ((t = av_dict_get(options, "", t, AV_DICT_IGNORE_SUFFIX))) {
        msg_Err(dec, "Unknown option \"%s\"", t->key);
    }
    av_dict_free(&options);

    if (ret < 0) {
        msg_Err(dec, "cannot open codec (%s)", namecodec);
        free(context->extradata);
        free(sys);
        return VLC_EGENERIC;
    }

    /* */
    msg_Dbg(dec, "libavcodec codec (%s) started", namecodec);
    dec->fmt_out.i_cat = SPU_ES;

    return VLC_SUCCESS;
}

/**
 * Decode one subtitle
 */
subpicture_t *DecodeSubtitle(decoder_t *dec, block_t **block_ptr)
{
    decoder_sys_t *sys = dec->p_sys;

    if (!block_ptr || !*block_ptr)
        return NULL;

    block_t *block = *block_ptr;

    if (block->i_flags & (BLOCK_FLAG_DISCONTINUITY|BLOCK_FLAG_CORRUPTED)) {
        block_Release(block);
        avcodec_flush_buffers(sys->p_context);
        return NULL;
    }

    if (block->i_buffer <= 0) {
        block_Release(block);
        return NULL;
    }

    *block_ptr =
    block      = block_Realloc(block,
                               0,
                               block->i_buffer + FF_INPUT_BUFFER_PADDING_SIZE);
    if (!block)
        return NULL;
    block->i_buffer -= FF_INPUT_BUFFER_PADDING_SIZE;
    memset(&block->p_buffer[block->i_buffer], 0, FF_INPUT_BUFFER_PADDING_SIZE);

    /* */
    AVSubtitle subtitle;
    memset(&subtitle, 0, sizeof(subtitle));

    AVPacket pkt;
    av_init_packet(&pkt);
    pkt.data = block->p_buffer;
    pkt.size = block->i_buffer;

    int has_subtitle = 0;
    int used = avcodec_decode_subtitle2(sys->p_context,
                                        &subtitle, &has_subtitle, &pkt);

    if (used < 0) {
        msg_Warn(dec, "cannot decode one subtitle (%zu bytes)",
                 block->i_buffer);

        block_Release(block);
        return NULL;
    } else if ((size_t)used > block->i_buffer) {
        used = block->i_buffer;
    }

    block->i_buffer -= used;
    block->p_buffer += used;

    /* */
    subpicture_t *spu = NULL;
    if (has_subtitle)
        spu = ConvertSubtitle(dec, &subtitle,
                              block->i_pts > 0 ? block->i_pts : block->i_dts);

    /* */
    if (!spu)
        block_Release(block);
    return spu;
}

/**
 * Convert a RGBA libavcodec region to our format.
 */
static subpicture_region_t *ConvertRegionRGBA(AVSubtitleRect *ffregion)
{
    if (ffregion->w <= 0 || ffregion->h <= 0)
        return NULL;

    video_format_t fmt;
    memset(&fmt, 0, sizeof(fmt));
    fmt.i_chroma         = VLC_FOURCC('R','G','B','A');
    fmt.i_width          =
    fmt.i_visible_width  = ffregion->w;
    fmt.i_height         =
    fmt.i_visible_height = ffregion->h;
    fmt.i_x_offset       = 0;
    fmt.i_y_offset       = 0;

    subpicture_region_t *region = subpicture_region_New(&fmt);
    if (!region)
        return NULL;

    region->i_x = ffregion->x;
    region->i_y = ffregion->y;
    region->i_align = SUBPICTURE_ALIGN_TOP | SUBPICTURE_ALIGN_LEFT;

    const plane_t *p = &region->p_picture->p[0];
    for (int y = 0; y < ffregion->h; y++) {
        for (int x = 0; x < ffregion->w; x++) {
            /* I don't think don't have paletized RGB_A_ */
            const uint8_t index = ffregion->pict.data[0][y * ffregion->w+x];
            assert(index < ffregion->nb_colors);

            uint32_t color;
            memcpy(&color, &ffregion->pict.data[1][4*index], 4);

            uint8_t *p_rgba = &p->p_pixels[y * p->i_pitch + x * p->i_pixel_pitch];
            p_rgba[0] = (color >> 16) & 0xff;
            p_rgba[1] = (color >>  8) & 0xff;
            p_rgba[2] = (color >>  0) & 0xff;
            p_rgba[3] = (color >> 24) & 0xff;
        }
    }

    return region;
}

/**
 * Convert a libavcodec subtitle to our format.
 */
static subpicture_t *ConvertSubtitle(decoder_t *dec, AVSubtitle *ffsub, mtime_t pts)
{
    subpicture_t *spu = decoder_NewSubpicture(dec, NULL);
    if (!spu)
        return NULL;

    //msg_Err(dec, "%lld %d %d",
    //        pts, ffsub->start_display_time, ffsub->end_display_time);
    spu->i_start    = pts + ffsub->start_display_time * INT64_C(1000);
    spu->i_stop     = pts + ffsub->end_display_time * INT64_C(1000);
    spu->b_absolute = true; /* FIXME How to set it right ? */
    spu->b_ephemer  = true; /* FIXME How to set it right ? */
    spu->i_original_picture_width =
        dec->fmt_in.subs.spu.i_original_frame_width;
    spu->i_original_picture_height =
        dec->fmt_in.subs.spu.i_original_frame_height;

    subpicture_region_t **region_next = &spu->p_region;

    for (unsigned i = 0; i < ffsub->num_rects; i++) {
        AVSubtitleRect *rec = ffsub->rects[i];

        //msg_Err(dec, "SUBS RECT[%d]: %dx%d @%dx%d",
        //         i, rec->w, rec->h, rec->x, rec->y);

        subpicture_region_t *region = NULL;
        switch (ffsub->format) {
        case 0:
            region = ConvertRegionRGBA(rec);
            break;
        default:
            msg_Warn(dec, "unsupported subtitle type");
            region = NULL;
            break;
        }
        if (region) {
            *region_next = region;
            region_next = &region->p_next;
        }
        /* Free AVSubtitleRect */
        avpicture_free(&rec->pict);
        av_free(rec);
    }
    av_free(ffsub->rects);

    return spu;
}

