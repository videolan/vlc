/*****************************************************************************
 * stl.c: EBU STL decoder
 *****************************************************************************
 * Copyright (C) 2010 Laurent Aimar
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
#include <vlc_charset.h>

#include "substext.h" /* required for font scaling / updater */

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open (vlc_object_t *);
static void Close(vlc_object_t *);

vlc_module_begin()
    set_description(N_("EBU STL subtitles decoder"))
    set_category(CAT_INPUT)
    set_subcategory(SUBCAT_INPUT_SCODEC)
    set_capability("spu decoder", 10)
    set_callbacks(Open, Close)
vlc_module_end()

/*****************************************************************************
 * Local definitions/prototypes
 *****************************************************************************/
#define GSI_BLOCK_SIZE 1024

#define STL_GROUPS_MAX 255

#define STL_TEXTFIELD_SIZE     112
#define STL_TTI_HEADER_SIZE    16
#define STL_TTI_SIZE           (STL_TTI_HEADER_SIZE + STL_TEXTFIELD_SIZE)

#define STL_TF_TELETEXT_FIRST     0x00
#define STL_TF_TELETEXT_LAST      0x1f
#define STL_TF_CHARCODE1_FIRST    0x20
#define STL_TF_CHARCODE1_LAST     0x7f
#define STL_TF_ITALICS_ON         0x80
#define STL_TF_ITALICS_OFF        0x81
#define STL_TF_UNDERLINE_ON       0x82
#define STL_TF_UNDERLINE_OFF      0x83
#define STL_TF_BOXING_ON          0x84
#define STL_TF_BOXING_OFF         0x85
#define STL_TF_LINEBREAK          0x8a
#define STL_TF_END_FILL           0x8f
#define STL_TF_CHARCODE2_FIRST    0xa1

typedef enum {
    CCT_ISO_6937_2 = 0x3030, CCT_BEGIN = CCT_ISO_6937_2,
    CCT_ISO_8859_5 = 0x3031,
    CCT_ISO_8859_6 = 0x3032,
    CCT_ISO_8859_7 = 0x3033,
    CCT_ISO_8859_8 = 0x3034, CCT_END = CCT_ISO_8859_8
} cct_number_value_t;

typedef struct
{
    uint8_t i_accumulating;
    uint8_t i_justify;
    vlc_tick_t i_start;
    vlc_tick_t i_end;
    text_style_t *p_style;
    text_segment_t *p_segment;
    text_segment_t **pp_segment_last;
} stl_sg_t;

typedef struct {
    cct_number_value_t value;
    const char *str;
} cct_number_t;

typedef struct
{
    stl_sg_t groups[STL_GROUPS_MAX + 1];
    cct_number_value_t cct;
    uint8_t i_fps;
} decoder_sys_t;

static cct_number_t cct_nums[] = { {CCT_ISO_6937_2, "ISO_6937-2"},
                                   {CCT_ISO_8859_5, "ISO_8859-5"},
                                   {CCT_ISO_8859_6, "ISO_8859-6"},
                                   {CCT_ISO_8859_7, "ISO_8859-7"},
                                   {CCT_ISO_8859_8, "ISO_8859-8"} };

static text_style_t * CreateGroupStyle()
{
    text_style_t *p_style = text_style_Create(STYLE_NO_DEFAULTS);
    if(p_style)
    {
        p_style->i_features = STYLE_HAS_FLAGS|STYLE_HAS_BACKGROUND_ALPHA|STYLE_HAS_BACKGROUND_COLOR;
        /* Teletext needs default background to black */
        p_style->i_background_alpha = STYLE_ALPHA_OPAQUE;
        p_style->i_background_color = 0x000000;
        p_style->i_font_size = 0;
        p_style->f_font_relsize = STYLE_DEFAULT_REL_FONT_SIZE;
    }
    return p_style;
}

