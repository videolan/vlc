/*****************************************************************************
 * substext.h : ARIB subtitles subpicture decoder
 *****************************************************************************
 * Copyright (C) 2012 Naohiro KORIYAMA
 *
 * Authors:  Naohiro KORIYAMA <nkoriyama@gmail.com>
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

typedef struct arib_text_region_s
{
    char                      *psz_text;

    char                      *psz_fontname;
    int                       i_font_color;
    int                       i_planewidth;
    int                       i_planeheight;
    int                       i_fontwidth;
    int                       i_fontheight;
    int                       i_verint;
    int                       i_horint;
    int                       i_charleft;
    int                       i_charbottom;
    int                       i_charleft_adj;
    int                       i_charbottom_adj;

    struct arib_text_region_s *p_next;
} arib_text_region_t;

typedef struct
{
    arib_text_region_t *p_region;
} arib_spu_updater_sys_t;

static void SubpictureTextUpdate(subpicture_t *subpic,
                                 const video_format_t *prev_src, const video_format_t *fmt_src,
                                 const video_format_t *prev_dst, const video_format_t *fmt_dst,
                                 vlc_tick_t ts)
{
    arib_spu_updater_sys_t *sys = subpic->updater.sys;
    VLC_UNUSED(fmt_src); VLC_UNUSED(ts);
    VLC_UNUSED(prev_src);

    if (video_format_IsSimilar(prev_dst, fmt_dst))
        return;

    vlc_spu_regions_Clear( &subpic->regions );

    if (fmt_dst->i_sar_num <= 0 || fmt_dst->i_sar_den <= 0)
    {
        return;
    }

    subpicture_region_t *r;
    arib_text_region_t *p_region;
    for( p_region = sys->p_region; p_region; p_region = p_region->p_next )
    {
        r = subpicture_region_NewText();
        if( r == NULL )
        {
            return;
        }
        vlc_spu_regions_push(&subpic->regions, r);
        r->fmt.i_sar_num = 1;
        r->fmt.i_sar_den = 1;

        r->p_text = text_segment_New( p_region->psz_text );
        r->i_align  = SUBPICTURE_ALIGN_LEFT | SUBPICTURE_ALIGN_TOP;

        if (p_region->i_planewidth > 0 && p_region->i_planeheight > 0)
        {
            subpic->i_original_picture_width  = p_region->i_planewidth;
            subpic->i_original_picture_height  = p_region->i_planeheight;
        }

        r->i_x = p_region->i_charleft - (p_region->i_fontwidth + p_region->i_horint / 2) + p_region->i_charleft_adj;
        r->i_y = p_region->i_charbottom - (p_region->i_fontheight + p_region->i_verint / 2) + p_region->i_charbottom_adj;
        r->p_text->style = text_style_Create( STYLE_NO_DEFAULTS );
        r->p_text->style->psz_fontname = p_region->psz_fontname ? strdup( p_region->psz_fontname ) : NULL;
        r->p_text->style->i_font_size = p_region->i_fontheight;
        r->p_text->style->i_font_color = p_region->i_font_color;
        r->p_text->style->i_features |= STYLE_HAS_FONT_COLOR;
        if( p_region->i_fontwidth < p_region->i_fontheight )
        {
            r->p_text->style->i_style_flags |= STYLE_HALFWIDTH;
            r->p_text->style->i_features |= STYLE_HAS_FLAGS;
        }
        r->p_text->style->i_spacing = p_region->i_horint;
    }
}
static void SubpictureTextDestroy(subpicture_t *subpic)
{
    arib_spu_updater_sys_t *sys = subpic->updater.sys;

    arib_text_region_t *p_region, *p_region_next;
    for( p_region = sys->p_region; p_region; p_region = p_region_next )
    {
        free( p_region->psz_text );
        free( p_region->psz_fontname );
        p_region_next = p_region->p_next;
        free( p_region );
    }
    sys->p_region = NULL;
    free( sys );
}

static inline subpicture_t *decoder_NewSubpictureText(decoder_t *decoder)
{
    arib_spu_updater_sys_t *sys = (arib_spu_updater_sys_t*)
        calloc( 1, sizeof(arib_spu_updater_sys_t) );

    static const struct vlc_spu_updater_ops spu_ops =
    {
        .update   = SubpictureTextUpdate,
        .destroy  = SubpictureTextDestroy,
    };

    subpicture_updater_t updater = {
        .sys = sys,
        .ops = &spu_ops,
    };
    subpicture_t *subpic = decoder_NewSubpicture(decoder, &updater);
    if( subpic == NULL )
    {
        free( sys );
    }
    return subpic;
}
