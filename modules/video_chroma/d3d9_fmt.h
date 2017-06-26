/*****************************************************************************
 * d3d9_fmt.h : D3D9 helper calls
 *****************************************************************************
 * Copyright © 2017 VLC authors, VideoLAN and VideoLabs
 *
 * Authors: Steve Lhomme <robux4@gmail.com>
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

#ifndef VLC_VIDEOCHROMA_D3D9_FMT_H_
#define VLC_VIDEOCHROMA_D3D9_FMT_H_

#include <vlc_picture.h>

/* owned by the vout for VLC_CODEC_D3D9_OPAQUE */
struct picture_sys_t
{
    LPDIRECT3DSURFACE9 surface;
};

#include "../codec/avcodec/va_surface.h"

static inline picture_sys_t *ActivePictureSys(picture_t *p_pic)
{
    struct va_pic_context *pic_ctx = (struct va_pic_context*)p_pic->context;
    return pic_ctx ? &pic_ctx->picsys : p_pic->p_sys;
}

static inline void AcquirePictureSys(picture_sys_t *p_sys)
{
    IDirect3DSurface9_AddRef(p_sys->surface);
}

static inline void ReleasePictureSys(picture_sys_t *p_sys)
{
    IDirect3DSurface9_Release(p_sys->surface);
}

#endif /* VLC_VIDEOCHROMA_D3D9_FMT_H_ */
