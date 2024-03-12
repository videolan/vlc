// SPDX-License-Identifier: LGPL-2.1-or-later
/*****************************************************************************
 * d3d11_tonemap: Direct3D11 VideoProcessor to handle tonemapping
 *****************************************************************************
 * Copyright © 2024 Videolabs, VLC authors and VideoLAN
 *
 * Authors: Steve Lhomme <robux4@videolabs.io>
 *****************************************************************************/

#ifndef VLC_D3D11_TONEMAP_H
#define VLC_D3D11_TONEMAP_H

#include "d3d11_quad.h"
#include "../../video_chroma/d3d11_fmt.h"
#include <vlc_vout_display.h>

#ifdef __cplusplus
extern "C" {
#endif

struct d3d11_tonemapper;

struct d3d11_tonemapper *D3D11_TonemapperCreate(vlc_object_t *, d3d11_device_t *,
                                                const video_format_t * in);
void D3D11_TonemapperDestroy(struct d3d11_tonemapper *);
HRESULT D3D11_TonemapperProcess(vlc_object_t *, struct d3d11_tonemapper *, picture_sys_d3d11_t *);
picture_sys_d3d11_t *D3D11_TonemapperGetOutput(struct d3d11_tonemapper *);

#ifdef __cplusplus
}
#endif

#endif // VLC_D3D11_TONEMAP_H
