/*****************************************************************************
 * d3d_shaders.c: Direct3D Shader APIs
 *****************************************************************************
 * Copyright (C) 2017-2021 VLC authors and VideoLAN
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

#include <vlc_common.h>

#include "d3d_shaders.h"

#if !VLC_WINSTORE_APP
static HINSTANCE Direct3D11LoadShaderLibrary(void)
{
    HINSTANCE instance = NULL;
    /* d3dcompiler_47 is the latest on windows 8.1 */
    for (int i = 47; i > 41; --i) {
        WCHAR filename[19];
        _snwprintf(filename, 19, TEXT("D3DCOMPILER_%d.dll"), i);
        instance = LoadLibrary(filename);
        if (instance) break;
    }
    return instance;
}
#endif // !VLC_WINSTORE_APP

int (D3D_InitShaders)(vlc_object_t *obj, d3d_shader_compiler_t *compiler)
{
#if !VLC_WINSTORE_APP
    compiler->compiler_dll = Direct3D11LoadShaderLibrary();
    if (!compiler->compiler_dll) {
        msg_Err(obj, "cannot load d3dcompiler.dll, aborting");
        return VLC_EGENERIC;
    }

    compiler->OurD3DCompile = (void *)GetProcAddress(compiler->compiler_dll, "D3DCompile");
    if (!compiler->OurD3DCompile) {
        msg_Err(obj, "Cannot locate reference to D3DCompile in d3dcompiler DLL");
        FreeLibrary(compiler->compiler_dll);
        return VLC_EGENERIC;
    }
#endif // !VLC_WINSTORE_APP

    return VLC_SUCCESS;
}

void D3D_ReleaseShaders(d3d_shader_compiler_t *compiler)
{
#if !VLC_WINSTORE_APP
    if (compiler->compiler_dll)
    {
        FreeLibrary(compiler->compiler_dll);
        compiler->compiler_dll = NULL;
    }
    compiler->OurD3DCompile = NULL;
#endif // !VLC_WINSTORE_APP
}
