/*****************************************************************************
 * vlc_vaapi.c: VAAPI helper for VLC
 *****************************************************************************
 * Copyright (C) 2017 VLC authors, VideoLAN and VideoLabs
 *
 * Authors: Thomas Guillem <thomas@gllm.fr>
 *          Petri Hintukainen <phintuka@gmail.com>
 *          Victorien Le Couviour--Tuffet <victorien.lecouviour.tuffet@gmail.com>
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

#include "vlc_vaapi.h"

#include <stdint.h>
#include <stdlib.h>
#include <inttypes.h>
#include <assert.h>

#include <va/va.h>

#include <vlc_common.h>
#include <vlc_atomic.h>
#include <vlc_fourcc.h>
#include <vlc_picture_pool.h>

/* This macro is designed to wrap any VA call, and in case of failure,
   display the VA error string then goto the 'error' label (which you must
   define). */
#define VA_CALL(o, f, args...)                          \
    do                                                  \
    {                                                   \
        VAStatus s = f(args);                           \
        if (s != VA_STATUS_SUCCESS)                     \
        {                                               \
            msg_Err(o, "%s: %s", #f, vaErrorStr(s));    \
            goto error;                                 \
        }                                               \
    } while (0)

/*****************
 * VAAPI display *
 *****************/

int
vlc_vaapi_Initialize(vlc_object_t *o, VADisplay dpy)
{
    int major = 0, minor = 0;
    VA_CALL(o, vaInitialize, dpy, &major, &minor);
    return VLC_SUCCESS;
error:
    vaTerminate(dpy);
    return VLC_EGENERIC;
}

/**************************
 * VAAPI create & destroy *
 **************************/

VAContextID
vlc_vaapi_CreateContext(vlc_object_t *o, VADisplay dpy, VAConfigID conf,
                        int pic_w, int pic_h, int flag,
                        VASurfaceID *render_targets, int num_render_targets)
{
    VAContextID ctx;
    VA_CALL(o, vaCreateContext, dpy, conf, pic_w, pic_h, flag,
            render_targets, num_render_targets, &ctx);
    return ctx;
error: return VA_INVALID_ID;
}

VABufferID
vlc_vaapi_CreateBuffer(vlc_object_t *o, VADisplay dpy, VAContextID ctx,
                       VABufferType type, unsigned int size,
                       unsigned int num_elements, void *data)
{
    VABufferID buf_id;
    VA_CALL(o, vaCreateBuffer, dpy, ctx, type,
            size, num_elements, data, &buf_id);
    return buf_id;
error: return VA_INVALID_ID;
}

int
vlc_vaapi_DeriveImage(vlc_object_t *o,
                      VADisplay dpy, VASurfaceID surface, VAImage *image)
{
    VA_CALL(o, vaDeriveImage, dpy, surface, image);
    return VLC_SUCCESS;
error: return VLC_EGENERIC;
}

int
vlc_vaapi_CreateImage(vlc_object_t *o, VADisplay dpy, VAImageFormat *format,
                      int width, int height, VAImage *image)
{
    VA_CALL(o, vaCreateImage, dpy, format, width, height, image);
    return VLC_SUCCESS;
error: return VLC_EGENERIC;
}

int
vlc_vaapi_DestroyConfig(vlc_object_t *o, VADisplay dpy, VAConfigID conf)
{
    VA_CALL(o, vaDestroyConfig, dpy, conf);
    return VLC_SUCCESS;
error: return VLC_EGENERIC;
}

int
vlc_vaapi_DestroyContext(vlc_object_t *o, VADisplay dpy, VAContextID ctx)
{
    VA_CALL(o, vaDestroyContext, dpy, ctx);
    return VLC_SUCCESS;
error: return VLC_EGENERIC;
}

int
vlc_vaapi_DestroyBuffer(vlc_object_t *o, VADisplay dpy, VABufferID buf)
{
    VA_CALL(o, vaDestroyBuffer, dpy, buf);
    return VLC_SUCCESS;
error: return VLC_EGENERIC;
}

int
vlc_vaapi_DestroyImage(vlc_object_t *o, VADisplay dpy, VAImageID image)
{
    VA_CALL(o, vaDestroyImage, dpy, image);
    return VLC_SUCCESS;
error: return VLC_EGENERIC;
}

/***********************
 * VAAPI buffer access *
 ***********************/

int
vlc_vaapi_MapBuffer(vlc_object_t *o, VADisplay dpy,
                    VABufferID buf_id, void **p_buf)
{
    VA_CALL(o, vaMapBuffer, dpy, buf_id, p_buf);
    return VLC_SUCCESS;
error: return VLC_EGENERIC;
}

int
vlc_vaapi_UnmapBuffer(vlc_object_t *o, VADisplay dpy, VABufferID buf_id)
{
    VA_CALL(o, vaUnmapBuffer, dpy, buf_id);
    return VLC_SUCCESS;
error: return VLC_EGENERIC;
}

int
vlc_vaapi_AcquireBufferHandle(vlc_object_t *o, VADisplay dpy, VABufferID buf_id,
                              VABufferInfo *buf_info)
{
    VA_CALL(o, vaAcquireBufferHandle, dpy, buf_id, buf_info);
    return VLC_SUCCESS;
error: return VLC_EGENERIC;
}

int
vlc_vaapi_ReleaseBufferHandle(vlc_object_t *o, VADisplay dpy, VABufferID buf_id)
{
    VA_CALL(o, vaReleaseBufferHandle, dpy, buf_id);
    return VLC_SUCCESS;
error: return VLC_EGENERIC;
}

/*****************
 * VAAPI queries *
 *****************/

int
vlc_vaapi_IsVideoProcFilterAvailable(vlc_object_t *o, VADisplay dpy,
                                     VAContextID ctx, VAProcFilterType filter)
{
    VAProcFilterType    filters[VAProcFilterCount];
    unsigned int        num_filters = VAProcFilterCount;

    VA_CALL(o, vaQueryVideoProcFilters, dpy, ctx, filters, &num_filters);
    for (unsigned int i = 0; i < num_filters; ++i)
        if (filter == filters[i])
            return VLC_SUCCESS;
    return VLC_EGENERIC;
error: return VLC_EGENERIC;
}

int
vlc_vaapi_QueryVideoProcFilterCaps(vlc_object_t *o, VADisplay dpy,
                                   VAContextID ctx, VAProcFilterType filter,
                                   void *caps, unsigned int *p_num_caps)
{
    VA_CALL(o, vaQueryVideoProcFilterCaps, dpy,
            ctx, filter, caps, p_num_caps);
    return VLC_SUCCESS;
error: return VLC_EGENERIC;
}

int
vlc_vaapi_QueryVideoProcPipelineCaps(vlc_object_t *o, VADisplay dpy,
                                     VAContextID ctx, VABufferID *filters,
                                     unsigned int num_filters,
                                     VAProcPipelineCaps *pipeline_caps)
{
    VA_CALL(o, vaQueryVideoProcPipelineCaps, dpy,
            ctx, filters, num_filters, pipeline_caps);
    return VLC_SUCCESS;
error: return VLC_EGENERIC;
}

/*******************
 * VAAPI rendering *
 *******************/

int
vlc_vaapi_BeginPicture(vlc_object_t *o, VADisplay dpy,
                       VAContextID ctx, VASurfaceID surface)
{
    VA_CALL(o, vaBeginPicture, dpy, ctx, surface);
    return VLC_SUCCESS;
error: return VLC_EGENERIC;
}

int
vlc_vaapi_RenderPicture(vlc_object_t *o, VADisplay dpy, VAContextID ctx,
                        VABufferID *buffers, int num_buffers)
{
    VA_CALL(o, vaRenderPicture, dpy, ctx, buffers, num_buffers);
    return VLC_SUCCESS;
error: return VLC_EGENERIC;
}

int
vlc_vaapi_EndPicture(vlc_object_t *o, VADisplay dpy, VAContextID ctx)
{
    VA_CALL(o, vaEndPicture, dpy, ctx);
    return VLC_SUCCESS;
error: return VLC_EGENERIC;
}

/*****************
 * VAAPI helpers *
 *****************/

static bool
IsVaProfileSupported(VADisplay dpy, VAProfile i_profile)
{
    /* Check if the selected profile is supported */
    if (i_profile == VAProfileNone)
        return true;
    int i_profiles_nb = vaMaxNumProfiles(dpy);
    if (i_profiles_nb < 0)
        return false;
    VAProfile *p_profiles_list = calloc(i_profiles_nb, sizeof(VAProfile));
    if (!p_profiles_list)
        return false;

    bool b_supported_profile = false;
    VAStatus status =
        vaQueryConfigProfiles(dpy, p_profiles_list, &i_profiles_nb);
    if (status != VA_STATUS_SUCCESS)
        goto error;

    for (int i = 0; i < i_profiles_nb; i++)
    {
        if (p_profiles_list[i] == i_profile)
        {
            b_supported_profile = true;
            break;
        }
    }

error:
    free(p_profiles_list);
    return b_supported_profile;
}

static bool
IsEntrypointAvailable(VADisplay dpy, VAProfile i_profile,
                      VAEntrypoint entrypoint)
{
    VAEntrypoint *      entrypoints;
    int                 num_entrypoints = vaMaxNumEntrypoints(dpy);
    bool                ret = false;

    if (num_entrypoints <= 0)
        return false;
    entrypoints = malloc(num_entrypoints * sizeof(VAEntrypoint));

    if (!entrypoints)
        return false;

    VAStatus status =
        vaQueryConfigEntrypoints(dpy, i_profile, entrypoints, &num_entrypoints);
    if (status != VA_STATUS_SUCCESS)
        goto error;

    for (int i = 0; i < num_entrypoints; ++i)
        if (entrypoint == entrypoints[i])
        {
            ret = true;
            break;
        }

error:
    free(entrypoints);
    return ret;
}

VAConfigID
vlc_vaapi_CreateConfigChecked(vlc_object_t *o, VADisplay dpy,
                              VAProfile i_profile, VAEntrypoint entrypoint,
                              int va_force_fourcc)
{
    if (!IsVaProfileSupported(dpy, i_profile))
    {
        msg_Err(o, "profile(%d) is not supported", i_profile);
        return VA_INVALID_ID;
    }
    if (!IsEntrypointAvailable(dpy, i_profile, entrypoint))
    {
        msg_Err(o, "entrypoint(%d) is not available", entrypoint);
        return VA_INVALID_ID;
    }

    /* Create a VA configuration */
    VAConfigAttrib attrib = {
        .type = VAConfigAttribRTFormat,
    };
    if (vaGetConfigAttributes(dpy, i_profile, entrypoint, &attrib, 1))
    {
        msg_Err(o, "vaGetConfigAttributes failed");
        return VA_INVALID_ID;
    }

    /* Not sure what to do if not, I don't have a way to test */
    if ((attrib.value & VA_RT_FORMAT_YUV420) == 0)
    {
        msg_Err(o, "config doesn't support VA_RT_FORMAT_YUV420");
        return VA_INVALID_ID;
    }

    unsigned int num_sattribs;
    VASurfaceAttrib *sattribs = NULL;
    VAConfigID va_config_id = VA_INVALID_ID;
    VA_CALL(o, vaCreateConfig, dpy, i_profile, entrypoint, &attrib, 1,
            &va_config_id);

    if (va_force_fourcc == 0)
        return va_config_id;

    /* Fetch VASurfaceAttrib list to make sure the decoder can output NV12 */
    if (vaQuerySurfaceAttributes(dpy, va_config_id, NULL, &num_sattribs)
        != VA_STATUS_SUCCESS)
        goto error;

    sattribs = malloc(num_sattribs * sizeof(*sattribs));
    if (sattribs == NULL)
        goto error;
    if (vaQuerySurfaceAttributes(dpy, va_config_id, sattribs, &num_sattribs)
        != VA_STATUS_SUCCESS)
        goto error;

    for (unsigned i = 0; i < num_sattribs; ++i)
    {
        VASurfaceAttrib *sattrib = &sattribs[i];
        if (sattrib->type == VASurfaceAttribPixelFormat
         && sattrib->flags & VA_SURFACE_ATTRIB_SETTABLE
         && sattrib->value.value.i == va_force_fourcc)
        {
            free(sattribs);
            return va_config_id;
        }

    }

error:
    free(sattribs);
    if (va_config_id != VA_INVALID_ID)
    {
        msg_Err(o, "config doesn't support forced fourcc");
        vlc_vaapi_DestroyConfig(o, dpy, va_config_id);
    }
    return VA_INVALID_ID;
}

struct vaapi_pic_ctx
{
    picture_context_t s;
    VASurfaceID surface;
    picture_t *picref;
};

struct pic_sys_vaapi_instance
{
    atomic_int refcount;
    VADisplay dpy;
    unsigned num_render_targets;
    VASurfaceID render_targets[];
};

struct picture_sys_t
{
    struct pic_sys_vaapi_instance *instance;
    struct vaapi_pic_ctx ctx;
};

static void
pool_pic_destroy_cb(picture_t *pic)
{
    picture_sys_t *p_sys = pic->p_sys;
    struct pic_sys_vaapi_instance *instance = p_sys->instance;

    if (atomic_fetch_sub(&instance->refcount, 1) == 1)
    {
        vaDestroySurfaces(instance->dpy, instance->render_targets,
                          instance->num_render_targets);
        vlc_vaapi_ReleaseInstance(instance->dpy);
        free(instance);
    }
    free(pic->p_sys);
    free(pic);
}

static void
pic_ctx_destroy_cb(struct picture_context_t *opaque)
{
    struct vaapi_pic_ctx *ctx = (struct vaapi_pic_ctx *) opaque;
    picture_Release(ctx->picref);
    free(opaque);
}

static struct picture_context_t *
pic_ctx_copy_cb(struct picture_context_t *opaque)
{
    struct vaapi_pic_ctx *src_ctx = (struct vaapi_pic_ctx *) opaque;
    struct vaapi_pic_ctx *dst_ctx = malloc(sizeof *dst_ctx);
    if (dst_ctx == NULL)
        return NULL;

    dst_ctx->s.destroy = pic_ctx_destroy_cb;
    dst_ctx->s.copy = pic_ctx_copy_cb;
    dst_ctx->surface = src_ctx->surface;
    dst_ctx->picref = picture_Hold(src_ctx->picref);
    return &dst_ctx->s;
}

static void
pic_sys_ctx_destroy_cb(struct picture_context_t *opaque)
{
    (void) opaque;
}

picture_pool_t *
vlc_vaapi_PoolNew(vlc_object_t *o, VADisplay dpy, unsigned count,
                  VASurfaceID **render_targets,
                  const video_format_t *restrict fmt,
                  unsigned va_rt_format, int va_force_fourcc)
{
    struct pic_sys_vaapi_instance *instance =
        malloc(sizeof(*instance) + count * sizeof(VASurfaceID));
    if (!instance)
        return NULL;
    instance->num_render_targets = count;
    atomic_init(&instance->refcount, 0);

    VASurfaceAttrib *attribs = NULL;
    unsigned num_attribs = 0;
    VASurfaceAttrib fourcc_attribs[1] = {
        {
            .type = VASurfaceAttribPixelFormat,
            .flags = VA_SURFACE_ATTRIB_SETTABLE,
            .value.type    = VAGenericValueTypeInteger,
            .value.value.i = va_force_fourcc,
        }
    };
    if (va_force_fourcc != 0)
    {
        attribs = fourcc_attribs;
        num_attribs = 1;
    }

    picture_t *pics[count];

    VA_CALL(o, vaCreateSurfaces, dpy, va_rt_format,
            fmt->i_visible_width, fmt->i_visible_height,
            instance->render_targets, instance->num_render_targets,
            attribs, num_attribs);

    for (unsigned i = 0; i < count; i++)
    {
        picture_sys_t *p_sys = malloc(sizeof *p_sys);
        if (p_sys == NULL)
        {
            count = i;
            goto error_pic;
        }
        p_sys->instance = instance;
        p_sys->ctx.s.destroy = pic_sys_ctx_destroy_cb;
        p_sys->ctx.s.copy = pic_ctx_copy_cb;
        p_sys->ctx.surface = instance->render_targets[i];
        p_sys->ctx.picref = NULL;
        picture_resource_t rsc = {
            .p_sys = p_sys,
            .pf_destroy = pool_pic_destroy_cb,
        };
        pics[i] = picture_NewFromResource(fmt, &rsc);
        if (pics[i] == NULL)
        {
            free(p_sys);
            count = i;
            goto error_pic;
        }
    }

    picture_pool_t *pool = picture_pool_New(count, pics);
    if (!pool)
        goto error_pic;

    atomic_store(&instance->refcount, count);
    instance->dpy = vlc_vaapi_GetInstance(); /* Hold a reference on VADisplay */
    assert(instance->dpy == dpy);

    *render_targets = instance->render_targets;
    return pool;

error_pic:
    while (count > 0)
        picture_Release(pics[--count]);

    VA_CALL(o, vaDestroySurfaces, instance->dpy, instance->render_targets,
            instance->num_render_targets);

error:
    free(instance);
    return NULL;
}

unsigned
vlc_vaapi_PicSysGetRenderTargets(picture_sys_t *sys,
                                 VASurfaceID **render_targets)
{
    assert(sys && sys->instance);
    *render_targets = sys->instance->render_targets;
    return sys->instance->num_render_targets;
}

void
vlc_vaapi_PicAttachContext(picture_t *pic)
{
    assert(pic->p_sys != NULL);
    assert(pic->context == NULL);

    pic->p_sys->ctx.picref = pic;
    pic->context = &pic->p_sys->ctx.s;
}

VASurfaceID
vlc_vaapi_PicGetSurface(picture_t *pic)
{
    assert(pic->context);

    return ((struct vaapi_pic_ctx *)pic->context)->surface;
}
