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
#include "compositor_dcomp.hpp"
#include "compositor_win7.hpp"
#endif

#ifdef QT_HAS_WAYLAND_COMPOSITOR
#  include "compositor_wayland.hpp"
#endif

#ifdef QT_HAS_X11_COMPOSITOR
#  include "compositor_x11.hpp"
#endif

#include "maininterface/windoweffects_module.hpp"

#include "compositor_platform.hpp"

#include <vlc_window.h>
#include <vlc_modules.h>


using namespace vlc;

template<typename T>
static Compositor* instanciateCompositor(qt_intf_t *p_intf) {
    return new T(p_intf);
}

struct {
    const char* name;
    Compositor* (*instantiate)(qt_intf_t *p_intf);
} static compositorList[] = {
#if defined(_WIN32)
    {"dcomp", &instanciateCompositor<CompositorDirectComposition> },
#endif
#if defined(_WIN32) || defined(__APPLE__)
    {"platform", &instanciateCompositor<CompositorPlatform> },
#endif
#if defined(_WIN32)
    {"win7", &instanciateCompositor<CompositorWin7> },
#endif
#ifdef QT_HAS_WAYLAND_COMPOSITOR
    {"wayland", &instanciateCompositor<CompositorWayland> },
#endif
#ifdef QT_HAS_X11_COMPOSITOR
    {"x11", &instanciateCompositor<CompositorX11> },
#endif
    {"dummy", &instanciateCompositor<CompositorDummy> }
};

CompositorFactory::CompositorFactory(qt_intf_t *p_intf, const char* compositor)
    : m_intf(p_intf)
    , m_compositorName(compositor)
{
}

Compositor* CompositorFactory::createCompositor()
{
    for (; m_compositorIndex < ARRAY_SIZE(compositorList); m_compositorIndex++)
    {
        if (m_compositorName == "auto" || m_compositorName == compositorList[m_compositorIndex].name)
        {
            std::unique_ptr<Compositor> compositor {
                compositorList[m_compositorIndex].instantiate(m_intf)
            };

            if (compositor->init())
            {
                //avoid looping over the same compositor if the current ones fails further initialisation steps
                m_compositorIndex++;
                return compositor.release();
            }
        }
    }
    msg_Err(m_intf, "no suitable compositor found");
    return nullptr;
}

namespace {

struct CompositorWindow {
    /* The CompositorVideo instance that created the window */
    vlc::CompositorVideo *compositor;

    /* Callback setup from commonSetupVoutWindow. It usually comes from the main qt.cpp
     * file so as to handle the reference counting of Qt's resources. */
    void (*destroy_cb)(struct vlc_window *wnd);
};

}

extern "C"
{
static int windowEnableCb(vlc_window_t* p_wnd, const vlc_window_cfg_t * cfg)
{
    assert(p_wnd->sys);
    auto that = static_cast<CompositorWindow*>(p_wnd->sys);
    int ret = VLC_EGENERIC;
    QMetaObject::invokeMethod(that->compositor, [&](){
        ret = that->compositor->windowEnable(cfg);
    }, Qt::BlockingQueuedConnection);
    return ret;
}

static void windowDisableCb(vlc_window_t* p_wnd)
{
    assert(p_wnd->sys);
    auto that = static_cast<CompositorWindow*>(p_wnd->sys);
    QMetaObject::invokeMethod(that->compositor, [that](){
        that->compositor->windowDisable();
    }, Qt::BlockingQueuedConnection);
}

static void windowResizeCb(vlc_window_t* p_wnd, unsigned width, unsigned height)
{
    assert(p_wnd->sys);
    auto that = static_cast<CompositorWindow*>(p_wnd->sys);
    that->compositor->windowResize(width, height);
}

static void windowDestroyCb(struct vlc_window * p_wnd)
{
    assert(p_wnd->sys);
    auto that = static_cast<CompositorWindow*>(p_wnd->sys);
    that->compositor->windowDestroy();
    that->compositor = nullptr;

    if (that->destroy_cb != nullptr)
        that->destroy_cb(p_wnd);

    /* Destroy p_wnd->sys */
    delete that;
    p_wnd->sys = nullptr;
}

static void windowSetStateCb(vlc_window_t* p_wnd, unsigned state)
{
    assert(p_wnd->sys);
    auto that = static_cast<CompositorWindow*>(p_wnd->sys);
    that->compositor->windowSetState(state);
}

static void windowUnsetFullscreenCb(vlc_window_t* p_wnd)
{
    assert(p_wnd->sys);
    auto that = static_cast<CompositorWindow*>(p_wnd->sys);
    that->compositor->windowUnsetFullscreen();
}

static void windowSetFullscreenCb(vlc_window_t* p_wnd, const char *id)
{
    assert(p_wnd->sys);
    auto that = static_cast<CompositorWindow*>(p_wnd->sys);
    that->compositor->windowSetFullscreen(id);
}

}

