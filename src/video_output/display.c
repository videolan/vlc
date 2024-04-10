/*****************************************************************************
 * display.c: "vout display" management
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
#include <stdatomic.h>

#include <vlc_common.h>
#include <vlc_vout_display.h>
#include <vlc_vout.h>
#include <vlc_block.h>
#include <vlc_modules.h>
#include <vlc_filter.h>
#include <vlc_picture_pool.h>
#include <vlc_codec.h>

#include <libvlc.h>

#include "display.h"
#include "vout_internal.h"

static int vout_display_Control(vout_display_t *vd, int query)
{
    return vd->ops->control(vd, query);
}

/*****************************************************************************
 *
 *****************************************************************************/

/* */
void vout_display_GetDefaultDisplaySize(unsigned *width, unsigned *height,
                                        const video_format_t *source,
                                        const struct vout_display_placement *dp)
{
    /* Use the original video size */
    if (source->i_sar_num >= source->i_sar_den) {
        *width  = (uint64_t)source->i_visible_width * source->i_sar_num * dp->sar.den / source->i_sar_den / dp->sar.num;
        *height = source->i_visible_height;
    } else {
        *width  = source->i_visible_width;
        *height = (uint64_t)source->i_visible_height * source->i_sar_den * dp->sar.num / source->i_sar_num / dp->sar.den;
    }

    *width  = *width  * dp->zoom.num / dp->zoom.den;
    *height = *height * dp->zoom.num / dp->zoom.den;

    if (ORIENT_IS_SWAP(source->orientation)) {
        /* Apply the source orientation only if the dimensions are initialized
         * from the source format */
        unsigned store = *width;
        *width = *height;
        *height = store;
    }
}

static void vout_display_PlaceRotatedPicture(vout_display_place_t *restrict place,
                                             const video_format_t *restrict source,
                                             const struct vout_display_placement *restrict dp)
{
    memset(place, 0, sizeof(*place));
    if (dp->width == 0 || dp->height == 0)
        return;

    unsigned display_width;
    unsigned display_height;

    if (dp->fitting != VLC_VIDEO_FIT_NONE) {
        display_width  = dp->width;
        display_height = dp->height;
    } else
        vout_display_GetDefaultDisplaySize(&display_width, &display_height,
                                           source, dp);

    const unsigned width  = source->i_visible_width;
    const unsigned height = source->i_visible_height;
    /* Compute the height if we use the width to fill up display_width */
    const int64_t scaled_height = (int64_t)height * display_width  * dp->sar.num * source->i_sar_den / (width  * source->i_sar_num * dp->sar.den);
    /* And the same but switching width/height */
    const int64_t scaled_width  = (int64_t)width  * display_height * dp->sar.den * source->i_sar_num / (height * source->i_sar_den * dp->sar.num);

    if (source->projection_mode == PROJECTION_MODE_RECTANGULAR) {
        bool fit_height;

        switch (dp->fitting) {
            case VLC_VIDEO_FIT_NONE:
            case VLC_VIDEO_FIT_SMALLER:
                /* We keep the solution fitting within the display */
                fit_height = scaled_width <= dp->width;
                break;
            case VLC_VIDEO_FIT_LARGER:
                fit_height = scaled_width >= dp->width;
                break;
            case VLC_VIDEO_FIT_WIDTH:
                fit_height = false;
                break;
            case VLC_VIDEO_FIT_HEIGHT:
                fit_height = true;
                break;
        }

        if (fit_height) {
            place->width  = scaled_width;
            place->height = display_height;
        } else {
            place->width  = display_width;
            place->height = scaled_height;
        }
    } else {
        /* No need to preserve an aspect ratio for 360 video.
         * They can fill the display. */
        place->width  = display_width;
        place->height = display_height;
    }

    /*  Compute position */
    switch (dp->align.horizontal) {
    case VLC_VIDEO_ALIGN_LEFT:
        place->x = 0;
        break;
    case VLC_VIDEO_ALIGN_RIGHT:
        place->x = dp->width - place->width;
        break;
    default:
        place->x = ((int)dp->width - (int)place->width) / 2;
        break;
    }

    switch (dp->align.vertical) {
    case VLC_VIDEO_ALIGN_TOP:
        place->y = 0;
        break;
    case VLC_VIDEO_ALIGN_BOTTOM:
        place->y = dp->height - place->height;
        break;
    default:
        place->y = ((int)dp->height - (int)place->height) / 2;
        break;
    }

}

