/*****************************************************************************
 * stl.c: EBU STL decoder
 *****************************************************************************
 * Copyright (C) 2010 Laurent Aimar
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
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
#include <assert.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_codec.h>
#include <vlc_memory.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open (vlc_object_t *);
static void Close(vlc_object_t *);

vlc_module_begin()
    set_description(N_("EBU STL subtitles decoder"))
    set_category(CAT_INPUT)
    set_subcategory(SUBCAT_INPUT_SCODEC)
    set_capability("decoder", 10)
    set_callbacks(Open, Close)
vlc_module_end()

/*****************************************************************************
 * Local definitions/prototypes
 *****************************************************************************/
struct decoder_sys_t {
    int dummy;
};

static char *ParseText(uint8_t *data, int size)
{
    char *text = strdup("");
    int  text_size = 0;

    for (int i = 0; i < size; i++) {
        uint8_t code = data[i];

        if (code == 0x8f)
            break;

        char tmp[16] = "";
        char *t = tmp;
        if (code >= 0x20 && code <= 0x7f)
            snprintf(tmp, sizeof(tmp), "%c", code);
#if 0
        else if (code == 0x80)
            snprintf(tmp, sizeof(tmp), "<i>");
        else if (code == 0x81)
            snprintf(tmp, sizeof(tmp), "</i>");
        else if (code == 0x82)
            snprintf(tmp, sizeof(tmp), "<u>");
        else if (code == 0x83)
            snprintf(tmp, sizeof(tmp), "</u>");
#endif
        else if (code == 0x8a)
            snprintf(tmp, sizeof(tmp), "\n");
        else {
            t = NULL;
        }

        if (!t)
            continue;
        size_t t_size = strlen(t);
        text = realloc_or_free(text, t_size + text_size + 1);
        if (!text)
            continue;
        memcpy(&text[text_size], t, t_size);
        text_size += t_size;
        text[text_size]   = '\0';
    }
    return text;
}

static subpicture_t *Decode(decoder_t *dec, block_t **block)
{
    if (block == NULL || *block == NULL)
        return NULL;

    subpicture_t *sub = NULL;

    block_t *b = *block; *block = NULL;
    if (b->i_flags & (BLOCK_FLAG_DISCONTINUITY|BLOCK_FLAG_CORRUPTED))
        goto exit;
    if (b->i_buffer < 128)
        goto exit;

    int     payload_size = (b->i_buffer / 128) * 112;
    uint8_t *payload = malloc(payload_size);
    if (!payload)
        goto exit;
    for (int i = 0; i < b->i_buffer / 128; i++)
        memcpy(&payload[112 * i], &b->p_buffer[128 * i + 16], 112);

    sub = decoder_NewSubpicture(dec, NULL);
    if (!sub) {
        free(payload);
        goto exit;
    }
    sub->i_start    = b->i_pts;
    sub->i_stop     = b->i_pts + b->i_length;
    sub->b_ephemer  = b->i_length == 0;
    sub->b_absolute = false;
    //sub->i_original_picture_width  = 0;
    //sub->i_original_picture_height = 0;

    video_format_t fmt;
    video_format_Init(&fmt, VLC_CODEC_TEXT);
    sub->p_region = subpicture_region_New(&fmt);
    video_format_Clean(&fmt);

    if (sub->p_region) {
        sub->p_region->psz_text = ParseText(payload, payload_size);
        sub->p_region->psz_html = NULL;
    }

    free(payload);

exit:
    block_Release(b);
    return sub;
}

static int Open(vlc_object_t *object)
{
    decoder_t *dec = (decoder_t*)object;

    if (dec->fmt_in.i_codec != VLC_CODEC_EBU_STL)
        return VLC_EGENERIC;

    decoder_sys_t *sys = malloc(sizeof(*sys));

    dec->p_sys = sys;
    dec->pf_decode_sub = Decode;
    dec->fmt_out.i_cat = SPU_ES;
    dec->fmt_out.i_codec = 0;
    return VLC_SUCCESS;
}

static void Close(vlc_object_t *object)
{
    decoder_t *dec = (decoder_t*)object;
    decoder_sys_t *sys = dec->p_sys;

    free(sys);
}

