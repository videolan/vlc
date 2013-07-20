/*****************************************************************************
 * va.h: Video Acceleration API for avcodec
 *****************************************************************************
 * Copyright (C) 2009 Laurent Aimar
 * $Id$
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

#ifndef VLC_AVCODEC_VA_H
#define VLC_AVCODEC_VA_H 1

typedef struct vlc_va_t vlc_va_t;
typedef struct vlc_va_sys_t vlc_va_sys_t;

struct vlc_va_t {
    VLC_COMMON_MEMBERS

    vlc_va_sys_t *sys;
    module_t *module;
    char *description;
    int pix_fmt;

    int  (*setup)(vlc_va_t *, void **hw, vlc_fourcc_t *output,
                  int width, int height);
    int  (*get)(vlc_va_t *, AVFrame *frame);
    void (*release)(vlc_va_t *, AVFrame *frame);
    int  (*extract)(vlc_va_t *, picture_t *dst, AVFrame *src);
};

/**
 * Creates an accelerated video decoding back-end for libavcodec.
 * @param obj parent VLC object
 * @param codec_id libavcodec codec ID of the content to decode
 * @param fmt VLC format of the content to decode
 * @return a new VLC object on success, NULL on error.
 */
vlc_va_t *vlc_va_New(vlc_object_t *obj, int codec_id, const es_format_t *fmt);

/**
 * Initializes the acceleration video decoding back-end for libavcodec.
 * @param hw pointer to libavcodec hardware context pointer [OUT]
 * @param output pointer to video chroma output by the back-end [OUT]
 * @param width coded video width in pixels
 * @param height coded video height in pixels
 * @return VLC_SUCCESS on success, otherwise an error code.
 */
static inline int vlc_va_Setup(vlc_va_t *va, void **hw, vlc_fourcc_t *output,
                               int width, int height)
{
    return va->setup(va, hw, output, width, height);
}

/**
 * Allocates a hardware video surface for a libavcodec frame.
 * The surface will be used as output for the hardware decoder, and possibly
 * also as a reference frame to decode other surfaces.
 *
 * @note This function needs not be reentrant. However it may be called
 * concurrently with vlc_va_Extract() and/or vlc_va_Release() from other
 * threads and other frames.
 *
 * @param frame libavcodec frame [IN/OUT]
 * @return VLC_SUCCESS on success, otherwise an error code.
 */
static inline int vlc_va_Get(vlc_va_t *va, AVFrame *frame)
{
    return va->get(va, frame);
}

/**
 * Releases a hardware surface from a libavcodec frame.
 * The surface has been previously allocated with vlc_va_Get().
 *
 * @note This function needs not be reentrant. However it may be called
 * concurrently with vlc_va_Get() and/or vlc_va_Extract() from other threads
 * and other frames.
 *
 * @param frame libavcodec frame previously allocated by vlc_va_Get()
 */
static inline void vlc_va_Release(vlc_va_t *va, AVFrame *frame)
{
    va->release(va, frame);
}

/**
 * Extracts a hardware surface from a libavcodec frame into a VLC picture.
 * The surface has been previously allocated with vlc_va_Get() and decoded
 * by the libavcodec hardware acceleration.
 * The surface may still be used by libavcodec as a reference frame until it is
 * freed with vlc_va_Release().
 *
 * @note This function needs not be reentrant, but it may run concurrently with
 * vlc_va_Get() or vlc_va_Release() in other threads (with distinct frames).
 *
 * @param frame libavcodec frame previously allocated by vlc_va_Get()
 */
static inline int vlc_va_Extract(vlc_va_t *va, picture_t *dst, AVFrame *src)
{
    return va->extract(va, dst, src);
}

/**
 * Destroys a libavcodec hardware acceleration back-end.
 * All allocated surfaces shall have been released beforehand.
 */
void vlc_va_Delete(vlc_va_t *);

#endif