/* */
void vout_display_PlacePicture(vout_display_place_t *restrict place,
                               const video_format_t *restrict source,
                               const struct vout_display_placement *restrict dp)
{
    video_format_t source_rot;
    video_format_ApplyRotation(&source_rot, source);
    vout_display_PlaceRotatedPicture(place, &source_rot, dp);
}

/** Translates window coordinates to video coordinates */
void vout_display_TranslateCoordinates(int *restrict xp, int *restrict yp,
                                       const video_format_t *restrict source,
                                       const struct vout_display_placement *restrict dp)
{
    video_format_t source_rot;
    video_format_ApplyRotation(&source_rot, source);

    vout_display_place_t place;
    vout_display_PlaceRotatedPicture(&place, &source_rot, dp);

    if (place.width <= 0 || place.height <= 0)
        return;

    const int wx = *xp, wy = *yp;
    int x, y;

    switch (source->orientation) {
        case ORIENT_TOP_LEFT:
            x = wx;
            y = wy;
            break;
        case ORIENT_TOP_RIGHT:
            x = place.width - wx;
            y = wy;
            break;
        case ORIENT_BOTTOM_LEFT:
            x = wx;
            y = place.height - wy;
            break;
        case ORIENT_BOTTOM_RIGHT:
            x = place.width - wx;
            y = place.height - wy;
            break;
        case ORIENT_LEFT_TOP:
            x = wy;
            y = wx;
            break;
        case ORIENT_LEFT_BOTTOM:
            x = place.height - wy;
            y = wx;
            break;
        case ORIENT_RIGHT_TOP:
            x = wy;
            y = place.width - wx;
            break;
        case ORIENT_RIGHT_BOTTOM:
            x = place.height - wy;
            y = place.width - wx;
            break;
        default:
            vlc_assert_unreachable();
    }

    x = source_rot.i_x_offset
        + (int64_t)(x - place.x) * source_rot.i_visible_width / place.width;
    y = source_rot.i_y_offset
        + (int64_t)(y - place.y) * source_rot.i_visible_height / place.height;
    *xp = x;
    *yp = y;
}

struct pooled_filter_chain {
    filter_chain_t *filters;
    picture_pool_t *pool;
};

typedef struct {
    vout_display_t  display;

    /* */
    vout_display_cfg_t cfg;
    struct vout_crop crop;

    /* */
    video_format_t source;
    video_format_t display_fmt;
    vlc_video_context *src_vctx;

    vout_display_place_t src_place;

    // filters to convert the vout source to fmt, NULL means no conversion is needed
    struct pooled_filter_chain *converter;
} vout_display_priv_t;

static bool PlaceVideoInDisplay(vout_display_priv_t *osys)
{
    struct vout_display_placement place_cfg = osys->cfg.display;
    vout_display_place_t prev_place = osys->src_place;
    vout_display_PlacePicture(&osys->src_place, &osys->source, &place_cfg);
    return vout_display_PlaceEquals(&prev_place, &osys->src_place);
}

/*****************************************************************************
 * FIXME/TODO see how to have direct rendering here (interact with vout.c)
 *****************************************************************************/
static picture_t *SourceConverterBuffer(filter_t *filter)
{
    vout_display_t *vd = filter->owner.sys;
    vout_display_priv_t *osys = container_of(vd, vout_display_priv_t, display);
    const video_format_t *fmt = &filter->fmt_out.video;

    assert( video_format_IsSameChroma( &osys->display_fmt, fmt) &&
           osys->display_fmt.i_width  == fmt->i_width  &&
           osys->display_fmt.i_height == fmt->i_height);

    return picture_pool_Get(osys->converter->pool);
}