static void TextBufferFlush(stl_sg_t *p_group, uint8_t *p_buf, uint8_t *pi_buf,
                            const char *psz_charset)
{
    if(*pi_buf == 0)
        return;

    char *psz_utf8 = FromCharset(psz_charset, p_buf, *pi_buf);
    if(psz_utf8)
    {
        *p_group->pp_segment_last = text_segment_New(psz_utf8);
        if(*p_group->pp_segment_last)
        {
            if(p_group->p_style)
                (*p_group->pp_segment_last)->style = text_style_Duplicate(p_group->p_style);
            p_group->pp_segment_last = &((*p_group->pp_segment_last)->p_next);
        }
        free(psz_utf8);
    }

    *pi_buf = 0;
}

static void GroupParseTeletext(stl_sg_t *p_group, uint8_t code)
{
    if(p_group->p_style == NULL &&
      !(p_group->p_style = CreateGroupStyle()))
        return;

    /* See ETS 300 706 Table 26 as EBU 3264 does only name values
       and does not explain at all */

    static const uint32_t colors[] =
    {
        0x000000,
        0xFF0000,
        0x00FF00,
        0xFFFF00,
        0x0000FF,
        0xFF00FF,
        0x00FFFF,
        0xFFFFFF,
    };

    /* Teletext data received, so we need to enable background */
    p_group->p_style->i_style_flags |= STYLE_BACKGROUND;

    switch(code)
    {
        case 0x0c:
            p_group->p_style->f_font_relsize = STYLE_DEFAULT_REL_FONT_SIZE;
            p_group->p_style->i_style_flags &= ~(STYLE_DOUBLEWIDTH|STYLE_HALFWIDTH);
            break;

        case 0x0d: /* double height */
            p_group->p_style->f_font_relsize = STYLE_DEFAULT_REL_FONT_SIZE * 2;
            p_group->p_style->i_style_flags &= ~STYLE_DOUBLEWIDTH;
            p_group->p_style->i_style_flags |= STYLE_HALFWIDTH;
            break;

        case 0x0e: /* double width */
            p_group->p_style->f_font_relsize = STYLE_DEFAULT_REL_FONT_SIZE;
            p_group->p_style->i_style_flags &= ~STYLE_HALFWIDTH;
            p_group->p_style->i_style_flags |= STYLE_DOUBLEWIDTH;
            break;

        case 0x0f: /* double size */
            p_group->p_style->f_font_relsize = STYLE_DEFAULT_REL_FONT_SIZE * 2;
            p_group->p_style->i_style_flags &= ~(STYLE_DOUBLEWIDTH|STYLE_HALFWIDTH);
            break;

        case 0x1d:
            p_group->p_style->i_background_color = p_group->p_style->i_font_color;
            p_group->p_style->i_features &= ~STYLE_HAS_FONT_COLOR;
            break;

        case 0x1c:
            p_group->p_style->i_background_color = colors[0];
            break;

        default:
            if(code < 8)
            {
                p_group->p_style->i_font_color = colors[code];
                p_group->p_style->i_features |= STYLE_HAS_FONT_COLOR;
            }

            /* Need to handle Mosaic ? Really ? */
            break;
    }

}

static void GroupApplyStyle(stl_sg_t *p_group, uint8_t code)
{
    if(p_group->p_style == NULL &&
      !(p_group->p_style = CreateGroupStyle()))
        return;

    switch(code)
    {
        case STL_TF_ITALICS_ON:
            p_group->p_style->i_style_flags |= STYLE_ITALIC;
            break;
        case STL_TF_ITALICS_OFF:
            p_group->p_style->i_style_flags &= ~STYLE_ITALIC;
            break;
        case STL_TF_UNDERLINE_ON:
            p_group->p_style->i_style_flags |= STYLE_UNDERLINE;
            break;
        case STL_TF_UNDERLINE_OFF:
            p_group->p_style->i_style_flags &= ~STYLE_UNDERLINE;
            break;
        case STL_TF_BOXING_ON:
        case STL_TF_BOXING_OFF:
        default:
            break;
    }
}

static vlc_tick_t ParseTimeCode(const uint8_t *data, double fps)
{
    return vlc_tick_from_sec( data[0] * 3600 +
                         data[1] *   60 +
                         data[2] *    1 +
                         data[3] /  fps);
}

