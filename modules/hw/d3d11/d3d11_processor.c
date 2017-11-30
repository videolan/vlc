/*****************************************************************************
 * d3d11_processor.c: D3D11 VideoProcessor helper
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
#include <initguid.h>
#include <d3d11.h>

#include "d3d11_processor.h"

#if defined(ID3D11VideoContext_VideoProcessorBlt)
void D3D11_ReleaseProcessor(d3d11_processor_t *out)
{
    if (out->videoProcessor)
    {
        ID3D11VideoProcessor_Release(out->videoProcessor);
        out->videoProcessor = NULL;
    }
    if (out->procEnumerator)
    {
        ID3D11VideoProcessorEnumerator_Release(out->procEnumerator);
        out->procEnumerator = NULL;
    }
    if (out->d3dviddev)
    {
        ID3D11VideoDevice_Release(out->d3dviddev);
        out->d3dviddev = NULL;
    }
    if (out->d3dvidctx)
    {
        ID3D11VideoContext_Release(out->d3dvidctx);
        out->d3dvidctx = NULL;
    }
}
#endif