CompositorVideo::CompositorVideo(qt_intf_t *p_intf, QObject* parent)
    : QObject(parent)
    , m_intf(p_intf)
{
}

CompositorVideo::~CompositorVideo()
{
    if (m_windowEffectsModule)
    {
        if (m_windowEffectsModule->p_module)
            module_unneed(m_windowEffectsModule, m_windowEffectsModule->p_module);
        vlc_object_delete(m_windowEffectsModule);
    }
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

    /* Allocate state that will be tracked by the window through vlc_window::sys */
    // NOTE: no std::nothrow here since no error code, forward the exception
    auto *compositor_window = new CompositorWindow;
    compositor_window->compositor = this;
    compositor_window->destroy_cb = destroyCb;

    /* We currently support only a single window in the compositor. */
    assert(m_wnd == nullptr);
    m_wnd = p_wnd;

    p_wnd->sys = compositor_window;
    p_wnd->ops = &ops;
    p_wnd->info.has_double_click = true;

    // These need to be connected here, since the compositor might not be ready when
    // these signals are emitted. VOut window might not be set, or worse, compositor's
    // internal preparations might not be completed yet:
    constexpr auto connType = static_cast<Qt::ConnectionType>(Qt::UniqueConnection | Qt::DirectConnection);
    connect(m_videoSurfaceProvider.get(), &VideoSurfaceProvider::surfacePropertiesChanged,
            this, &CompositorVideo::onSurfacePropertiesChanged, connType);
}

