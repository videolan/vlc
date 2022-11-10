/*****************************************************************************
 * ARIB STD-B24 caption decoder/renderer using libaribcaption.
 *****************************************************************************
 * Copyright (C) 2022 magicxqq
 *
 * Authors:  magicxqq <xqq@xqq.im>
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
#   include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <limits.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_atomic.h>
#include <vlc_codec.h>

#include <aribcaption/aribcaption.h>


/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
typedef struct
{
    /* The following fields of decoder_sys_t are shared between decoder and spu units */
    vlc_atomic_rc_t    rc;

    int                i_cfg_rendering_backend;
    char*              psz_cfg_font_name;
    bool               b_cfg_replace_drcs;
    bool               b_cfg_force_stroke_text;
    bool               b_cfg_ignore_background;
    bool               b_cfg_ignore_ruby;
    bool               b_cfg_fadeout;
    float              f_cfg_stroke_width;

    aribcc_context_t  *p_context;
    aribcc_decoder_t  *p_decoder;
    aribcc_renderer_t *p_renderer;

    vlc_mutex_t        dec_lock;    // protect p_dec pointer
    decoder_t*         p_dec;       // pointer to decoder_t for logcat callback, NULL if decoder closed
} decoder_sys_t;

typedef struct
{
    decoder_sys_t         *p_dec_sys;
    vlc_tick_t             i_pts;
    unsigned               i_render_area_width;
    unsigned               i_render_area_height;
    aribcc_render_result_t render_result;
} libaribcaption_spu_updater_sys_t;


static void DecSysRetain(decoder_sys_t *p_sys)
{
    vlc_atomic_rc_inc(&p_sys->rc);
}

static void DecSysRelease(decoder_sys_t *p_sys)
{
    if (!vlc_atomic_rc_dec(&p_sys->rc))
        return;

    if (p_sys->p_renderer)
        aribcc_renderer_free(p_sys->p_renderer);
    if (p_sys->p_decoder)
        aribcc_decoder_free(p_sys->p_decoder);
    if (p_sys->p_context)
        aribcc_context_free(p_sys->p_context);

    free(p_sys);
}


/****************************************************************************
 *
 ****************************************************************************/
static int SubpictureValidate(subpicture_t *p_subpic,
                              bool b_src_changed, const video_format_t *p_src_format,
                              bool b_dst_changed, const video_format_t *p_dst_format,
                              vlc_tick_t i_ts)
{
    libaribcaption_spu_updater_sys_t *p_spusys = p_subpic->updater.p_sys;
    decoder_sys_t *p_sys = p_spusys->p_dec_sys;

    if (b_src_changed || b_dst_changed) {
        const video_format_t *fmt = p_dst_format;
        /* don't let library freely scale using either the min of width or height ratio */
        p_spusys->i_render_area_width = fmt->i_visible_width;
        p_spusys->i_render_area_height = p_src_format->i_visible_height * fmt->i_visible_width /
                                         p_src_format->i_visible_width;
        aribcc_renderer_set_frame_size(p_sys->p_renderer, p_spusys->i_render_area_width,
                                                          p_spusys->i_render_area_height);
    }

    const vlc_tick_t i_stream_date = p_spusys->i_pts + (i_ts - p_subpic->i_start);

    aribcc_render_status_t status = aribcc_renderer_render(p_sys->p_renderer,
                                                           MS_FROM_VLC_TICK(i_stream_date),
                                                           &p_spusys->render_result);
    if (status == ARIBCC_RENDER_STATUS_ERROR) {
        return VLC_SUCCESS;
    }

    bool b_changed = (status != ARIBCC_RENDER_STATUS_GOT_IMAGE_UNCHANGED);

    if (!b_changed && !b_src_changed && !b_dst_changed &&
        (p_spusys->render_result.images != NULL) == (p_subpic->p_region != NULL)) {
        aribcc_render_result_cleanup(&p_spusys->render_result);
        return VLC_SUCCESS;
    }

    return VLC_EGENERIC;
}

