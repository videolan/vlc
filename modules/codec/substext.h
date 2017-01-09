/*****************************************************************************
 * subsdec.c : text subtitle decoder
 *****************************************************************************
 * Copyright © 2011-2015 VLC authors and VideoLAN
 *
 * Authors: Laurent Aimer <fenrir@videolan.org>
 *          Jean-Baptiste Kempf <jb@videolan.org>
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

#include <vlc_strings.h>
#include <vlc_text_style.h>

typedef struct subpicture_updater_sys_region_t subpicture_updater_sys_region_t;

enum subpicture_updater_sys_region_flags_e
{
    UPDT_REGION_ORIGIN_X_IS_PERCENTILE = 1 << 0,
    UPDT_REGION_ORIGIN_Y_IS_PERCENTILE = 1 << 1,
    UPDT_REGION_EXTENT_X_IS_PERCENTILE = 1 << 2,
    UPDT_REGION_EXTENT_Y_IS_PERCENTILE = 1 << 3,
    UPDT_REGION_IGNORE_BACKGROUND      = 1 << 4,
    UPDT_REGION_USES_GRID_COORDINATES  = 1 << 5,
    UPDT_REGION_FIX_DONE               = 1 << 31,
};

struct subpicture_updater_sys_region_t
{
    struct
    {
        int x;
        int y;
    } origin, extent;
    /* store above percentile meanings as modifier flags */
    int flags; /* subpicture_updater_sys_region_flags_e */
    int align; /* alignment of the region itself */
    int inner_align; /* alignment of content inside the region */
    text_style_t *p_region_style;
    text_segment_t *p_segments;
    subpicture_updater_sys_region_t *p_next;
};

struct subpicture_updater_sys_t {

    /* a min of one region */
    subpicture_updater_sys_region_t region;

    /* styling */
    text_style_t *p_default_style; /* decoder (full or partial) defaults */
};

static inline void SubpictureUpdaterSysRegionClean(subpicture_updater_sys_region_t *p_updtregion)
{
    text_segment_ChainDelete( p_updtregion->p_segments );
    text_style_Delete( p_updtregion->p_region_style );
}

static inline void SubpictureUpdaterSysRegionInit(subpicture_updater_sys_region_t *p_updtregion)
{
    memset(p_updtregion, 0, sizeof(*p_updtregion));
}

static inline subpicture_updater_sys_region_t *SubpictureUpdaterSysRegionNew( )
{
    subpicture_updater_sys_region_t *p_region = malloc(sizeof(*p_region));
    if(p_region)
        SubpictureUpdaterSysRegionInit(p_region);
    return p_region;
}

static inline void SubpictureUpdaterSysRegionAdd(subpicture_updater_sys_region_t *p_prev,
                                                 subpicture_updater_sys_region_t *p_new)
{
    subpicture_updater_sys_region_t **pp_next = &p_prev->p_next;
    for(; *pp_next; pp_next = &(*pp_next)->p_next);
    *pp_next = p_new;
}

static int SubpictureTextValidate(subpicture_t *subpic,
                                  bool has_src_changed, const video_format_t *fmt_src,
                                  bool has_dst_changed, const video_format_t *fmt_dst,
                                  mtime_t ts)
{
    subpicture_updater_sys_t *sys = subpic->updater.p_sys;
    VLC_UNUSED(fmt_src); VLC_UNUSED(fmt_dst); VLC_UNUSED(ts);

    if (!has_src_changed && !has_dst_changed)
        return VLC_SUCCESS;

    subpicture_updater_sys_region_t *p_updtregion = &sys->region;

    if (!(p_updtregion->flags & UPDT_REGION_FIX_DONE) &&
        subpic->b_absolute && subpic->p_region &&
        subpic->i_original_picture_width > 0 &&
        subpic->i_original_picture_height > 0)
    {
        p_updtregion->flags |= UPDT_REGION_FIX_DONE;
        p_updtregion->origin.x = subpic->p_region->i_x;
        p_updtregion->origin.y = subpic->p_region->i_y;
        p_updtregion->extent.x = subpic->i_original_picture_width;
        p_updtregion->extent.y = subpic->i_original_picture_height;
    }

    return VLC_EGENERIC;
}

