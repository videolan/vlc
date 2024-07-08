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

#include <vlc_window.h>

#ifndef QT_GUI_PRIVATE
#warning "QRhiD3D11 and QRhi headers are required for DirectComposition compositor."
#endif

#ifndef QT_CORE_PRIVATE
#warning "QSystemLibrary private header is required for DirectComposition compositor."
#endif

#include <QtGui/qpa/qplatformnativeinterface.h>
#include <QtCore/private/qsystemlibrary_p.h>

#if __has_include(<dxgi1_6.h>)

#if __has_include(<d3d11_1.h>)
#define QRhiD3D11_ACTIVE
#include <QtGui/private/qrhid3d11_p.h>
#endif

#if __has_include(<d3d12.h>) && __has_include(<d3d12sdklayers.h>)
#include <QtGui/private/qrhid3d12_p.h>
#if (QT_VERSION < QT_VERSION_CHECK(6, 7, 0)) || defined(QRHI_D3D12_AVAILABLE)
#define QRhiD3D12_ACTIVE
#endif
#endif

#endif

#if !defined(QRhiD3D11_ACTIVE) && !defined(QRhiD3D12_ACTIVE)
#warning "Neither D3D11 nor D3D12 headers are available. compositor_dcomp will not work."
#endif

#include "compositor_dcomp_acrylicsurface.hpp"
#include "maininterface/interface_window_handler.hpp"