static void ClearTeletextStyles(stl_sg_t *p_group)
{
    if(p_group->p_style)
    {
        p_group->p_style->i_features &= ~STYLE_HAS_FONT_COLOR;
        p_group->p_style->i_background_color = 0x000000;
        p_group->p_style->f_font_relsize = STYLE_DEFAULT_REL_FONT_SIZE;
        p_group->p_style->i_style_flags &= ~(STYLE_DOUBLEWIDTH|STYLE_HALFWIDTH);
    }
}

/* Returns true if group is we need to output group */
static bool ParseTTI(stl_sg_t *p_group, const uint8_t *p_data, const char *psz_charset, double fps)
{
    uint8_t p_buffer[STL_TEXTFIELD_SIZE];
    uint8_t i_buffer = 0;

    /* Header */
    uint8_t ebn = p_data[3];
    if(ebn > 0xef && ebn != 0xff)
        return false;

    if(p_data[15] != 0x00) /* comment flag */
        return false;

    if(p_data[14] > 0x00)
        p_group->i_justify = p_data[14];

    /* Accumulating started or continuing.
     * We must not flush current segments on output and continue on next block */
    p_group->i_accumulating = (p_data[4] == 0x01 || p_data[4] == 0x02);

    p_group->i_start = ParseTimeCode( &p_data[5], fps );
    p_group->i_end = ParseTimeCode( &p_data[9], fps );

    /* Text Field */
    for (size_t i = STL_TTI_HEADER_SIZE; i < STL_TTI_SIZE; i++)
    {
        const uint8_t code = p_data[i];
        switch(code)
        {
            case STL_TF_LINEBREAK:
                p_buffer[i_buffer++] = '\n';
                TextBufferFlush(p_group, p_buffer, &i_buffer, psz_charset);
                /* Clear teletext styles on each new row */
                ClearTeletextStyles(p_group);
                break;

            case STL_TF_END_FILL:
                TextBufferFlush(p_group, p_buffer, &i_buffer, psz_charset);
                ClearTeletextStyles(p_group);
                return true;

            default:
                if(code <= STL_TF_TELETEXT_LAST)
                {
                    TextBufferFlush(p_group, p_buffer, &i_buffer, psz_charset);
                    GroupParseTeletext(p_group, code);
                }
                else if((code >= STL_TF_CHARCODE1_FIRST && code <= STL_TF_CHARCODE1_LAST) ||
                    code >= STL_TF_CHARCODE2_FIRST)
                {
                    p_buffer[i_buffer++] = code;
                }
                else if(code >= STL_TF_ITALICS_ON && code <= STL_TF_BOXING_OFF)
                {
                    TextBufferFlush(p_group, p_buffer, &i_buffer, psz_charset);
                    GroupApplyStyle(p_group, code);
                }
                break;
        }
    }

    TextBufferFlush(p_group, p_buffer, &i_buffer, psz_charset);

    return false;
}

static void FillSubpictureUpdater(stl_sg_t *p_group, subtext_updater_sys_t *p_spu_sys)
{
    if(p_group->i_accumulating)
    {
        p_spu_sys->region.p_segments = text_segment_Copy(p_group->p_segment);
    }
    else
    {
        p_spu_sys->region.p_segments = p_group->p_segment;
        p_group->p_segment = NULL;
        p_group->pp_segment_last = &p_group->p_segment;
    }

    p_spu_sys->region.align = SUBPICTURE_ALIGN_BOTTOM;
    if(p_group->i_justify == 0x01)
        p_spu_sys->region.inner_align = SUBPICTURE_ALIGN_LEFT;
    else if(p_group->i_justify == 0x03)
        p_spu_sys->region.inner_align = SUBPICTURE_ALIGN_RIGHT;
}

static void ResetGroups(decoder_sys_t *p_sys)
{
    for(size_t i=0; i<=STL_GROUPS_MAX; i++)
    {
        stl_sg_t *p_group = &p_sys->groups[i];
        if(p_group->p_segment)
        {
            text_segment_ChainDelete(p_group->p_segment);
            p_group->p_segment = NULL;
            p_group->pp_segment_last = &p_group->p_segment;
        }

        if(p_group->p_style)
        {
            text_style_Delete(p_group->p_style);
            p_group->p_style = NULL;
        }

        p_group->i_accumulating = false;
        p_group->i_end = VLC_TICK_INVALID;
        p_group->i_start = VLC_TICK_INVALID;
        p_group->i_justify = 0;
    }
}

