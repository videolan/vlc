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
#include <QPointF>
#include <QSizeF>

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
struct WindowEffectsModule;

namespace vlc {

class Compositor {
public:
    enum Type
    {
        DummyCompositor,
        Win7Compositor,
        DirectCompositionCompositor,
        X11Compositor,
        WaylandCompositor,
        PlatformCompositor
    };

    typedef void (*VoutDestroyCb)(vlc_window_t *p_wnd);

public:
    virtual ~Compositor() = default;

    [[nodiscard]] virtual bool init() = 0;

    [[nodiscard]] virtual bool makeMainInterface(MainCtx* intf, std::function<void(QQuickWindow*)> aboutToShowQuickWindowCallback = {}) = 0;

    /**
     * @brief destroyMainInterface must released all resources, this is called before
     * destroying the compositor
     */
    virtual void destroyMainInterface() = 0;

    /**
     * @brief unloadGUI must destroy the resources associated to the UI part,
     * this includes QML resources.
     * Resources that are dependencies of the video surfaces should not be
     * destroyed as the video window may be destroyed after the interface.
     *
     * this means that if your video depends on a QQuickView, you must unload
     * the QML content but keep the QQuickView
     */
    virtual void unloadGUI() = 0;

    [[nodiscard]] virtual bool setupVoutWindow(vlc_window_t *p_wnd, VoutDestroyCb destroyCb) = 0;

    virtual Type type() const = 0;

    virtual QWindow* interfaceMainWindow() const = 0;

    // Override this to return the quick window if quick window
    // is not the interface main window:
    virtual QQuickWindow* quickWindow() const;

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
        HAS_ACRYLIC = 2,
        HAS_EXTENDED_FRAME = 4,
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

    // The following method should return true if surface updates can be threaded:
    virtual bool canDoThreadedSurfaceUpdates() const { return false; };

protected:
    void commonSetupVoutWindow(vlc_window_t* p_wnd, VoutDestroyCb destroyCb);
    void commonWindowEnable();
    void commonWindowDisable();

    bool setBlurBehind(QWindow* window, bool enable = true);

    // The following method should return true if surface updates can be combined:
    // Note that in such a case, `::commitSurface()` must be implemented.
    virtual bool canDoCombinedSurfaceUpdates() const { return false; };

    virtual void commitSurface() {}

protected:
    /**
     * @brief commonGUICreate setup the QML view for video composition
     * this should be called from makeMainInterface specialisation
     */
    bool commonGUICreate(QWindow* window, QmlUISurface* , CompositorVideo::Flags flags);
    bool commonGUICreate(QWindow* window, QQuickView* , CompositorVideo::Flags flags);

    /**
     * @brief commonIntfDestroy release GUI resources allocated by commonGUICreate
     * this should be called from unloadGUI specialisation
     */
    void commonGUIDestroy();

    /**
     * @brief commonIntfDestroy release interface resources allocated by commonGUICreate
     * this should be called from destroyMainInterface specialisation
     */
    void commonIntfDestroy();

private:
    bool commonGUICreateImpl(QWindow* window, CompositorVideo::Flags flags);

private slots:
    void adjustBlurBehind();

protected slots:
    // WARNING: If `::canDoCombinedSurfaceUpdates()` returns `true`, individual
    //          position and size change handlers should ideally not commit the
    //          changes , for the compositor to apply the changes at the same
    //          time with an explicit call to `commitSurface()`:
    virtual void onSurfacePositionChanged(const QPointF&) {}
    virtual void onSurfaceSizeChanged(const QSizeF&) {}
    virtual void onSurfaceScaleChanged(qreal) {}

    virtual void onSurfacePropertiesChanged(const std::optional<QSizeF>& size,
                                            const std::optional<QPointF>& position,
                                            const std::optional<qreal>& scale)
    {
        if (size)
            onSurfaceSizeChanged(*size);
        if (position)
            onSurfacePositionChanged(*position);
        if (scale)
            onSurfaceScaleChanged(*scale);

        if (canDoCombinedSurfaceUpdates())
            commitSurface();
    }

protected:
    qt_intf_t *m_intf = nullptr;
    vlc_window_t* m_wnd = nullptr;

    MainCtx* m_mainCtx = nullptr;

    std::unique_ptr<VideoWindowHandler> m_videoWindowHandler;

    std::unique_ptr<InterfaceWindowHandler> m_interfaceWindowHandler;
    std::unique_ptr<MainUI> m_ui;
    std::unique_ptr<VideoSurfaceProvider> m_videoSurfaceProvider;
#ifdef _WIN32
    std::unique_ptr<WinTaskbarWidget> m_taskbarWidget;
#endif

    WindowEffectsModule* m_windowEffectsModule = nullptr;
    bool m_failedToLoadWindowEffectsModule = false;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(CompositorVideo::Flags)


/**
 * @brief The CompositorFactory class will instantiate a compositor
 * in auto mode, compositor will be instantiated from the list by order declaration,
 * compositor can be explicitly defined by passing its name.
 *
 * the usual scenario is:
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
