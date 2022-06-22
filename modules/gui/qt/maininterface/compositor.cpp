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

#include "compositor.hpp"
#include "compositor_dummy.hpp"
#include "mainctx.hpp"
#include "video_window_handler.hpp"
#include "videosurface.hpp"
#include "interface_window_handler.hpp"
#include "mainui.hpp"

#ifdef _WIN32
#include "mainctx_win32.hpp"
#ifdef HAVE_DCOMP_H
#  include "compositor_dcomp.hpp"
#endif
#  include "compositor_win7.hpp"
#endif

#ifdef QT5_HAS_XCB
#  include "compositor_x11.hpp"
#endif

using namespace vlc;

template<typename T>
static Compositor* instanciateCompositor(qt_intf_t *p_intf) {
    return new T(p_intf);
}

template<typename T>
static bool preInit(qt_intf_t *p_intf) {
    return T::preInit(p_intf);
}

struct {
    const char* name;
    Compositor* (*instantiate)(qt_intf_t *p_intf);
    bool (*preInit)(qt_intf_t *p_intf);
} static compositorList[] = {
#ifdef _WIN32
#ifdef HAVE_DCOMP_H
    {"dcomp", &instanciateCompositor<CompositorDirectComposition>, &preInit<CompositorDirectComposition> },
#endif
    {"win7", &instanciateCompositor<CompositorWin7>, &preInit<CompositorWin7> },
#endif
#ifdef QT5_HAS_X11_COMPOSITOR
    {"x11", &instanciateCompositor<CompositorX11>, &preInit<CompositorX11> },
#endif
    {"dummy", &instanciateCompositor<CompositorDummy>, &preInit<CompositorDummy> }
};

CompositorFactory::CompositorFactory(qt_intf_t *p_intf, const char* compositor)
    : m_intf(p_intf)
    , m_compositorName(compositor)
{
}

bool CompositorFactory::preInit()
{
    for (; m_compositorIndex < ARRAY_SIZE(compositorList); m_compositorIndex++)
    {
        if (m_compositorName == "auto" || m_compositorName == compositorList[m_compositorIndex].name)
        {
            if (compositorList[m_compositorIndex].preInit(m_intf))
                return true;
        }
    }
    return false;
}

Compositor* CompositorFactory::createCompositor()
{
    for (; m_compositorIndex < ARRAY_SIZE(compositorList); m_compositorIndex++)
    {
        if (m_compositorName == "auto" || m_compositorName == compositorList[m_compositorIndex].name)
        {
            Compositor* compositor = compositorList[m_compositorIndex].instantiate(m_intf);
            if (compositor->init())
            {
                //avoid looping over the same compositor if the current ones fails further initialisation steps
                m_compositorIndex++;
                return compositor;
            }
        }
    }
    msg_Err(m_intf, "no suitable compositor found");
    return nullptr;
}


extern "C"
{

static int windowEnableCb(vlc_window_t* p_wnd, const vlc_window_cfg_t * cfg)
{
    assert(p_wnd->sys);
    auto that = static_cast<vlc::CompositorVideo*>(p_wnd->sys);
    int ret = VLC_EGENERIC;
    QMetaObject::invokeMethod(that, [&](){
        ret = that->windowEnable(cfg);
    }, Qt::BlockingQueuedConnection);
    return ret;
}

static void windowDisableCb(vlc_window_t* p_wnd)
{
    assert(p_wnd->sys);
    auto that = static_cast<vlc::CompositorVideo*>(p_wnd->sys);
    QMetaObject::invokeMethod(that, [that](){
        that->windowDisable();
    }, Qt::BlockingQueuedConnection);
}

static void windowResizeCb(vlc_window_t* p_wnd, unsigned width, unsigned height)
{
    assert(p_wnd->sys);
    auto that = static_cast<vlc::CompositorVideo*>(p_wnd->sys);
    that->windowResize(width, height);
}

static void windowDestroyCb(struct vlc_window * p_wnd)
{
    assert(p_wnd->sys);
    auto that = static_cast<vlc::CompositorVideo*>(p_wnd->sys);
    that->windowDestroy();
}

static void windowSetStateCb(vlc_window_t* p_wnd, unsigned state)
{
    assert(p_wnd->sys);
    auto that = static_cast<vlc::CompositorVideo*>(p_wnd->sys);
    that->windowSetState(state);
}

static void windowUnsetFullscreenCb(vlc_window_t* p_wnd)
{
    assert(p_wnd->sys);
    auto that = static_cast<vlc::CompositorVideo*>(p_wnd->sys);
    that->windowUnsetFullscreen();
}

static void windowSetFullscreenCb(vlc_window_t* p_wnd, const char *id)
{
    assert(p_wnd->sys);
    auto that = static_cast<vlc::CompositorVideo*>(p_wnd->sys);
    that->windowSetFullscreen(id);
}

}

CompositorVideo::CompositorVideo(qt_intf_t *p_intf, QObject* parent)
    : QObject(parent)
    , m_intf(p_intf)
{
}

