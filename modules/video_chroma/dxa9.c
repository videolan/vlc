/*****************************************************************************
 * dxa9.c : DXVA2 GPU surface conversion module for vlc
 *****************************************************************************
 * Copyright (C) 2015 VLC authors, VideoLAN and VideoLabs
 * $Id$
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_filter.h>

#include "copy.h"

static int  OpenConverter( vlc_object_t * );
static void CloseConverter( vlc_object_t * );

/*****************************************************************************
 * Module descriptor.
 *****************************************************************************/
vlc_module_begin ()
    set_description( N_("Conversions from DxVA2 to YUV") )
    set_capability( "video filter", 10 )
    set_callbacks( OpenConverter, CloseConverter )
vlc_module_end ()

#include <windows.h>
#include <d3d9.h>

struct picture_sys_t
{
    LPDIRECT3DSURFACE9 surface;
};

static bool GetLock(filter_t *p_filter, LPDIRECT3DSURFACE9 d3d,
                    D3DLOCKED_RECT *p_lock, D3DSURFACE_DESC *p_desc)
{
    if (FAILED( IDirect3DSurface9_GetDesc(d3d, p_desc)))
        return false;

    /* */
    if (FAILED(IDirect3DSurface9_LockRect(d3d, p_lock, NULL, D3DLOCK_READONLY))) {
        msg_Err(p_filter, "Failed to lock surface");
        return false;
    }

    return true;
}

static void DXA9_YV12(filter_t *p_filter, picture_t *src, picture_t *dst)
{
    copy_cache_t *p_copy_cache = (copy_cache_t*) p_filter->p_sys;

    D3DSURFACE_DESC desc;
    D3DLOCKED_RECT lock;
    if (!GetLock(p_filter, src->p_sys->surface, &lock, &desc))
        return;

    if (dst->format.i_chroma == VLC_CODEC_I420) {
        uint8_t *tmp = dst->p[1].p_pixels;
        dst->p[1].p_pixels = dst->p[2].p_pixels;
        dst->p[2].p_pixels = tmp;
    }

    if (desc.Format == MAKEFOURCC('Y','V','1','2') ||
        desc.Format == MAKEFOURCC('I','M','C','3')) {
        bool imc3 = desc.Format == MAKEFOURCC('I','M','C','3');
        size_t chroma_pitch = imc3 ? lock.Pitch : (lock.Pitch / 2);

        size_t pitch[3] = {
            lock.Pitch,
            chroma_pitch,
            chroma_pitch,
        };

        uint8_t *plane[3] = {
            (uint8_t*)lock.pBits,
            (uint8_t*)lock.pBits + pitch[0] * src->format.i_height,
            (uint8_t*)lock.pBits + pitch[0] * src->format.i_height
                                 + pitch[1] * src->format.i_height / 2,
        };

        if (imc3) {
            uint8_t *V = plane[1];
            plane[1] = plane[2];
            plane[2] = V;
        }
        CopyFromYv12(dst, plane, pitch, src->format.i_height, p_copy_cache);
    } else if (desc.Format == MAKEFOURCC('N','V','1','2')) {
        uint8_t *plane[2] = {
            lock.pBits,
            (uint8_t*)lock.pBits + lock.Pitch * src->format.i_height
        };
        size_t  pitch[2] = {
            lock.Pitch,
            lock.Pitch,
        };
        CopyFromNv12(dst, plane, pitch, src->format.i_height, p_copy_cache);
    } else {
        msg_Err(p_filter, "Unsupported DXA9 conversion from 0x%08X to YV12", desc.Format);
    }

    if (dst->format.i_chroma == VLC_CODEC_I420) {
        uint8_t *tmp = dst->p[1].p_pixels;
        dst->p[1].p_pixels = dst->p[2].p_pixels;
        dst->p[2].p_pixels = tmp;
    }

    /* */
    IDirect3DSurface9_UnlockRect(src->p_sys->surface);
}

static void DXA9_NV12(filter_t *p_filter, picture_t *src, picture_t *dst)
{
    copy_cache_t *p_copy_cache = (copy_cache_t*) p_filter->p_sys;

    D3DSURFACE_DESC desc;
    D3DLOCKED_RECT lock;
    if (!GetLock(p_filter, src->p_sys->surface, &lock, &desc))
        return;

    if (desc.Format == MAKEFOURCC('N','V','1','2')) {
        uint8_t *plane[2] = {
            lock.pBits,
            (uint8_t*)lock.pBits + lock.Pitch * src->format.i_height
        };
        size_t  pitch[2] = {
            lock.Pitch,
            lock.Pitch,
        };
        CopyFromNv12ToNv12(dst, plane, pitch, src->format.i_height, p_copy_cache);
    } else {
        msg_Err(p_filter, "Unsupported DXA9 conversion from 0x%08X to NV12", desc.Format);
    }

    /* */
    IDirect3DSurface9_UnlockRect(src->p_sys->surface);
}

VIDEO_FILTER_WRAPPER (DXA9_YV12)
VIDEO_FILTER_WRAPPER (DXA9_NV12)

static int OpenConverter( vlc_object_t *obj )
{
    filter_t *p_filter = (filter_t *)obj;
    if ( p_filter->fmt_in.video.i_chroma != VLC_CODEC_D3D9_OPAQUE )
        return VLC_EGENERIC;

    if ( p_filter->fmt_in.video.i_height != p_filter->fmt_out.video.i_height
         || p_filter->fmt_in.video.i_width != p_filter->fmt_out.video.i_width )
        return VLC_EGENERIC;

    switch( p_filter->fmt_out.video.i_chroma ) {
    case VLC_CODEC_I420:
    case VLC_CODEC_YV12:
        p_filter->pf_video_filter = DXA9_YV12_Filter;
        break;
    case VLC_CODEC_NV12:
        p_filter->pf_video_filter = DXA9_NV12_Filter;
        break;
    default:
        return VLC_EGENERIC;
    }

    copy_cache_t *p_copy_cache = calloc(1, sizeof(*p_copy_cache));
    if (!p_copy_cache)
         return VLC_ENOMEM;
    CopyInitCache(p_copy_cache, p_filter->fmt_in.video.i_width );
    p_filter->p_sys = (filter_sys_t*) p_copy_cache;

    return VLC_SUCCESS;
}

static void CloseConverter( vlc_object_t *obj )
{
    filter_t *p_filter = (filter_t *)obj;
    copy_cache_t *p_copy_cache = (copy_cache_t*) p_filter->p_sys;
    CopyCleanCache(p_copy_cache);
    free( p_copy_cache );
    p_filter->p_sys = NULL;
}
