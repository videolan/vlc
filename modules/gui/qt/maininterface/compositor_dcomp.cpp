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

#include <QtGui/qpa/qplatformnativeinterface.h>

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

#if !defined(QRhiD3D11_ACTIVE) && !defined(QRhiD3D12_ACTIVE)
#warning "Neither D3D11 nor D3D12 headers are available. compositor_dcomp will not work."
#endif

#include "compositor_dcomp_acrylicsurface.hpp"
#include "maininterface/interface_window_handler.hpp"

#include <memory>
#include <type_traits>

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
    //m_acrylicSurface should be released before the RHI context is destroyed
    assert(!m_acrylicSurface);
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

    std::unique_ptr<std::remove_pointer_t<HMODULE>, BOOL WINAPI (*)(HMODULE)>
        dcomplib(::LoadLibraryEx(TEXT("dcomp.dll"), nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32), ::FreeLibrary);

    typedef HRESULT (__stdcall *DCompositionCreateDeviceFuncPtr)(
        _In_opt_ IDXGIDevice *dxgiDevice,
        _In_ REFIID iid,
        _Outptr_ void **dcompositionDevice);
    DCompositionCreateDeviceFuncPtr func = nullptr;
    if (dcomplib)
    {
        func = reinterpret_cast<DCompositionCreateDeviceFuncPtr>(
            GetProcAddress(dcomplib.get(), "DCompositionCreateDevice"));
    }

    Microsoft::WRL::ComPtr<IDCompositionDevice> device;
    if (!func || FAILED(func(nullptr, IID_PPV_ARGS(&device))))
    {
        msg_Warn(m_intf, "Can not create DCompositionDevice. CompositorDirectComposition will not work.");
        return false;
    }

    const QString& sceneGraphBackend = qEnvironmentVariable("QT_QUICK_BACKEND");
    if (!sceneGraphBackend.isEmpty() /* if empty, RHI is used */ &&
        sceneGraphBackend != QLatin1String("rhi"))
    {
        // No RHI means no D3D11 or D3D12, the graphics API check
        // below is only relevant when RHI is in use.
        // If QT_QUICK_BACKEND is set to software or openvg, then
        // `QQuickWindow::graphicsApi()` might still report D3D11 or
        // D3D12 until the scene graph is initialized.
        // Unlike `QQuickWindow::graphicsApi()`, `sceneGraphBackend()`
        // is only valid after the window is constructed, so instead
        // of using `QQuickWindow::sceneGraphBackend()`, simply probe
        // the environment variable.
        return false;
    }

    const auto graphicsApi = QQuickWindow::graphicsApi();
    if (graphicsApi != QSGRendererInterface::Direct3D11 &&
        graphicsApi != QSGRendererInterface::Direct3D12)
        return false;

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

    if (!m_mainCtx->hasAcrylicSurface())
    {
        if (var_InheritBool(m_intf, "qt-backdrop-blur"))
        {
            try
            {
                m_acrylicSurface = std::make_unique<CompositorDCompositionAcrylicSurface>(m_intf, this, m_mainCtx, m_dcompDevice);
            }
            catch (const std::exception& exception)
            {
                if (const auto what = exception.what())
                    msg_Warn(m_intf, "%s", what);
            }
        }
    }

    {
        QMutexLocker lock(&m_setupStateLock);
        m_setupState = SetupState::Success;
        m_setupStateCond.notify_all();
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

    connect(quickViewPtr,
            &QQuickWindow::frameSwapped, // At this stage, we can be sure that QRhi and QRhiSwapChain are valid.
            this,
            &CompositorDirectComposition::setup,
            Qt::SingleShotConnection);

    {
        // Express the desire to use WS_EX_NOREDIRECTIONBITMAP, it has the following advantages:
        // - Increased performance due to not having to copy buffers (within GPU, or inter GPU-CPU unlike WS_EX_LAYERED).
        // - Win32 window background brush is invalidated, so window does not flash white when opened.
        // - No more resize artifact until the opaque scene covers the new area when the window is resized.
        // - Make it possible to use the Windows 11 22H2 native acrylic backdrop effect, because the white background does not remain as an artifact.
        //   When the window has a frame (currently it is the case for both SSD and CSD), clear color does not clear that background, which means
        //   that the window should not be exposed (transparent UI). Currently there is either the acrylic simulation visual in the background which is
        //   completely opaque or the video visual or the UI visual does not get transparent, so we don't suffer from this issue. With `WS_EX_NOREDIRECTIONBITMAP`,
        //   the background does not paint a rectangle with `WNDCLASSEX::hbrBackground`, even if `WS_EX_LAYERED` is not used, so we are fine. The scene graph
        //   does not have anything in the window to clear, so clear color can be transparent which means that the UI can be transparent and expose the window
        //   for the window to provide the (native) backdrop acrylic effect.

        // WS_EX_NOREDIRECTIONBITMAP is only compatible with the flip swapchain model and D3D backend.
        // In CompositorDirectComposition::init(), we already ensure that D3D is used (no OpenGL or Vulkan).
        // Flip model is supported since Windows 8 and Qt does not use it only if the following environment
        // variable is set. Since it targets Windows 10+, it does *not* check if flip model is supported and
        // use the legacy swapchain model. For D3D12, flip model is always used.
        const bool legacySwapchainModelIsExplicitlyWanted = qEnvironmentVariableIntValue("QT_D3D_NO_FLIP");

        const char* const envDisableRedirectionSurface = "QT_QPA_DISABLE_REDIRECTION_SURFACE";
        const bool redirectionSurfaceIsExplicitlyWanted = !qEnvironmentVariableIsEmpty(envDisableRedirectionSurface) && !qEnvironmentVariableIntValue(envDisableRedirectionSurface);

        if (!legacySwapchainModelIsExplicitlyWanted && !redirectionSurfaceIsExplicitlyWanted)
            qputenv(envDisableRedirectionSurface, "1"); // TODO: Other QQuickWindow (toolbar editor, independent popups)

        m_quickView->create();

        if (!legacySwapchainModelIsExplicitlyWanted && !redirectionSurfaceIsExplicitlyWanted)
            qunsetenv(envDisableRedirectionSurface); // NOTE: We need to disable it, otherwise regular QWidget windows would have issues
    }

    const bool ret = commonGUICreate(quickViewPtr, quickViewPtr, CompositorVideo::CAN_SHOW_PIP | CompositorVideo::HAS_ACRYLIC);

    if (!ret)
        return false;

    connect(quickViewPtr,
            &QQuickWindow::sceneGraphError,
            this,
            [this](QQuickWindow::SceneGraphError error, const QString &message) {
                qWarning() << "CompositorDComp: Scene Graph Error: " << error << ", Message: " << message;
                QMutexLocker lock(&m_setupStateLock);
                m_setupState = SetupState::Fail;
                m_setupStateCond.notify_all();
            }, static_cast<Qt::ConnectionType>(Qt::SingleShotConnection | Qt::DirectConnection));

    // Qt "terminates the application" by default if there is no connection made to the signal
    // QQuickWindow::sceneGraphError(). We need to do the same, because by the time the error
    // is reported, it will likely be too late (`makeMainInterface()` already returned true,
    // which is the latest point recovery is still possible) to recover from that error and
    // the interface will remain unfunctional. It was proposed to wait here until the scene
    // graph is done, but that was not changed in order not to slow down the application
    // start up.
    connect(quickViewPtr,
            &QQuickWindow::sceneGraphError,
            m_mainCtx,
            &MainCtx::askToQuit);

    m_quickView->setVisible(true);
    return true;
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
    m_quickView.reset();
}

void CompositorDirectComposition::unloadGUI()
{
    m_acrylicSurface.reset();
    m_interfaceWindowHandler.reset();

    //at this point we need to unload the QML content but the window still need to
    //be valid as it may still be used by the vout window.
    //we cant' just delete the qmlEngine as the QmlView as the root item is parented to the QmlView
    //setSource() to nothing will effectively destroy the root item
    if (Q_LIKELY(m_quickView))
        m_quickView->setSource(QUrl());

    commonGUIDestroy();
}

bool CompositorDirectComposition::setupVoutWindow(vlc_window_t *p_wnd, VoutDestroyCb destroyCb)
{
    if (m_wnd)
        return false;

    {
        QMutexLocker lock(&m_setupStateLock);
        while (m_setupState == SetupState::Uninitialized)
        {
            const bool ret = m_setupStateCond.wait(&m_setupStateLock);
            if (!ret)
                return false;
        }
        if (m_setupState != SetupState::Success)
        {
            return false;
        }
    }

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
            m_acrylicSurface.reset();
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
