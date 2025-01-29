/*****************************************************************************
 * Copyright (C) 2023 VLC authors and VideoLAN
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
#include "compositor_wayland.hpp"

#include "maininterface/mainctx.hpp"
#include "maininterface/interface_window_handler.hpp"

#ifndef QT_GUI_PRIVATE
#warning "qplatformnativeinterface is requried for wayland compositor"
#endif

#include <QtGui/qpa/qplatformnativeinterface.h>
#include <QtGui/qpa/qplatformwindow.h>

#ifdef QT_WAYLAND_HAS_CUSTOM_MARGIN_SUPPORT
#include <QtGui/qpa/qplatformwindow_p.h>
#endif

#include <vlc_window.h>
#include <vlc_modules.h>

#include "compositor_wayland_module.h"

namespace vlc {

static qreal dprForWindow(QQuickWindow* window)
{
    QPlatformWindow* nativeWindow = window->handle();
    if (!nativeWindow)
        return 1.0;

    return nativeWindow->devicePixelRatio();
}

CompositorWayland::CompositorWayland(qt_intf_t *p_intf, QObject* parent)
    : CompositorVideo(p_intf, parent)
{
}

CompositorWayland::~CompositorWayland()
{
    if (m_waylandImpl)
    {
        if (m_waylandImpl->p_module)
        {
            m_waylandImpl->close(m_waylandImpl);
            module_unneed(m_waylandImpl, m_waylandImpl->p_module);
        }
        vlc_object_delete(m_waylandImpl);
    }
}

bool CompositorWayland::init()
{
    QPlatformNativeInterface* native = QGuiApplication::platformNativeInterface();

    //get "wl_display" and not "display", as "display" may refer to other concept in other QPA
    //so this will fail if qt doesn't use wayland
    void* qpni_display = native->nativeResourceForIntegration("wl_display");
    if (!qpni_display)
        return false;

    /*
     * a separate wayland module is used to perform direct wayland calls
     * without requiring Qt module to be directly linked to wayland
     */
    m_waylandImpl = static_cast<qtwayland_t*>(vlc_object_create(m_intf, sizeof(qtwayland_t)));
    if (!m_waylandImpl)
        return false;

    m_waylandImpl->p_module = module_need(m_waylandImpl, "qtwayland", nullptr, false);
    if (!m_waylandImpl->p_module)
    {
        msg_Err(m_intf, "the qtwayland module is not available, wayland"
                        "embedded video output will not be possible");
        vlc_object_delete(m_waylandImpl);
        m_waylandImpl = nullptr;

        return false;
    }

    return m_waylandImpl->init(m_waylandImpl, qpni_display);
}

bool CompositorWayland::makeMainInterface(MainCtx* mainCtx)
{
    assert(mainCtx);
    m_mainCtx = mainCtx;

    m_qmlView = std::make_unique<QQuickView>();
    m_qmlView->setResizeMode(QQuickView::SizeRootObjectToView);
    m_qmlView->setColor(QColor(Qt::transparent));

    m_qmlView->create();

    QPlatformNativeInterface *nativeInterface = QGuiApplication::platformNativeInterface();
    void* interfaceSurface = nativeInterface->nativeResourceForWindow("surface", m_qmlView.get());
    if (!interfaceSurface)
        return false;

    m_waylandImpl->setupInterface(m_waylandImpl, interfaceSurface, dprForWindow(m_qmlView.get()));

    CompositorVideo::Flags flags = CompositorVideo::CAN_SHOW_PIP | CompositorVideo::HAS_ACRYLIC;

#ifdef QT_WAYLAND_HAS_CUSTOM_MARGIN_SUPPORT
    connect(m_intf->p_mi, &MainCtx::windowExtendedMarginChanged, this, [this](const unsigned margin) {
        const auto quickViewPtr = m_qmlView.get();
        assert(quickViewPtr);
        connect(quickViewPtr, &QWindow::widthChanged, this, &CompositorWayland::adjustQuickWindowMask, Qt::UniqueConnection);
        connect(quickViewPtr, &QWindow::heightChanged, this, &CompositorWayland::adjustQuickWindowMask, Qt::UniqueConnection);

        const auto waylandWindow = dynamic_cast<QNativeInterface::Private::QWaylandWindow *>(quickViewPtr->handle());
        assert(waylandWindow);
        const QMargins margins(margin, margin, margin, margin);
        waylandWindow->setCustomMargins(margins);
    });

    flags |= CompositorVideo::HAS_EXTENDED_FRAME;
#endif

    const bool ret = commonGUICreate(m_qmlView.get(), m_qmlView.get(), flags);

    if (ret)
        m_qmlView->setVisible(true);

    return ret;
}

