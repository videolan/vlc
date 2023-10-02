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

/**
 * @brief Get a kCVImageBufferYCbCrMatrix_X value corresponding 
 * to a video_color_space_t
 * 
 * @param color_space a video format color space value
 * @return a CFStringRef or NULL if there's no value match
 */
CFStringRef 
cvpx_map_YCbCrMatrix_from_vcs(video_color_space_t color_space);

/**
 * @brief Get a kCVImageBufferColorPrimaries_X value corresponding 
 * to a video_color_primaries_t
 * 
 * @param color_primaries a video format color primaries value
 * @return a CFStringRef or NULL if there's no value match
 */
CFStringRef 
cvpx_map_ColorPrimaries_from_vcp(video_color_primaries_t color_primaries);

/**
 * @brief Get a kCVImageBufferTransferFunction_X value corresponding 
 * to a video_transfer_func_t
 * 
 * @param transfer_func a video format transfer func value
 * @return a CFStringRef or NULL if there's no value match
 */
CFStringRef 
cvpx_map_TransferFunction_from_vtf(video_transfer_func_t transfer_func);

/**
 * @brief Check if an image buffer has an attachment corresponding to the key 
 * parameter
 * 
 * @param pixelBuffer the image buffer where attachment is searched
 * @param key the attachment's key to search
 * @return true if attachment is present
 * @return false if key didn't match any attachment
 */
bool cvpx_has_attachment(CVPixelBufferRef pixelBuffer, CFStringRef key);

/**
 * @brief Try to map and attach kCVImageBufferYCbCrMatrixKey, 
 * kCVImageBufferColorPrimariesKey, kCVImageBufferTransferFunctionKey and 
 * kCVImageBufferGammaLevelKey if correspondance is found from a video_format_t.
 * Attachments can be optionally kept if already present or overwritten
 * 
 * @param cvpx The image buffer where properties will be attached
 * @param fmt The video format that contains the source color properties
 */
void cvpx_attach_mapped_color_properties(CVPixelBufferRef cvpx, 
                                         const video_format_t *fmt);
/**
 * @brief Check if current system has at least one metal GPU device
 * 
 * @return true if there's at least one metal device available
 * @return false if there's no metal device
 */
bool cvpx_system_has_metal_device();

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
