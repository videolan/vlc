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

/*
 * Attach a cvpx buffer to a picture
 *
 * The cvpx ref will be released when the picture is released
 * @return VLC_SUCCESS or VLC_ENOMEM
 */
int cvpxpic_attach(picture_t *p_pic, CVPixelBufferRef cvpx);

/*
 * Get the cvpx buffer attached to a picture
 */
CVPixelBufferRef cvpxpic_get_ref(picture_t *pic);

#endif
