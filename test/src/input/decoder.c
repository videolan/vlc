/*****************************************************************************
 * decoder.c
 *****************************************************************************
 * Copyright (C) 2017 VLC authors and VideoLAN
 *
 * Authors: Shaleen Jain <shaleen.jain95@gmail.com>
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
#include <vlc_modules.h>
#include <vlc_codec.h>
#include <vlc_stream.h>
#include <vlc_access.h>
#include <vlc_meta.h>
#include <vlc_block.h>
#include <vlc_url.h>

#include <vlc/libvlc.h>
#include "../../lib/libvlc_internal.h"

#include "common.h"
#include "decoder.h"

static picture_t *video_new_buffer_decoder(decoder_t *dec)
{
    return picture_NewFromFormat(&dec->fmt_out.video);
}

static subpicture_t *spu_new_buffer_decoder(decoder_t *dec,
                                            const subpicture_updater_t * p_subpic)
{
    (void) dec;
    return subpicture_New (p_subpic);
}

static int video_update_format_decoder(decoder_t *dec)
{
    (void) dec;
    return 0;
}
static int queue_video(decoder_t *dec, picture_t *pic)
{
    (void) dec;
    picture_Release(pic);
    return 0;
}

static int queue_audio(decoder_t *dec, block_t *p_block)
{
    (void) dec;
    block_Release(p_block);
    return 0;
}
static int queue_cc(decoder_t *dec, block_t *p_block, const decoder_cc_desc_t *desc)
{
    (void) dec; (void) desc;
    block_Release(p_block);
    return 0;
}
static int queue_sub(decoder_t *dec, subpicture_t *p_subpic)
{
    (void) dec;
    subpicture_Delete(p_subpic);
    return 0;
}

static int decoder_load(decoder_t *decoder, bool is_packetizer,
                         const es_format_t *restrict fmt)
{
    decoder->b_frame_drop_allowed = true;
    decoder->i_extra_picture_buffers = 0;

    decoder->pf_decode = NULL;
    decoder->pf_get_cc = NULL;
    decoder->pf_packetize = NULL;
    decoder->pf_flush = NULL;

    es_format_Copy(&decoder->fmt_in, fmt);
    es_format_Init(&decoder->fmt_out, fmt->i_cat, 0);

    if (!is_packetizer)
    {
        static const char caps[ES_CATEGORY_COUNT][16] = {
            [VIDEO_ES] = "video decoder",
            [AUDIO_ES] = "audio decoder",
            [SPU_ES] = "spu decoder",
        };
        decoder->p_module =
            module_need(decoder, caps[decoder->fmt_in.i_cat], NULL, false);
    }
    else
        decoder->p_module = module_need(decoder, "packetizer", NULL, false);

    if (!decoder->p_module)
    {
        es_format_Clean(&decoder->fmt_in);
        return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

static void decoder_unload(decoder_t *decoder)
{
    if (decoder->p_module != NULL)
    {
        module_unneed(decoder, decoder->p_module);
        decoder->p_module = NULL;
        es_format_Clean(&decoder->fmt_out);
    }
    es_format_Clean(&decoder->fmt_in);
    if (decoder->p_description)
    {
        vlc_meta_Delete(decoder->p_description);
        decoder->p_description = NULL;
    }
}

void test_decoder_destroy(decoder_t *decoder)
{
    decoder_t *packetizer = (void *) decoder->p_owner;

    decoder_unload(packetizer);
    decoder_unload(decoder);
    vlc_object_release(packetizer);
    vlc_object_release(decoder);
}

decoder_t *test_decoder_create(vlc_object_t *parent, const es_format_t *fmt)
{
    assert(parent && fmt);
    decoder_t *packetizer = NULL;
    decoder_t *decoder = NULL;

    packetizer = vlc_object_create(parent, sizeof(*packetizer));
    decoder = vlc_object_create(parent, sizeof(*decoder));

    if (packetizer == NULL || decoder == NULL)
    {
        if (packetizer)
            vlc_object_release(packetizer);
        return NULL;
    }

    decoder->pf_vout_format_update = video_update_format_decoder;
    decoder->pf_vout_buffer_new = video_new_buffer_decoder;
    decoder->pf_spu_buffer_new = spu_new_buffer_decoder;
    decoder->pf_queue_video = queue_video;
    decoder->pf_queue_audio = queue_audio;
    decoder->pf_queue_cc = queue_cc;
    decoder->pf_queue_sub = queue_sub;
    decoder->p_owner = (void *)packetizer;

    if (decoder_load(packetizer, true, fmt) != VLC_SUCCESS)
        goto end;

    if (decoder_load(decoder, false, &packetizer->fmt_out) != VLC_SUCCESS)
        goto end;

    return decoder;

end:
    test_decoder_destroy(decoder);
    return NULL;
}

int test_decoder_process(decoder_t *decoder, block_t *p_block)
{
    decoder_t *packetizer = (void *) decoder->p_owner;

    /* This case can happen if a decoder reload failed */
    if (decoder->p_module == NULL)
    {
        if (p_block != NULL)
            block_Release(p_block);
        return VLC_EGENERIC;
    }

    block_t **pp_block = p_block ? &p_block : NULL;
    block_t *p_packetized_block;
    while ((p_packetized_block =
                packetizer->pf_packetize(packetizer, pp_block)))
    {

        if (!es_format_IsSimilar(&decoder->fmt_in, &packetizer->fmt_out))
        {
            debug("restarting module due to input format change\n");

            /* Drain the decoder module */
            decoder->pf_decode(decoder, NULL);

            /* Reload decoder */
            decoder_unload(decoder);
            if (decoder_load(decoder, false, &packetizer->fmt_out) != VLC_SUCCESS)
            {
                block_ChainRelease(p_packetized_block);
                return VLC_EGENERIC;
            }
        }

        if (packetizer->pf_get_cc)
        {
            decoder_cc_desc_t desc;
            block_t *p_cc = packetizer->pf_get_cc(packetizer, &desc);
            if (p_cc)
                block_Release(p_cc);
        }

        while (p_packetized_block != NULL)
        {

            block_t *p_next = p_packetized_block->p_next;
            p_packetized_block->p_next = NULL;

            int ret = decoder->pf_decode(decoder, p_packetized_block);

            if (ret == VLCDEC_ECRITICAL)
            {
                block_ChainRelease(p_next);
                return VLC_EGENERIC;
            }

            p_packetized_block = p_next;
        }
    }
    if (p_block == NULL) /* Drain */
        decoder->pf_decode(decoder, NULL);
    return VLC_SUCCESS;
}