QWindow* CompositorWayland::interfaceMainWindow() const
{
    return m_qmlView.get();
}

void CompositorWayland::destroyMainInterface()
{
    unloadGUI();
    m_qmlView.reset();
}

void CompositorWayland::unloadGUI()
{
    //needs to be released before the window
    m_interfaceWindowHandler.reset();

    //at this point we need to unload the QML content but the window still need to
    //be valid as it may still be used by the vout window.
    //we cant' just delete the qmlEngine as the QmlView as the root item is parented to the QmlView
    //setSource() to nothing will effectively destroy the root item
    m_qmlView->setSource(QUrl());

    commonGUIDestroy();
}

Compositor::Type CompositorWayland::type() const
{
    return Compositor::WaylandCompositor;
}

QQuickItem * CompositorWayland::activeFocusItem() const
{
    return m_qmlView->activeFocusItem();
}

// vout functions

bool CompositorWayland::setupVoutWindow(vlc_window_t* p_wnd, VoutDestroyCb destroyCb)
{
    if (m_wnd)
        return false;

    int ret = m_waylandImpl->setupVoutWindow(m_waylandImpl, p_wnd);
    if (ret != VLC_SUCCESS)
        return false;

    commonSetupVoutWindow(p_wnd, destroyCb);

    return true;
}

void CompositorWayland::windowDestroy()
{
    m_waylandImpl->teardownVoutWindow(m_waylandImpl);
    CompositorVideo::windowDestroy();
}

int CompositorWayland::windowEnable(const vlc_window_cfg_t * cfg)
{
    m_waylandImpl->enable(m_waylandImpl, cfg);
    commonWindowEnable();
    return VLC_SUCCESS;
}

void CompositorWayland::windowDisable()
{
    m_waylandImpl->disable(m_waylandImpl);
    commonWindowDisable();
}

void CompositorWayland::onSurfacePositionChanged(const QPointF& position)
{
    QMargins margins = m_qmlView->frameMargins();

    qreal qtDpr = m_qmlView.get()->effectiveDevicePixelRatio();
    qreal nativeDpr = dprForWindow(m_qmlView.get());

    m_waylandImpl->move(
        m_waylandImpl,
        (margins.left() * qtDpr + position.x() ) / nativeDpr,
        (margins.top()  * qtDpr + position.y() ) / nativeDpr
    );
}

void CompositorWayland::onSurfaceSizeChanged(const QSizeF& size)
{
    qreal nativeDpr = dprForWindow(m_qmlView.get());

    m_waylandImpl->resize(m_waylandImpl,
                        std::ceil(size.width() / nativeDpr),
                        std::ceil(size.height() / nativeDpr));
}

void CompositorWayland::onSurfaceScaleChanged(qreal dpr)
{
    assert(m_waylandImpl);

    m_waylandImpl->rescale(m_waylandImpl, dpr);
}

#ifdef QT_WAYLAND_HAS_CUSTOM_MARGIN_SUPPORT
void CompositorWayland::adjustQuickWindowMask()
{
    assert(m_intf);
    assert(m_intf->p_mi);
    unsigned maskMargin = 0;
    if (Q_LIKELY(static_cast<unsigned>(m_intf->p_mi->CSDBorderSize()) < m_intf->p_mi->windowExtendedMargin()))
        maskMargin = m_intf->p_mi->windowExtendedMargin() - m_intf->p_mi->CSDBorderSize();
    const QMargins maskMargins(maskMargin, maskMargin, maskMargin, maskMargin);
    m_qmlView->setMask(m_qmlView->geometry().marginsRemoved(maskMargins));
}
#endif

}