static int Decode(decoder_t *p_dec, block_t *p_block)
{
    if (p_block == NULL) /* No Drain */
        return VLCDEC_SUCCESS;

    decoder_sys_t *p_sys = p_dec->p_sys;

    if(p_block->i_buffer < STL_TTI_SIZE)
        p_block->i_flags |= BLOCK_FLAG_CORRUPTED;

    if(p_block->i_flags & (BLOCK_FLAG_CORRUPTED|BLOCK_FLAG_DISCONTINUITY))
    {
        ResetGroups(p_dec->p_sys);

        if(p_block->i_flags & BLOCK_FLAG_CORRUPTED)
        {
            block_Release(p_block);
            return VLCDEC_SUCCESS;
        }
    }

    const char *psz_charset = cct_nums[p_sys->cct - CCT_BEGIN].str;
    for (size_t i = 0; i < p_block->i_buffer / STL_TTI_SIZE; i++)
    {
        stl_sg_t *p_group = &p_sys->groups[p_block->p_buffer[0]];
        if(ParseTTI(p_group, &p_block->p_buffer[i * STL_TTI_SIZE], psz_charset, p_sys->i_fps) &&
           p_group->p_segment != NULL )
        {
            /* output */
            subpicture_t *p_sub = decoder_NewSubpictureText(p_dec);
            if( p_sub )
            {
                FillSubpictureUpdater(p_group, p_sub->updater.p_sys );

                p_sub->b_absolute = false;

                if(p_group->i_end != VLC_TICK_INVALID && p_group->i_start >= p_block->i_dts)
                {
                    p_sub->i_start = VLC_TICK_0 + p_group->i_start;
                    p_sub->i_stop =  VLC_TICK_0 + p_group->i_end;
                }
                else
                {
                    p_sub->i_start    = p_block->i_pts;
                    p_sub->i_stop     = p_block->i_pts + p_block->i_length;
                    p_sub->b_ephemer  = (p_block->i_length == VLC_TICK_INVALID);
                }
                decoder_QueueSub(p_dec, p_sub);
            }
        }
    }

    ResetGroups(p_sys);

    block_Release(p_block);
    return VLCDEC_SUCCESS;
}

static int ParseGSI(decoder_t *dec, decoder_sys_t *p_sys)
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

    char dfc_fps_str[] = { header[6], header[7], '\0' };
    int fps = strtol(dfc_fps_str, NULL, 10);
    if (1 > fps || 60 < fps) {
        msg_Warn(dec, "EBU header contains unsupported DFC fps ('%s'); falling back to 25\n", dfc_fps_str);
        fps = 25;
    }

    int cct = (header[12] << 8) | header[13];
    if (CCT_BEGIN > cct || CCT_END < cct) {
        msg_Err(dec, "EBU header contains illegal CCT (0x%x)\n", cct);
        return VLC_EGENERIC;
    }

    msg_Dbg(dec, "DFC fps=%d, CCT=0x%x", fps, cct);
    p_sys->i_fps = fps;
    p_sys->cct = cct;

    return VLC_SUCCESS;
}

static int Open(vlc_object_t *object)
{
    decoder_t *dec = (decoder_t*)object;

    if (dec->fmt_in.i_codec != VLC_CODEC_EBU_STL)
        return VLC_EGENERIC;

    decoder_sys_t *sys = calloc(1, sizeof(*sys));
    if (!sys)
        return VLC_ENOMEM;

    int rc = ParseGSI(dec, sys);
    if (VLC_SUCCESS != rc)
        return rc;

    for(size_t i=0; i<=STL_GROUPS_MAX; i++)
        sys->groups[i].pp_segment_last = &sys->groups[i].p_segment;

    dec->p_sys = sys;
    dec->pf_decode = Decode;
    dec->fmt_out.i_codec = 0;
    return VLC_SUCCESS;
}

static void Close(vlc_object_t *object)
{
    decoder_t *dec = (decoder_t*)object;
    decoder_sys_t *p_sys = dec->p_sys;

    ResetGroups(p_sys);
    free(p_sys);
}