static void SubpictureTextUpdate(subpicture_t *subpic,
                                 const video_format_t *fmt_src,
                                 const video_format_t *fmt_dst,
                                 mtime_t ts)
{
    subpicture_updater_sys_t *sys = subpic->updater.p_sys;
    VLC_UNUSED(fmt_src); VLC_UNUSED(ts);

    if (fmt_dst->i_sar_num <= 0 || fmt_dst->i_sar_den <= 0)
        return;

    subpic->i_original_picture_width  = fmt_dst->i_width * fmt_dst->i_sar_num / fmt_dst->i_sar_den;
    subpic->i_original_picture_height = fmt_dst->i_height;

    video_format_t fmt;
    video_format_Init(&fmt, VLC_CODEC_TEXT);
    fmt.i_sar_num = 1;
    fmt.i_sar_den = 1;

    subpicture_region_t **pp_last_region = &subpic->p_region;

    for( subpicture_updater_sys_region_t *p_updtregion = &sys->region;
                                          p_updtregion; p_updtregion = p_updtregion->p_next )
    {
        subpicture_region_t *r = *pp_last_region = subpicture_region_New(&fmt);
        if (!r)
            return;
        pp_last_region = &r->p_next;

        r->p_text = text_segment_Copy( p_updtregion->p_segments );
        r->i_align  = p_updtregion->inner_align | p_updtregion->align; /* we do not support text align by itself */
        r->b_noregionbg = p_updtregion->flags & UPDT_REGION_IGNORE_BACKGROUND;
        r->b_gridmode = p_updtregion->flags & UPDT_REGION_USES_GRID_COORDINATES;
        if (!(p_updtregion->flags & UPDT_REGION_FIX_DONE))
        {
            const float margin_ratio = 0.04;
            const int   margin_h     = margin_ratio * fmt_dst->i_visible_width;
            const int   margin_v     = margin_ratio * fmt_dst->i_visible_height;

            r->i_x = 0;
            if (r->i_align & SUBPICTURE_ALIGN_LEFT)
                r->i_x += margin_h + fmt_dst->i_x_offset;
            else if (r->i_align & SUBPICTURE_ALIGN_RIGHT)
                r->i_x += margin_h + fmt_dst->i_width - (fmt_dst->i_visible_width + fmt_dst->i_x_offset);

            r->i_y = 0;
            if (r->i_align & SUBPICTURE_ALIGN_TOP )
                r->i_y += margin_v + fmt_dst->i_y_offset;
            else if (r->i_align & SUBPICTURE_ALIGN_BOTTOM )
                r->i_y += margin_v + fmt_dst->i_height - (fmt_dst->i_visible_height + fmt_dst->i_y_offset);
        } else {
            /* FIXME it doesn't adapt on crop settings changes */
            r->i_x = p_updtregion->origin.x * fmt_dst->i_width  / p_updtregion->extent.x;
            r->i_y = p_updtregion->origin.y * fmt_dst->i_height / p_updtregion->extent.y;
        }

        /* Add missing default style, if any, to all segments */
        for ( text_segment_t* p_segment = r->p_text; p_segment; p_segment = p_segment->p_next )
        {
            /* Add decoder defaults */
            if( p_segment->style )
                text_style_Merge( p_segment->style, sys->p_default_style, false );
            else
                p_segment->style = text_style_Duplicate( sys->p_default_style );
            /* Update all segments font sizes in pixels, *** metric used by renderers *** */
            /* We only do this when a fixed font size isn't set */
            if( p_segment->style && p_segment->style->f_font_relsize && !p_segment->style->i_font_size )
            {
                p_segment->style->i_font_size = p_segment->style->f_font_relsize *
                        subpic->i_original_picture_height / 100;
            }
        }

    }
}
static void SubpictureTextDestroy(subpicture_t *subpic)
{
    subpicture_updater_sys_t *sys = subpic->updater.p_sys;

    SubpictureUpdaterSysRegionClean( &sys->region );
    subpicture_updater_sys_region_t *p_region = sys->region.p_next;
    while( p_region )
    {
        subpicture_updater_sys_region_t *p_next = p_region->p_next;
        SubpictureUpdaterSysRegionClean( p_region );
        p_region = p_next;
    }
    text_style_Delete( sys->p_default_style );
    free(sys);
}

static inline subpicture_t *decoder_NewSubpictureText(decoder_t *decoder)
{
    subpicture_updater_sys_t *sys = calloc(1, sizeof(*sys));
    subpicture_updater_t updater = {
        .pf_validate = SubpictureTextValidate,
        .pf_update   = SubpictureTextUpdate,
        .pf_destroy  = SubpictureTextDestroy,
        .p_sys       = sys,
    };
    SubpictureUpdaterSysRegionInit( &sys->region );
    sys->p_default_style = text_style_Create( STYLE_NO_DEFAULTS );
    if(unlikely(!sys->p_default_style))
    {
        free(sys);
        return NULL;
    }
    subpicture_t *subpic = decoder_NewSubpicture(decoder, &updater);
    if (!subpic)
    {
        text_style_Delete(sys->p_default_style);
        free(sys);
    }
    return subpic;
}
