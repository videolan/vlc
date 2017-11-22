/*****************************************************************************
 * d3d9_instance.c: D3D9 unique device context instance
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_filter.h>
#include <vlc_picture.h>

#include <assert.h>

#define COBJMACROS
#include <d3d9.h>

#include "d3d9_filters.h"

static vlc_mutex_t inst_lock = VLC_STATIC_MUTEX;
static d3d9_device_t device = { .dev = NULL };
static size_t instances = 0;

void D3D9_FilterHoldInstance(filter_t *filter, d3d9_device_t *out, D3DSURFACE_DESC *dstDesc)
{
    out->dev = NULL;

    picture_t *pic = filter_NewPicture(filter);
    if (!pic)
        return;

    picture_sys_t *p_sys = ActivePictureSys(pic);

    vlc_mutex_lock(&inst_lock);
    if (p_sys)
    {
        if (FAILED(IDirect3DSurface9_GetDevice( p_sys->surface, &out->dev )))
            goto done;
        IDirect3DDevice9_Release(out->dev);
        if (FAILED(IDirect3DSurface9_GetDesc( p_sys->surface, dstDesc )))
        {
            out->dev = NULL;
            goto done;
        }

        if (device.dev == NULL)
        {
            device.dev = out->dev;
            instances++;
        }
    }
    else
    {
        *out = device;
        if (device.dev != NULL)
            instances++;

        memset(dstDesc, 0, sizeof(*dstDesc));
        if (filter->fmt_in.video.i_chroma == VLC_CODEC_D3D9_OPAQUE_10B)
            dstDesc->Format = MAKEFOURCC('P','0','1','0');
        else
            dstDesc->Format = MAKEFOURCC('N','V','1','2');
        dstDesc->Width  = filter->fmt_out.video.i_width;
        dstDesc->Height = filter->fmt_out.video.i_height;
    }

    out->owner = false;
    if (unlikely(out->dev == NULL))
        msg_Warn(filter, "no context available");
    else
        IDirect3DDevice9_AddRef(out->dev);

done:
    vlc_mutex_unlock(&inst_lock);

    picture_Release(pic);
}

void D3D9_FilterReleaseInstance(d3d9_device_t *d3d_dev)
{
    vlc_mutex_lock(&inst_lock);
    if (d3d_dev->dev && d3d_dev->dev == device.dev)
    {
        assert(instances != 0);
        if (--instances == 0)
            device.dev = NULL;
    }
    D3D9_ReleaseDevice(d3d_dev);
    vlc_mutex_unlock(&inst_lock);
}