static void CopyImageToRegion(subpicture_region_t *p_region, const aribcc_image_t *image)
{
    if(image->pixel_format != ARIBCC_PIXELFORMAT_RGBA8888)
        return;

    plane_t *p_dstplane = &p_region->p_picture->p[0];
    plane_t srcplane;
    srcplane.i_lines = image->height;
    srcplane.i_pitch = image->stride;
    srcplane.i_pixel_pitch = p_dstplane->i_pixel_pitch;
    srcplane.i_visible_lines = image->height;
    srcplane.i_visible_pitch = image->width /* in pixels */ * p_dstplane->i_pixel_pitch;
    srcplane.p_pixels = image->bitmap;
    plane_CopyPixels( p_dstplane, &srcplane );
}

static void SubpictureUpdate(subpicture_t *p_subpic,
                             const video_format_t *p_src_format,
                             const video_format_t *p_dst_format,
                             vlc_tick_t i_ts)
{
    VLC_UNUSED(p_src_format); VLC_UNUSED(p_dst_format); VLC_UNUSED(i_ts);

    libaribcaption_spu_updater_sys_t *p_spusys = p_subpic->updater.p_sys;

    video_format_t  fmt = *p_dst_format;
    fmt.i_chroma         = VLC_CODEC_RGBA;
    fmt.i_bits_per_pixel = 0;
    fmt.i_x_offset       = 0;
    fmt.i_y_offset       = 0;

    aribcc_image_t *p_images = p_spusys->render_result.images;
    uint32_t        i_image_count = p_spusys->render_result.image_count;

    p_subpic->i_original_picture_width = p_spusys->i_render_area_width;
    p_subpic->i_original_picture_height = p_spusys->i_render_area_height;

    if (!p_images || i_image_count == 0) {
        return;
    }

    /* Allocate the regions and draw them */
    subpicture_region_t **pp_region_last = &p_subpic->p_region;

    for (uint32_t i = 0; i < i_image_count; i++) {
        aribcc_image_t *image = &p_images[i];
        video_format_t  fmt_region = fmt;

        fmt_region.i_width =
        fmt_region.i_visible_width  = image->width;
        fmt_region.i_height =
        fmt_region.i_visible_height = image->height;
        fmt_region.i_sar_num = 1;
        fmt_region.i_sar_den = 1;

        subpicture_region_t *region = subpicture_region_New(&fmt_region);
        if (!region)
            break;

        region->i_x = image->dst_x;
        region->i_y = image->dst_y;
        region->i_align = SUBPICTURE_ALIGN_TOP | SUBPICTURE_ALIGN_LEFT;

        CopyImageToRegion(region, image);

        *pp_region_last = region;
        pp_region_last = &region->p_next;
    }

    aribcc_render_result_cleanup(&p_spusys->render_result);
}

static void SubpictureDestroy(subpicture_t *p_subpic)
{
    libaribcaption_spu_updater_sys_t *p_spusys = p_subpic->updater.p_sys;
    DecSysRelease(p_spusys->p_dec_sys);
    free(p_spusys);
}

/****************************************************************************
 * Decode:
 ****************************************************************************/
static int Decode(decoder_t *p_dec, block_t *p_block)
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    if (p_block == NULL) /* No Drain */
        return VLCDEC_SUCCESS;

    if (p_block->i_flags & BLOCK_FLAG_CORRUPTED) {
        block_Release(p_block);
        return VLCDEC_SUCCESS;
    }

    aribcc_caption_t caption;
    aribcc_decode_status_t status = aribcc_decoder_decode(p_sys->p_decoder,
                                                          p_block->p_buffer,
                                                          p_block->i_buffer,
                                                          MS_FROM_VLC_TICK(p_block->i_pts),
                                                          &caption);
    if (status == ARIBCC_DECODE_STATUS_ERROR) {
        msg_Err(p_dec, "aribcc_decoder_decode() returned with error");
    }

    if (status == ARIBCC_DECODE_STATUS_ERROR || status == ARIBCC_DECODE_STATUS_NO_CAPTION) {
        block_Release(p_block);
        return VLCDEC_SUCCESS;
    }

    aribcc_renderer_append_caption(p_sys->p_renderer, &caption);
    aribcc_caption_cleanup(&caption);


    libaribcaption_spu_updater_sys_t *p_spusys = malloc(sizeof(libaribcaption_spu_updater_sys_t));
    if (!p_spusys) {
        block_Release(p_block);
        return VLCDEC_SUCCESS;
    }
    p_spusys->p_dec_sys = p_sys;
    p_spusys->i_pts = p_block->i_pts;
    memset(&p_spusys->render_result, 0, sizeof(p_spusys->render_result));

    subpicture_updater_t updater = {
        .pf_validate = SubpictureValidate,
        .pf_update   = SubpictureUpdate,
        .pf_destroy  = SubpictureDestroy,
        .p_sys       = p_spusys,
    };

    subpicture_t *p_spu = decoder_NewSubpicture(p_dec, &updater);
    if (!p_spu) {
        msg_Warn(p_dec, "can't get spu buffer");
        free(p_spusys);
        block_Release(p_block);
        return VLCDEC_SUCCESS;
    }
    p_spu->i_start = p_block->i_pts;
    p_spu->i_stop = p_block->i_pts;
    p_spu->b_absolute = true;
    p_spu->b_fade = p_sys->b_cfg_fadeout;

    if (caption.wait_duration == ARIBCC_DURATION_INDEFINITE) {
        p_spu->b_ephemer = true;
    } else {
        p_spu->i_stop = p_block->i_pts + VLC_TICK_FROM_MS(caption.wait_duration);
    }


    DecSysRetain(p_sys); /* Keep a reference for the returned subpicture */

    block_Release(p_block);

    decoder_QueueSub(p_dec, p_spu);

    return VLCDEC_SUCCESS;
}