static vlc_decoder_device * DisplayHoldDecoderDevice(vlc_object_t *o, void *sys)
{
    VLC_UNUSED(o);
    vout_display_t *vd = sys;
    vout_display_priv_t *osys = container_of(vd, vout_display_priv_t, display);
    return osys->src_vctx ? vlc_video_context_HoldDevice(osys->src_vctx) : NULL;
}

static const struct filter_video_callbacks vout_display_filter_cbs = {
    SourceConverterBuffer, DisplayHoldDecoderDevice,
};
static void VoutConverterRelease(struct pooled_filter_chain *conv)
{
    filter_chain_Delete(conv->filters);
    picture_pool_Release(conv->pool);
    free(conv);
}

static struct pooled_filter_chain *VoutSetupConverter(vlc_object_t *o,
                              filter_owner_t *owner,
                              const video_format_t *fmt_in,
                              vlc_video_context *vctx_in,
                              const video_format_t *fmt_out)
{
    struct pooled_filter_chain *conv = malloc(sizeof(*conv));
    if (unlikely(conv == NULL))
        return NULL;

    // 1 for current converter + 1 for previously displayed
    conv->pool = picture_pool_NewFromFormat(fmt_out, 1+1);
    if (unlikely(conv->pool == NULL))
    {
        msg_Err(o, "Failed to allocate converter pool");
        free(conv);
        return NULL;
    }

    conv->filters = filter_chain_NewVideo(o, false, owner);
    if (unlikely(conv->filters == NULL))
    {
        msg_Err(o, "Failed to create converter filter chain");
        picture_pool_Release(conv->pool);
        free(conv);
        return NULL;
    }

    /* */
    es_format_t src;
    es_format_InitFromVideo(&src, fmt_in);

    /* */
    es_format_t dst;
    es_format_InitFromVideo(&dst, fmt_out);
    dst.video.i_sar_num = 0;
    dst.video.i_sar_den = 0;

    filter_chain_Reset(conv->filters, &src, vctx_in, &dst);
    int ret = filter_chain_AppendConverter(conv->filters, &dst);
    es_format_Clean(&dst);
    es_format_Clean(&src);

    if (ret != VLC_SUCCESS)
    {
        VoutConverterRelease(conv);
        return NULL;
    }
    return conv;
}

static int VoutDisplayCreateRender(vout_display_t *vd)
{
    vout_display_priv_t *osys = container_of(vd, vout_display_priv_t, display);
    filter_owner_t owner = {
        .video = &vout_display_filter_cbs,
        .sys = vd,
    };

    video_format_t v_src = osys->source;
    v_src.i_sar_num = 0;
    v_src.i_sar_den = 0;

    video_format_t v_dst = osys->display_fmt;
    v_dst.i_sar_num = 0;
    v_dst.i_sar_den = 0;

    const bool convert = memcmp(&v_src, &v_dst, sizeof(v_src)) != 0;
    if (!convert)
        return VLC_SUCCESS;

    msg_Dbg(vd, "A filter to adapt decoder %4.4s to display %4.4s is needed",
            (const char *)&v_src.i_chroma, (const char *)&v_dst.i_chroma);

    osys->converter = VoutSetupConverter(VLC_OBJECT(vd), &owner,
                                 &v_src, osys->src_vctx, &osys->display_fmt);
    if (osys->converter == NULL) {
        msg_Err(vd, "Failed to adapt decoder format to display");
        return VLC_ENOTSUP;
    }
    return VLC_SUCCESS;
}

static void VoutDisplayCropRatio(unsigned *left, unsigned *top, unsigned *right, unsigned *bottom,
                                 const video_format_t *source,
                                 unsigned num, unsigned den)
{
    unsigned scaled_width  = (uint64_t)source->i_visible_height * num * source->i_sar_den / den / source->i_sar_num;
    unsigned scaled_height = (uint64_t)source->i_visible_width  * den * source->i_sar_num / num / source->i_sar_den;

    if (source->i_visible_width > scaled_width) {
        *left   = (source->i_visible_width - scaled_width) / 2;
        *top    = 0;
        *right  = *left + scaled_width;
        *bottom = *top  + source->i_visible_height;
    } else if (source->i_visible_height > scaled_height) {
        *left   = 0;
        *top    = (source->i_visible_height - scaled_height) / 2;
        *right  = *left + source->i_visible_width;
        *bottom = *top  + scaled_height;
    } else {
        *left   = 0;
        *top    = 0;
        *right  = source->i_visible_width;
        *bottom = source->i_visible_height;
    }
}

