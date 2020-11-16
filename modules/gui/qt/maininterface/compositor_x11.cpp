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
#include <QX11Info>
#include <QByteArray>
#include <QHBoxLayout>
#include <QWidget>
#include <QScreen>
#include "compositor_x11.hpp"
#include "compositor_x11_renderwindow.hpp"
#include "compositor_x11_uisurface.hpp"
#include "main_interface.hpp"
#include "interface_window_handler.hpp"
#include "video_window_handler.hpp"
#include "mainui.hpp"

using namespace vlc;

int CompositorX11::windowEnable(const vout_window_cfg_t *)
{
    commonWindowEnable();
    m_renderWindow->enableVideoWindow();
    return VLC_SUCCESS;
}

void CompositorX11::windowDisable()
{
    m_renderWindow->disableVideoWindow();
    commonWindowDisable();
}

CompositorX11::CompositorX11(qt_intf_t *p_intf, QObject *parent)
    : CompositorVideo(p_intf, parent)
{
}

CompositorX11::~CompositorX11()
{
    if (m_conn)
        xcb_disconnect(m_conn);
}

bool CompositorX11::preInit(qt_intf_t*)
{
    return true;
}

static bool checkExtensionPresent(qt_intf_t* intf, xcb_connection_t *conn, const char* extension)
{
    bool ret = queryExtension(conn, extension, nullptr, nullptr);
    if (! ret)
        msg_Warn(intf, "X11 extension %s is missing", extension);
    return ret;
}

#define REGISTER_XCB_EXTENSION(c, extension, minor, major) \
    do { \
        xcb_ ## extension  ##_query_version_cookie_t cookie = xcb_## extension  ##_query_version(c, minor, major); \
        xcb_generic_error_t* error = nullptr; \
        auto reply = wrap_cptr(xcb_## extension  ##_query_version_reply(c, cookie, &error)); \
        if (error) { \
            msg_Warn(m_intf, "X11 extension %s is too old", #extension); \
            free(error); \
            return false; \
        } \
    } while(0)


bool CompositorX11::init()
{
    if (!QX11Info::isPlatformX11())
    {
        msg_Info(m_intf, "not running an X11 platform");
        return false;
    }

    //open a separated X11 connection from Qt to be able to receive
    //damage events independently from Qt (even when Qt's main loop is stalled)
    m_conn = xcb_connect(nullptr, nullptr);
    if (xcb_connection_has_error(m_conn) != 0)
    {
        msg_Warn(m_intf, "can't open X11 connection");
        return false;
    }

    //check X11 extensions
    if (!checkExtensionPresent(m_intf, m_conn, "DAMAGE"))
        return false;
    REGISTER_XCB_EXTENSION(m_conn, damage, XCB_DAMAGE_MAJOR_VERSION, XCB_DAMAGE_MINOR_VERSION);

    if (!checkExtensionPresent(m_intf, m_conn, "RENDER"))
        return false;
    //0.11 required for Blend mode operators
    static_assert (XCB_RENDER_MAJOR_VERSION == 0 && XCB_RENDER_MINOR_VERSION >= 11,
            "X11 Render version is too old, 0.11+ is required");
    REGISTER_XCB_EXTENSION(m_conn, render, XCB_RENDER_MAJOR_VERSION, XCB_RENDER_MINOR_VERSION);

    if (!checkExtensionPresent(m_intf, m_conn, "Composite"))
        return false;
    //0.2 is required for NameWindowPixmap
    static_assert (XCB_COMPOSITE_MAJOR_VERSION == 0 && XCB_COMPOSITE_MINOR_VERSION >= 2,
            "X11 Composite version is too old, 0.2+ is required");
    REGISTER_XCB_EXTENSION(m_conn, composite, XCB_COMPOSITE_MAJOR_VERSION, XCB_COMPOSITE_MINOR_VERSION);

    // check whether we're running under "XWAYLAND"
    auto screens = qApp->screens();
    bool isXWayland = std::any_of(screens.begin(), screens.end(), [](QScreen* screen){
        return screen->name().startsWith("XWAYLAND");
    });

    if (isXWayland)
    {
        // if the X11 server is XWayland, test if it is a broken version
        const xcb_setup_t* setup = xcb_get_setup(m_conn);

        if (setup == nullptr)
        {
            msg_Info(m_intf, "error checking for XWayland version");
            return false;
        }

        if (setup->release_number < 12100000u)
        {
            msg_Info(m_intf, "running a broken XWayland version, disabling X11 composition");
            return false;
        }
    }

    return true;
}

bool CompositorX11::makeMainInterface(MainInterface* mainInterface)
{
    m_mainInterface = mainInterface;

    m_videoWidget = std::make_unique<DummyNativeWidget>();
    m_videoWidget->setWindowFlag(Qt::WindowType::BypassWindowManagerHint);
    m_videoWidget->setWindowFlag(Qt::WindowType::WindowTransparentForInput);
    m_videoWidget->winId();
    m_videoWidget->show();

    bool useCSD = m_mainInterface->useClientSideDecoration();
    m_renderWindow = std::make_unique<vlc::CompositorX11RenderWindow>(m_intf, m_conn, useCSD);
    if (!m_renderWindow->init())
        return false;

    m_interfaceWindow = m_renderWindow->getWindow();

    m_qmlView = std::make_unique<CompositorX11UISurface>(m_interfaceWindow);
    m_qmlView->setFlag(Qt::WindowType::BypassWindowManagerHint);
    m_qmlView->setFlag(Qt::WindowType::WindowTransparentForInput);
    m_qmlView->winId();
    m_qmlView->show();

    CompositorVideo::Flags flags = CompositorVideo::CAN_SHOW_PIP;
    if (m_renderWindow->hasAcrylic())
        flags |= CompositorVideo::HAS_ACRYLIC;
    commonGUICreate(m_interfaceWindow, m_qmlView.get(), flags);

    m_renderWindow->setInterfaceWindow(m_qmlView.get());
    m_renderWindow->setVideoWindow(m_videoWidget->windowHandle());

    m_renderWindow->startRendering();

    return true;
}

void CompositorX11::destroyMainInterface()
{
    commonIntfDestroy();
    m_videoWidget.reset();
    m_renderWindow.reset();
}

void CompositorX11::unloadGUI()
{
    m_renderWindow->stopRendering();
    commonGUIDestroy();
    m_qmlView.reset();
}

void CompositorX11::onSurfacePositionChanged(const QPointF& position)
{
    m_renderWindow->setVideoPosition(position.toPoint());
}

void CompositorX11::onSurfaceSizeChanged(const QSizeF& size)
{
    m_renderWindow->setVideoSize((size / m_videoWidget->window()->devicePixelRatioF()).toSize());
}

bool CompositorX11::setupVoutWindow(vout_window_t* p_wnd, VoutDestroyCb destroyCb)
{
    p_wnd->type = VOUT_WINDOW_TYPE_XID;
    p_wnd->handle.xid = m_videoWidget->winId();
    commonSetupVoutWindow(p_wnd, destroyCb);
    return true;
}
