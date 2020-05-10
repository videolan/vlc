/*****************************************************************************
 * vlc_vaapi.h: VAAPI helper for VLC
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

#ifndef VLC_VAAPI_H
# define VLC_VAAPI_H

#include <va/va.h>

#ifndef VA_RT_FORMAT_YUV420_10BPP
# define VA_RT_FORMAT_YUV420_10BPP 0x00000100
#endif

#ifndef VA_FOURCC_P010
#define VA_FOURCC_P010 0x30313050
#endif

#include <vlc_common.h>
#include <vlc_codec.h>
#include <vlc_fourcc.h>
#include <vlc_picture_pool.h>

/**************************
 * VAAPI create & destroy *
 **************************/

/* Creates a VA context from the VA configuration and the width / height of the
 * pictures to process. */
VAContextID
vlc_vaapi_CreateContext(vlc_object_t *o, VADisplay dpy, VAConfigID conf,
                        int pic_w, int pic_h, int flag,
                        VASurfaceID *render_targets, int num_render_targets);

/* Creates a VA buffer for 'num_elements' elements of 'size' bytes and
 * initalized with 'data'. If 'data' is NULL, then the content of the buffer is
 * undefined. */
VABufferID
vlc_vaapi_CreateBuffer(vlc_object_t *o, VADisplay dpy, VAContextID ctx,
                       VABufferType type, unsigned int size,
                       unsigned int num_elements, void *data);

/* Creates a VA image from a VA surface. */
int
vlc_vaapi_DeriveImage(vlc_object_t *o, VADisplay dpy,
                      VASurfaceID surface, VAImage *image);

/* Creates a VA image */
int
vlc_vaapi_CreateImage(vlc_object_t *o, VADisplay dpy, VAImageFormat *format,
                      int width, int height, VAImage *image);

/* Destroys a VA configuration. */
int
vlc_vaapi_DestroyConfig(vlc_object_t *o, VADisplay dpy, VAConfigID conf);

/* Destroys a VA context. */
int
vlc_vaapi_DestroyContext(vlc_object_t *o, VADisplay dpy, VAContextID ctx);

/* Destroys a VA buffer. */
int
vlc_vaapi_DestroyBuffer(vlc_object_t *o, VADisplay dpy, VABufferID buf);

/* Destroys a VA image. */
int
vlc_vaapi_DestroyImage(vlc_object_t *o, VADisplay dpy, VAImageID image);

/***********************
 * VAAPI buffer access *
 ***********************/

/* Maps the specified buffer to '*p_buf'. */
int
vlc_vaapi_MapBuffer(vlc_object_t *o, VADisplay dpy,
                    VABufferID buf_id, void **p_buf);

/* Unmaps the specified buffer so that the driver can read from it. */
int
vlc_vaapi_UnmapBuffer(vlc_object_t *o, VADisplay dpy, VABufferID buf_id);

int
vlc_vaapi_AcquireBufferHandle(vlc_object_t *o, VADisplay dpy, VABufferID buf_id,
                              VABufferInfo *buf_info);

int
vlc_vaapi_ReleaseBufferHandle(vlc_object_t *o, VADisplay dpy, VABufferID buf_id);

/*****************
 * VAAPI queries *
 *****************/

/* Checks if the specified filter is available. */
int
vlc_vaapi_IsVideoProcFilterAvailable(vlc_object_t *o,
                                     VADisplay dpy, VAContextID ctx,
                                     VAProcFilterType filter);

/* Retrieves the list of available capabilities of a filter. */
int
vlc_vaapi_QueryVideoProcFilterCaps(vlc_object_t *o, VADisplay dpy,
                                   VAContextID ctx,
                                   VAProcFilterType filter, void *caps,
                                   unsigned int *p_num_caps);

/* Retrieves the available capabilities of the pipeline. */
int
vlc_vaapi_QueryVideoProcPipelineCaps(vlc_object_t *o, VADisplay dpy,
                                     VAContextID ctx, VABufferID *filters,
                                     unsigned int num_filters,
                                     VAProcPipelineCaps *pipeline_caps);

/*******************
 * VAAPI rendering *
 *******************/

/* Tells the driver the specified surface is the next surface to render. */
int
vlc_vaapi_BeginPicture(vlc_object_t *o, VADisplay dpy,
                       VAContextID ctx, VASurfaceID surface);

/* Send buffers (describing rendering operations to perform on the current
 * surface) to the driver, which are automatically destroyed afterwards. */
int
vlc_vaapi_RenderPicture(vlc_object_t *o, VADisplay dpy, VAContextID ctx,
                        VABufferID *buffers, int num_buffers);

/* Tells the driver it can begins to process all the pending operations
 * (specified with vlc_vaapi_RenderPicture) on the current surface. */
int
vlc_vaapi_EndPicture(vlc_object_t *o, VADisplay dpy, VAContextID ctx);

/*****************
 * VAAPI helpers *
 *****************/

struct vaapi_pic_context
{
    picture_context_t s;
    VASurfaceID surface;
    VADisplay va_dpy;
};

/* Creates a VAConfigID */
VAConfigID
vlc_vaapi_CreateConfigChecked(vlc_object_t *o, VADisplay dpy,
                              VAProfile i_profile, VAEntrypoint entrypoint,
                              int i_force_vlc_chroma);

/* Create a pool backed by VASurfaceID. render_targets will destroyed once
 * the pool and every pictures are released. */
picture_pool_t *
vlc_vaapi_PoolNew(vlc_object_t *o, vlc_video_context *vctx,
                  VADisplay dpy, unsigned count, VASurfaceID **render_targets,
                  const video_format_t *restrict fmt);

/* Attachs the VASurface to the picture context, the picture must be allocated
 * by a vaapi pool (see vlc_vaapi_PoolNew()) */
void
vlc_vaapi_PicAttachContext(picture_t *pic);

/* Attachs the VASurface to the picture context */
void
vlc_vaapi_PicSetContext(picture_t *pic, struct vaapi_pic_context *);

/* Get the VASurfaceID attached to the pic */
VASurfaceID
vlc_vaapi_PicGetSurface(picture_t *pic);

/* Get the VADisplay attached to the pic (valid while the pic is alive) */
VADisplay
vlc_vaapi_PicGetDisplay(picture_t *pic);

static inline bool
vlc_vaapi_IsChromaOpaque(int i_vlc_chroma)
{
    return i_vlc_chroma == VLC_CODEC_VAAPI_420
        || i_vlc_chroma == VLC_CODEC_VAAPI_420_10BPP;
}

void vlc_chroma_to_vaapi(int i_vlc_chroma, unsigned *va_rt_format, int *va_fourcc);

#if VA_CHECK_VERSION(1, 1, 0)
int
vlc_vaapi_ExportSurfaceHandle(vlc_object_t *o,
                              VADisplay dpy,
                              VASurfaceID surface,
                              uint32_t mem_type,
                              uint32_t flags,
                              void *descriptor);
#endif

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

#endif /* VLC_VAAPI_H */