picture_t *vout_ConvertForDisplay(vout_display_t *vd, picture_t *picture)
{
    vout_display_priv_t *osys = container_of(vd, vout_display_priv_t, display);

    if (osys->converter == NULL)
        return picture;

    return filter_chain_VideoFilter(osys->converter->filters, picture);
}

picture_t *vout_display_Prepare(vout_display_t *vd, picture_t *picture,
                                const vlc_render_subpicture *subpic, vlc_tick_t date)
{
    assert(subpic == NULL); /* TODO */
    picture = vout_ConvertForDisplay(vd, picture);

    if (picture != NULL && vd->ops->prepare != NULL)
        vd->ops->prepare(vd, picture, subpic, date);
    return picture;
}

void vout_FilterFlush(vout_display_t *vd)
{
    vout_display_priv_t *osys = container_of(vd, vout_display_priv_t, display);

    if (osys->converter != NULL)
        filter_chain_VideoFlush(osys->converter->filters);
}

static void vout_display_Reset(vout_display_t *vd)
{
    vout_display_priv_t *osys = container_of(vd, vout_display_priv_t, display);

    if (osys->converter != NULL)
    {
        VoutConverterRelease(osys->converter);
        osys->converter = NULL;
    }

    assert(vd->ops->reset_pictures);
    if (vd->ops->reset_pictures(vd, &osys->display_fmt) != VLC_SUCCESS
     || VoutDisplayCreateRender(vd))
        msg_Err(vd, "Failed to adjust render format");
}

static int vout_UpdateSourceCrop(vout_display_t *vd)
{
    vout_display_priv_t *osys = container_of(vd, vout_display_priv_t, display);
    unsigned left, top, right, bottom;

    video_format_Print(VLC_OBJECT(vd), "SOURCE ", &osys->source);

    switch (osys->crop.mode) {
        case VOUT_CROP_NONE:
            left = top = 0;
            right = osys->source.i_visible_width;
            bottom = osys->source.i_visible_height;
            break;
        case VOUT_CROP_RATIO:
            VoutDisplayCropRatio(&left, &top, &right, &bottom, &osys->source,
                                 osys->crop.ratio.num, osys->crop.ratio.den);
            break;
        case VOUT_CROP_WINDOW:
            left = osys->crop.window.x;
            top = osys->crop.window.y;
            right = left + osys->crop.window.width;
            bottom = top + osys->crop.window.height;
            break;
        case VOUT_CROP_BORDER:
            left = osys->crop.border.left;
            top = osys->crop.border.top;
            right = osys->source.i_visible_width - osys->crop.border.right;
            bottom = osys->source.i_visible_height - osys->crop.border.bottom;
            break;
        default:
            /* left/top/right/bottom must be initialized */
            vlc_assert_unreachable();
    }

    if (left >= right)
        left = right - 1;
    if (top >= bottom)
        top = bottom - 1;
    if (right > osys->source.i_visible_width)
        right = osys->source.i_visible_width;
    if (bottom > osys->source.i_visible_height)
        bottom = osys->source.i_visible_height;

    osys->source.i_x_offset += left;
    osys->source.i_y_offset += top;
    osys->source.i_visible_width = right - left;
    osys->source.i_visible_height = bottom - top;
    video_format_Print(VLC_OBJECT(vd), "CROPPED ", &osys->source);

    bool place_changed = PlaceVideoInDisplay(osys);

    int res1 = vout_display_Control(vd, VOUT_DISPLAY_CHANGE_SOURCE_CROP);
    if (place_changed)
    {
        int res2 = vout_display_Control(vd, VOUT_DISPLAY_CHANGE_SOURCE_PLACE);
        if (res2 != VLC_SUCCESS)
            res1 = res2;
    }
    return res1;
}

