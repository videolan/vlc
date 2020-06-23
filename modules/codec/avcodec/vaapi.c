/*****************************************************************************
 * vaapi.c: VAAPI helpers for the libavcodec decoder
 *****************************************************************************
 * Copyright (C) 2017 VLC authors and VideoLAN
 * Copyright (C) 2009-2010 Laurent Aimar
 * Copyright (C) 2012-2014 Rémi Denis-Courmont
 *
 * Authors: Laurent Aimar <fenrir_AT_ videolan _DOT_ org>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_fourcc.h>
#include <vlc_picture.h>
#include <vlc_picture_pool.h>

#ifdef VLC_VA_BACKEND_DRM
# include <sys/types.h>
# include <sys/stat.h>
# include <fcntl.h>
# include <vlc_fs.h>
# include <va/va_drm.h>
#endif
#include <libavcodec/avcodec.h>
#include <libavcodec/vaapi.h>

#include "avcodec.h"
#include "va.h"
#include "../../hw/vaapi/vlc_vaapi.h"
#include "va_surface.h"

struct vlc_va_sys_t
{
    struct vaapi_context hw_ctx;
    vlc_video_context *vctx;
    va_pool_t *va_pool;
    VASurfaceID render_targets[MAX_SURFACE_COUNT];
};

static int GetVaProfile(const AVCodecContext *ctx, const es_format_t *fmt_in,
                        VAProfile *va_profile, int *vlc_chroma,
                        unsigned *pic_count)
{
    VAProfile i_profile;
    unsigned count = 3;
    int i_vlc_chroma = VLC_CODEC_VAAPI_420;

    switch(ctx->codec_id)
    {
    case AV_CODEC_ID_MPEG1VIDEO:
    case AV_CODEC_ID_MPEG2VIDEO:
        i_profile = VAProfileMPEG2Main;
        count = 4;
        break;
    case AV_CODEC_ID_MPEG4:
        i_profile = VAProfileMPEG4AdvancedSimple;
        break;
    case AV_CODEC_ID_WMV3:
        i_profile = VAProfileVC1Main;
        break;
    case AV_CODEC_ID_VC1:
        i_profile = VAProfileVC1Advanced;
        break;
    case AV_CODEC_ID_H264:
        i_profile = VAProfileH264High;
        count = 18;
        break;
    case AV_CODEC_ID_HEVC:
        if (fmt_in->i_profile == FF_PROFILE_HEVC_MAIN)
            i_profile = VAProfileHEVCMain;
        else if (fmt_in->i_profile == FF_PROFILE_HEVC_MAIN_10)
        {
            i_profile = VAProfileHEVCMain10;
            i_vlc_chroma = VLC_CODEC_VAAPI_420_10BPP;
        }
        else
            return VLC_EGENERIC;
        count = 18;
        break;
    case AV_CODEC_ID_VP8:
        i_profile = VAProfileVP8Version0_3;
        count = 5;
        break;
    case AV_CODEC_ID_VP9:
        if (ctx->profile == FF_PROFILE_VP9_0)
            i_profile = VAProfileVP9Profile0;
#if VA_CHECK_VERSION( 0, 39, 0 )
        else if (ctx->profile == FF_PROFILE_VP9_2)
        {
            i_profile = VAProfileVP9Profile2;
            i_vlc_chroma = VLC_CODEC_VAAPI_420_10BPP;
        }
#endif
        else
            return VLC_EGENERIC;
        count = 10;
        break;
    default:
        return VLC_EGENERIC;
    }

    *va_profile = i_profile;
    *pic_count = count + ctx->thread_count;
    *vlc_chroma = i_vlc_chroma;
    return VLC_SUCCESS;
}

typedef struct {
    struct vaapi_pic_context ctx;
    vlc_va_surface_t *va_surface;
} vaapi_dec_pic_context;

static void vaapi_dec_pic_context_destroy(picture_context_t *context)
{
    vaapi_dec_pic_context *pic_ctx = container_of(context, vaapi_dec_pic_context, ctx.s);
    struct vlc_va_surface_t *va_surface = pic_ctx->va_surface;
    free(pic_ctx);
    va_surface_Release(va_surface);
}

static picture_context_t *vaapi_dec_pic_context_copy(picture_context_t *src)
{
    vaapi_dec_pic_context *src_ctx = container_of(src, vaapi_dec_pic_context, ctx.s);
    vaapi_dec_pic_context *pic_ctx = malloc(sizeof(*pic_ctx));
    if (unlikely(pic_ctx == NULL))
        return NULL;
    *pic_ctx = *src_ctx;
    vlc_video_context_Hold(pic_ctx->ctx.s.vctx);
    va_surface_AddRef(pic_ctx->va_surface);
    return &pic_ctx->ctx.s;
}

static int Get(vlc_va_t *va, picture_t *pic, uint8_t **data)
{
    vlc_va_sys_t *sys = va->sys;
    vlc_va_surface_t *va_surface = va_pool_Get(sys->va_pool);
    if (unlikely(va_surface == NULL))
        return VLC_ENOITEM;
    vaapi_dec_pic_context *vaapi_ctx = malloc(sizeof(*vaapi_ctx));
    if (unlikely(vaapi_ctx == NULL))
    {
        va_surface_Release(va_surface);
        return VLC_ENOMEM;
    }
    vaapi_ctx->ctx.s = (picture_context_t) {
        vaapi_dec_pic_context_destroy, vaapi_dec_pic_context_copy,
        sys->vctx,
    };
    vaapi_ctx->ctx.surface = sys->render_targets[va_surface_GetIndex(va_surface)];
    vaapi_ctx->ctx.va_dpy = sys->hw_ctx.display;
    vaapi_ctx->va_surface = va_surface;
    vlc_vaapi_PicSetContext(pic, &vaapi_ctx->ctx);
    data[3] = (void *) (uintptr_t) vaapi_ctx->ctx.surface;

    return VLC_SUCCESS;
}

static void Delete(vlc_va_t *va)
{
    vlc_va_sys_t *sys = va->sys;

    vlc_video_context_Release(sys->vctx);
    va_pool_Close(sys->va_pool);
}

static const struct vlc_va_operations ops = { Get, Delete, };

static int VAAPICreateDevice(vlc_va_t *va)
{
    VLC_UNUSED(va);
    return VLC_SUCCESS;
}

static void VAAPIDestroyDevice(void *opaque)
{
    vlc_va_sys_t *sys = opaque;
    if (sys->hw_ctx.context_id != VA_INVALID_ID)
        vlc_vaapi_DestroyContext(NULL, sys->hw_ctx.display, sys->hw_ctx.context_id);
    if (sys->hw_ctx.config_id != VA_INVALID_ID)
        vlc_vaapi_DestroyConfig(NULL, sys->hw_ctx.display, sys->hw_ctx.config_id);
    free(sys);
}

static int VAAPICreateDecoderSurfaces(vlc_va_t *va, int codec_id,
                                      const video_format_t *fmt,
                                      size_t count)
{
    VLC_UNUSED(codec_id);
    vlc_va_sys_t *sys = va->sys;

    unsigned va_rt_format;
    int va_fourcc;
    vlc_chroma_to_vaapi(fmt->i_chroma, &va_rt_format, &va_fourcc);

    VASurfaceAttrib fourcc_attribs[1] = {
        {
            .type = VASurfaceAttribPixelFormat,
            .flags = VA_SURFACE_ATTRIB_SETTABLE,
            .value.type    = VAGenericValueTypeInteger,
            .value.value.i = va_fourcc,
        }
    };

    VA_CALL(VLC_OBJECT(va), vaCreateSurfaces, sys->hw_ctx.display, va_rt_format,
            fmt->i_visible_width, fmt->i_visible_height,
            sys->render_targets, count,
            fourcc_attribs, 1);

    return VLC_SUCCESS;
error:
    return VLC_EGENERIC;
}

static void VAAPISetupAVCodecContext(void *opaque, AVCodecContext *avctx)
{
    vlc_va_sys_t *sys = opaque;
    avctx->hwaccel_context = &sys->hw_ctx;
}

static int Create(vlc_va_t *va, AVCodecContext *ctx, enum PixelFormat hwfmt, const AVPixFmtDescriptor *desc,
                  const es_format_t *fmt_in, vlc_decoder_device *dec_device,
                  video_format_t *fmt_out, vlc_video_context **vtcx_out)
{
    VLC_UNUSED(desc);
    if ( hwfmt != AV_PIX_FMT_VAAPI_VLD || dec_device == NULL ||
        dec_device->type != VLC_DECODER_DEVICE_VAAPI)
        return VLC_EGENERIC;

    vlc_va_sys_t *sys = malloc(sizeof *sys);
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;
    memset(sys, 0, sizeof (*sys));

    vlc_object_t *o = VLC_OBJECT(va);

    int ret = VLC_EGENERIC;

    VADisplay va_dpy = dec_device->opaque;

    VAProfile i_profile;
    unsigned count;
    int i_vlc_chroma;
    if (GetVaProfile(ctx, fmt_in, &i_profile, &i_vlc_chroma, &count) != VLC_SUCCESS)
        goto error;

    /* */
    sys->hw_ctx.display = va_dpy;
    sys->hw_ctx.config_id = VA_INVALID_ID;
    sys->hw_ctx.context_id = VA_INVALID_ID;
    va->sys = sys;

    struct va_pool_cfg pool_cfg = {
        VAAPICreateDevice,
        VAAPIDestroyDevice,
        VAAPICreateDecoderSurfaces,
        VAAPISetupAVCodecContext,
        sys,
    };
    sys->va_pool = va_pool_Create(va, &pool_cfg);
    if (sys->va_pool == NULL)
        goto error;

    fmt_out->i_chroma = i_vlc_chroma;
    int err = va_pool_SetupDecoder(va, sys->va_pool, ctx, fmt_out, count);
    if (err != VLC_SUCCESS)
        goto error;

    VASurfaceID *render_targets = sys->render_targets;

    sys->hw_ctx.config_id =
        vlc_vaapi_CreateConfigChecked(o, sys->hw_ctx.display, i_profile,
                                      VAEntrypointVLD, i_vlc_chroma);
    if (sys->hw_ctx.config_id == VA_INVALID_ID)
        goto error;

    /* Create a context */
    sys->hw_ctx.context_id =
        vlc_vaapi_CreateContext(o, sys->hw_ctx.display, sys->hw_ctx.config_id,
                                ctx->coded_width, ctx->coded_height, VA_PROGRESSIVE,
                                render_targets, count);
    if (sys->hw_ctx.context_id == VA_INVALID_ID)
        goto error;

    msg_Info(va, "Using %s", vaQueryVendorString(sys->hw_ctx.display));

    sys->vctx = vlc_video_context_Create( dec_device, VLC_VIDEO_CONTEXT_VAAPI, 0, NULL );
    if (sys->vctx == NULL)
        goto error;

    va->ops = &ops;
    *vtcx_out = sys->vctx;
    return VLC_SUCCESS;

error:
    if (sys->va_pool != NULL)
        va_pool_Close(sys->va_pool);
    else
        free(sys);
    return ret;
}

vlc_module_begin ()
    set_description( N_("VA-API video decoder") )
    set_va_callback( Create, 100 )
    add_shortcut( "vaapi" )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_VCODEC )
vlc_module_end ()
