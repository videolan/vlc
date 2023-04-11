// SPDX-License-Identifier: LGPL-2.1-or-later
/*****************************************************************************
 * d3d11_scaler: Direct3D11 VideoProcessor based output scaling
 *****************************************************************************
 * Copyright © 2023 Videolabs, VLC authors and VideoLAN
 *
 * Authors: Chilledheart <hukeyue@hotmail.com>
 *          Steve Lhomme <robux4@videolabs.io>
 *****************************************************************************/

#ifndef VLC_D3D11_SCALER_H
#define VLC_D3D11_SCALER_H

#include "../../video_chroma/d3d11_fmt.h"
#include <vlc_vout_display.h>

#ifdef __cplusplus
extern "C" {
#endif

struct d3d11_scaler;

struct d3d11_scaler *D3D11_UpscalerCreate(vlc_object_t *, d3d11_device_t*, vlc_fourcc_t i_chroma);
void D3D11_UpscalerDestroy(struct d3d11_scaler *);
int D3D11_UpscalerUpdate(vlc_object_t *, struct d3d11_scaler *, d3d11_device_t*,
                         const video_format_t *, video_format_t *,
                         const vout_display_placement *);
int D3D11_UpscalerScale(vlc_object_t *, struct d3d11_scaler *, picture_sys_d3d11_t *);
bool D3D11_UpscalerUsed(const struct d3d11_scaler *);
void D3D11_UpscalerGetSRV(const struct d3d11_scaler *, ID3D11ShaderResourceView *SRV[DXGI_MAX_SHADER_VIEW]);
void D3D11_UpscalerGetSize(const struct d3d11_scaler *, unsigned *i_width, unsigned *i_height);

#ifdef __cplusplus
}
#endif

#endif // VLC_D3D11_SCALER_H
