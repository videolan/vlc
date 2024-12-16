/*****************************************************************************
 * Copyright (C) 2024 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * ( at your option ) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/
#include "csdmenu.hpp"

#include <QApplication>
#include <QAction>
#include <QWindow>

#ifdef QT_GUI_PRIVATE
#include <QtGui/qpa/qplatformnativeinterface.h>
#include <QtGui/qpa/qplatformwindow.h>
#include <QtGui/qpa/qplatformwindow_p.h>
#endif

#include "maininterface/mainctx.hpp"
#include "util/csdmenu_module.h"
#include "menus/menus.hpp"

#include <vlc_common.h>
#include <vlc_modules.h>

#if QT_VERSION < QT_VERSION_CHECK(6, 7, 0)
#if defined(Q_OS_UNIX)
#define QT_FEATURE_wayland 1
#else
#define QT_FEATURE_wayland -1
#endif
#endif

struct xdg_surface;

namespace {

//vlc module callback
static int csdMenuInfoOpen(void *func, bool forced, va_list ap)
{
    VLC_UNUSED(forced);
    qt_csd_menu_open open = reinterpret_cast<qt_csd_menu_open>(func);
    qt_csd_menu_t* obj = va_arg(ap, qt_csd_menu_t*);
    qt_csd_menu_info* info = va_arg(ap, qt_csd_menu_info*);
    return open(obj, info);
}

bool isWindowFixedSize(const QWindow *window)
{
    if (window->flags() & Qt::MSWindowsFixedSizeDialogHint)
        return true;

    const auto minSize = window->minimumSize();
    const auto maxSize = window->maximumSize();

    return minSize.isValid() && maxSize.isValid() && minSize == maxSize;
}

}

class CSDMenuPrivate
{
    Q_DECLARE_PUBLIC(CSDMenu)
public:

    CSDMenuPrivate(CSDMenu* parent)
        : q_ptr(parent)
    {
        const QString& platform = qApp->platformName();;
        if (platform == QLatin1String("windows") || platform == QLatin1String("direct2d"))
            m_plateform = QT_CSD_PLATFORM_WINDOWS;
        else if (platform.startsWith( QLatin1String("wayland")))
            m_plateform = QT_CSD_PLATFORM_WAYLAND;
        else if (platform == QLatin1String("xcb"))
            m_plateform = QT_CSD_PLATFORM_X11;
    }

    static void notifyMenuVisible(void* data, bool visible)
    {
        auto that = static_cast<CSDMenuPrivate*>(data);
        that->m_menuVisible = visible;
        emit that->q_func()->menuVisibleChanged(visible);
    }

    void fallbackMenu(QPoint pos)
    {
        auto menu = new VLCMenu(m_ctx->getIntf());
        menu->setAttribute(Qt::WA_DeleteOnClose);

        QObject::connect(menu, &QMenu::aboutToShow, q_ptr, [this](){
            notifyMenuVisible(this, true);
        });

        QObject::connect(menu, &QMenu::aboutToHide, q_ptr, [this](){
            notifyMenuVisible(this, false);
        });

        QWindow::Visibility visibility = m_ctx->interfaceVisibility();
        QAction* action;
        action = menu->addAction(qtr("Restore"));
        action->setEnabled(visibility != QWindow::Windowed);
        action->connect(action, &QAction::triggered, q_ptr, [this]() {
            m_ctx->requestInterfaceNormal();
        });
        action = menu->addAction(qtr("Minimize"));
        action->connect(action, &QAction::triggered, q_ptr, [this]() {
            m_ctx->requestInterfaceMinimized();
        });
        action = menu->addAction(qtr("Maximized"));
        action->setEnabled(visibility != QWindow::Maximized);
        action->connect(action, &QAction::triggered, q_ptr, [this]() {
            m_ctx->requestInterfaceMaximized();
        });
        action = menu->addAction(qtr("Always on top"));
        action->setCheckable(true);
        action->setChecked(m_ctx->isInterfaceAlwaysOnTop());
        action->connect(action, &QAction::triggered, q_ptr, [this](bool checked) {
            m_ctx->setInterfaceAlwaysOnTop(checked);
        });
        action = menu->addAction(qtr("Close"));
        action->connect(action, &QAction::triggered, q_ptr, [this]() {
            m_ctx->intfMainWindow()->close();
        });

        /* Ideally we should allow moving and resizing the window through startSystemXXX
         * But, startSystemXXX is usually active from a mouse press until a mouse release,
         * so calling it from a menu is broken with some desktop environments
         */

        menu->popup(pos);
    }

    void loadModule()
    {
        QWindow* window = m_ctx->intfMainWindow();
        assert(window);

        qt_csd_menu_info info = {};
        info.platform = QT_CSD_PLATFORM_UNKNOWN;
#ifdef _WIN32
        if (m_plateform == QT_CSD_PLATFORM_WINDOWS)
        {
            info.platform = QT_CSD_PLATFORM_WINDOWS;
        };
#else
#ifdef QT_GUI_PRIVATE
        if (m_plateform == QT_CSD_PLATFORM_X11)
        {
            QPlatformNativeInterface* native = qApp->platformNativeInterface();
            info.platform = QT_CSD_PLATFORM_X11;
            info.data.x11.rootwindow = reinterpret_cast<intptr_t>(native->nativeResourceForIntegration(QByteArrayLiteral("rootwindow")));
            info.data.x11.connection = reinterpret_cast<struct xcb_connection_t*>(native->nativeResourceForIntegration(QByteArrayLiteral("connection")));
        }

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0) && QT_CONFIG(wayland)
        if (m_plateform == QT_CSD_PLATFORM_WAYLAND)
        {
            info.platform = QT_CSD_PLATFORM_WAYLAND;

            auto appNative = qApp->nativeInterface<QNativeInterface::QWaylandApplication>();
            auto windowNative = window->nativeInterface<QNativeInterface::Private::QWaylandWindow>();

            info.data.wayland.display = appNative->display();
            info.data.wayland.toplevel = windowNative->surfaceRole<xdg_toplevel>();
        }
