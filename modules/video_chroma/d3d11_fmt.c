/*****************************************************************************
 * d3d11_fmt.c : D3D11 helper calls
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

#include <vlc_common.h>
#include <vlc_picture.h>

#define COBJMACROS
#include <d3d11.h>
#include <assert.h>
#if !defined(NDEBUG) && defined(HAVE_DXGIDEBUG_H)
# include <initguid.h>
# include <dxgidebug.h>
#endif

#include "d3d11_fmt.h"

#include "../codec/avcodec/va_surface.h"

picture_sys_t *ActivePictureSys(picture_t *p_pic)
{
    struct va_pic_context *pic_ctx = (struct va_pic_context*)p_pic->context;
    return pic_ctx ? &pic_ctx->picsys : p_pic->p_sys;
}

void AcquirePictureSys(picture_sys_t *p_sys)
{
    for (int i=0; i<D3D11_MAX_SHADER_VIEW; i++) {
        if (p_sys->resourceView[i])
            ID3D11ShaderResourceView_AddRef(p_sys->resourceView[i]);
        if (p_sys->texture[i])
            ID3D11Texture2D_AddRef(p_sys->texture[i]);
    }
    if (p_sys->context)
        ID3D11DeviceContext_AddRef(p_sys->context);
    if (p_sys->decoder)
        ID3D11VideoDecoderOutputView_AddRef(p_sys->decoder);
    if (p_sys->processorInput)
        ID3D11VideoProcessorInputView_AddRef(p_sys->processorInput);
    if (p_sys->processorOutput)
        ID3D11VideoProcessorOutputView_AddRef(p_sys->processorOutput);
}

void ReleasePictureSys(picture_sys_t *p_sys)
{
    for (int i=0; i<D3D11_MAX_SHADER_VIEW; i++) {
        if (p_sys->resourceView[i])
            ID3D11ShaderResourceView_Release(p_sys->resourceView[i]);
        if (p_sys->texture[i])
            ID3D11Texture2D_Release(p_sys->texture[i]);
    }
    if (p_sys->context)
        ID3D11DeviceContext_Release(p_sys->context);
    if (p_sys->decoder)
        ID3D11VideoDecoderOutputView_Release(p_sys->decoder);
    if (p_sys->processorInput)
        ID3D11VideoProcessorInputView_Release(p_sys->processorInput);
    if (p_sys->processorOutput)
        ID3D11VideoProcessorOutputView_Release(p_sys->processorOutput);
}

/* map texture planes to resource views */
int AllocateShaderView(vlc_object_t *obj, ID3D11Device *d3ddevice,
                              const d3d_format_t *format,
                              ID3D11Texture2D *p_texture[D3D11_MAX_SHADER_VIEW], UINT slice_index,
                              ID3D11ShaderResourceView *resourceView[D3D11_MAX_SHADER_VIEW])
{
    HRESULT hr;
    int i;
    D3D11_SHADER_RESOURCE_VIEW_DESC resviewDesc = { 0 };
    D3D11_TEXTURE2D_DESC texDesc;
    ID3D11Texture2D_GetDesc(p_texture[0], &texDesc);
    assert(texDesc.BindFlags & D3D11_BIND_SHADER_RESOURCE);

    if (texDesc.ArraySize == 1)
    {
        resviewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        resviewDesc.Texture2D.MipLevels = 1;
    }
    else
    {
        resviewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
        resviewDesc.Texture2DArray.MipLevels = -1;
        resviewDesc.Texture2DArray.ArraySize = 1;
        resviewDesc.Texture2DArray.FirstArraySlice = slice_index;
        assert(slice_index < texDesc.ArraySize);
    }
    for (i=0; i<D3D11_MAX_SHADER_VIEW; i++)
    {
        resviewDesc.Format = format->resourceFormat[i];
        if (resviewDesc.Format == DXGI_FORMAT_UNKNOWN)
            resourceView[i] = NULL;
        else
        {
            hr = ID3D11Device_CreateShaderResourceView(d3ddevice, (ID3D11Resource*)p_texture[i], &resviewDesc, &resourceView[i]);
            if (FAILED(hr)) {
                msg_Err(obj, "Could not Create the Texture ResourceView %d slice %d. (hr=0x%lX)", i, slice_index, hr);
                break;
            }
        }
    }

    if (i != D3D11_MAX_SHADER_VIEW)
    {
        while (--i >= 0)
        {
            ID3D11ShaderResourceView_Release(resourceView[i]);
            resourceView[i] = NULL;
        }
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}


#if !VLC_WINSTORE_APP
static HKEY GetAdapterRegistry(DXGI_ADAPTER_DESC *adapterDesc)
{
    HKEY hKey;
    TCHAR key[128];
    TCHAR szData[256], lookup[256];
    DWORD len = 256;

    _sntprintf(lookup, 256, TEXT("pci\\ven_%04x&dev_%04x"), adapterDesc->VendorId, adapterDesc->DeviceId);
    for (int i=0;;i++)
    {
        _sntprintf(key, 128, TEXT("SYSTEM\\CurrentControlSet\\Control\\Class\\{4d36e968-e325-11ce-bfc1-08002be10318}\\%04d"), i);
        if( RegOpenKeyEx(HKEY_LOCAL_MACHINE, key, 0, KEY_READ, &hKey) != ERROR_SUCCESS )
            return NULL;

        len = sizeof(szData);
        if( RegQueryValueEx( hKey, TEXT("MatchingDeviceId"), NULL, NULL, (LPBYTE) &szData, &len ) == ERROR_SUCCESS ) {
            if (_tcsncmp(lookup, szData, _tcslen(lookup)) == 0)
                return hKey;
        }

        RegCloseKey(hKey);
    }
    return NULL;
}
#endif

#undef D3D11_GetDriverVersion
void D3D11_GetDriverVersion(vlc_object_t *obj, d3d11_device_t *d3d_dev)
{
#if VLC_WINSTORE_APP
    return;
#else
    memset(&d3d_dev->WDDM, 0, sizeof(d3d_dev->WDDM));

    IDXGIAdapter *pAdapter = D3D11DeviceAdapter(d3d_dev->d3ddevice);
    if (!pAdapter)
        return;

    DXGI_ADAPTER_DESC adapterDesc;
    HRESULT hr = IDXGIAdapter_GetDesc(pAdapter, &adapterDesc);
    IDXGIAdapter_Release(pAdapter);
    if (FAILED(hr))
        return;

    LONG err = ERROR_ACCESS_DENIED;
    TCHAR szData[256];
    DWORD len = 256;
    HKEY hKey = GetAdapterRegistry(&adapterDesc);
    if (hKey == NULL)
        return;

    err = RegQueryValueEx( hKey, TEXT("DriverVersion"), NULL, NULL, (LPBYTE) &szData, &len );
    RegCloseKey(hKey);

    if (err != ERROR_SUCCESS )
        return;

    int wddm, d3d_features, revision, build;
    /* see https://msdn.microsoft.com/windows/hardware/commercialize/design/compatibility/device-graphics */
    if (_stscanf(szData, TEXT("%d.%d.%d.%d"), &wddm, &d3d_features, &revision, &build) != 4)
        return;
    d3d_dev->WDDM.wddm         = wddm;
    d3d_dev->WDDM.d3d_features = d3d_features;
    d3d_dev->WDDM.revision     = revision;
    d3d_dev->WDDM.build        = build;
    msg_Dbg(obj, "%s WDDM driver %d.%d.%d.%d", DxgiVendorStr(adapterDesc.VendorId), wddm, d3d_features, revision, build);
#endif
}

void D3D11_ReleaseDevice(d3d11_device_t *d3d_dev)
{
    if (d3d_dev->d3dcontext)
    {
        ID3D11DeviceContext_Flush(d3d_dev->d3dcontext);
        ID3D11DeviceContext_Release(d3d_dev->d3dcontext);
        d3d_dev->d3dcontext = NULL;
    }
    if (d3d_dev->d3ddevice)
    {
        ID3D11Device_Release(d3d_dev->d3ddevice);
        d3d_dev->d3ddevice = NULL;
    }
}

#undef D3D11_CreateDevice
HRESULT D3D11_CreateDevice(vlc_object_t *obj, d3d11_handle_t *hd3d,
                           bool hw_decoding, d3d11_device_t *out)
{
#if !VLC_WINSTORE_APP
# define D3D11CreateDevice(args...)             pf_CreateDevice(args)
    /* */
    PFN_D3D11_CREATE_DEVICE pf_CreateDevice;
    pf_CreateDevice = (void *)GetProcAddress(hd3d->hdll, "D3D11CreateDevice");
    if (!pf_CreateDevice) {
        msg_Err(obj, "Cannot locate reference to D3D11CreateDevice ABI in DLL");
        return E_NOINTERFACE;
    }
#endif /* VLC_WINSTORE_APP */

    HRESULT hr = E_NOTIMPL;
    UINT creationFlags = 0;

    if (hw_decoding || !obj->obj.force)
        creationFlags |= D3D11_CREATE_DEVICE_VIDEO_SUPPORT;

#if !defined(NDEBUG)
# if !VLC_WINSTORE_APP
    if (IsDebuggerPresent())
# endif /* VLC_WINSTORE_APP */
    {
        HINSTANCE sdklayer_dll = LoadLibrary(TEXT("d3d11_1sdklayers.dll"));
        if (sdklayer_dll) {
            creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
            FreeLibrary(sdklayer_dll);
        }
    }
#endif

    static const D3D_DRIVER_TYPE driverAttempts[] = {
        D3D_DRIVER_TYPE_HARDWARE,
#if 0 /* ifndef NDEBUG */
        D3D_DRIVER_TYPE_REFERENCE,
#endif
    };

    static D3D_FEATURE_LEVEL D3D11_features[] = {
        D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0,
        D3D_FEATURE_LEVEL_9_3, D3D_FEATURE_LEVEL_9_2, D3D_FEATURE_LEVEL_9_1
    };

    for (UINT driver = 0; driver < ARRAY_SIZE(driverAttempts); driver++) {
        D3D_FEATURE_LEVEL i_feature_level;
        hr = D3D11CreateDevice(NULL, driverAttempts[driver], NULL, creationFlags,
                    D3D11_features, ARRAY_SIZE(D3D11_features), D3D11_SDK_VERSION,
                    &out->d3ddevice, &i_feature_level, &out->d3dcontext);
        if (SUCCEEDED(hr)) {
#ifndef NDEBUG
            msg_Dbg(obj, "Created the D3D11 device 0x%p ctx 0x%p type %d level %x.",
                    (void *)out->d3ddevice, (void *)out->d3dcontext,
                    driverAttempts[driver], i_feature_level);
            D3D11_GetDriverVersion( obj, out );
#endif
            /* we can work with legacy levels but only if forced */
            if ( obj->obj.force || i_feature_level >= D3D_FEATURE_LEVEL_11_0 )
                break;
            msg_Dbg(obj, "Incompatible feature level %x", i_feature_level);
            ID3D11DeviceContext_Release(out->d3dcontext);
            ID3D11Device_Release(out->d3ddevice);
            out->d3dcontext = NULL;
            out->d3ddevice = NULL;
            hr = E_NOTIMPL;
        }
    }

    if (SUCCEEDED(hr))
        out->owner = true;

    return hr;
}

IDXGIAdapter *D3D11DeviceAdapter(ID3D11Device *d3ddev)
{
    IDXGIDevice *pDXGIDevice = NULL;
    HRESULT hr = ID3D11Device_QueryInterface(d3ddev, &IID_IDXGIDevice, (void **)&pDXGIDevice);
    if (FAILED(hr)) {
        return NULL;
    }

    IDXGIAdapter *p_adapter;
    hr = IDXGIDevice_GetAdapter(pDXGIDevice, &p_adapter);
    IDXGIDevice_Release(pDXGIDevice);
    if (FAILED(hr)) {
        return NULL;
    }
    return p_adapter;
}

bool isXboxHardware(ID3D11Device *d3ddev)
{
    IDXGIAdapter *p_adapter = D3D11DeviceAdapter(d3ddev);
    if (!p_adapter)
        return NULL;

    bool result = false;
    DXGI_ADAPTER_DESC adapterDesc;
    if (SUCCEEDED(IDXGIAdapter_GetDesc(p_adapter, &adapterDesc))) {
        if (adapterDesc.VendorId == 0 &&
            adapterDesc.DeviceId == 0 &&
            !wcscmp(L"ROOT\\SraKmd\\0000", adapterDesc.Description))
            result = true;
    }

    IDXGIAdapter_Release(p_adapter);
    return result;
}

static bool isNvidiaHardware(ID3D11Device *d3ddev)
{
    IDXGIAdapter *p_adapter = D3D11DeviceAdapter(d3ddev);
    if (!p_adapter)
        return false;

    DXGI_ADAPTER_DESC adapterDesc;
    if (FAILED(IDXGIAdapter_GetDesc(p_adapter, &adapterDesc)))
        adapterDesc.VendorId = 0;
    IDXGIAdapter_Release(p_adapter);

    return adapterDesc.VendorId == GPU_MANUFACTURER_NVIDIA;
}

bool CanUseVoutPool(d3d11_device_t *d3d_dev, UINT slices)
{
#if VLC_WINSTORE_APP
    /* Phones and the Xbox are memory constrained, rely on the d3d11va pool
     * which is always smaller, we still get direct rendering from the decoder */
    return false;
#else
    /* NVIDIA cards crash when calling CreateVideoDecoderOutputView
     * on more than 30 slices */
    return slices <= 30 || !isNvidiaHardware(d3d_dev->d3ddevice);
#endif
}

int D3D11CheckDriverVersion(d3d11_device_t *d3d_dev, UINT vendorId, const struct wddm_version *min_ver)
{
    IDXGIAdapter *pAdapter = D3D11DeviceAdapter(d3d_dev->d3ddevice);
    if (!pAdapter)
        return VLC_EGENERIC;

    DXGI_ADAPTER_DESC adapterDesc;
    HRESULT hr = IDXGIAdapter_GetDesc(pAdapter, &adapterDesc);
    IDXGIAdapter_Release(pAdapter);
    if (FAILED(hr))
        return VLC_EGENERIC;

    if (vendorId && adapterDesc.VendorId != vendorId)
        return VLC_SUCCESS;

#if VLC_WINSTORE_APP
    return VLC_EGENERIC;
#else
    bool newer =
           d3d_dev->WDDM.wddm > min_ver->wddm ||
          (d3d_dev->WDDM.wddm == min_ver->wddm && (d3d_dev->WDDM.d3d_features > min_ver->d3d_features ||
                                    (d3d_dev->WDDM.d3d_features == min_ver->d3d_features &&
                                                (d3d_dev->WDDM.revision > min_ver->revision ||
                                                (d3d_dev->WDDM.revision == min_ver->revision &&
                                                       d3d_dev->WDDM.build > min_ver->build)))));

    return newer ? VLC_SUCCESS : VLC_EGENERIC;
#endif
}

const d3d_format_t *FindD3D11Format(ID3D11Device *d3ddevice,
                                    vlc_fourcc_t i_src_chroma,
                                    bool rgb_only,
                                    uint8_t bits_per_channel,
                                    bool allow_opaque,
                                    UINT supportFlags)
{
    supportFlags |= D3D11_FORMAT_SUPPORT_TEXTURE2D;
    for (const d3d_format_t *output_format = GetRenderFormatList();
         output_format->name != NULL; ++output_format)
    {
        if (i_src_chroma && i_src_chroma != output_format->fourcc)
            continue;
        if (bits_per_channel && bits_per_channel > output_format->bitsPerChannel)
            continue;
        if (!allow_opaque && is_d3d11_opaque(output_format->fourcc))
            continue;
        if (rgb_only && vlc_fourcc_IsYUV(output_format->fourcc))
            continue;

        DXGI_FORMAT textureFormat;
        if (output_format->formatTexture == DXGI_FORMAT_UNKNOWN)
            textureFormat = output_format->resourceFormat[0];
        else
            textureFormat = output_format->formatTexture;

        if( DeviceSupportsFormat( d3ddevice, textureFormat, supportFlags ) )
            return output_format;
    }
    return NULL;
}

int AllocateTextures( vlc_object_t *obj, d3d11_device_t *d3d_dev,
                      const d3d_format_t *cfg, const video_format_t *fmt,
                      unsigned pool_size, ID3D11Texture2D *textures[] )
{
    plane_t planes[PICTURE_PLANE_MAX];
    int plane, plane_count;
    HRESULT hr;
    ID3D11Texture2D *slicedTexture = NULL;
    D3D11_TEXTURE2D_DESC texDesc;
    ZeroMemory(&texDesc, sizeof(texDesc));
    texDesc.MipLevels = 1;
    texDesc.SampleDesc.Count = 1;
    texDesc.MiscFlags = 0; //D3D11_RESOURCE_MISC_SHARED;
    texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    if (is_d3d11_opaque(fmt->i_chroma)) {
        texDesc.BindFlags |= D3D11_BIND_DECODER;
        texDesc.Usage = D3D11_USAGE_DEFAULT;
        texDesc.CPUAccessFlags = 0;
    } else {
        texDesc.Usage = D3D11_USAGE_DYNAMIC;
        texDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    }
    texDesc.ArraySize = pool_size;

    const vlc_chroma_description_t *p_chroma_desc = vlc_fourcc_GetChromaDescription( fmt->i_chroma );
    if( !p_chroma_desc )
        return VLC_EGENERIC;

    if (cfg->formatTexture == DXGI_FORMAT_UNKNOWN) {
        if (p_chroma_desc->plane_count == 0)
        {
            msg_Dbg(obj, "failed to get the pixel format planes for %4.4s", (char *)&fmt->i_chroma);
            return VLC_EGENERIC;
        }
        assert(p_chroma_desc->plane_count <= D3D11_MAX_SHADER_VIEW);
        plane_count = p_chroma_desc->plane_count;

        texDesc.Format = cfg->resourceFormat[0];
        assert(cfg->resourceFormat[1] == cfg->resourceFormat[0]);
        assert(cfg->resourceFormat[2] == cfg->resourceFormat[0]);

        for( int i = 0; i < plane_count; i++ )
        {
            plane_t *p = &planes[i];

            p->i_lines         = fmt->i_height * p_chroma_desc->p[i].h.num / p_chroma_desc->p[i].h.den;
            p->i_visible_lines = fmt->i_visible_height * p_chroma_desc->p[i].h.num / p_chroma_desc->p[i].h.den;
            p->i_pitch         = fmt->i_width * p_chroma_desc->p[i].w.num / p_chroma_desc->p[i].w.den * p_chroma_desc->pixel_size;
            p->i_visible_pitch = fmt->i_visible_width * p_chroma_desc->p[i].w.num / p_chroma_desc->p[i].w.den * p_chroma_desc->pixel_size;
            p->i_pixel_pitch   = p_chroma_desc->pixel_size;
        }
    } else {
        plane_count = 1;
        texDesc.Format = cfg->formatTexture;
        texDesc.Height = fmt->i_height;
        texDesc.Width = fmt->i_width;

        hr = ID3D11Device_CreateTexture2D( d3d_dev->d3ddevice, &texDesc, NULL, &slicedTexture );
        if (FAILED(hr)) {
            msg_Err(obj, "CreateTexture2D failed for the %d pool. (hr=0x%0lx)", pool_size, hr);
            goto error;
        }
    }

    for (unsigned picture_count = 0; picture_count < pool_size; picture_count++) {
        for (plane = 0; plane < plane_count; plane++)
        {
            if (slicedTexture) {
                textures[picture_count * D3D11_MAX_SHADER_VIEW + plane] = slicedTexture;
                ID3D11Texture2D_AddRef(slicedTexture);
            } else {
                texDesc.Height = fmt->i_height * p_chroma_desc->p[plane].h.num / p_chroma_desc->p[plane].h.den;
                texDesc.Width = fmt->i_width * p_chroma_desc->p[plane].w.num / p_chroma_desc->p[plane].w.den;
                hr = ID3D11Device_CreateTexture2D( d3d_dev->d3ddevice, &texDesc, NULL, &textures[picture_count * D3D11_MAX_SHADER_VIEW + plane] );
                if (FAILED(hr)) {
                    msg_Err(obj, "CreateTexture2D failed for the %d pool. (hr=0x%0lx)", pool_size, hr);
                    goto error;
                }
            }
        }
        for (; plane < D3D11_MAX_SHADER_VIEW; plane++) {
            if (!cfg->resourceFormat[plane])
                textures[picture_count * D3D11_MAX_SHADER_VIEW + plane] = NULL;
            else
            {
                textures[picture_count * D3D11_MAX_SHADER_VIEW + plane] = textures[picture_count * D3D11_MAX_SHADER_VIEW];
                ID3D11Texture2D_AddRef(textures[picture_count * D3D11_MAX_SHADER_VIEW + plane]);
            }
        }
    }

    if (!is_d3d11_opaque(fmt->i_chroma) && cfg->formatTexture != DXGI_FORMAT_UNKNOWN) {
        D3D11_MAPPED_SUBRESOURCE mappedResource;
        hr = ID3D11DeviceContext_Map(d3d_dev->d3dcontext, (ID3D11Resource*)textures[0], 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
        if( FAILED(hr) ) {
            msg_Err(obj, "The texture cannot be mapped. (hr=0x%lX)", hr);
            goto error;
        }
        ID3D11DeviceContext_Unmap(d3d_dev->d3dcontext, (ID3D11Resource*)textures[0], 0);
        if (mappedResource.RowPitch < p_chroma_desc->pixel_size * texDesc.Width) {
            msg_Err( obj, "The texture row pitch is too small (%d instead of %d)", mappedResource.RowPitch,
                     p_chroma_desc->pixel_size * texDesc.Width );
            goto error;
        }
        if ( fmt->i_width > 64 &&
             mappedResource.RowPitch >=
             2* (fmt->i_width * p_chroma_desc->p[0].w.num / p_chroma_desc->p[0].w.den * p_chroma_desc->pixel_size) )
        {
            msg_Err(obj, "Bogus %4.4s pitch detected. %d vs %d", (const char*)&fmt->i_chroma,
                    mappedResource.RowPitch,
                    (fmt->i_width * p_chroma_desc->p[0].w.num / p_chroma_desc->p[0].w.den * p_chroma_desc->pixel_size));
            goto error;
        }
    }

    if (slicedTexture)
        ID3D11Texture2D_Release(slicedTexture);
    return VLC_SUCCESS;
error:
    if (slicedTexture)
        ID3D11Texture2D_Release(slicedTexture);
    return VLC_EGENERIC;
}

#undef D3D11_Create
int D3D11_Create(vlc_object_t *obj, d3d11_handle_t *hd3d)
{
#if !VLC_WINSTORE_APP
    hd3d->hdll = LoadLibrary(TEXT("D3D11.DLL"));
    if (!hd3d->hdll)
    {
        msg_Warn(obj, "cannot load d3d11.dll, aborting");
        return VLC_EGENERIC;
    }

# if !defined(NDEBUG) && defined(HAVE_DXGIDEBUG_H)
    if (IsDebuggerPresent())
    {
        hd3d->dxgidebug_dll = LoadLibrary(TEXT("DXGIDEBUG.DLL"));
        HRESULT (WINAPI * pf_DXGIGetDebugInterface)(const GUID *riid, void **ppDebug) = NULL;
        if (hd3d->dxgidebug_dll)
            pf_DXGIGetDebugInterface =
                    (void *)GetProcAddress(hd3d->dxgidebug_dll, "DXGIGetDebugInterface");
        if (pf_DXGIGetDebugInterface) {
            IDXGIDebug *pDXGIDebug;
            if (SUCCEEDED(pf_DXGIGetDebugInterface(&IID_IDXGIDebug, (void**)&pDXGIDebug)))
                IDXGIDebug_ReportLiveObjects(pDXGIDebug, DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
        }
    }
# endif
#endif
    return VLC_SUCCESS;
}

void D3D11_Destroy(d3d11_handle_t *hd3d)
{
#if !VLC_WINSTORE_APP
    if (hd3d->hdll)
        FreeLibrary(hd3d->hdll);

#if !defined(NDEBUG) && defined(HAVE_DXGIDEBUG_H)
    if (hd3d->dxgidebug_dll)
        FreeLibrary(hd3d->dxgidebug_dll);
#endif
#endif
}

#ifndef NDEBUG
#undef D3D11_LogProcessorSupport
void D3D11_LogProcessorSupport(vlc_object_t *o,
                               ID3D11VideoProcessorEnumerator *processorEnumerator)
{
    UINT flags;
    HRESULT hr;
    for (int format = 0; format < 188; format++) {
        hr = ID3D11VideoProcessorEnumerator_CheckVideoProcessorFormat(processorEnumerator, format, &flags);
        if (FAILED(hr))
            continue;
        const char *name = DxgiFormatToStr(format);
        const char *support = NULL;
        if ((flags & (D3D11_VIDEO_PROCESSOR_FORMAT_SUPPORT_INPUT|D3D11_VIDEO_PROCESSOR_FORMAT_SUPPORT_OUTPUT))
                 == (D3D11_VIDEO_PROCESSOR_FORMAT_SUPPORT_INPUT|D3D11_VIDEO_PROCESSOR_FORMAT_SUPPORT_OUTPUT))
            support = "input/output";
        else if (flags & D3D11_VIDEO_PROCESSOR_FORMAT_SUPPORT_INPUT)
            support = "input";
        else if (flags & D3D11_VIDEO_PROCESSOR_FORMAT_SUPPORT_OUTPUT)
            support = "output";
        if (support)
        {
            if (name)
                msg_Dbg(o, "processor format %s is supported for %s", name, support);
            else
                msg_Dbg(o, "processor format (%d) is supported for %s", format, support);
        }
    }
}

#endif
