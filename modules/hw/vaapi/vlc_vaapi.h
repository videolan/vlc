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

/**************************
 * VA instance management *
 **************************/

/* Allocates the VA instance and sets the reference counter to 1. */
int
vlc_vaapi_SetInstance(VADisplay dpy);

/* Retrieve the VA instance and increases the reference counter by 1. */
VADisplay
vlc_vaapi_GetInstance(void);

/* Decreases the reference counter by 1 and frees the instance if that counter
   reaches 0. */
void
vlc_vaapi_ReleaseInstance(VADisplay *);

/*****************
 * VAAPI display *
 *****************/

int
vlc_vaapi_Initialize(vlc_object_t *o, VADisplay va_dpy);

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

#endif /* VLC_VAAPI_H */
