/*****************************************************************************
 * textst.c: HDMV TextST subtitles decoder
 *****************************************************************************
 * Copyright (C) 2017 Videolan Authors
 *
 * Adapted from libluray textst_decode.c
 * Copyright (C) 2013  Petri Hintukainen <phintuka@users.sourceforge.net>
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

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_codec.h>

#include "substext.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open (vlc_object_t *);
static void Close(vlc_object_t *);

struct decoder_sys_t
{
    uint32_t palette[256];
};

vlc_module_begin()
    set_description(N_("HDMV TextST subtitles decoder"))
    set_category(CAT_INPUT)
    set_subcategory(SUBCAT_INPUT_SCODEC)
    set_capability("spu decoder", 10)
    set_callbacks(Open, Close)
vlc_module_end()

#define BD_TEXTST_DATA_STRING      1
#define BD_TEXTST_DATA_FONT_ID     2
#define BD_TEXTST_DATA_FONT_STYLE  3
#define BD_TEXTST_DATA_FONT_SIZE   4
#define BD_TEXTST_DATA_FONT_COLOR  5
#define BD_TEXTST_DATA_NEWLINE     0x0a
#define BD_TEXTST_DATA_RESET_STYLE 0x0b

static size_t textst_FillRegion(decoder_t *p_dec, const uint8_t *p_data, size_t i_data,
                                subpicture_updater_sys_region_t *p_region)
{
    VLC_UNUSED(p_dec);
    text_segment_t **pp_last = &p_region->p_segments;
    text_style_t *p_style = NULL;

     /* p_data[0] */
     /*   continous_present_flag b1 */
     /*   forced_on_flag b1 */
     /*   ? b6 */

     //uint8_t region_style_id_ref = p_data[1];
     uint16_t i_data_length = GetWBE(&p_data[2]);

     p_data += 4; i_data -= 4;
     if( i_data < i_data_length )
         return i_data;
     else
         i_data = i_data_length;

     while (i_data > 3)
     {
         /* parse header */
         uint8_t code = p_data[0];
         if (code != 0x1b) {
             p_data++; i_data--;
             continue;
         }

         uint8_t type   = p_data[1];
         uint8_t length = p_data[2];

         p_data += 3; i_data -= 3;

         if(length > i_data)
             break;

         switch (type)
         {
             case BD_TEXTST_DATA_STRING:
                {
                    char *psz = strndup((char *)p_data, length);
                    *pp_last = text_segment_New(psz);
                    free(psz);
                    if(p_style && *pp_last)
                        (*pp_last)->style = text_style_Duplicate(p_style);
                }
                break;
             case BD_TEXTST_DATA_FONT_ID:
                 //p_data[0] font_id;
                 break;
             case BD_TEXTST_DATA_FONT_STYLE:
                 if(i_data > 2 && (p_style || (p_style = text_style_Create( STYLE_NO_DEFAULTS ))))
                 {
                    if(p_data[0] & 0x01)
                        p_style->i_style_flags |= STYLE_BOLD;
                    if(p_data[0] & 0x02)
                        p_style->i_style_flags |= STYLE_ITALIC;
                    if(p_data[0] & 0x04)
                        p_style->i_style_flags |= STYLE_OUTLINE;
                    p_style->i_outline_color = p_dec->p_sys->palette[p_data[1]] & 0x00FFFFFF;
                    p_style->i_outline_alpha = p_dec->p_sys->palette[p_data[1]] >> 24;
                    p_style->i_features |= STYLE_HAS_FLAGS | STYLE_HAS_OUTLINE_ALPHA | STYLE_HAS_OUTLINE_COLOR;
                    //p_data[2] outline__thickness
                 }
                 break;
             case BD_TEXTST_DATA_FONT_SIZE:
                 /*if(i_data > 0)
                   p_style->f_font_relsize = STYLE_DEFAULT_REL_FONT_SIZE *
                                           (p_data[0] << 4) / STYLE_DEFAULT_FONT_SIZE;*/
                 break;
             case BD_TEXTST_DATA_FONT_COLOR:
                 if(i_data > 1 && (p_style || (p_style = text_style_Create( STYLE_NO_DEFAULTS ))))
                 {
                    p_style->i_font_color = p_dec->p_sys->palette[p_data[1]] & 0x00FFFFFF;
                    p_style->i_font_alpha = p_dec->p_sys->palette[p_data[1]] >> 24;
                    p_style->i_features |= STYLE_HAS_FONT_ALPHA | STYLE_HAS_FONT_COLOR;
                 }
                 break;
             case BD_TEXTST_DATA_NEWLINE:
                 *pp_last = text_segment_New("\n");
                 break;
             case BD_TEXTST_DATA_RESET_STYLE:
                 if(p_style)
                 {
                     text_style_Delete(p_style);
                     p_style = NULL;
                 }
                 break;
             default:
                 break;
         }

         if(*pp_last)
             pp_last = &(*pp_last)->p_next;

         p_data += length; i_data -= length;
     }

     if(p_style)
        text_style_Delete(p_style);

     return i_data_length;
}

