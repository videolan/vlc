/**
 * @file display.c
 * @brief Vulkan video output module
 */
/*****************************************************************************
 * Copyright © 2018 Niklas Haas
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

#include <stdlib.h>
#include <assert.h>

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_vout_display.h>

#include "../placebo_utils.h"
#include "instance.h"

#include <libplacebo/renderer.h>
#include <libplacebo/utils/upload.h>
#include <libplacebo/swapchain.h>
#include <libplacebo/vulkan.h>

struct vout_display_sys_t
{
    vlc_vk_t *vk;
    const struct pl_tex *plane_tex[4];
    struct pl_renderer *renderer;

    // Pool of textures for the subpictures
    struct pl_overlay *overlays;
    const struct pl_tex **overlay_tex;
    int num_overlays;

    // Dynamic during rendering
    vout_display_place_t place;
    uint64_t counter;

    // Storage for rendering parameters
    struct pl_filter_config upscaler;
    struct pl_filter_config downscaler;
    struct pl_deband_params deband;
    struct pl_sigmoid_params sigmoid;
    struct pl_color_map_params color_map;
    struct pl_dither_params dither;
    struct pl_render_params params;
    struct pl_color_space target;
#if PL_API_VER >= 13
    struct pl_peak_detect_params peak_detect;
#endif
    enum pl_chroma_location yuv_chroma_loc;
    int dither_depth;
};

// Display callbacks
static void PictureRender(vout_display_t *, picture_t *, subpicture_t *, mtime_t);
static void PictureDisplay(vout_display_t *, picture_t *);
static int Control(vout_display_t *, int, va_list);
static void Close(vout_display_t *);
static void UpdateParams(vout_display_t *);

// Allocates a Vulkan surface and instance for video output.
static int Open(vout_display_t *vd, const vout_display_cfg_t *cfg,
                video_format_t *fmt, vlc_video_context *context)
{
    vout_display_sys_t *sys = vd->sys =
        vlc_obj_calloc(VLC_OBJECT(vd), 1, sizeof (*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;

    vout_window_t *window = vd->cfg->window;
    if (window == NULL)
    {
        msg_Err(vd, "parent window not available");
        goto error;
    }

    sys->vk = vlc_vk_Create(window, NULL);
    if (sys->vk == NULL)
        goto error;

    const struct pl_gpu *gpu = sys->vk->vulkan->gpu;
    sys->renderer = pl_renderer_create(sys->vk->ctx, gpu);
    if (!sys->renderer)
        goto error;

    // Attempt using the input format as the display format
    if (vlc_placebo_FormatSupported(gpu, vd->source.i_chroma)) {
        fmt->i_chroma = vd->source.i_chroma;
    } else {
        const vlc_fourcc_t *fcc;
        for (fcc = vlc_fourcc_GetFallback(vd->source.i_chroma); *fcc; fcc++) {
            if (vlc_placebo_FormatSupported(gpu, *fcc)) {
                fmt->i_chroma = *fcc;
                break;
            }
        }

        if (!fmt->i_chroma) {
            fmt->i_chroma = VLC_CODEC_RGBA;
            msg_Warn(vd, "Failed picking any suitable input format, falling "
                     "back to RGBA for sanity!");
        }
    }
    sys->yuv_chroma_loc = vlc_fourcc_IsYUV(fmt->i_chroma) ?
                          vlc_placebo_ChromaLoc(fmt) : PL_CHROMA_UNKNOWN;

    // Hard-coded list of supported subtitle chromas (non-planar only!)
    static const vlc_fourcc_t subfmts[] = {
        VLC_CODEC_RGBA,
        VLC_CODEC_BGRA,
        VLC_CODEC_RGB8,
        VLC_CODEC_RGB12,
        VLC_CODEC_RGB15,
        VLC_CODEC_RGB16,
        VLC_CODEC_RGB24,
        VLC_CODEC_RGB32,
        VLC_CODEC_GREY,
        0
    };

    vd->info.subpicture_chromas = subfmts;

    vd->prepare = PictureRender;
    vd->display = PictureDisplay;
    vd->control = Control;
    vd->close = Close;

    UpdateParams(vd);
    (void) cfg; (void) context;
    return VLC_SUCCESS;

error:
    pl_renderer_destroy(&sys->renderer);
    if (sys->vk != NULL)
        vlc_vk_Release(sys->vk);
    return VLC_EGENERIC;
}

static void Close(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;
    const struct pl_gpu *gpu = sys->vk->vulkan->gpu;

    for (int i = 0; i < 4; i++)
        pl_tex_destroy(gpu, &sys->plane_tex[i]);
    for (int i = 0; i < sys->num_overlays; i++)
        pl_tex_destroy(gpu, &sys->overlay_tex[i]);

    if (sys->overlays) {
        free(sys->overlays);
        free(sys->overlay_tex);
    }

    pl_renderer_destroy(&sys->renderer);

    vlc_vk_Release(sys->vk);
}

static void PictureRender(vout_display_t *vd, picture_t *pic,
                          subpicture_t *subpicture, mtime_t date)
{
    VLC_UNUSED(date);
    vout_display_sys_t *sys = vd->sys;
    const struct pl_gpu *gpu = sys->vk->vulkan->gpu;
    bool failed = false;

    struct pl_swapchain_frame frame;
    if (!pl_swapchain_start_frame(sys->vk->swapchain, &frame))
        return; // Probably benign error, ignore it

    struct pl_image img = {
        .signature  = sys->counter++,
        .num_planes = pic->i_planes,
        .width      = pic->format.i_visible_width,
        .height     = pic->format.i_visible_height,
        .color      = vlc_placebo_ColorSpace(&vd->source),
        .repr       = vlc_placebo_ColorRepr(&vd->source),
        .src_rect = {
            .x0 = pic->format.i_x_offset,
            .y0 = pic->format.i_y_offset,
            .x1 = pic->format.i_x_offset + pic->format.i_visible_width,
            .y1 = pic->format.i_y_offset + pic->format.i_visible_height,
        },
    };

    // Upload the image data for each plane
    struct pl_plane_data data[4];
    if (!vlc_placebo_PlaneData(pic, data, NULL)) {
        // This should never happen, in theory
        assert(!"Failed processing the picture_t into pl_plane_data!?");
    }

    for (int i = 0; i < pic->i_planes; i++) {
        struct pl_plane *plane = &img.planes[i];
        if (!pl_upload_plane(gpu, plane, &sys->plane_tex[i], &data[i])) {
            msg_Err(vd, "Failed uploading image data!");
            failed = true;
            goto done;
        }

        // Matches only the chroma planes, never luma or alpha
        if (sys->yuv_chroma_loc != PL_CHROMA_UNKNOWN && i != 0 && i != 3)
            pl_chroma_location_offset(sys->yuv_chroma_loc, &plane->shift_x,
                                      &plane->shift_y);
    }

    struct pl_render_target target;
    pl_render_target_from_swapchain(&target, &frame);
    target.dst_rect.x0 = sys->place.x;
    target.dst_rect.y0 = sys->place.y;
    target.dst_rect.x1 = sys->place.x + sys->place.width;
    target.dst_rect.y1 = sys->place.y + sys->place.height;

    // Override the target colorimetry only if the user requests it
    if (sys->target.primaries)
        target.color.primaries = sys->target.primaries;
    if (sys->target.transfer) {
        target.color.transfer = sys->target.transfer;
        target.color.light = PL_COLOR_LIGHT_UNKNOWN; // re-infer
    }
    if (sys->target.sig_avg > 0.0)
        target.color.sig_avg = sys->target.sig_avg;
    if (sys->dither_depth > 0) {
        // override the sample depth without affecting the color encoding
        struct pl_bit_encoding *bits = &target.repr.bits;
        float scale = bits->color_depth / bits->sample_depth;
        bits->sample_depth = sys->dither_depth;
        bits->color_depth = scale * sys->dither_depth;
    }

    if (subpicture) {
        int num_regions = 0;
        for (subpicture_region_t *r = subpicture->p_region; r; r = r->p_next)
            num_regions++;

        // Grow the overlays array if needed
        if (num_regions > sys->num_overlays) {
            sys->overlays = realloc(sys->overlays, num_regions * sizeof(struct pl_overlay));
            sys->overlay_tex = realloc(sys->overlay_tex, num_regions * sizeof(struct pl_tex *));
            if (!sys->overlays || !sys->overlay_tex) {
                // Unlikely OOM, just do whatever
                sys->num_overlays = 0;
                failed = true;
                goto done;
            }
            // Clear the newly added texture pointers for pl_upload_plane
            for (int i = sys->num_overlays; i < num_regions; i++)
                sys->overlay_tex[i] = NULL;
            sys->num_overlays = num_regions;
        }

        // Upload all of the regions
        subpicture_region_t *r = subpicture->p_region;
        for (int i = 0; i < num_regions; i++) {
            assert(r->p_picture->i_planes == 1);
            struct pl_plane_data subdata;
            if (!vlc_placebo_PlaneData(r->p_picture, &subdata, NULL))
                assert(!"Failed processing the subpicture_t into pl_plane_data!?");

            struct pl_overlay *overlay = &sys->overlays[i];
            *overlay = (struct pl_overlay) {
                .rect = {
                    .x0 = target.dst_rect.x0 + r->i_x,
                    .y0 = target.dst_rect.y0 + r->i_y,
                    .x1 = target.dst_rect.x0 + r->i_x + r->fmt.i_visible_width,
                    .y1 = target.dst_rect.y0 + r->i_y + r->fmt.i_visible_height,
                },
                .mode = PL_OVERLAY_NORMAL,
                .color = vlc_placebo_ColorSpace(&r->fmt),
                .repr  = vlc_placebo_ColorRepr(&r->fmt),
            };

            if (!pl_upload_plane(gpu, &overlay->plane, &sys->overlay_tex[i], &subdata)) {
                msg_Err(vd, "Failed uploading subpicture region!");
                num_regions = i; // stop here
                break;
            }
        }

        // Update the target information to reference the subpictures
        target.overlays = sys->overlays;
        target.num_overlays = num_regions;
    }

    // If we don't cover the entire output, clear it first
    struct pl_rect2d full = {0, 0, frame.fbo->params.w, frame.fbo->params.h };
    if (!pl_rect2d_eq(target.dst_rect, full)) {
        // TODO: make background color configurable?
        pl_tex_clear(gpu, frame.fbo, (float[4]){ 0.0, 0.0, 0.0, 0.0 });
    }

    // Dispatch the actual image rendering with the pre-configured parameters
    if (!pl_render_image(sys->renderer, &img, &target, &sys->params)) {
        msg_Err(vd, "Failed rendering frame!");
        failed = true;
        goto done;
    }

done:

    if (failed)
        pl_tex_clear(gpu, frame.fbo, (float[4]){ 1.0, 0.0, 0.0, 1.0 });

    if (!pl_swapchain_submit_frame(sys->vk->swapchain)) {
        msg_Err(vd, "Failed rendering frame!");
        return;
    }
}

static void PictureDisplay(vout_display_t *vd, picture_t *pic)
{
    VLC_UNUSED(pic);
    vout_display_sys_t *sys = vd->sys;
    pl_swapchain_swap_buffers(sys->vk->swapchain);
}

static int Control(vout_display_t *vd, int query, va_list ap)
{
    vout_display_sys_t *sys = vd->sys;

    switch (query)
    {
    case VOUT_DISPLAY_RESET_PICTURES:
        assert(!"VOUT_DISPLAY_RESET_PICTURES");

    case VOUT_DISPLAY_CHANGE_DISPLAY_SIZE:
    case VOUT_DISPLAY_CHANGE_DISPLAY_FILLED:
    case VOUT_DISPLAY_CHANGE_SOURCE_ASPECT:
    case VOUT_DISPLAY_CHANGE_SOURCE_CROP:
    case VOUT_DISPLAY_CHANGE_ZOOM: {
        vout_display_cfg_t cfg = *va_arg (ap, const vout_display_cfg_t *);
        vout_display_PlacePicture(&sys->place, &vd->source, &cfg);
        return VLC_SUCCESS;
    }

    default:
        msg_Err (vd, "Unknown request %d", query);
    }

    return VLC_EGENERIC;
}

// Options

#define VK_TEXT N_("Vulkan surface extension")
#define PROVIDER_LONGTEXT N_( \
    "Extension which provides the Vulkan surface to use.")

#define DISABLE_DR_TEXT "Disable direct rendering / zero-copy upload"
#define DISABLE_DR_LONGTEXT "Direct rendering is a technique where image data is uploaded via a mapped buffer instead of via memcpy. On some platforms this might be very slow (due to poor readback performance from mapped memory), in which cases this flag would help."

vlc_module_begin ()
    set_shortname ("Vulkan")
    set_description (N_("Vulkan video output"))
    set_category (CAT_VIDEO)
    set_subcategory (SUBCAT_VIDEO_VOUT)
    set_callback_display(Open, 0)
    add_shortcut ("vulkan", "vk")
    add_module ("vk", "vulkan", NULL, VK_TEXT, PROVIDER_LONGTEXT)

    set_section("Scaling", NULL)
    add_integer("upscaler-preset", SCALE_BUILTIN,
            UPSCALER_PRESET_TEXT, SCALER_PRESET_LONGTEXT, false)
            change_integer_list(scale_values, scale_text)
    add_integer("downscaler-preset", SCALE_BUILTIN,
            DOWNSCALER_PRESET_TEXT, SCALER_PRESET_LONGTEXT, false)
            change_integer_list(scale_values, scale_text)
    add_integer_with_range("lut-entries", 64, 16, 256,
            LUT_ENTRIES_TEXT, LUT_ENTRIES_LONGTEXT, false)
    add_float_with_range("antiringing", 0.0,
            0.0, 1.0, ANTIRING_TEXT, ANTIRING_LONGTEXT, false)
    add_bool("sigmoid", !!pl_render_default_params.sigmoid_params,
            SIGMOID_TEXT, SIGMOID_LONGTEXT, true)
    add_float_with_range("sigmoid-center", pl_sigmoid_default_params.center,
            0., 1., SIGMOID_CENTER_TEXT, SIGMOID_CENTER_LONGTEXT, true)
    add_float_with_range("sigmoid-slope", pl_sigmoid_default_params.slope,
            1., 20., SIGMOID_SLOPE_TEXT, SIGMOID_SLOPE_LONGTEXT, true)

    set_section("Debanding", NULL)
    add_bool("debanding", false, DEBAND_TEXT, DEBAND_LONGTEXT, false)
    add_integer("iterations", pl_deband_default_params.iterations,
            DEBAND_ITER_TEXT, DEBAND_ITER_LONGTEXT, false)
    add_float("threshold", pl_deband_default_params.threshold,
            DEBAND_THRESH_TEXT, DEBAND_THRESH_LONGTEXT, false)
    add_float("radius", pl_deband_default_params.radius,
            DEBAND_RADIUS_TEXT, DEBAND_RADIUS_LONGTEXT, false)
    add_float("grain", pl_deband_default_params.grain,
            DEBAND_GRAIN_TEXT, DEBAND_GRAIN_LONGTEXT, false)

    set_section("Colorspace conversion", NULL)
    add_integer("intent", pl_color_map_default_params.intent,
            RENDER_INTENT_TEXT, RENDER_INTENT_LONGTEXT, false)
            change_integer_list(intent_values, intent_text)
    add_integer("target-prim", PL_COLOR_PRIM_UNKNOWN, PRIM_TEXT, PRIM_LONGTEXT, false) \
            change_integer_list(prim_values, prim_text) \
    add_integer("target-trc", PL_COLOR_TRC_UNKNOWN, TRC_TEXT, TRC_LONGTEXT, false) \
            change_integer_list(trc_values, trc_text) \

    // TODO: support for ICC profiles / 3DLUTs.. we will need some way of loading
    // this from the operating system / user

    set_section("Tone mapping", NULL)
    add_integer("tone-mapping", pl_color_map_default_params.tone_mapping_algo,
            TONEMAPPING_TEXT, TONEMAPPING_LONGTEXT, false)
            change_integer_list(tone_values, tone_text)
    add_float("tone-mapping-param", pl_color_map_default_params.tone_mapping_param,
            TONEMAP_PARAM_TEXT, TONEMAP_PARAM_LONGTEXT, true)
#if PL_API_VER >= 10
    add_float("desat-strength", pl_color_map_default_params.desaturation_strength,
            DESAT_STRENGTH_TEXT, DESAT_STRENGTH_LONGTEXT, false)
    add_float("desat-exponent", pl_color_map_default_params.desaturation_exponent,
            DESAT_EXPONENT_TEXT, DESAT_EXPONENT_LONGTEXT, false)
    add_float("desat-base", pl_color_map_default_params.desaturation_base,
            DESAT_BASE_TEXT, DESAT_BASE_LONGTEXT, false)
    add_float("max-boost", pl_color_map_default_params.max_boost,
            MAX_BOOST_TEXT, MAX_BOOST_LONGTEXT, false)
#else
    add_float("tone-mapping-desat", pl_color_map_default_params.tone_mapping_desaturate,
            TONEMAP_DESAT_TEXT, TONEMAP_DESAT_LONGTEXT, false)
#endif
    add_bool("gamut-warning", false, GAMUT_WARN_TEXT, GAMUT_WARN_LONGTEXT, true)

#if PL_API_VER < 12
    add_integer_with_range("peak-frames", pl_color_map_default_params.peak_detect_frames,
            0, 255, PEAK_FRAMES_TEXT, PEAK_FRAMES_LONGTEXT, false)
    add_float_with_range("scene-threshold", pl_color_map_default_params.scene_threshold,
            0., 10., SCENE_THRESHOLD_TEXT, SCENE_THRESHOLD_LONGTEXT, false)
#endif

#if PL_API_VER >= 13
    add_float_with_range("peak-period", pl_peak_detect_default_params.smoothing_period,
            0., 1000., PEAK_PERIOD_TEXT, PEAK_PERIOD_LONGTEXT, false)
    add_float("scene-threshold-low", pl_peak_detect_default_params.scene_threshold_low,
            SCENE_THRESHOLD_LOW_TEXT, SCENE_THRESHOLD_LOW_LONGTEXT, false)
    add_float("scene-threshold-high", pl_peak_detect_default_params.scene_threshold_high,
            SCENE_THRESHOLD_HIGH_TEXT, SCENE_THRESHOLD_HIGH_LONGTEXT, false)
#endif

    add_float_with_range("target-avg", 0.25,
            0.0, 1.0, TARGET_AVG_TEXT, TARGET_AVG_LONGTEXT, false)

    set_section("Dithering", NULL)
    add_integer("dither", -1,
            DITHER_TEXT, DITHER_LONGTEXT, false)
            change_integer_list(dither_values, dither_text)
    add_integer_with_range("dither-size", pl_dither_default_params.lut_size,
            1, 8, DITHER_SIZE_TEXT, DITHER_SIZE_LONGTEXT, false)
    add_bool("temporal-dither", pl_dither_default_params.temporal,
            TEMPORAL_DITHER_TEXT, TEMPORAL_DITHER_LONGTEXT, false)
    add_integer_with_range("dither-depth", 0,
            0, 16, DITHER_DEPTH_TEXT, DITHER_DEPTH_LONGTEXT, false)

    set_section("Custom upscaler (when preset = custom)", NULL)
    add_integer("upscaler-kernel", FILTER_BOX,
            KERNEL_TEXT, KERNEL_LONGTEXT, true)
            change_integer_list(filter_values, filter_text)
    add_integer("upscaler-window", FILTER_NONE,
            WINDOW_TEXT, WINDOW_LONGTEXT, true)
            change_integer_list(filter_values, filter_text)
    add_bool("upscaler-polar", false, POLAR_TEXT, POLAR_LONGTEXT, true)
    add_float_with_range("upscaler-clamp", 0.0,
            0.0, 1.0, CLAMP_TEXT, CLAMP_LONGTEXT, true)
    add_float_with_range("upscaler-blur", 1.0,
            0.0, 100.0, BLUR_TEXT, BLUR_LONGTEXT, true)
    add_float_with_range("upscaler-taper", 0.0,
            0.0, 10.0, TAPER_TEXT, TAPER_LONGTEXT, true)

    set_section("Custom downscaler (when preset = custom)", NULL)
    add_integer("downscaler-kernel", FILTER_BOX,
            KERNEL_TEXT, KERNEL_LONGTEXT, true)
            change_integer_list(filter_values, filter_text)
    add_integer("downscaler-window", FILTER_NONE,
            WINDOW_TEXT, WINDOW_LONGTEXT, true)
            change_integer_list(filter_values, filter_text)
    add_bool("downscaler-polar", false, POLAR_TEXT, POLAR_LONGTEXT, true)
    add_float_with_range("downscaler-clamp", 0.0,
            0.0, 1.0, CLAMP_TEXT, CLAMP_LONGTEXT, true)
    add_float_with_range("downscaler-blur", 1.0,
            0.0, 100.0, BLUR_TEXT, BLUR_LONGTEXT, true)
    add_float_with_range("downscaler-taper", 0.0,
            0.0, 10.0, TAPER_TEXT, TAPER_LONGTEXT, true)

    set_section("Performance tweaks / debugging", NULL)
    add_bool("skip-aa", false, SKIP_AA_TEXT, SKIP_AA_LONGTEXT, false)
    add_float_with_range("polar-cutoff", 0.001,
            0., 1., POLAR_CUTOFF_TEXT, POLAR_CUTOFF_LONGTEXT, false)
    add_bool("overlay-direct", false, OVERLAY_DIRECT_TEXT, OVERLAY_DIRECT_LONGTEXT, false)
    add_bool("disable-linear", false, DISABLE_LINEAR_TEXT, DISABLE_LINEAR_LONGTEXT, false)
    add_bool("force-general", false, FORCE_GENERAL_TEXT, FORCE_GENERAL_LONGTEXT, false)
#if PL_API_VER >= 13
    add_bool("delayed-peak", false, DELAYED_PEAK_TEXT, DELAYED_PEAK_LONGTEXT, false)
#endif

vlc_module_end ()

// Update the renderer settings based on the current configuration.
//
// XXX: This could be called every time the parameters change, but currently
// VLC does not allow that - so we're stuck with doing it once on Open().
// Should be changed as soon as it's possible!
static void UpdateParams(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;

    sys->deband = pl_deband_default_params;
    sys->deband.iterations = var_InheritInteger(vd, "iterations");
    sys->deband.threshold = var_InheritFloat(vd, "threshold");
    sys->deband.radius = var_InheritFloat(vd, "radius");
    sys->deband.grain = var_InheritFloat(vd, "grain");
    bool use_deband = sys->deband.iterations > 0 || sys->deband.grain > 0;
    use_deband &= var_InheritBool(vd, "debanding");

    sys->sigmoid = pl_sigmoid_default_params;
    sys->sigmoid.center = var_InheritFloat(vd, "sigmoid-center");
    sys->sigmoid.slope = var_InheritFloat(vd, "sigmoid-slope");
    bool use_sigmoid = var_InheritBool(vd, "sigmoid");

    sys->color_map = pl_color_map_default_params;
    sys->color_map.intent = var_InheritInteger(vd, "intent");
    sys->color_map.tone_mapping_algo = var_InheritInteger(vd, "tone-mapping");
    sys->color_map.tone_mapping_param = var_InheritFloat(vd, "tone-mapping-param");
#if PL_API_VER >= 10
    sys->color_map.desaturation_strength = var_InheritFloat(vd, "desat-strength");
    sys->color_map.desaturation_exponent = var_InheritFloat(vd, "desat-exponent");
    sys->color_map.desaturation_base = var_InheritFloat(vd, "desat-base");
    sys->color_map.max_boost = var_InheritFloat(vd, "max-boost");
#else
    sys->color_map.tone_mapping_desaturate = var_InheritFloat(vd, "tone-mapping-desat");
#endif
    sys->color_map.gamut_warning = var_InheritBool(vd, "gamut-warning");
#if PL_API_VER < 12
    sys->color_map.peak_detect_frames = var_InheritInteger(vd, "peak-frames");
    sys->color_map.scene_threshold = var_InheritFloat(vd, "scene-threshold");
#endif

    sys->dither = pl_dither_default_params;
    int method = var_InheritInteger(vd, "dither");
    bool use_dither = method >= 0;
    sys->dither.method = use_dither ? method : 0;
    sys->dither.lut_size = var_InheritInteger(vd, "dither-size");
    sys->dither.temporal = var_InheritBool(vd, "temporal-dither");

    sys->params = pl_render_default_params;
    sys->params.deband_params = use_deband ? &sys->deband : NULL;
    sys->params.sigmoid_params = use_sigmoid ? &sys->sigmoid : NULL;
    sys->params.color_map_params = &sys->color_map;
    sys->params.dither_params = use_dither ? &sys->dither : NULL;
    sys->params.lut_entries = var_InheritInteger(vd, "lut-entries");
    sys->params.antiringing_strength = var_InheritFloat(vd, "antiringing");
    sys->params.skip_anti_aliasing = var_InheritBool(vd, "skip-aa");
    sys->params.polar_cutoff = var_InheritFloat(vd, "polar-cutoff");
    sys->params.disable_overlay_sampling = var_InheritBool(vd, "overlay-direct");
    sys->params.disable_linear_scaling = var_InheritBool(vd, "disable-linear");
    sys->params.disable_builtin_scalers = var_InheritBool(vd, "force-general");

#if PL_API_VER >= 13
    sys->peak_detect.smoothing_period = var_InheritFloat(vd, "peak-period");
    sys->peak_detect.scene_threshold_low = var_InheritFloat(vd, "scene-threshold-low");
    sys->peak_detect.scene_threshold_high = var_InheritFloat(vd, "scene-threshold-high");
    if (sys->peak_detect.smoothing_period > 0.0) {
        sys->params.peak_detect_params = &sys->peak_detect;
        sys->params.allow_delayed_peak_detect = var_InheritBool(vd, "delayed-peak");
    }
#endif

    int preset = var_InheritInteger(vd, "upscaler-preset");
    sys->params.upscaler = scale_config[preset];
    if (preset == SCALE_CUSTOM) {
        sys->params.upscaler = &sys->upscaler;
        sys->upscaler = (struct pl_filter_config) {
            .kernel = filter_fun[var_InheritInteger(vd, "upscaler-kernel")],
            .window = filter_fun[var_InheritInteger(vd, "upscaler-window")],
            .clamp  = var_InheritFloat(vd, "upscaler-clamp"),
            .blur   = var_InheritFloat(vd, "upscaler-blur"),
            .taper  = var_InheritFloat(vd, "upscaler-taper"),
            .polar  = var_InheritBool(vd, "upscaler-polar"),
        };

        if (!sys->upscaler.kernel) {
            msg_Err(vd, "Tried specifying a custom upscaler with no kernel!");
            sys->params.upscaler = NULL;
        }
    };

    preset = var_InheritInteger(vd, "downscaler-preset");
    sys->params.downscaler = scale_config[preset];
    if (preset == SCALE_CUSTOM) {
        sys->params.downscaler = &sys->downscaler;
        sys->downscaler = (struct pl_filter_config) {
            .kernel = filter_fun[var_InheritInteger(vd, "downscaler-kernel")],
            .window = filter_fun[var_InheritInteger(vd, "downscaler-window")],
            .clamp  = var_InheritFloat(vd, "downscaler-clamp"),
            .blur   = var_InheritFloat(vd, "downscaler-blur"),
            .taper  = var_InheritFloat(vd, "downscaler-taper"),
            .polar  = var_InheritBool(vd, "downscaler-polar"),
        };

        if (!sys->downscaler.kernel) {
            msg_Err(vd, "Tried specifying a custom downscaler with no kernel!");
            sys->params.downscaler = NULL;
        }
    };

    sys->dither_depth = var_InheritInteger(vd, "dither-depth");
    sys->target = (struct pl_color_space) {
        .primaries = var_InheritInteger(vd, "target-prim"),
        .transfer = var_InheritInteger(vd, "target-trc"),
        .sig_avg = var_InheritFloat(vd, "target-avg"),
    };
}
