/*****************************************************************************
 * dxgi_swapchain.h: DXGI swapchain handled by the display module
 *****************************************************************************
 * Copyright (C) 2014-2021 VLC authors and VideoLAN
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

#ifndef VLC_DXGI_SWAPCHAIN_H
#define VLC_DXGI_SWAPCHAIN_H

#include <vlc_common.h>

#include <vlc/libvlc.h>
#include <vlc/libvlc_picture.h>
#include <vlc/libvlc_media.h>
#include <vlc/libvlc_renderer_discoverer.h>
#include <vlc/libvlc_media_player.h>

#include <dxgi1_5.h>
#include "../../video_chroma/dxgi_fmt.h"

#define DXGI_SWAP_FRAME_COUNT   3

struct dxgi_swapchain;

struct dxgi_swapchain *DXGI_CreateLocalSwapchainHandleHwnd(vlc_object_t *, HWND);

#ifdef HAVE_DCOMP_H
struct dxgi_swapchain *DXGI_CreateLocalSwapchainHandleDComp(vlc_object_t *,
                                           void /*IDCompositionDevice*/ * dcompDevice,
                                           void /*IDCompositionVisual*/ * dcompVisual);
#endif

IDXGISwapChain1 *DXGI_GetSwapChain1( struct dxgi_swapchain * );
IDXGISwapChain4 *DXGI_GetSwapChain4( struct dxgi_swapchain * );
const d3d_format_t  *DXGI_GetPixelFormat( struct dxgi_swapchain * );

void DXGI_SelectSwapchainColorspace( struct dxgi_swapchain *, const libvlc_video_render_cfg_t * );
void DXGI_LocalSwapchainCleanupDevice( struct dxgi_swapchain * );
void DXGI_SwapchainUpdateOutput( struct dxgi_swapchain *, libvlc_video_output_cfg_t * );
bool DXGI_UpdateSwapChain( struct dxgi_swapchain *, IDXGIAdapter *,
                           IUnknown *pFactoryDevice,
                           const d3d_format_t *, const libvlc_video_render_cfg_t * );

void DXGI_LocalSwapchainSwap( struct dxgi_swapchain * );
void DXGI_LocalSwapchainSetMetadata( struct dxgi_swapchain *, libvlc_video_metadata_type_t, const void * );

#endif /* VLC_D3D11_SWAPCHAIN_H */