static int vout_SetSourceAspect(vout_display_t *vd,
                                unsigned sar_num, unsigned sar_den)
{
    vout_display_priv_t *osys = container_of(vd, vout_display_priv_t, display);
    int err1, err2 = VLC_SUCCESS;

    if (sar_num > 0 && sar_den > 0) {
        osys->source.i_sar_num = sar_num;
        osys->source.i_sar_den = sar_den;
    }

    bool place_changed = PlaceVideoInDisplay(osys);

    err1 = vout_display_Control(vd, VOUT_DISPLAY_CHANGE_SOURCE_ASPECT);

    /* If a crop ratio is requested, recompute the parameters */
    if (osys->crop.mode != VOUT_CROP_NONE)
        err2 = vout_UpdateSourceCrop(vd);

    if (place_changed)
    {
        int res2 = vout_display_Control(vd, VOUT_DISPLAY_CHANGE_SOURCE_PLACE);
        if (res2 != VLC_SUCCESS)
            err1 = res2;
    }

    if (err1 != VLC_SUCCESS)
        return err1;

    return err2;
}

void VoutFixFormatAR(video_format_t *fmt)
{
    vlc_ureduce( &fmt->i_sar_num, &fmt->i_sar_den,
                 fmt->i_sar_num,  fmt->i_sar_den, 50000 );
    if (fmt->i_sar_num <= 0 || fmt->i_sar_den <= 0) {
        fmt->i_sar_num = 1;
        fmt->i_sar_den = 1;
    }
}

void vout_UpdateDisplaySourceProperties(vout_display_t *vd, const video_format_t *source, const vlc_rational_t *forced_dar)
{
    vout_display_priv_t *osys = container_of(vd, vout_display_priv_t, display);
    int err1 = VLC_SUCCESS, err2 = VLC_SUCCESS;

    video_format_t fixed_src = *source;
    VoutFixFormatAR( &fixed_src );
    if (fixed_src.i_sar_num * osys->source.i_sar_den !=
        fixed_src.i_sar_den * osys->source.i_sar_num) {

        if (forced_dar->num == 0) {
            osys->source.i_sar_num = fixed_src.i_sar_num;
            osys->source.i_sar_den = fixed_src.i_sar_den;

            err1 = vout_SetSourceAspect(vd, osys->source.i_sar_num,
                                        osys->source.i_sar_den);
        }
    }
    if (source->i_x_offset       != osys->source.i_x_offset ||
        source->i_y_offset       != osys->source.i_y_offset ||
        source->i_visible_width  != osys->source.i_visible_width ||
        source->i_visible_height != osys->source.i_visible_height) {

        video_format_CopyCrop(&osys->source, source);

        /* Force the vout to reapply the current user crop settings
         * over the new decoder crop settings. */
        err2 = vout_UpdateSourceCrop(vd);
    }

    if (err1 != VLC_SUCCESS || err2 != VLC_SUCCESS)
        vout_display_Reset(vd);
}

void vout_display_SetSize(vout_display_t *vd, unsigned width, unsigned height)
{
    vout_display_priv_t *osys = container_of(vd, vout_display_priv_t, display);

    osys->cfg.display.width  = width;
    osys->cfg.display.height = height;

    bool place_changed = PlaceVideoInDisplay(osys);

    int res1 = vout_display_Control(vd, VOUT_DISPLAY_CHANGE_DISPLAY_SIZE);

    if (place_changed)
    {
        int res2 = vout_display_Control(vd, VOUT_DISPLAY_CHANGE_SOURCE_PLACE);
        if (res2 != VLC_SUCCESS)
            res1 = res2;
    }

    if (res1 != VLC_SUCCESS)
        vout_display_Reset(vd);
}

void vout_SetDisplayFitting(vout_display_t *vd, enum vlc_video_fitting fit)
{
    vout_display_priv_t *osys = container_of(vd, vout_display_priv_t, display);

    if (fit == osys->cfg.display.fitting)
        return; /* nothing to do */

    osys->cfg.display.fitting = fit;

    bool place_changed = PlaceVideoInDisplay(osys);
    if (place_changed)
    {
        int res2 = vout_display_Control(vd, VOUT_DISPLAY_CHANGE_SOURCE_PLACE);
        if (res2 != VLC_SUCCESS)
            vout_display_Reset(vd);
    }
}

