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

struct decoder_owner
{
    decoder_t dec;
    decoder_t *packetizer;
};

static inline struct decoder_owner *dec_get_owner(decoder_t *dec)
{
    return container_of(dec, struct decoder_owner, dec);
}

static subpicture_t *spu_new_buffer_decoder(decoder_t *dec,
                                            const subpicture_updater_t * p_subpic)
{
    (void) dec;
    return subpicture_New (p_subpic);
}

static vlc_decoder_device * get_no_device( decoder_t *dec )
{
    (void) dec;
    // no decoder device for this test
    return NULL;
}

static void queue_video(decoder_t *dec, picture_t *pic)
{
    (void) dec;
    picture_Release(pic);
}

static void queue_audio(decoder_t *dec, block_t *p_block)
{
    (void) dec;
    block_Release(p_block);
}
static void queue_cc(decoder_t *dec, block_t *p_block, const decoder_cc_desc_t *desc)
{
    (void) dec; (void) desc;
    block_Release(p_block);
}
static void queue_sub(decoder_t *dec, subpicture_t *p_subpic)
{
    (void) dec;
    subpicture_Delete(p_subpic);
}

static int decoder_load(decoder_t *decoder, bool is_packetizer,
                         const es_format_t *restrict fmt)
{
    decoder_Init( decoder, fmt );

    decoder->b_frame_drop_allowed = true;

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
        decoder_Clean( decoder );
        return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

void test_decoder_destroy(decoder_t *decoder)
{
    struct decoder_owner *owner = dec_get_owner(decoder);

    decoder_Destroy(owner->packetizer);
    decoder_Destroy(decoder);
}

decoder_t *test_decoder_create(vlc_object_t *parent, const es_format_t *fmt)
{
    assert(parent && fmt);
    decoder_t *packetizer = NULL;
    decoder_t *decoder = NULL;

    packetizer = vlc_object_create(parent, sizeof(*packetizer));
    struct decoder_owner *owner = vlc_object_create(parent, sizeof(*owner));

    if (packetizer == NULL || owner == NULL)
    {
        if (packetizer)
            vlc_object_delete(packetizer);
        return NULL;
    }
    decoder = &owner->dec;
    owner->packetizer = packetizer;

    static const struct decoder_owner_callbacks dec_video_cbs =
    {
        .video = {
            .get_device = get_no_device,
            .queue = queue_video,
            .queue_cc = queue_cc,
        },
    };
    static const struct decoder_owner_callbacks dec_audio_cbs =
    {
        .audio = {
            .queue = queue_audio,
        },
    };
    static const struct decoder_owner_callbacks dec_spu_cbs =
    {
        .spu = {
            .buffer_new = spu_new_buffer_decoder,
            .queue = queue_sub,
        },
    };

    switch (fmt->i_cat)
    {
        case VIDEO_ES:
            decoder->cbs = &dec_video_cbs;
            break;
        case AUDIO_ES:
            decoder->cbs = &dec_audio_cbs;
            break;
        case SPU_ES:
            decoder->cbs = &dec_spu_cbs;
            break;
        default:
            vlc_object_delete(packetizer);
            vlc_object_delete(decoder);
            return NULL;
    }

    if (decoder_load(packetizer, true, fmt) != VLC_SUCCESS)
    {
        vlc_object_delete(packetizer);
        vlc_object_delete(decoder);
        return NULL;
    }

    if (decoder_load(decoder, false, &packetizer->fmt_out) != VLC_SUCCESS)
    {
        decoder_Destroy(packetizer);
        vlc_object_delete(decoder);
        return NULL;
    }

    return decoder;
}

int test_decoder_process(decoder_t *decoder, block_t *p_block)
{
    struct decoder_owner *owner = dec_get_owner(decoder);
    decoder_t *packetizer = owner->packetizer;

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
            decoder_Clean(decoder);
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
