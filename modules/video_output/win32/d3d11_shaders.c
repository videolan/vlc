/*****************************************************************************
 * d3d11_shaders.c: Direct3D11 Shaders
 *****************************************************************************
 * Copyright (C) 2017 VLC authors and VideoLAN
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

#if !defined(_WIN32_WINNT) || _WIN32_WINNT < _WIN32_WINNT_WIN7
# undef _WIN32_WINNT
# define _WIN32_WINNT _WIN32_WINNT_WIN7
#endif

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>

#include <assert.h>

#define COBJMACROS
#include <d3d11.h>

#include "d3d11_shaders.h"

#if !VLC_WINSTORE_APP
# define D3DCompile(args...)                    hd3d->OurD3DCompile(args)
#endif

#undef D3D11_CompileShader
ID3DBlob* D3D11_CompileShader(vlc_object_t *obj, const d3d11_handle_t *hd3d, const d3d11_device_t *d3d_dev,
                              const char *psz_shader, bool pixel)
{
    ID3DBlob* pShaderBlob = NULL, *pErrBlob;
    const char *target;
    if (pixel)
    {
        if (likely(d3d_dev->feature_level >= D3D_FEATURE_LEVEL_10_0))
            target = "ps_4_0";
        else if (d3d_dev->feature_level >= D3D_FEATURE_LEVEL_9_3)
            target = "ps_4_0_level_9_3";
        else
            target = "ps_4_0_level_9_1";
    }
    else
    {
        if (likely(d3d_dev->feature_level >= D3D_FEATURE_LEVEL_10_0))
            target = "vs_4_0";
        else if (d3d_dev->feature_level >= D3D_FEATURE_LEVEL_9_3)
            target = "vs_4_0_level_9_3";
        else
            target = "vs_4_0_level_9_1";
    }

    HRESULT hr = D3DCompile(psz_shader, strlen(psz_shader),
                            NULL, NULL, NULL, "main", target,
                            0, 0, &pShaderBlob, &pErrBlob);

    if (FAILED(hr)) {
        char *err = pErrBlob ? ID3D10Blob_GetBufferPointer(pErrBlob) : NULL;
        msg_Err(obj, "invalid %s Shader (hr=0x%lX): %s", pixel?"Pixel":"Vertex", hr, err );
        if (pErrBlob)
            ID3D10Blob_Release(pErrBlob);
        return NULL;
    }
    return pShaderBlob;
}


