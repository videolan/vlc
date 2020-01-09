/*****************************************************************************
 * vt_utils.h: videotoolbox/cvpx utility functions
 *****************************************************************************
 * Copyright (C) 2017 VLC authors, VideoLAN and VideoLabs
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

#ifndef VLC_CODEC_VTUTILS_H_
#define VLC_CODEC_VTUTILS_H_

#include <VideoToolbox/VideoToolbox.h>
#include <vlc_picture.h>

CFMutableDictionaryRef cfdict_create(CFIndex capacity);

void cfdict_set_int32(CFMutableDictionaryRef dict, CFStringRef key, int value);

/*
 * Attach a cvpx buffer to a picture
 *
 * The cvpx ref will be released when the picture is released
 * @return VLC_SUCCESS or VLC_ENOMEM
 */
int cvpxpic_attach(picture_t *p_pic, CVPixelBufferRef cvpx, vlc_video_context *vctx,
                   void (*on_released_cb)(vlc_video_context *vctx, unsigned));

/*
 * Get the cvpx buffer attached to a picture
 */
CVPixelBufferRef cvpxpic_get_ref(picture_t *pic);

/*
 * Create a picture mapped to a cvpx buffer
 *
 * @param fmt i_chroma must be VLC_CODEC_UYVY, VLC_CODEC_NV12 or VLC_CODEC_I420
 * @param cvpx buffer to map
 * @param readonly true to map read-only, false otherwise
 * @return a valid picture, call picture_Release() or cvpxpic_unmap() to free
 * the picture and unmap the cvpx buffer.
 */
picture_t *cvpxpic_create_mapped(const video_format_t *fmt,
                                 CVPixelBufferRef cvpx, vlc_video_context *vctx,
                                 bool readonly);

/*
 * Create a picture attached to an unmapped cvpx buffer
 *
 * @param mapped_pic must be a picture created with cvpxpic_create_mapped()
 * @return a valid picture, the pic chroma will one of VLC_CODEC_CVPX_* chromas
 */
picture_t *cvpxpic_unmap(picture_t *mapped_pic);

/*
 * Create a cvpx pool
 *
 * @param fmt i_chroma must be one of VLC_CODEC_CVPX_* chromas
 * @param count number of pictures to alloc
 * @return a valid cvpx pool or NULL, release it with CVPixelBufferPoolRelease()
 */
CVPixelBufferPoolRef cvpxpool_create(const video_format_t *fmt, unsigned count);

/*
 * Get a cvpx buffer from a pool
 */
CVPixelBufferRef cvpxpool_new_cvpx(CVPixelBufferPoolRef pool);

enum cvpx_video_context_type
{
    CVPX_VIDEO_CONTEXT_DEFAULT,
    CVPX_VIDEO_CONTEXT_VIDEOTOOLBOX,
    CVPX_VIDEO_CONTEXT_CIFILTERS,
};

/*
 * Create a CVPX video context for a subtype
 * The private data of a subtype (VIDEOTOOLBOX, CIFILTERS) if only compatible
 * for this subtype
 */
vlc_video_context *
vlc_video_context_CreateCVPX(vlc_decoder_device *device,
                             enum cvpx_video_context_type type, size_t type_size,
                             const struct vlc_video_context_operations *ops);

/*
 * Get the video context sub private data
 */
void *
vlc_video_context_GetCVPXPrivate(vlc_video_context *vctx,
                                 enum cvpx_video_context_type type);

#endif