void vout_SetDisplayZoom(vout_display_t *vd, unsigned num, unsigned den)
{
    vout_display_priv_t *osys = container_of(vd, vout_display_priv_t, display);
    unsigned onum = osys->cfg.display.zoom.num;
    unsigned oden = osys->cfg.display.zoom.den;

    osys->cfg.display.zoom.num = num;
    osys->cfg.display.zoom.den = den;

    if (osys->cfg.display.fitting != VLC_VIDEO_FIT_NONE)
        return; /* zoom has no effects */
    if (onum * den == num * oden)
        return; /* zoom has not changed */

    bool place_changed = PlaceVideoInDisplay(osys);
    if (place_changed)
    {
        int res2 = vout_display_Control(vd, VOUT_DISPLAY_CHANGE_SOURCE_PLACE);
        if (res2 != VLC_SUCCESS)
            vout_display_Reset(vd);
    }
}

void vout_SetDisplayAspect(vout_display_t *vd, unsigned dar_num, unsigned dar_den)
{
    vout_display_priv_t *osys = container_of(vd, vout_display_priv_t, display);

    unsigned sar_num, sar_den;
    if (dar_num > 0 && dar_den > 0) {
        sar_num = dar_num * osys->source.i_visible_height;
        sar_den = dar_den * osys->source.i_visible_width;
        vlc_ureduce(&sar_num, &sar_den, sar_num, sar_den, 0);
    } else {
        sar_num = 0;
        sar_den = 0;
    }

    if (vout_SetSourceAspect(vd, sar_num, sar_den) != VLC_SUCCESS)
        vout_display_Reset(vd);
}

void vout_SetDisplayCrop(vout_display_t *vd,
                         const struct vout_crop *restrict crop)
{
    vout_display_priv_t *osys = container_of(vd, vout_display_priv_t, display);

    if (!vout_CropEqual(crop, &osys->crop)) {
        osys->crop = *crop;

        if (vout_UpdateSourceCrop(vd) != VLC_SUCCESS)
            vout_display_Reset(vd);
    }
}

void vout_SetDisplayViewpoint(vout_display_t *vd,
                              const vlc_viewpoint_t *p_viewpoint)
{
    vout_display_priv_t *osys = container_of(vd, vout_display_priv_t, display);

    if (osys->cfg.viewpoint.yaw   != p_viewpoint->yaw ||
        osys->cfg.viewpoint.pitch != p_viewpoint->pitch ||
        osys->cfg.viewpoint.roll  != p_viewpoint->roll ||
        osys->cfg.viewpoint.fov   != p_viewpoint->fov) {
        vlc_viewpoint_t old_vp = osys->cfg.viewpoint;

        osys->cfg.viewpoint = *p_viewpoint;

        if (vd->ops->set_viewpoint)
        {
            if (vd->ops->set_viewpoint(vd, &osys->cfg.viewpoint)) {
                msg_Err(vd, "Failed to change Viewpoint");
                osys->cfg.viewpoint = old_vp;
            }
        }
    }
}

void vout_SetDisplayIccProfile(vout_display_t *vd,
                               const vlc_icc_profile_t *profile)
{
    vout_display_priv_t *osys = container_of(vd, vout_display_priv_t, display);

    osys->cfg.icc_profile = (vlc_icc_profile_t *) profile;
    if (vd->ops->set_icc_profile)
        vd->ops->set_icc_profile(vd, profile);
}

int vout_SetDisplayFormat(vout_display_t *vd, const video_format_t *fmt,
                          vlc_video_context *vctx)
{
    if (!vd->ops->update_format)
        return VLC_EGENERIC;

    int ret = vd->ops->update_format(vd, fmt, vctx);
    if (ret != VLC_SUCCESS)
        return ret;

    vout_display_priv_t *osys = container_of(vd, vout_display_priv_t, display);

    /* Update source format */
    assert(!fmt->p_palette);
    video_format_Clean(&osys->source);
    osys->source = *fmt;
    if (vctx)
        vlc_video_context_Hold(vctx);
    if (osys->src_vctx)
        vlc_video_context_Release(osys->src_vctx);
    osys->src_vctx = vctx;

    /* On update_format success, the vout display accepts the target format, so
     * no display converters are needed. */
    if (osys->converter != NULL)
    {
        VoutConverterRelease(osys->converter);
        osys->converter = NULL;
    }

    return VLC_SUCCESS;
}

