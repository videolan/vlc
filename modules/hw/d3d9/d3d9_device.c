/*****************************************************************************
 * d3d9_device.c : D3D9 decoder device from external IDirect3DDevice9
 *****************************************************************************
 * Copyright © 2019 VLC authors, VideoLAN and VideoLabs
 *
 * Authors: Steve Lhomme <robux4@ycbcr.xyz>
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
#include <vlc_codec.h>

#include <vlc/libvlc.h>
#include <vlc/libvlc_picture.h>
#include <vlc/libvlc_media.h>
#include <vlc/libvlc_renderer_discoverer.h>
#include <vlc/libvlc_media_player.h>

#include "d3d9_filters.h"

typedef struct {
    void                            *opaque;
    libvlc_video_output_cleanup_cb  cleanupDeviceCb;

    d3d9_decoder_device_t                    dec_device;
} d3d9_decoder_device;

static void D3D9CloseDecoderDevice(vlc_decoder_device *device)
{
    d3d9_decoder_device *sys = device->sys;

    D3D9_ReleaseDevice( &sys->dec_device.d3ddev );
    D3D9_Destroy( &sys->dec_device.hd3d );

    if ( sys->cleanupDeviceCb )
        sys->cleanupDeviceCb( sys->opaque );
    vlc_obj_free( VLC_OBJECT(device), sys );
}
static const struct vlc_decoder_device_operations d3d9_dev_ops = {
    .close = D3D9CloseDecoderDevice,
};

int D3D9OpenDecoderDevice(vlc_decoder_device *device, vout_window_t *wnd)
{
    VLC_UNUSED(wnd);
    d3d9_decoder_device *sys = vlc_obj_malloc(VLC_OBJECT(device), sizeof(*sys));
    if (unlikely(sys==NULL))
        return VLC_ENOMEM;

    int adapter;
    sys->cleanupDeviceCb = NULL;
    libvlc_video_output_setup_cb setupDeviceCb = var_InheritAddress( device, "vout-cb-setup" );
    if ( setupDeviceCb )
    {
        /* external rendering */
        libvlc_video_setup_device_info_t out = { .device_context = NULL, .adapter = 0 };
        sys->opaque          = var_InheritAddress( device, "vout-cb-opaque" );
        sys->cleanupDeviceCb = var_InheritAddress( device, "vout-cb-cleanup" );
        libvlc_video_setup_device_cfg_t cfg = {
            .hardware_decoding = true, /* ignored anyway */
        };
        if (!setupDeviceCb( &sys->opaque, &cfg, &out ))
        {
            if ( sys->cleanupDeviceCb )
                sys->cleanupDeviceCb( sys->opaque );
            goto error;
        }

        D3D9_CloneExternal( &sys->dec_device.hd3d, (IDirect3D9*) out.device_context );
        adapter = out.adapter;
    }
    else
    {
        /* internal rendering */
        if (D3D9_Create(device, &sys->dec_device.hd3d) != VLC_SUCCESS)
        {
            msg_Err( device, "Direct3D9 could not be initialized" );
            goto error;
        }
        /* find the best adapter to use, not based on the HWND used */
        adapter = -1;
    }

    HRESULT hr = D3D9_CreateDevice( device, &sys->dec_device.hd3d, adapter, &sys->dec_device.d3ddev );
    if ( FAILED(hr) )
    {
        if ( sys->cleanupDeviceCb )
            sys->cleanupDeviceCb( sys->opaque );
        D3D9_Destroy( &sys->dec_device.hd3d );
        goto error;
    }

    device->ops = &d3d9_dev_ops;
    device->opaque = &sys->dec_device;
    device->type = VLC_DECODER_DEVICE_DXVA2;
    device->sys = sys;

    return VLC_SUCCESS;
error:
    vlc_obj_free( VLC_OBJECT(device), sys );
    return VLC_EGENERIC;
}
