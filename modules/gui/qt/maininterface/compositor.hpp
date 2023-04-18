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
#ifndef VLC_QT_COMPOSITOR
#define VLC_QT_COMPOSITOR

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <memory>

#include <QObject>

#include <vlc_common.h>

#include "qt.hpp"


extern "C" {
    typedef struct vlc_window vlc_window_t;
    typedef struct vlc_window_cfg vlc_window_cfg_t;
}

class MainCtx;
class VideoWindowHandler;
class VideoSurfaceProvider;
class InterfaceWindowHandler;
class WinTaskbarWidget;
class MainUI;
class QQmlEngine;
class QQmlComponent;
class QWindow;
class QQuickItem;
class QQuickView;
class QQuickWindow;

namespace vlc {

class Compositor {
public:
    enum Type
    {
        DummyCompositor,
        Win7Compositor,
        DirectCompositionCompositor,
        X11Compositor
    };

    typedef void (*VoutDestroyCb)(vlc_window_t *p_wnd);

public:
    virtual ~Compositor() = default;

    virtual bool init() = 0;

    virtual bool makeMainInterface(MainCtx* intf) = 0;
    virtual void destroyMainInterface() = 0;

    virtual void unloadGUI() = 0;

    virtual bool setupVoutWindow(vlc_window_t *p_wnd, VoutDestroyCb destroyCb) = 0;

    virtual Type type() const = 0;

    virtual QWindow* interfaceMainWindow() const = 0;

    virtual QQuickItem * activeFocusItem() const = 0;
};

/**
 * @brief The CompositorVideo class is a base class for compositor that implements video embeding
 */
class CompositorVideo: public QObject, public Compositor
{
    Q_OBJECT
public:
    enum Flag : unsigned
    {
        CAN_SHOW_PIP = 1,
        HAS_ACRYLIC = 2
    };
    Q_DECLARE_FLAGS(Flags, Flag)

    class QmlUISurface
    {
    public:
        virtual ~QmlUISurface() = default;
        virtual QQmlEngine* engine() const = 0;
        virtual void setContent(QQmlComponent *component, QQuickItem *item) = 0;

        virtual QQuickItem * activeFocusItem() const = 0;
    };

public:
    explicit CompositorVideo(qt_intf_t* p_intf, QObject* parent = nullptr);
    virtual ~CompositorVideo();

public:
    virtual int windowEnable(const vlc_window_cfg_t *) = 0;
    virtual void windowDisable() = 0;
    virtual void windowDestroy();
    virtual void windowResize(unsigned width, unsigned height);
    virtual void windowSetState(unsigned state);
    virtual void windowUnsetFullscreen();
    virtual void windowSetFullscreen(const char *id);

protected:
    void commonSetupVoutWindow(vlc_window_t* p_wnd, VoutDestroyCb destroyCb);
    void commonWindowEnable();
    void commonWindowDisable();

protected:
    bool commonGUICreate(QWindow* window, QWidget* rootWidget, QmlUISurface* , CompositorVideo::Flags flags);
    bool commonGUICreate(QWindow* window, QWidget* rootWidget, QQuickView* , CompositorVideo::Flags flags);
    void commonGUIDestroy();
    void commonIntfDestroy();

private:
    bool commonGUICreateImpl(QWindow* window, QWidget* rootWidget, CompositorVideo::Flags flags);

protected slots:
    virtual void onSurfacePositionChanged(const QPointF&) {}
    virtual void onSurfaceSizeChanged(const QSizeF&) {}

protected:
    qt_intf_t *m_intf = nullptr;
    vlc_window_t* m_wnd = nullptr;

    MainCtx* m_mainCtx = nullptr;

    VoutDestroyCb m_destroyCb = nullptr;
    std::unique_ptr<VideoWindowHandler> m_videoWindowHandler;

    std::unique_ptr<InterfaceWindowHandler> m_interfaceWindowHandler;
    std::unique_ptr<MainUI> m_ui;
    std::unique_ptr<VideoSurfaceProvider> m_videoSurfaceProvider;
#ifdef _WIN32
    std::unique_ptr<WinTaskbarWidget> m_taskbarWidget;
#endif
};

Q_DECLARE_OPERATORS_FOR_FLAGS(CompositorVideo::Flags)


/**
 * @brief The CompositorFactory class will instantiate a compositor
 * in auto mode, compositor will be instantiated from the list by order declaration,
 * compositor can be explicitly defined by passing its name.
 *
 * the usual scenario is:
 *
 *   - call to preInit that will try to preInit compositors from list until we find
 *     a matching candidate
 *
 *   - start Qt main loop
 *
 *   - call to createCompositor to instantiate the compositor, if it fails it will
 *     try to initialize the next compositors from the list
 */
class CompositorFactory {
public:

    CompositorFactory(qt_intf_t *p_intf, const char* compositor = "auto");

    /**
     * @brief preInit will check whether a compositor can be used, before starting Qt,
     * each candidate may perform some basic checks and can setup Qt environment variable if required
     *
     * @note if a compositor return true on preinit but fails to initialize afterwards, next
     * compositor in chain will be initialized without the preinit phaze (as Qt will be already started)
     * this might lead to an unstable configuration if incompatible operations are done in the preInit phase
     *
     * @return true if a compositor can be instantiated
     */
    bool preInit();

    /**
     * @brief createCompositor will instantiate a compositor
     *
     * @return the compositor instance, null if no compositor can be instantiated
     */
    Compositor* createCompositor();

private:
    qt_intf_t* m_intf = nullptr;
    QString m_compositorName;
    size_t m_compositorIndex = 0;
};

}

#endif /* VLC_QT_COMPOSITOR */