#endif // QT 6.5
#endif // QT_GUI_PRIVATE
#endif // _WIN32
        info.isRtl = QGuiApplication::isRightToLeft();

        info.userData = this;
        info.notifyMenuVisible = &notifyMenuVisible;

        m_systemMenuState = UNAVAILABLE;
        m_systemMenu = vlc_object_create<qt_csd_menu_t>(m_ctx->getIntf());
        if (!m_systemMenu)
            return;
        m_module = vlc_module_load(vlc_object_logger(m_systemMenu), "qtcsdmenu", nullptr, false, &csdMenuInfoOpen, m_systemMenu, &info);
        if (!m_module)
        {
            vlc_object_delete(m_systemMenu);
            m_systemMenu = nullptr;
            return;
        }
        m_systemMenuState = AVAILABLE;
    }

    void unloadModule()
    {
        if (m_module)
        {
            module_unneed(m_systemMenu, m_module);
            m_module = nullptr;
        }
        if (m_systemMenu)
        {
            vlc_object_delete(m_systemMenu);
            m_systemMenu = nullptr;
        }
        m_systemMenuState = CSDMenuPrivate::NEED_INITIALISATION;
    }

    enum {
        NEED_INITIALISATION,
        UNAVAILABLE,
        AVAILABLE
    } m_systemMenuState = NEED_INITIALISATION;

    module_t* m_module = nullptr;
    qt_csd_menu_t* m_systemMenu = nullptr;

    qt_csd_menu_platform m_plateform = QT_CSD_PLATFORM_UNKNOWN;
    MainCtx* m_ctx = nullptr;
    bool m_menuVisible = false;

    CSDMenu* q_ptr;
};

CSDMenu::CSDMenu(QObject* parent)
    : QObject(parent)
    , d_ptr(new CSDMenuPrivate(this))
{
}

CSDMenu::~CSDMenu()
{
    Q_D(CSDMenu);
    d->unloadModule();
}

MainCtx* CSDMenu::getCtx() const
{
    Q_D(const CSDMenu);
    return d->m_ctx;
}

void CSDMenu::setCtx(MainCtx* ctx)
{
    Q_D(CSDMenu);

    if (d->m_ctx == ctx)
        return;
    d->m_ctx = ctx;
    emit ctxChanged(ctx);
}

bool CSDMenu::getMenuVisible() const
{
    Q_D(const CSDMenu);
    return d->m_menuVisible;
}

void CSDMenu::popup(const QPoint &pos)
{
    Q_D(CSDMenu);

    assert(d->m_ctx);

    QWindow* window = d->m_ctx->intfMainWindow();
    assert(window);

    if (d->m_systemMenuState == CSDMenuPrivate::NEED_INITIALISATION)
        d->loadModule();

    if (d->m_systemMenuState == CSDMenuPrivate::AVAILABLE) {
        assert(d->m_systemMenu);

        qreal qtDpr = window->devicePixelRatio();
        QPlatformWindow* nativeWindow = window->handle();
        qreal nativeDpr = nativeWindow->devicePixelRatio();

        qt_csd_menu_event event = {};
        event.platform = QT_CSD_PLATFORM_UNKNOWN;
        event.x = (pos.x() * qtDpr) / nativeDpr;
        event.y = (pos.y() * qtDpr) / nativeDpr;

        const auto winState = window->windowStates();
        int windowFlags = (winState.testFlag(Qt::WindowMaximized) ? QT_CSD_WINDOW_MAXIMIZED : 0)
            | (winState.testFlag(Qt::WindowFullScreen) ? QT_CSD_WINDOW_FULLSCREEN : 0)
            | (winState.testFlag(Qt::WindowMinimized) ? QT_CSD_WINDOW_MINIMIZED: 0)
            | (isWindowFixedSize(window) ? QT_CSD_WINDOW_FIXED_SIZE : 0);

        event.windowState = static_cast<qt_csd_menu_window_state>(windowFlags);

#ifdef _WIN32
        if (d->m_plateform == QT_CSD_PLATFORM_WINDOWS)
        {
            event.platform = QT_CSD_PLATFORM_WINDOWS;
            event.data.win32.hwnd = reinterpret_cast<void*>(window->winId());
        }
#else

#ifdef QT_GUI_PRIVATE
        if (d->m_plateform == QT_CSD_PLATFORM_X11)
        {
            event.platform = QT_CSD_PLATFORM_X11;
            event.data.x11.window = window->winId();
        }

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0) && QT_CONFIG(wayland)
        if (d->m_plateform == QT_CSD_PLATFORM_WAYLAND)
        {
            event.platform = QT_CSD_PLATFORM_WAYLAND;
            auto appNative = qApp->nativeInterface<QNativeInterface::QWaylandApplication>();
            assert(appNative);
            event.data.wayland.seat = appNative->lastInputSeat();
            event.data.wayland.serial = appNative->lastInputSerial();
        }
#endif // Qt 6.5
#endif // QT_GUI_PRIVATE
#endif // _WIN32

        bool ret = d->m_systemMenu->popup(d->m_systemMenu, &event);
        if (ret)
            return;
    }

    d->fallbackMenu(pos);
}

