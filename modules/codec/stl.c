/*****************************************************************************
 * stl.c: EBU STL decoder
 *****************************************************************************
 * Copyright (C) 2010 Laurent Aimar
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
#include <vlc_plugin.h>
#include <vlc_codec.h>
#include <vlc_memory.h>
#include <vlc_charset.h>

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
#define GSI_BLOCK_SIZE 1024

typedef enum {
    CCT_ISO_6937_2 = 0x3030, CCT_BEGIN = CCT_ISO_6937_2,
    CCT_ISO_8859_5 = 0x3031,
    CCT_ISO_8859_6 = 0x3032,
    CCT_ISO_8859_7 = 0x3033,
    CCT_ISO_8859_8 = 0x3034, CCT_END = CCT_ISO_8859_8
} cct_number_value_t;

typedef struct {
    cct_number_value_t value;
    const char *str;
} cct_number_t;

struct decoder_sys_t {
    cct_number_value_t cct;
};

static cct_number_t cct_nums[] = { {CCT_ISO_6937_2, "ISO_6937-2"},
                                   {CCT_ISO_8859_5, "ISO_8859-5"},
                                   {CCT_ISO_8859_6, "ISO_8859-6"},
                                   {CCT_ISO_8859_7, "ISO_8859-7"},
                                   {CCT_ISO_8859_8, "ISO_8859-8"} };


static char *ParseText(uint8_t *data, int size, const char *charset)
{
    char *text = strdup("");
    int  text_size = 0;

    for (int i = 0; i < size; i++) {
        uint8_t code = data[i];

        if (code == 0x8f)
            break;

        char tmp[16] = "";
        char *t = tmp;
        if ((code >= 0x20 && code <= 0x7e) ||
            (code >= 0xa0) )
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
        else if (code == 0x8a)
            snprintf(tmp, sizeof(tmp), "\n");
#endif
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
    return FromCharset(charset, text, text_size);
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
    for (unsigned i = 0; i < b->i_buffer / 128; i++)
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
        sub->p_region->psz_text = ParseText(payload,
                                            payload_size,
                                            cct_nums[dec->p_sys->cct - CCT_BEGIN].str);
        sub->p_region->psz_html = NULL;
    }

    free(payload);

exit:
    block_Release(b);
    return sub;
}

static int ExtractCCT(const decoder_t *dec, cct_number_value_t *cct_number)
{
    uint8_t *header = dec->fmt_in.p_extra;
    if (!header) {
        msg_Err(dec, "NULL EBU header (GSI block)\n");
        return VLC_EGENERIC;
    }

    if (GSI_BLOCK_SIZE != dec->fmt_in.i_extra) {
        msg_Err(dec, "EBU header is not in expected size (%d)\n", dec->fmt_in.i_extra);
        return VLC_EGENERIC;
    }

    int cct = (header[12] << 8) | header[13];
    if (CCT_BEGIN > cct || CCT_END < cct) {
        msg_Err(dec, "EBU header contains illegal CCT (0x%x)\n", cct);
        return VLC_EGENERIC;
    }

    *cct_number = cct;

    return VLC_SUCCESS;
}

static int Open(vlc_object_t *object)
{
    decoder_t *dec = (decoder_t*)object;

    if (dec->fmt_in.i_codec != VLC_CODEC_EBU_STL)
        return VLC_EGENERIC;

    cct_number_value_t cct;
    int rc = ExtractCCT(dec, &cct);
    if (VLC_SUCCESS != rc)
        return rc;

    msg_Dbg(dec, "CCT=0x%x", cct);

    decoder_sys_t *sys = malloc(sizeof(*sys));
    if (!sys)
        return VLC_ENOMEM;

    sys->cct = cct;

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