static size_t textst_Decode_palette(decoder_t *p_dec, const uint8_t *p_data, size_t i_data)
{
    if(i_data < 2)
        return i_data;
    uint16_t i_size = GetWBE(&p_data[0]);
    p_data += 2; i_data -= 2;

    i_size = i_data = __MIN(i_data, i_size);
    while (i_data > 4)
    {
        p_dec->p_sys->palette[p_data[0]] = /* YCrCbT to ARGB */
                ( (uint32_t)((float)p_data[1] +1.402f * (p_data[2]-128)) << 16 ) |
                ( (uint32_t)((float)p_data[1] -0.34414 * (p_data[3]-128) -0.71414 * (p_data[2]-128)) << 8 ) |
                ( (uint32_t)((float)p_data[1] +1.722 * (p_data[3]-128)) ) |
                ( (0xFF - p_data[4]) << 24 );
        p_data += 5; i_data -= 5;
    }

    return i_size;
}

static void textst_FillRegions(decoder_t *p_dec, const uint8_t *p_data, size_t i_data,
                               subpicture_updater_sys_region_t *p_region)
{
    subpicture_updater_sys_region_t **pp_last = &p_region;
    bool palette_update_flag = p_data[0] >> 7;
    p_data++; i_data--;

    if (palette_update_flag)
    {
        size_t i_read = textst_Decode_palette(p_dec, p_data, i_data);
        p_data += i_read; i_data -= i_read;
    }

    if(i_data > 2)
    {
        uint8_t i_region_count = p_data[0];
        p_data++; i_data--;

        for(uint8_t i=0; i<i_region_count && i_data > 0; i++)
        {
            if(*pp_last == NULL)
            {
                *pp_last = SubpictureUpdaterSysRegionNew();
                if(!*pp_last)
                    break;
            }
            size_t i_read = textst_FillRegion(p_dec, p_data, i_data, *pp_last);
            (*pp_last)->align = SUBPICTURE_ALIGN_BOTTOM;
            pp_last = &(*pp_last)->p_next;
            p_data += i_read; i_data -= i_read;
        }
    }
}

static int Decode(decoder_t *p_dec, block_t *p_block)
{
    subpicture_t *p_sub = NULL;
    if (p_block == NULL) /* No Drain */
        return VLCDEC_SUCCESS;

    if (p_block->i_buffer > 18 &&
        (p_block->i_flags & BLOCK_FLAG_CORRUPTED) == 0 &&
        (p_sub = decoder_NewSubpictureText(p_dec)))
    {
        p_sub->i_start = ((int64_t) (p_block->p_buffer[3] & 0x01) << 32) | GetDWBE(&p_block->p_buffer[4]);
        p_sub->i_stop = ((int64_t) (p_block->p_buffer[8] & 0x01) << 32) | GetDWBE(&p_block->p_buffer[9]);
        p_sub->i_start = VLC_TS_0 + p_sub->i_start * 100 / 9;
        p_sub->i_stop = VLC_TS_0 + p_sub->i_stop * 100 / 9;
        if (p_sub->i_start < p_block->i_dts)
        {
            p_sub->i_stop += p_block->i_dts - p_sub->i_start;
            p_sub->i_start = p_block->i_dts;
        }

        textst_FillRegions(p_dec, &p_block->p_buffer[13], p_block->i_buffer - 13,
                           &p_sub->updater.p_sys->region);

        p_sub->b_absolute = false;
        decoder_QueueSub(p_dec, p_sub);
    }

    block_Release(p_block);
    return VLCDEC_SUCCESS;
}

static void Close(vlc_object_t *object)
{
    decoder_t *p_dec = (decoder_t*)object;
    free(p_dec->p_sys);
}

static int Open(vlc_object_t *object)
{
    decoder_t *p_dec = (decoder_t*)object;

    if (p_dec->fmt_in.i_codec != VLC_CODEC_BD_TEXT)
        return VLC_EGENERIC;

    decoder_sys_t *p_sys = malloc(sizeof(decoder_sys_t));
    if(!p_sys)
        return VLC_ENOMEM;
    memset(p_sys->palette, 0xFF, 256 * sizeof(uint32_t));

    p_dec->p_sys = p_sys;
    p_dec->pf_decode = Decode;
    p_dec->fmt_out.i_codec = 0;

    return VLC_SUCCESS;
}