/*****************************************************************************
 * Flush:
 *****************************************************************************/
static void Flush(decoder_t *p_dec)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    aribcc_decoder_flush(p_sys->p_decoder);
    aribcc_renderer_flush(p_sys->p_renderer);
}

/*****************************************************************************
 * libaribcaption logcat callback function
 *****************************************************************************/
static void LogcatCallback(aribcc_loglevel_t level, const char *message, void *userdata)
{
    decoder_sys_t *p_sys = userdata;

    if (p_sys->p_dec != NULL) {
        vlc_mutex_lock(&p_sys->dec_lock);

        if (p_sys->p_dec != NULL) {
            decoder_t* p_dec = p_sys->p_dec;

            if (level == ARIBCC_LOGLEVEL_ERROR) {
                msg_Err(p_dec, "%s", message);
            } else if (level == ARIBCC_LOGLEVEL_WARNING) {
                msg_Warn(p_dec, "%s", message);
            } else {
                msg_Dbg(p_dec, "%s", message);
            }
        }

        vlc_mutex_unlock(&p_sys->dec_lock);
    }
}

#define ARIBCAPTION_CFG_PREFIX "aribcaption-"

/*****************************************************************************
 * Open: Create libaribcaption context/decoder/renderer.
 *****************************************************************************/
