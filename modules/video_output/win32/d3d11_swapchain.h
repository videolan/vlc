/*****************************************************************************
 * d3d11_swapchain.h: Direct3D11 swapchain handled by the display module
 *****************************************************************************
 * Copyright (C) 2014-2019 VLC authors and VideoLAN
 *
 * Authors: Martell Malone <martellmalone@gmail.com>
 *          Steve Lhomme <robux4@gmail.com>
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

#ifndef VLC_D3D11_SWAPCHAIN_H
#define VLC_D3D11_SWAPCHAIN_H

#include <vlc_common.h>
#include "dxgi_swapchain.h"
#include "../../video_chroma/d3d11_fmt.h"

#ifndef VLC_WINSTORE_APP
void *D3D11_CreateLocalSwapchainHandleHwnd(vlc_object_t *, HWND, d3d11_device_t *d3d_dev);
#if defined(HAVE_DCOMP_H)
void *D3D11_CreateLocalSwapchainHandleDComp(vlc_object_t *, void* dcompDevice, void* dcompVisual, d3d11_device_t *d3d_dev);
#endif // HAVE_DCOMP_H

void D3D11_LocalSwapchainCleanupDevice( void *opaque );
bool D3D11_LocalSwapchainUpdateOutput( void *opaque, const libvlc_video_render_cfg_t *cfg, libvlc_video_output_cfg_t *out );
bool D3D11_LocalSwapchainStartEndRendering( void *opaque, bool enter );
bool D3D11_LocalSwapchainSelectPlane( void *opaque, size_t plane, void *output );
void D3D11_LocalSwapchainSwap( void *opaque );
void D3D11_LocalSwapchainSetMetadata( void *opaque, libvlc_video_metadata_type_t, const void * );
#endif // !VLC_WINSTORE_APP

#endif /* VLC_D3D11_SWAPCHAIN_H */