vout_display_t *vout_display_New(vlc_object_t *parent,
                                 const video_format_t *source,
                                 vlc_video_context *vctx,
                                 const vout_display_cfg_t *cfg,
                                 const char *module,
                                 const vout_display_owner_t *owner)
{
    vout_display_priv_t *osys = vlc_custom_create(parent, sizeof (*osys),
                                                  "vout display");
    if (unlikely(osys == NULL))
        return NULL;

    osys->cfg = *cfg;

    if (cfg->display.width == 0 || cfg->display.height == 0) {
        /* Work around buggy window provider */
        msg_Warn(parent, "window size missing");
        vout_display_GetDefaultDisplaySize(&osys->cfg.display.width,
                                           &osys->cfg.display.height,
                                           source, &cfg->display);
    }

    osys->converter = NULL;

    video_format_Copy(&osys->source, source);
    osys->crop.mode = VOUT_CROP_NONE;

    osys->src_vctx = vctx ? vlc_video_context_Hold( vctx ) : NULL;

    /* */
    vout_display_t *vd = &osys->display;
    vd->source = &osys->source;
    vd->fmt = &osys->display_fmt;
    vd->info = (vout_display_info_t){ 0 };
    vd->cfg = &osys->cfg;
    vd->ops = NULL;
    vd->sys = NULL;
    if (owner)
        vd->owner = *owner;

    PlaceVideoInDisplay(osys);

    if (module == NULL || module[0] == '\0')
        module = "any";

    module_t **mods;
    size_t strict;
    ssize_t n = vlc_module_match("vout display", module, true, &mods, &strict);

    msg_Dbg(vd, "looking for %s module matching \"%s\": %zd candidates",
            "vout display", module, n);

    for (ssize_t i = 0; i < n; i++) {
        vout_display_open_cb cb = vlc_module_map(vlc_object_logger(vd),
                                                 mods[i]);
        if (cb == NULL)
            continue;

        /* Picture buffer does not have the concept of aspect ratio */
        video_format_Copy(&osys->display_fmt, vd->source);
        vd->obj.force = i < (ssize_t)strict; /* TODO: pass to cb() instead? */

        int ret = cb(vd, &osys->display_fmt, vctx);
        if (ret == VLC_SUCCESS) {
            assert(vd->ops->prepare != NULL || vd->ops->display != NULL);
            if (VoutDisplayCreateRender(vd) == 0) {
                msg_Dbg(vd, "using %s module \"%s\"", "vout display",
                        module_get_object(mods[i]));
                free(mods);
                return vd;
            }

            if (vd->ops->close != NULL)
                vd->ops->close(vd);
        }

        vlc_objres_clear(VLC_OBJECT(vd));
        video_format_Clean(&osys->display_fmt);
        osys->display.info = (vout_display_info_t){};
    }

    msg_Dbg(vd, "no %s modules matched with name %s", "vout display", module);
    free(mods);
    video_format_Clean(&osys->source);
    vlc_object_delete(vd);
    return NULL;
}

void vout_display_Delete(vout_display_t *vd)
{
    vout_display_priv_t *osys = container_of(vd, vout_display_priv_t, display);

    if (osys->src_vctx)
    {
        vlc_video_context_Release( osys->src_vctx );
        osys->src_vctx = NULL;
    }

    if (osys->converter != NULL)
    {
        VoutConverterRelease(osys->converter);
    }

    if (vd->ops->close != NULL)
        vd->ops->close(vd);
    vlc_objres_clear(VLC_OBJECT(vd));

    video_format_Clean(&osys->source);
    video_format_Clean(&osys->display_fmt);
    vlc_object_delete(vd);
}
