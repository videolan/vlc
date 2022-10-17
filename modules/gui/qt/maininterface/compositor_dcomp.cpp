/*****************************************************************************
 * Copyright (C) 2020 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "compositor_dcomp.hpp"

#include "maininterface/mainctx_win32.hpp"

#include <comdef.h>

#include <QApplication>
#include <QDesktopWidget>
#include <QQuickWidget>
#include <QLibrary>
#include <QScreen>

#include <QOpenGLFunctions>
#include <QOpenGLFramebufferObject>
#include <QOpenGLExtraFunctions>

#include QPNI_HEADER
#include "compositor_dcomp_error.hpp"
#include "maininterface/interface_window_handler.hpp"

namespace vlc {

using namespace Microsoft::WRL;

//Signature for DCompositionCreateDevice
typedef HRESULT (WINAPI* DCompositionCreateDeviceFun)(IDXGIDevice *dxgiDevice, REFIID iid, void** dcompositionDevice);

int CompositorDirectComposition::windowEnable(const vlc_window_cfg_t *)
{
    if (!m_videoVisual)
    {
        msg_Err(m_intf, "m_videoVisual is null");
        return VLC_EGENERIC;
    }

    try
    {
        commonWindowEnable();
        HR(m_rootVisual->AddVisual(m_videoVisual.Get(), FALSE, m_uiVisual.Get()), "add video visual to root");
        HR(m_dcompDevice->Commit(), "commit");
    }
    catch (const DXError& err)
    {
        msg_Err(m_intf, "failed to enable window: %s code 0x%lX", err.what(), err.code());
        return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

void CompositorDirectComposition::windowDisable()
{
    try
    {
        commonWindowDisable();
        HR(m_rootVisual->RemoveVisual(m_videoVisual.Get()), "remove video visual from root");
        HR(m_dcompDevice->Commit(), "commit");
    }
    catch (const DXError& err)
    {
        msg_Err(m_intf, "failed to disable window: '%s' code: 0x%lX", err.what(), err.code());
    }
}

void CompositorDirectComposition::windowDestroy()
{
    m_videoVisual.Reset();
    CompositorVideo::windowDestroy();
}

CompositorDirectComposition::CompositorDirectComposition( qt_intf_t* p_intf,  QObject *parent)
    : CompositorVideo(p_intf, parent)
{
}

CompositorDirectComposition::~CompositorDirectComposition()
{
    destroyMainInterface();
    m_dcompDevice.Reset();
    m_d3d11Device.Reset();
    if (m_dcomp_dll)
        FreeLibrary(m_dcomp_dll);
}

bool CompositorDirectComposition::preInit(qt_intf_t * p_intf)
{
    //import DirectComposition API (WIN8+)
    QLibrary dcompDll("DCOMP.dll");
    if (!dcompDll.load())
        return false;
    DCompositionCreateDeviceFun myDCompositionCreateDevice = (DCompositionCreateDeviceFun)dcompDll.resolve("DCompositionCreateDevice");
    if (!myDCompositionCreateDevice)
    {
        msg_Dbg(p_intf, "Direct Composition is not present, can't initialize direct composition");
        return false;
    }

    //check whether D3DCompiler is available. whitout it Angle won't work
    QLibrary d3dCompilerDll;
    for (int i = 47; i > 41; --i)
    {
        d3dCompilerDll.setFileName(QString("D3DCOMPILER_%1.dll").arg(i));
        if (d3dCompilerDll.load())
            break;
    }
    if (!d3dCompilerDll.isLoaded())
    {
        msg_Dbg(p_intf, "can't find d3dcompiler_xx.dll, can't initialize direct composition");
        return false;
    }

    HRESULT hr;
    UINT creationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT
        //| D3D11_CREATE_DEVICE_DEBUG
            ;

    D3D_FEATURE_LEVEL requestedFeatureLevels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
    };

    ComPtr<ID3D11Device> d3dDevice;
    hr = D3D11CreateDevice(
        nullptr,    // Adapter
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,    // Module
        creationFlags,
        requestedFeatureLevels,
        ARRAY_SIZE(requestedFeatureLevels),
        D3D11_SDK_VERSION,
        &d3dDevice,
        nullptr,    // Actual feature level
        nullptr);

    if (FAILED(hr))
    {
        msg_Dbg(p_intf, "can't create D3D11 device, can't initialize direct composition");
        return false;
    }

    //check that we can create a shared texture
    D3D11_FEATURE_DATA_D3D11_OPTIONS d3d11Options;
    HRESULT checkFeatureHR = d3dDevice->CheckFeatureSupport(D3D11_FEATURE_D3D11_OPTIONS, &d3d11Options, sizeof(d3d11Options));

    D3D11_TEXTURE2D_DESC texDesc = { };
    texDesc.MipLevels = 1;
    texDesc.SampleDesc.Count = 1;
    texDesc.MiscFlags = 0;
    texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.CPUAccessFlags = 0;
    texDesc.ArraySize = 1;
    texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    texDesc.Height = 16;
    texDesc.Width  = 16;
    if (SUCCEEDED(checkFeatureHR) && d3d11Options.ExtendedResourceSharing) //D3D11.1 feature
        texDesc.MiscFlags = D3D11_RESOURCE_MISC_SHARED | D3D11_RESOURCE_MISC_SHARED_NTHANDLE;
    else
        texDesc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;

    ComPtr<ID3D11Texture2D> d3dTmpTexture;
    hr = d3dDevice->CreateTexture2D( &texDesc, NULL, &d3dTmpTexture );
    if (FAILED(hr))
    {
        msg_Dbg(p_intf, "can't create shared texture, can't initialize direct composition");
        return false;
    }

    //sanity check succeeded, we can now setup global Qt settings

    //force usage of ANGLE backend
    QApplication::setAttribute( Qt::AA_UseOpenGLES );

    return true;
}

bool CompositorDirectComposition::init()
{
    //import DirectComposition API (WIN8+)
    m_dcomp_dll = LoadLibrary(TEXT("DCOMP.dll"));
    if (!m_dcomp_dll)
        return false;
    DCompositionCreateDeviceFun myDCompositionCreateDevice = (DCompositionCreateDeviceFun)GetProcAddress(m_dcomp_dll, "DCompositionCreateDevice");
    if (!myDCompositionCreateDevice)
    {
        FreeLibrary(m_dcomp_dll);
        m_dcomp_dll = nullptr;
        return false;
    }

    HRESULT hr;
    UINT creationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT
        //| D3D11_CREATE_DEVICE_DEBUG
            ;

    D3D_FEATURE_LEVEL requestedFeatureLevels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
    };

    hr = D3D11CreateDevice(
        nullptr,    // Adapter
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,    // Module
        creationFlags,
        requestedFeatureLevels,
        ARRAY_SIZE(requestedFeatureLevels),
        D3D11_SDK_VERSION,
        &m_d3d11Device,
        nullptr,    // Actual feature level
        nullptr);

    if (FAILED(hr))
        return false;

    ComPtr<IDXGIDevice> dxgiDevice;
    m_d3d11Device.As(&dxgiDevice);

    // Create the DirectComposition device object.
    hr = myDCompositionCreateDevice(dxgiDevice.Get(), IID_PPV_ARGS(&m_dcompDevice));
    if (FAILED(hr))
        return false;

    return true;
}

bool CompositorDirectComposition::makeMainInterface(MainCtx* mainCtx)
{
    try
    {
        bool ret;
        m_mainCtx = mainCtx;

        m_rootWindow = new QWindow();

        m_videoWindowHandler = std::make_unique<VideoWindowHandler>(m_intf);
        m_videoWindowHandler->setWindow( m_rootWindow );

        HR(m_dcompDevice->CreateTargetForHwnd((HWND)m_rootWindow->winId(), TRUE, &m_dcompTarget), "create target");
        HR(m_dcompDevice->CreateVisual(&m_rootVisual), "create root visual");
        HR(m_dcompTarget->SetRoot(m_rootVisual.Get()), "set root visual");

        HR(m_dcompDevice->CreateVisual(&m_uiVisual), "create ui visual");

        m_uiSurface  = std::make_unique<CompositorDCompositionUISurface>(m_intf,
                                                                         m_rootWindow,
                                                                         m_uiVisual);
        ret = m_uiSurface->init();
        if (!ret)
            return false;

        ret = commonGUICreate(m_rootWindow, nullptr, m_uiSurface.get(), CompositorVideo::CAN_SHOW_PIP);
        if (!ret)
            return false;

        HR(m_rootVisual->AddVisual(m_uiVisual.Get(), FALSE, nullptr), "add ui visual to root");
        HR(m_dcompDevice->Commit(), "commit UI visual");

        auto resetAcrylicSurface = [this](QScreen * = nullptr)
        {
            m_acrylicSurface.reset(new CompositorDCompositionAcrylicSurface(m_intf, this, m_mainCtx, m_d3d11Device.Get()));
        };

        resetAcrylicSurface();
        connect(qGuiApp, &QGuiApplication::screenAdded, this, resetAcrylicSurface);
        connect(qGuiApp, &QGuiApplication::screenRemoved, this, resetAcrylicSurface);

        m_rootWindow->show();
        return true;
    }
    catch (const DXError& err)
    {
        msg_Err(m_intf, "failed to initialise compositor: '%s' code: 0x%lX", err.what(), err.code());
        return false;
    }
}

void CompositorDirectComposition::onSurfacePositionChanged(const QPointF& position)
{
    HR(m_videoVisual->SetOffsetX(position.x()));
    HR(m_videoVisual->SetOffsetY(position.y()));
    HR(m_dcompDevice->Commit(), "commit UI visual");
}

void CompositorDirectComposition::onSurfaceSizeChanged(const QSizeF&)
{
    //N/A
}

void CompositorDirectComposition::destroyMainInterface()
{
    if (m_videoVisual)
        msg_Err(m_intf, "video surface still active while destroying main interface");

    commonIntfDestroy();

    m_rootVisual.Reset();
    m_dcompTarget.Reset();
    if (m_rootWindow)
    {
        delete m_rootWindow;
        m_rootWindow = nullptr;
    }
}

void CompositorDirectComposition::unloadGUI()

{
    if (m_uiVisual)
    {
        m_rootVisual->RemoveVisual(m_uiVisual.Get());
        m_uiVisual.Reset();
    }
    m_acrylicSurface.reset();
    m_uiSurface.reset();
    commonGUIDestroy();
}

bool CompositorDirectComposition::setupVoutWindow(vlc_window_t *p_wnd, VoutDestroyCb destroyCb)
{
    //Only the first video is embedded
    if (m_videoVisual.Get())
        return false;

    HRESULT hr = m_dcompDevice->CreateVisual(&m_videoVisual);
    if (FAILED(hr))
    {
        msg_Err(p_wnd, "create to create DComp video visual");
        return false;
    }

    commonSetupVoutWindow(p_wnd, destroyCb);
    p_wnd->type = VLC_WINDOW_TYPE_DCOMP;
    p_wnd->display.dcomp_device = m_dcompDevice.Get();
    p_wnd->handle.dcomp_visual = m_videoVisual.Get();
    return true;
}

QWindow *CompositorDirectComposition::interfaceMainWindow() const
{
    return m_rootWindow;
}

Compositor::Type CompositorDirectComposition::type() const
{
    return Compositor::DirectCompositionCompositor;
}

void CompositorDirectComposition::addVisual(Microsoft::WRL::ComPtr<IDCompositionVisual> visual)
{
    vlc_assert(m_rootVisual);

    HRESULT hr = m_rootVisual->AddVisual(visual.Get(), TRUE, NULL);
    if (FAILED(hr))
        msg_Err(m_intf, "failed to add visual, code: 0x%lX", hr);

    m_dcompDevice->Commit();
}

void CompositorDirectComposition::removeVisual(Microsoft::WRL::ComPtr<IDCompositionVisual> visual)
{
    auto hr = m_rootVisual->RemoveVisual(visual.Get());
    if (FAILED(hr))
        msg_Err(m_intf, "failed to remove visual, code: 0x%lX", hr);

    m_dcompDevice->Commit();
}

QQuickItem * CompositorDirectComposition::activeFocusItem() const /* override */
{
    return m_uiSurface->activeFocusItem();
}

}