void CompositorVideo::windowDestroy()
{
    // Current thread may not be the thread where
    // m_videoSurfaceProvider belongs to, so do not delete
    // it here:
    disconnect(m_videoSurfaceProvider.get(), &VideoSurfaceProvider::surfacePropertiesChanged,
               this, &CompositorVideo::onSurfacePropertiesChanged);

    m_wnd = nullptr;
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


bool CompositorVideo::commonGUICreateImpl(QWindow* window, CompositorVideo::Flags flags)
{
    assert(m_mainCtx);
    assert(window);

    m_videoSurfaceProvider = std::make_unique<VideoSurfaceProvider>(canDoThreadedSurfaceUpdates());
    m_mainCtx->setVideoSurfaceProvider(m_videoSurfaceProvider.get());
    const bool backendIsOpenVg = QQuickWindow::sceneGraphBackend() == QLatin1String("openvg");
    if (!backendIsOpenVg && (flags & CompositorVideo::CAN_SHOW_PIP) && var_InheritBool(m_intf, "qt-pip-mode"))
    {
        m_mainCtx->setCanShowVideoPIP(true);
    }
    m_mainCtx->setWindowSuportExtendedFrame(flags & CompositorVideo::HAS_EXTENDED_FRAME);
    if (!backendIsOpenVg && (flags & CompositorVideo::HAS_ACRYLIC))
    {
#ifndef _WIN32
        assert(qGuiApp);
        if (qGuiApp->platformName().startsWith(QLatin1String("wayland")) && Q_LIKELY(!window->isActive()))
        {
            connect(window, &QWindow::activeChanged, this, [this, window = QPointer(window)]() {
               setBlurBehind(window, true);
            }, Qt::SingleShotConnection);
        }
        else
#endif
        {
            setBlurBehind(window, true);
        }
    }
    m_videoWindowHandler = std::make_unique<VideoWindowHandler>(m_intf);
    m_videoWindowHandler->setWindow( window );

#ifdef _WIN32
    m_interfaceWindowHandler = std::make_unique<InterfaceWindowHandlerWin32>(m_intf, m_mainCtx, window);
#else
    m_interfaceWindowHandler = std::make_unique<InterfaceWindowHandler>(m_intf, m_mainCtx, window);
#endif

#ifdef _WIN32
    m_taskbarWidget = std::make_unique<WinTaskbarWidget>(m_intf, window);
    qApp->installNativeEventFilter(m_taskbarWidget.get());
#endif
    m_ui = std::make_unique<MainUI>(m_intf, m_mainCtx, window);
    return true;
}

void CompositorVideo::adjustBlurBehind()
{
    if (m_mainCtx && m_windowEffectsModule)
    {
        if (QWindow *const window = interfaceMainWindow())
        {
            assert(m_windowEffectsModule->isEffectAvailable(window, WindowEffectsModule::BlurBehind));
            const bool blurBehind = m_mainCtx->hasAcrylicSurface();
            if (blurBehind && m_mainCtx->windowSuportExtendedFrame() && m_mainCtx->useClientSideDecoration())
            {
                QRegion blurRegion;
                const QRegion maskRegion = window->mask();
                if (maskRegion.isEmpty())
                {
                    // If there is window mask, we assume that the content area is smaller than the
                    // window geometry. We also assume that in that case window has custom margins
                    // set (Wayland case). KWin 6.2 Wayland already considers window custom margins
                    // when applying the blur effect, but KWin X11 does not consider `_GTK_FRAME_EXTENTS`.
                    // Since the mask region represents the actual content area, and the outside region
                    // is included in the frame margins, we can set the blur region only if the mask is
                    // empty. This fixes blur backdrop applied to client shadows on X11:
                    const auto margin = m_mainCtx->windowExtendedMargin();
                    blurRegion = QRegion(margin, margin, window->width() - 2 * margin, window->height() - 2 * margin);
                }
                m_windowEffectsModule->setBlurBehind(window, true, blurRegion);
            }
            else
            {
                m_windowEffectsModule->setBlurBehind(window, blurBehind, {});
            }
        }
    }
}

bool CompositorVideo::commonGUICreate(QWindow* window, QmlUISurface* qmlSurface, CompositorVideo::Flags flags)
{
    bool ret = commonGUICreateImpl(window, flags);
    if (!ret)
        return false;
    ret = m_ui->setup(qmlSurface->engine());
    if (! ret)
        return false;
    qmlSurface->setContent(m_ui->getComponent(), m_ui->createRootItem());
    return true;
}

bool CompositorVideo::commonGUICreate(QWindow* window, QQuickView* qmlView, CompositorVideo::Flags flags)
{
    bool ret = commonGUICreateImpl(window, flags);
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
    m_mainCtx = nullptr;
}

void CompositorVideo::commonIntfDestroy()
{
    m_videoWindowHandler.reset();
    m_videoSurfaceProvider.reset();
    unloadGUI();
}

bool CompositorVideo::setBlurBehind(QWindow *window, const bool enable)
{
    assert(window);
    assert(m_intf);

    if (enable)
    {
        if (!var_InheritBool(m_intf, "qt-backdrop-blur"))
        {
            return false;
        }
    }

    if (m_failedToLoadWindowEffectsModule)
        return false;

    if (!m_windowEffectsModule)
    {
        m_windowEffectsModule = vlc_object_create<WindowEffectsModule>(m_intf);
        if (!m_windowEffectsModule) // do not set m_failedToLoadWindowEffectsModule here
            return false;
    }

    if (!m_windowEffectsModule->p_module)
    {
        m_windowEffectsModule->p_module = module_need(m_windowEffectsModule, "qtwindoweffects", nullptr, false);
        if (!m_windowEffectsModule->p_module)
        {
            msg_Dbg(m_intf, "A module providing window effects capability could not be instantiated. " \
                            "Native background blur effect will not be available. " \
                            "The application may compensate this with a simulated effect on certain platform(s).");
            m_failedToLoadWindowEffectsModule = true;
            vlc_object_delete(m_windowEffectsModule);
            m_windowEffectsModule = nullptr;
            return false;
        }
    }

    if (!m_windowEffectsModule->isEffectAvailable(window, WindowEffectsModule::BlurBehind))
        return false;

    m_mainCtx->setHasAcrylicSurface(enable);

    if (m_mainCtx->windowSuportExtendedFrame())
    {
        assert(m_mainCtx);

        if (enable)
        {
            connect(window, &QWindow::widthChanged, this, &CompositorVideo::adjustBlurBehind, Qt::UniqueConnection);
            connect(window, &QWindow::heightChanged, this, &CompositorVideo::adjustBlurBehind, Qt::UniqueConnection);
            connect(m_mainCtx, &MainCtx::windowExtendedMarginChanged, this, &CompositorVideo::adjustBlurBehind, Qt::UniqueConnection);
            connect(m_mainCtx, &MainCtx::useClientSideDecorationChanged, this, &CompositorVideo::adjustBlurBehind, Qt::UniqueConnection);
        }
        else
        {
            disconnect(window, &QWindow::widthChanged, this, &CompositorVideo::adjustBlurBehind);
            disconnect(window, &QWindow::heightChanged, this, &CompositorVideo::adjustBlurBehind);
            disconnect(m_mainCtx, &MainCtx::windowExtendedMarginChanged, this, &CompositorVideo::adjustBlurBehind);
            disconnect(m_mainCtx, &MainCtx::useClientSideDecorationChanged, this, &CompositorVideo::adjustBlurBehind);
        }

        adjustBlurBehind();
    }
    else
    {
        m_windowEffectsModule->setBlurBehind(window, enable, {});
    }

    return true;
}

QQuickWindow *Compositor::quickWindow() const
{
    return qobject_cast<QQuickWindow*>(interfaceMainWindow());
}
