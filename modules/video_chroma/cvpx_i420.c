/*****************************************************************************
 * cvpx_i420.c: core video buffer to picture converter
 *****************************************************************************
 * Copyright (C) 2015 Videolabs SAS
 * $Id$
 *
 * Authors: Felix Paul Kühne <fkuehne at videolan dot org>
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

#include <QuartzCore/QuartzCore.h>
#include <VideoToolbox/VideoToolbox.h>

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_filter.h>
#include "copy.h"

struct picture_sys_t {
    CVPixelBufferRef pixelBuffer;
};

static int  Activate(vlc_object_t * );
static void CVPX_I420(filter_t *, picture_t *, picture_t *);
static picture_t *CVPX_I420_Filter( filter_t *, picture_t * );

vlc_module_begin ()
set_description( N_("Conversions from CoreVideo buffers to I420") )
set_capability( "video filter", 10 )
set_callbacks( Activate, NULL )
vlc_module_end ()

static int Activate(vlc_object_t *obj)
{
    filter_t *p_filter = (filter_t *)obj;
    if (p_filter->fmt_in.video.i_chroma != VLC_CODEC_CVPX_OPAQUE)
        return VLC_EGENERIC;

    if (p_filter->fmt_in.video.i_height != p_filter->fmt_out.video.i_height
        || p_filter->fmt_in.video.i_width != p_filter->fmt_out.video.i_width)
        return VLC_EGENERIC;

    if (p_filter->fmt_out.video.i_chroma != VLC_CODEC_I420)
        return VLC_EGENERIC;

    p_filter->pf_video_filter = CVPX_I420_Filter;

    return VLC_SUCCESS;
}

VIDEO_FILTER_WRAPPER( CVPX_I420 )

static void CVPX_I420(filter_t *p_filter, picture_t *sourcePicture, picture_t *destinationPicture)
{
    VLC_UNUSED(p_filter);
    picture_sys_t *picsys = sourcePicture->p_sys;

    if (picsys == NULL)
        return;

    if (picsys->pixelBuffer == nil)
        return;

    unsigned width = CVPixelBufferGetWidthOfPlane(picsys->pixelBuffer, 0);
    unsigned height = CVPixelBufferGetHeightOfPlane(picsys->pixelBuffer, 0);

    if (width == 0 || height == 0)
        return;

    uint8_t *pp_plane[2];
    size_t pi_pitch[2];

    CVPixelBufferLockBaseAddress(picsys->pixelBuffer, kCVPixelBufferLock_ReadOnly);

    for (int i = 0; i < 2; i++) {
        pp_plane[i] = CVPixelBufferGetBaseAddressOfPlane(picsys->pixelBuffer, i);
        pi_pitch[i] = CVPixelBufferGetBytesPerRowOfPlane(picsys->pixelBuffer, i);
    }

    CopyFromNv12ToI420(destinationPicture, pp_plane, pi_pitch, height);

    CVPixelBufferUnlockBaseAddress(picsys->pixelBuffer, kCVPixelBufferLock_ReadOnly);
}
