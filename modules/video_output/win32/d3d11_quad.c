/*****************************************************************************
 * d3d11_quad.c: Direct3D11 Qaud handling
 *****************************************************************************
 * Copyright (C) 2017-2018 VLC authors and VideoLAN
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

#include <assert.h>
#include <vlc_common.h>

#if !defined(_WIN32_WINNT) || _WIN32_WINNT < _WIN32_WINNT_WIN7
# undef _WIN32_WINNT
# define _WIN32_WINNT _WIN32_WINNT_WIN7
#endif

#define COBJMACROS
#include <d3d11.h>

#include "d3d11_quad.h"

void D3D11_RenderQuad(d3d11_device_t *d3d_dev, d3d_quad_t *quad, ID3D11ShaderResourceView *resourceView[D3D11_MAX_SHADER_VIEW],
                      ID3D11RenderTargetView *d3drenderTargetView)
{
    UINT offset = 0;

    ID3D11DeviceContext_OMSetRenderTargets(d3d_dev->d3dcontext, 1, &d3drenderTargetView, NULL);

    /* Render the quad */
    /* vertex shader */
    ID3D11DeviceContext_IASetVertexBuffers(d3d_dev->d3dcontext, 0, 1, &quad->pVertexBuffer, &quad->vertexStride, &offset);
    ID3D11DeviceContext_IASetIndexBuffer(d3d_dev->d3dcontext, quad->pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
    if ( quad->pVertexShaderConstants )
        ID3D11DeviceContext_VSSetConstantBuffers(d3d_dev->d3dcontext, 0, 1, &quad->pVertexShaderConstants);

    ID3D11DeviceContext_VSSetShader(d3d_dev->d3dcontext, quad->d3dvertexShader, NULL, 0);

    /* pixel shader */
    ID3D11DeviceContext_PSSetShader(d3d_dev->d3dcontext, quad->d3dpixelShader, NULL, 0);

    ID3D11DeviceContext_PSSetConstantBuffers(d3d_dev->d3dcontext, 0, quad->PSConstantsCount, quad->pPixelShaderConstants);
    ID3D11DeviceContext_PSSetShaderResources(d3d_dev->d3dcontext, 0, quad->resourceCount, resourceView);

    ID3D11DeviceContext_RSSetViewports(d3d_dev->d3dcontext, 1, &quad->cropViewport);

    ID3D11DeviceContext_DrawIndexed(d3d_dev->d3dcontext, quad->indexCount, 0, 0);
}