namespace vlc {

int CompositorDirectComposition::windowEnable(const vlc_window_cfg_t *)
{
    assert(m_dcompDevice);
    assert(m_rootVisual);
    assert(m_videoVisual);
    assert(m_uiVisual);

    commonWindowEnable();

    const auto ret = m_rootVisual->AddVisual(m_videoVisual.Get(), FALSE, m_uiVisual);
    m_dcompDevice->Commit();

    if (ret == S_OK)
        return VLC_SUCCESS;
    else
        return VLC_EGENERIC;
}

void CompositorDirectComposition::windowDisable()
{
    assert(m_dcompDevice);
    assert(m_rootVisual);
    assert(m_videoVisual);

    commonWindowDisable();

    const auto ret = m_rootVisual->RemoveVisual(m_videoVisual.Get());
    assert(ret == S_OK);
    m_dcompDevice->Commit();
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
}

bool CompositorDirectComposition::init()
{
#if !defined(QRhiD3D11_ACTIVE) && !defined(QRhiD3D12_ACTIVE)
    msg_Warn(m_intf, "compositor_dcomp was not built with D3D11 or D3D12 headers. It will not work.");
    return false;
#endif

    {
        const QString& platformName = qApp->platformName();
        if (!(platformName == QLatin1String("windows") || platformName == QLatin1String("direct2d")))
            return false;
    }

    QSystemLibrary dcomplib(QLatin1String("dcomp"));

    typedef HRESULT (__stdcall *DCompositionCreateDeviceFuncPtr)(
        _In_opt_ IDXGIDevice *dxgiDevice,
        _In_ REFIID iid,
        _Outptr_ void **dcompositionDevice);
    DCompositionCreateDeviceFuncPtr func = reinterpret_cast<DCompositionCreateDeviceFuncPtr>(
        dcomplib.resolve("DCompositionCreateDevice"));

    Microsoft::WRL::ComPtr<IDCompositionDevice> device;
    if (!func || FAILED(func(nullptr, IID_PPV_ARGS(&device))))
    {
        msg_Warn(m_intf, "Can not create DCompositionDevice. CompositorDirectComposition will not work.");
        return false;
    }

    return true;
}

void CompositorDirectComposition::setup()
{
    assert(m_quickView);
    const auto rhi = m_quickView->rhi();
    assert(rhi);

    QRhiImplementation* const rhiImplementation = rhi->implementation();
    assert(rhiImplementation);
    QRhiSwapChain* const rhiSwapChain = m_quickView->swapChain();
    assert(rhiSwapChain);

    assert(m_quickView->rhi()->backend() == QRhi::D3D11 || m_quickView->rhi()->backend() == QRhi::D3D12);

    IDCompositionTarget* dcompTarget;
    if (rhi->backend() == QRhi::D3D11)
    {
#ifdef QRhiD3D11_ACTIVE
        m_dcompDevice = static_cast<QRhiD3D11*>(rhiImplementation)->dcompDevice;
        auto qswapchain = static_cast<QD3D11SwapChain*>(rhiSwapChain);
        dcompTarget = qswapchain->dcompTarget;
        m_uiVisual = qswapchain->dcompVisual;
#endif
    }
    else if (rhi->backend() == QRhi::D3D12)
    {
#ifdef QRhiD3D12_ACTIVE
        m_dcompDevice = static_cast<QRhiD3D12*>(rhiImplementation)->dcompDevice;
        auto qswapchain = static_cast<QD3D12SwapChain*>(rhiSwapChain);
        dcompTarget = qswapchain->dcompTarget;
        m_uiVisual = qswapchain->dcompVisual;
#endif
    }
    else
        Q_UNREACHABLE();

    assert(m_dcompDevice);
    assert(dcompTarget);
    assert(m_uiVisual);

    HRESULT res;
    res = m_dcompDevice->CreateVisual(&m_rootVisual);
    assert(res == S_OK);

    res = dcompTarget->SetRoot(m_rootVisual.Get());
    assert(res == S_OK);

    res = m_rootVisual->AddVisual(m_uiVisual, FALSE, NULL);
    assert(res == S_OK);

    m_dcompDevice->Commit();

    if (!m_blurBehind)
    {
        if (var_InheritBool(m_intf, "qt-backdrop-blur"))
        {
            try
            {
                m_acrylicSurface = new CompositorDCompositionAcrylicSurface(m_intf, this, m_mainCtx, m_dcompDevice);
            }
            catch (const std::exception& exception)
            {
                if (const auto what = exception.what())
                    msg_Warn(m_intf, "%s", what);
                delete m_acrylicSurface.data();
            }
        }
    }
}

bool CompositorDirectComposition::makeMainInterface(MainCtx* mainCtx)
{
    assert(mainCtx);
    m_mainCtx = mainCtx;

    m_quickView = std::make_unique<QQuickView>();
    const auto quickViewPtr = m_quickView.get();

    m_quickView->setResizeMode(QQuickView::SizeRootObjectToView);
    m_quickView->setColor(Qt::transparent);

    m_quickView->installEventFilter(this);

    bool appropriateGraphicsApi = true;

    QEventLoop eventLoop;
    connect(quickViewPtr,
            &QQuickWindow::sceneGraphInitialized,
            &eventLoop,
            [&eventLoop, &appropriateGraphicsApi, quickViewPtr, this]() {
                if (QQuickWindow::graphicsApi() == QSGRendererInterface::Direct3D11 ||
                    QQuickWindow::graphicsApi() == QSGRendererInterface::Direct3D12)
                {
                    connect(quickViewPtr,
                            &QQuickWindow::frameSwapped, // At this stage, we can be sure that QRhi and QRhiSwapChain are valid.
                            this,
                            [this, &eventLoop]() {
                                setup();
                                eventLoop.quit();
                            },
                            Qt::SingleShotConnection);
                }
                else
                {
                    appropriateGraphicsApi = false;
                    eventLoop.quit();
                }
        }, static_cast<Qt::ConnectionType>(Qt::SingleShotConnection | Qt::DirectConnection));

    connect(quickViewPtr,
            &QQuickWindow::sceneGraphError,
            &eventLoop,
            [&eventLoop, &appropriateGraphicsApi](QQuickWindow::SceneGraphError error, const QString &message) {
                qWarning() << "CompositorDComp: Scene Graph Error: " << error << ", Message: " << message;
                appropriateGraphicsApi = false;
                eventLoop.quit();
        }, static_cast<Qt::ConnectionType>(Qt::SingleShotConnection | Qt::DirectConnection));

    CompositorVideo::Flags flags = CompositorVideo::CAN_SHOW_PIP | CompositorVideo::HAS_ACRYLIC;

    m_quickView->create();

    const bool ret = commonGUICreate(quickViewPtr, quickViewPtr, flags);

    if (ret)
        m_quickView->show();
    else
        return false;

    if (!m_quickView->isSceneGraphInitialized())
        eventLoop.exec();
    return (ret && appropriateGraphicsApi);
}

void CompositorDirectComposition::onSurfacePositionChanged(const QPointF& position)
{
    assert(m_videoVisual);
    assert(m_dcompDevice);

    m_videoVisual->SetOffsetX(position.x());
    m_videoVisual->SetOffsetY(position.y());
    m_dcompDevice->Commit();
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
}

void CompositorDirectComposition::unloadGUI()
{
    m_interfaceWindowHandler.reset();
    m_quickView.reset();
    commonGUIDestroy();
}

bool CompositorDirectComposition::setupVoutWindow(vlc_window_t *p_wnd, VoutDestroyCb destroyCb)
{
    assert(m_dcompDevice);

    const HRESULT hr = m_dcompDevice->CreateVisual(&m_videoVisual);

    if (FAILED(hr))
    {
        msg_Err(p_wnd, "create to create DComp video visual");
        return false;
    }

    commonSetupVoutWindow(p_wnd, destroyCb);
    p_wnd->type = VLC_WINDOW_TYPE_DCOMP;

    p_wnd->display.dcomp_device = m_dcompDevice;
    p_wnd->handle.dcomp_visual = m_videoVisual.Get();

    return true;
}

QWindow *CompositorDirectComposition::interfaceMainWindow() const
{
    return m_quickView.get();
}

Compositor::Type CompositorDirectComposition::type() const
{
    return Compositor::DirectCompositionCompositor;
}

void CompositorDirectComposition::addVisual(IDCompositionVisual *visual)
{
    assert(visual);
    assert(m_rootVisual);
    assert(m_dcompDevice);

    HRESULT hr = m_rootVisual->AddVisual(visual, TRUE, NULL);
    if (FAILED(hr))
        msg_Err(m_intf, "failed to add visual, code: 0x%lX", hr);

    m_dcompDevice->Commit();
}

void CompositorDirectComposition::removeVisual(IDCompositionVisual *visual)
{
    assert(visual);
	assert(m_rootVisual);
	assert(m_dcompDevice);

    auto hr = m_rootVisual->RemoveVisual(visual);
    if (FAILED(hr))
        msg_Err(m_intf, "failed to remove visual, code: 0x%lX", hr);

    m_dcompDevice->Commit();
}

QQuickItem * CompositorDirectComposition::activeFocusItem() const /* override */
{
    return m_quickView->activeFocusItem();
}

bool CompositorDirectComposition::eventFilter(QObject *watched, QEvent *event)
{
    switch (event->type())
    {
    case QEvent::PlatformSurface:
        if (watched == m_quickView.get() &&
            static_cast<QPlatformSurfaceEvent *>(event)->surfaceEventType() == QPlatformSurfaceEvent::SurfaceAboutToBeDestroyed)
        {
            m_videoVisual.Reset();
            delete m_acrylicSurface.data();
            // Just in case root visual deletes its children
            // when it is deleted: (Qt's UI visual should be
            // deleted by Qt itself)
            m_rootVisual->RemoveVisual(m_uiVisual);
            m_rootVisual.Reset();

            // When the window receives the event `SurfaceAboutToBeDestroyed`,
            // the RHI and the RHI swap chain are going to be destroyed.
            // It should be noted that event filters receive events
            // before the watched object receives them.
            // Since these objects belong to Qt, we should clear them
            // in order to prevent potential dangling pointer dereference:
            m_dcompDevice = nullptr;
            m_uiVisual = nullptr;
        }
        break;
    default:
        break;
    }

    return QObject::eventFilter(watched, event);
}

}
