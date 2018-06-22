/*****************************************************************************
 * imageupdater.h : TTML image to SPU updater
 *****************************************************************************
 * Copyright © 2018 Videolabs, VideoLAN and VLC authors
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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *****************************************************************************/
#include <vlc_subpicture.h>

typedef struct ttml_image_updater_region_t ttml_image_updater_region_t;

struct ttml_image_updater_region_t
{
    struct
    {
        float x;
        float y;
    } origin, extent;
    int i_flags;
    picture_t *p_pic;
    ttml_image_updater_region_t *p_next;
};

enum ttml_image_updater_region_flags_e
{
    ORIGIN_X_IS_RATIO      = 1 << 0,
    ORIGIN_Y_IS_RATIO      = 1 << 1,
    EXTENT_X_IS_RATIO      = 1 << 2,
    EXTENT_Y_IS_RATIO      = 1 << 3,
};

static void TTML_ImageUpdaterRegionDelete(ttml_image_updater_region_t *p_updtregion)
{
    if (p_updtregion->p_pic)
        picture_Release(p_updtregion->p_pic);
    free(p_updtregion);
}

static ttml_image_updater_region_t *TTML_ImageUpdaterRegionNew(picture_t *p_pic)
{
    ttml_image_updater_region_t *p_region = calloc(1, sizeof(*p_region));
    if(p_region)
        p_region->p_pic = p_pic;
    return p_region;
}

/*
 * UPDATER
*/

typedef struct
{
    ttml_image_updater_region_t *p_regions;
    ttml_image_updater_region_t **pp_append;

} ttml_image_updater_sys_t;

static void TTML_ImageSpuAppendRegion(ttml_image_updater_sys_t *p_sys,
                                      ttml_image_updater_region_t *p_new)
{
    *p_sys->pp_append = p_new;
    p_sys->pp_append = &p_new->p_next;
}

static int TTML_ImageSpuValidate(subpicture_t *p_spu,
                                 bool b_src_changed, const video_format_t *p_fmt_src,
                                 bool b_dst_changed, const video_format_t *p_fmt_dst,
                                 vlc_tick_t ts)
{
    VLC_UNUSED(p_spu);
    VLC_UNUSED(b_src_changed); VLC_UNUSED(p_fmt_src);
    VLC_UNUSED(p_fmt_dst);
    VLC_UNUSED(ts);
    return b_dst_changed ? VLC_EGENERIC: VLC_SUCCESS;
}

static void TTML_ImageSpuUpdate(subpicture_t *p_spu,
                                const video_format_t *p_fmt_src,
                                const video_format_t *p_fmt_dst,
                                vlc_tick_t i_ts)
{
    VLC_UNUSED(p_fmt_src); VLC_UNUSED(p_fmt_dst);
    VLC_UNUSED(i_ts);
    ttml_image_updater_sys_t *p_sys = p_spu->updater.p_sys;
    subpicture_region_t **pp_last_region = &p_spu->p_region;

    /* !WARN: SMPTE-TT image profile requires no scaling, and even it
              would, it does not store the necessary original pic size */

    for(ttml_image_updater_region_t *p_updtregion = p_sys->p_regions;
                                     p_updtregion; p_updtregion = p_updtregion->p_next)
    {
        subpicture_region_t *r = subpicture_region_New(&p_updtregion->p_pic->format);
        if (!r)
            return;
        picture_Release(r->p_picture);
        r->p_picture = picture_Clone(p_updtregion->p_pic);
        if(!r->p_picture)
        {
            subpicture_region_Delete(r);
            return;
        }

        r->i_align = SUBPICTURE_ALIGN_LEFT|SUBPICTURE_ALIGN_TOP;

        if( p_updtregion->i_flags & ORIGIN_X_IS_RATIO )
            r->i_x = p_updtregion->origin.x * p_fmt_dst->i_visible_width;
        else
            r->i_x = p_updtregion->origin.x;

        if( p_updtregion->i_flags & ORIGIN_Y_IS_RATIO )
            r->i_y = p_updtregion->origin.y * p_fmt_dst->i_visible_height;
        else
            r->i_y = p_updtregion->origin.y;

        *pp_last_region = r;
        pp_last_region = &r->p_next;
    }
}

static void TTML_ImageSpuDestroy(subpicture_t *p_spu)
{
    ttml_image_updater_sys_t *p_sys = p_spu->updater.p_sys;
    while(p_sys->p_regions)
    {
        ttml_image_updater_region_t *p_next = p_sys->p_regions->p_next;
        TTML_ImageUpdaterRegionDelete(p_sys->p_regions);
        p_sys->p_regions = p_next;
    }
    free(p_sys);
}

static inline subpicture_t *decoder_NewTTML_ImageSpu(decoder_t *p_dec)
{
    ttml_image_updater_sys_t *p_sys = calloc(1, sizeof(*p_sys));
    if(!p_sys)
        return NULL;
    subpicture_updater_t updater = {
        .pf_validate = TTML_ImageSpuValidate,
        .pf_update   = TTML_ImageSpuUpdate,
        .pf_destroy  = TTML_ImageSpuDestroy,
        .p_sys       = p_sys,
    };
    p_sys->p_regions = NULL;
    p_sys->pp_append = &p_sys->p_regions;
    subpicture_t *p_spu = decoder_NewSubpicture(p_dec, &updater);
    if (!p_spu)
        free(p_sys);
    return p_spu;
}