static int Open(vlc_object_t *p_this)
{
    decoder_t     *p_dec = (decoder_t *)p_this;
    decoder_sys_t *p_sys;

    if (p_dec->p_fmt_in->i_codec != VLC_CODEC_ARIB_A &&
        p_dec->p_fmt_in->i_codec != VLC_CODEC_ARIB_C) {
        return VLC_ENOTSUP;
    }

    p_sys = malloc(sizeof(decoder_sys_t));
    if (unlikely(p_sys == NULL))
        return VLC_ENOMEM;

    vlc_atomic_rc_init(&p_sys->rc);

    p_sys->i_cfg_rendering_backend = var_InheritInteger(p_this, ARIBCAPTION_CFG_PREFIX "rendering-backend");
    p_sys->psz_cfg_font_name = var_InheritString(p_this, ARIBCAPTION_CFG_PREFIX "font");
    p_sys->b_cfg_replace_drcs = var_InheritBool(p_this, ARIBCAPTION_CFG_PREFIX "replace-drcs");
    p_sys->b_cfg_force_stroke_text = var_InheritBool(p_this, ARIBCAPTION_CFG_PREFIX "force-stroke-text");
    p_sys->b_cfg_ignore_background = var_InheritBool(p_this, ARIBCAPTION_CFG_PREFIX "ignore-background");
    p_sys->b_cfg_ignore_ruby = var_InheritBool(p_this, ARIBCAPTION_CFG_PREFIX "ignore-ruby");
    p_sys->b_cfg_fadeout = var_InheritBool(p_this, ARIBCAPTION_CFG_PREFIX "fadeout");
    p_sys->f_cfg_stroke_width = var_InheritFloat(p_this, ARIBCAPTION_CFG_PREFIX "stroke-width");

    vlc_mutex_init(&p_sys->dec_lock);
    p_sys->p_dec = p_dec;

    /* Create libaribcaption context */
    aribcc_context_t *p_ctx = p_sys->p_context = aribcc_context_alloc();
    if (!p_ctx) {
        msg_Err(p_dec, "libaribcaption context allocation failed");
        DecSysRelease(p_sys);
        return VLC_EGENERIC;
    }

    aribcc_context_set_logcat_callback(p_ctx, LogcatCallback, p_sys);


    /* Create the decoder */
    aribcc_decoder_t* p_decoder = p_sys->p_decoder = aribcc_decoder_alloc(p_ctx);
    if (!p_decoder) {
        msg_Err(p_dec, "libaribcaption decoder creation failed");
        DecSysRelease(p_sys);
        return VLC_EGENERIC;
    }

    aribcc_profile_t i_profile = ARIBCC_PROFILE_A;
    if (p_dec->p_fmt_in->i_codec == VLC_CODEC_ARIB_C) {
        i_profile = ARIBCC_PROFILE_C;
    }

    bool b_succ = aribcc_decoder_initialize(p_decoder,
                                            ARIBCC_ENCODING_SCHEME_AUTO,
                                            ARIBCC_CAPTIONTYPE_CAPTION,
                                            i_profile,
                                            ARIBCC_LANGUAGEID_FIRST);
    if (!b_succ) {
        msg_Err(p_dec, "libaribcaption decoder initialization failed");
        DecSysRelease(p_sys);
        return VLC_EGENERIC;
    }


    /* Create the renderer */
    aribcc_renderer_t* p_renderer = p_sys->p_renderer = aribcc_renderer_alloc(p_ctx);
    if (!p_renderer) {
        msg_Err(p_dec, "libaribcaption renderer creation failed");
        DecSysRelease(p_sys);
        return VLC_EGENERIC;
    }

    b_succ = aribcc_renderer_initialize(p_renderer,
                                        ARIBCC_CAPTIONTYPE_CAPTION,
                                        ARIBCC_FONTPROVIDER_TYPE_AUTO,
                                        (aribcc_textrenderer_type_t)p_sys->i_cfg_rendering_backend);
    if (!b_succ) {
        msg_Err(p_dec, "libaribcaption renderer initialization failed");
        DecSysRelease(p_sys);
        return VLC_EGENERIC;
    }
    aribcc_renderer_set_storage_policy(p_renderer, ARIBCC_CAPTION_STORAGE_POLICY_MINIMUM, 0);
    aribcc_renderer_set_replace_drcs(p_renderer, p_sys->b_cfg_replace_drcs);
    aribcc_renderer_set_force_stroke_text(p_renderer, p_sys->b_cfg_force_stroke_text);
    aribcc_renderer_set_force_no_background(p_renderer, p_sys->b_cfg_ignore_background);
    aribcc_renderer_set_force_no_ruby(p_renderer, p_sys->b_cfg_ignore_ruby);
    aribcc_renderer_set_stroke_width(p_renderer, p_sys->f_cfg_stroke_width);

    if (p_sys->psz_cfg_font_name && strlen(p_sys->psz_cfg_font_name) > 0) {
        const char* font_families[] = { p_sys->psz_cfg_font_name };
        aribcc_renderer_set_default_font_family(p_renderer, font_families, 1, true);
    }

    p_dec->p_sys = p_sys;
    p_dec->pf_decode = Decode;
    p_dec->pf_flush  = Flush;
    p_dec->fmt_out.i_codec = VLC_CODEC_RGBA;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: finish
 *****************************************************************************/
static void Close(vlc_object_t *p_this)
{
    decoder_t *p_dec = (decoder_t *)p_this;
    decoder_sys_t *p_sys = p_dec->p_sys;

    vlc_mutex_lock(&p_sys->dec_lock);
    p_sys->p_dec = NULL;
    vlc_mutex_unlock(&p_sys->dec_lock);

    DecSysRelease(p_sys);
}


#define CFG_TEXT_RENDERING_BACKEND N_("Text rendering backend")
#define CFG_LONGTEXT_RENDERING_BACKEND N_("Select text rendering backend")

#define CFG_TEXT_FONT N_("Font")
#define CFG_LONGTEXT_FONT N_("Select font")

#define CFG_TEXT_REPLACE_DRCS N_("Replace known DRCS")
#define CFG_LONGTEXT_REPLACE_DRCS N_("Replace known DRCS in the caption")

#define CFG_TEXT_FORCE_STROKE_TEXT N_("Force stroke text")
#define CFG_LONGTEXT_FORCE_STROKE_TEXT N_("Always render characters with stroke")

#define CFG_TEXT_IGNORE_BACKGROUND N_("Ignore background")
#define CFG_LONGTEXT_IGNORE_BACKGROUND N_("Ignore rendering caption background")

#define CFG_TEXT_IGNORE_RUBY N_("Ignore ruby (furigana)")
#define CFG_LONGTEXT_IGNORE_RUBY N_("Ignore ruby-like characters, like furigana")

#define CFG_TEXT_FADEOUT N_("Fadeout")
#define CFG_LONGTEXT_FADEOUT N_("Enable Fadeout")

#define CFG_TEXT_STROKE_WIDTH N_("Stroke width")
#define CFG_LONGTEXT_STROKE_WIDTH N_("Indicate stroke width for stroke text")


static const int rendering_backend_values[] = {
    ARIBCC_TEXTRENDERER_TYPE_AUTO,

#if defined(ARIBCC_USE_CORETEXT)
    ARIBCC_TEXTRENDERER_TYPE_CORETEXT,
#endif

#if defined(ARIBCC_USE_DIRECTWRITE)
    ARIBCC_TEXTRENDERER_TYPE_DIRECTWRITE,
#endif

#if defined(ARIBCC_USE_FREETYPE)
    ARIBCC_TEXTRENDERER_TYPE_FREETYPE
#endif
};

static const char* const ppsz_rendering_backend_descriptions[] = {
    N_("Auto"),

#if defined(ARIBCC_USE_CORETEXT)
    N_("CoreText"),
#endif

#if defined(ARIBCC_USE_DIRECTWRITE)
    N_("DirectWrite"),
#endif

#if defined(ARIBCC_USE_FREETYPE)
    N_("FreeType")
#endif
};

#ifdef __APPLE__
# define DEFAULT_FAMILY "Hiragino Maru Gothic ProN"
#elif defined(_WIN32)
# define DEFAULT_FAMILY "MS Gothic"
#else
# define DEFAULT_FAMILY "sans-serif"
#endif

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin ()
    set_shortname(N_("ARIB caption"))
    set_description(N_("ARIB caption renderer"))
    set_capability("spu decoder", 60)
    set_subcategory(SUBCAT_INPUT_SCODEC)
    set_callbacks(Open, Close)

    add_integer(ARIBCAPTION_CFG_PREFIX "rendering-backend", 0, CFG_TEXT_RENDERING_BACKEND, CFG_LONGTEXT_RENDERING_BACKEND)
        change_integer_list(rendering_backend_values, ppsz_rendering_backend_descriptions)
    add_font(ARIBCAPTION_CFG_PREFIX "font", DEFAULT_FAMILY, CFG_TEXT_FONT, CFG_LONGTEXT_FONT)
    add_bool(ARIBCAPTION_CFG_PREFIX "replace-drcs", true, CFG_TEXT_REPLACE_DRCS, CFG_LONGTEXT_REPLACE_DRCS)
    add_bool(ARIBCAPTION_CFG_PREFIX "force-stroke-text", false, CFG_TEXT_FORCE_STROKE_TEXT, CFG_LONGTEXT_FORCE_STROKE_TEXT)
    add_bool(ARIBCAPTION_CFG_PREFIX "ignore-background", false, CFG_TEXT_IGNORE_BACKGROUND, CFG_LONGTEXT_IGNORE_BACKGROUND)
    add_bool(ARIBCAPTION_CFG_PREFIX "ignore-ruby", false, CFG_TEXT_IGNORE_RUBY, CFG_LONGTEXT_IGNORE_RUBY)
    add_bool(ARIBCAPTION_CFG_PREFIX "fadeout", false, CFG_TEXT_FADEOUT, CFG_LONGTEXT_FADEOUT)
    add_float_with_range(ARIBCAPTION_CFG_PREFIX "stroke-width", 1.5f, 0.0f, 3.0f, CFG_TEXT_STROKE_WIDTH, CFG_LONGTEXT_STROKE_WIDTH)
vlc_module_end ()