CompositorVideo::~CompositorVideo()
{

}

void CompositorVideo::commonSetupVoutWindow(vlc_window_t* p_wnd, VoutDestroyCb destroyCb)
{
    static const struct vlc_window_operations ops = {
        windowEnableCb,
        windowDisableCb,
        windowResizeCb,
        windowDestroyCb,
        windowSetStateCb,
        windowUnsetFullscreenCb,
        windowSetFullscreenCb,
        nullptr, //window_set_title
    };

    m_wnd = p_wnd;
    m_destroyCb = destroyCb;
    p_wnd->sys = this;
    p_wnd->ops = &ops;
    p_wnd->info.has_double_click = true;
}

void CompositorVideo::windowDestroy()
{
    if (m_destroyCb)
        m_destroyCb(m_wnd);
}

void CompositorVideo::windowResize(unsigned width, unsigned height)
{
    m_videoWindowHandler->requestResizeVideo(width, height);
}

void CompositorVideo::windowSetState(unsigned state)
{
    m_videoWindowHandler->requestVideoState(static_cast<vlc_window_state>(state));
}

void CompositorVideo::windowUnsetFullscreen()
{
    m_videoWindowHandler->requestVideoWindowed();
}

void CompositorVideo::windowSetFullscreen(const char *id)
{
    m_videoWindowHandler->requestVideoFullScreen(id);
}

void CompositorVideo::commonWindowEnable()
{
    m_videoSurfaceProvider->enable(m_wnd);
    m_videoSurfaceProvider->setVideoEmbed(true);
}

void CompositorVideo::commonWindowDisable()
{
    m_videoSurfaceProvider->setVideoEmbed(false);
    m_videoSurfaceProvider->disable();
    m_videoWindowHandler->disable();
}


bool CompositorVideo::commonGUICreateImpl(QWindow* window, QWidget* rootWidget, CompositorVideo::Flags flags)
{
    assert(m_mainCtx);

    m_videoSurfaceProvider = std::make_unique<VideoSurfaceProvider>();
    m_mainCtx->setVideoSurfaceProvider(m_videoSurfaceProvider.get());
    if (flags & CompositorVideo::CAN_SHOW_PIP)
    {
        m_mainCtx->setCanShowVideoPIP(true);
        connect(m_videoSurfaceProvider.get(), &VideoSurfaceProvider::surfacePositionChanged,
                this, &CompositorVideo::onSurfacePositionChanged);
        connect(m_videoSurfaceProvider.get(), &VideoSurfaceProvider::surfaceSizeChanged,
                this, &CompositorVideo::onSurfaceSizeChanged);
    }
    m_videoWindowHandler = std::make_unique<VideoWindowHandler>(m_intf);
    m_videoWindowHandler->setWindow( window );

#ifdef _WIN32
    m_interfaceWindowHandler = std::make_unique<InterfaceWindowHandlerWin32>(m_intf, m_mainCtx, window, rootWidget);
#else
    m_interfaceWindowHandler = std::make_unique<InterfaceWindowHandler>(m_intf, m_mainCtx,  window, rootWidget);
#endif
    m_mainCtx->setHasAcrylicSurface(flags & CompositorVideo::HAS_ACRYLIC);

#ifdef _WIN32
    m_taskbarWidget = std::make_unique<WinTaskbarWidget>(m_intf, window);
    qApp->installNativeEventFilter(m_taskbarWidget.get());
#endif
    m_ui = std::make_unique<MainUI>(m_intf, m_mainCtx, window);
    return true;
}

bool CompositorVideo::commonGUICreate(QWindow* window, QWidget* rootWidget, QmlUISurface* qmlSurface, CompositorVideo::Flags flags)
{
    bool ret = commonGUICreateImpl(window, rootWidget, flags);
    if (!ret)
        return false;
    ret = m_ui->setup(qmlSurface->engine());
    if (! ret)
        return false;
    qmlSurface->setContent(m_ui->getComponent(), m_ui->createRootItem());
    return true;
}

bool CompositorVideo::commonGUICreate(QWindow* window, QWidget* rootWidget, QQuickView* qmlView, CompositorVideo::Flags flags)
{
    bool ret = commonGUICreateImpl(window, rootWidget, flags);
    if (!ret)
        return false;
    ret = m_ui->setup(qmlView->engine());
    if (! ret)
        return false;
    qmlView->setContent(QUrl(), m_ui->getComponent(), m_ui->createRootItem());
    return true;
}

void CompositorVideo::commonGUIDestroy()
{
    m_ui.reset();
#ifdef _WIN32
    qApp->removeNativeEventFilter(m_taskbarWidget.get());
    m_taskbarWidget.reset();
#endif
    m_interfaceWindowHandler.reset();
}

void CompositorVideo::commonIntfDestroy()
{
    unloadGUI();
    m_videoWindowHandler.reset();
    m_videoSurfaceProvider.reset();
}
