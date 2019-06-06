/*****************************************************************************
 * subtitle.c: subtitle decoder using libavcodec library
 *****************************************************************************
 * Copyright (C) 2009 Laurent Aimar
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

typedef struct
{
    AVCodecContext *p_context;
    const AVCodec  *p_codec;
    bool b_need_ephemer; /* Does the format need the ephemer flag (no end time set) */
} decoder_sys_t;

static subpicture_t *ConvertSubtitle(decoder_t *, AVSubtitle *, vlc_tick_t pts,
                                     AVCodecContext *avctx);
static int  DecodeSubtitle(decoder_t *, block_t *);
static void Flush(decoder_t *);

/**
 * Initialize subtitle decoder
 */
int InitSubtitleDec(vlc_object_t *obj)
{
    decoder_t *dec = (decoder_t *)obj;
    const AVCodec *codec;
    AVCodecContext *context = ffmpeg_AllocContext(dec, &codec);
    if (context == NULL)
        return VLC_EGENERIC;

    decoder_sys_t *sys;

    /* */
    switch (codec->id) {
    case AV_CODEC_ID_HDMV_PGS_SUBTITLE:
    case AV_CODEC_ID_XSUB:
    case AV_CODEC_ID_DVB_SUBTITLE:
        break;
    default:
        msg_Warn(dec, "refusing to decode non validated subtitle codec");
        avcodec_free_context(&context);
        return VLC_EGENERIC;
    }

    /* */
    dec->p_sys = sys = malloc(sizeof(*sys));
    if (unlikely(sys == NULL))
    {
        avcodec_free_context(&context);
        return VLC_ENOMEM;
    }

    sys->p_context = context;
    sys->p_codec = codec;
    sys->b_need_ephemer = codec->id == AV_CODEC_ID_HDMV_PGS_SUBTITLE;

    /* */
    context->extradata_size = 0;
    context->extradata = NULL;

    if( codec->id == AV_CODEC_ID_DVB_SUBTITLE )
    {
        if( dec->fmt_in.i_extra > 3 )
        {
            context->extradata = malloc( dec->fmt_in.i_extra );
            if( context->extradata )
            {
                context->extradata_size = dec->fmt_in.i_extra;
                memcpy( context->extradata, dec->fmt_in.p_extra, dec->fmt_in.i_extra );
            }
        }
        else
        {
            context->extradata = malloc( 4 );
            if( context->extradata )
            {
                context->extradata_size = 4;
                SetWBE( &context->extradata[0], dec->fmt_in.subs.dvb.i_id & 0xFFFF );
                SetWBE( &context->extradata[2], dec->fmt_in.subs.dvb.i_id >> 16 );
            }
        }
    }

#if LIBAVFORMAT_VERSION_MICRO >= 100
    av_codec_set_pkt_timebase(context, AV_TIME_BASE_Q);
#endif

    /* */
    int ret;
    char *psz_opts = var_InheritString(dec, "avcodec-options");
    AVDictionary *options = NULL;
    if (psz_opts) {
        vlc_av_get_options(psz_opts, &options);
        free(psz_opts);
    }

    vlc_avcodec_lock();
    ret = avcodec_open2(context, codec, options ? &options : NULL);
    vlc_avcodec_unlock();

    AVDictionaryEntry *t = NULL;
    while ((t = av_dict_get(options, "", t, AV_DICT_IGNORE_SUFFIX))) {
        msg_Err(dec, "Unknown option \"%s\"", t->key);
    }
    av_dict_free(&options);

    if (ret < 0) {
        msg_Err(dec, "cannot open codec (%s)", codec->name);
        free(sys);
        avcodec_free_context(&context);
        return VLC_EGENERIC;
    }

    /* */
    msg_Dbg(dec, "libavcodec codec (%s) started", codec->name);
    dec->pf_decode = DecodeSubtitle;
    dec->pf_flush  = Flush;

    return VLC_SUCCESS;
}

void EndSubtitleDec(vlc_object_t *obj)
{
    decoder_t *dec = (decoder_t *)obj;
    decoder_sys_t *sys = dec->p_sys;
    AVCodecContext *ctx = sys->p_context;

    avcodec_free_context(&ctx);
    free(sys);
}

/**
 * Flush
 */
static void Flush(decoder_t *dec)
{
    decoder_sys_t *sys = dec->p_sys;

    avcodec_flush_buffers(sys->p_context);
}

/**
 * Decode one subtitle
 */
static subpicture_t *DecodeBlock(decoder_t *dec, block_t **block_ptr)
{
    decoder_sys_t *sys = dec->p_sys;

    if (!block_ptr || !*block_ptr)
        return NULL;

    block_t *block = *block_ptr;

    if (block->i_flags & (BLOCK_FLAG_DISCONTINUITY | BLOCK_FLAG_CORRUPTED)) {
        if (block->i_flags & BLOCK_FLAG_CORRUPTED) {
            Flush(dec);
            block_Release(block);
            return NULL;
        }
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

    if( sys->p_codec->id == AV_CODEC_ID_DVB_SUBTITLE && block->i_buffer > 3 )
    {
        block->p_buffer += 2; /* drop data identifier / stream id */
        block->i_buffer -= 3; /* drop 0x3F/FF */
    }

    /* */
    AVSubtitle subtitle;
    memset(&subtitle, 0, sizeof(subtitle));

    AVPacket pkt;
    av_init_packet(&pkt);
    pkt.data = block->p_buffer;
    pkt.size = block->i_buffer;
    pkt.pts  = TO_AV_TS(block->i_pts);

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
                              FROM_AV_TS(subtitle.pts),
                              sys->p_context);

    /* */
    if (!spu)
        block_Release(block);
    return spu;
}

static int DecodeSubtitle(decoder_t *dec, block_t *block)
{
    block_t **block_ptr = block ? &block : NULL;
    subpicture_t *spu;
    while ((spu = DecodeBlock(dec, block_ptr)) != NULL)
        decoder_QueueSub(dec, spu);
    return VLCDEC_SUCCESS;
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
    fmt.i_chroma         = VLC_CODEC_RGBA;
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
            const uint8_t index = ffregion->data[0][y * ffregion->w+x];
            assert(index < ffregion->nb_colors);

            uint32_t color;
            memcpy(&color, &ffregion->data[1][4*index], 4);

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
static subpicture_t *ConvertSubtitle(decoder_t *dec, AVSubtitle *ffsub, vlc_tick_t pts,
                                     AVCodecContext *avctx)
{
    subpicture_t *spu = decoder_NewSubpicture(dec, NULL);
    if (!spu)
        return NULL;

    decoder_sys_t *p_sys = dec->p_sys;

    //msg_Err(dec, "%lld %d %d",
    //        pts, ffsub->start_display_time, ffsub->end_display_time);
    spu->i_start    = pts + VLC_TICK_FROM_MS(ffsub->start_display_time);
    spu->i_stop     = pts + VLC_TICK_FROM_MS(ffsub->end_display_time);
    spu->b_absolute = true; /* We have offset and size for subtitle */
    spu->b_ephemer  = p_sys->b_need_ephemer;
                    /* We only show subtitle for i_stop time only */

    if (avctx->coded_width != 0 && avctx->coded_height != 0) {
        spu->i_original_picture_width = avctx->coded_width;
        spu->i_original_picture_height = avctx->coded_height;
    } else {
        spu->i_original_picture_width =
            dec->fmt_in.subs.spu.i_original_frame_width;
        spu->i_original_picture_height =
            dec->fmt_in.subs.spu.i_original_frame_height;
    }

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
    }
    avsubtitle_free(ffsub);

    return spu;
}

