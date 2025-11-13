/*****************************************************************************
 * Copyright (C) 2020 VideoLAN and AUTHORS
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
#include "interface_window_handler.hpp"
#include "mainctx.hpp"
#include "compositor.hpp"
#include <player/player_controller.hpp>
#include <playlist/playlist_controller.hpp>
#include "util/keyhelper.hpp"
#include "dialogs/systray/systray.hpp"
#include "widgets/native/qvlcframe.hpp"
#include "dialogs/dialogs/dialogmodel.hpp"
#include <QScreen>
#include <QQmlProperty>
#include <cmath>

namespace
{

void setWindowState(QWindow *window, Qt::WindowState state)
{
    // make sure to preserve original state, Qt saves this info
    // in underlying platform window but we need this in top level
    // so that our window handling code works.
    // see issue #28071
    const auto original = window->windowStates();
    window->setWindowStates(original | state);
}

}

const Qt::Key InterfaceWindowHandler::kc[10] =
{
        Qt::Key_Up, Qt::Key_Up,
        Qt::Key_Down, Qt::Key_Down,
        Qt::Key_Left, Qt::Key_Right, Qt::Key_Left, Qt::Key_Right,
        Qt::Key_B, Qt::Key_A
};

InterfaceWindowHandler::InterfaceWindowHandler(qt_intf_t *_p_intf, MainCtx* mainCtx, QWindow* window, QObject *parent)
    : QObject(parent)
    , p_intf(_p_intf)
    , m_window(window)
    , m_mainCtx(mainCtx)
{
    assert(m_window);
    assert(m_mainCtx);

    if (m_mainCtx->useClientSideDecoration())
        m_window->setFlag(Qt::FramelessWindowHint);

    /* */
    m_pauseOnMinimize = var_InheritBool( p_intf, "qt-pause-minimized" );

    m_window->setIcon( QApplication::windowIcon() );
    m_window->setOpacity( var_InheritFloat( p_intf, "qt-opacity" ) );

    QVLCTools::restoreWindowPosition( getSettings(), m_window, QSize(600, 420) );

    // this needs to be called asynchronously
    // otherwise QQuickWidget won't initialize properly
    QMetaObject::invokeMethod(this, [this]()
    {
        WindowStateHolder::holdOnTop( m_window,  WindowStateHolder::INTERFACE, m_mainCtx->isInterfaceAlwaysOnTop() );
        WindowStateHolder::holdFullscreen( m_window,  WindowStateHolder::INTERFACE, m_window->visibility() == QWindow::FullScreen );

        if (m_mainCtx->isHideAfterCreation())
            m_window->hide();
    }, Qt::QueuedConnection);

    m_window->setTitle("");

    if( var_InheritBool( p_intf, "qt-name-in-title" ) )
    {
        connect( THEMIM, &PlayerController::nameChanged, m_window, &QWindow::setTitle );
    }

    connect( m_window, &QWindow::screenChanged, m_mainCtx, &MainCtx::updateIntfScaleFactor );

    const auto updateMinimumSize = [this]()
    {
        if (m_mainCtx->isMinimalView())
        {
            m_window->setMinimumSize({128, 16});
            return;
        }

        int margin = m_mainCtx->windowExtendedMargin() * 2;
        int width = 320 + margin;
        int height = 360 + margin;

        double intfScaleFactor = m_mainCtx->getIntfScaleFactor();
        int scaledWidth = std::ceil( intfScaleFactor * width );
        int scaledHeight = std::ceil( intfScaleFactor * height );

        m_window->setMinimumSize( QSize(scaledWidth, scaledHeight) );
    };
    connect( m_mainCtx, &MainCtx::intfScaleFactorChanged, this, updateMinimumSize );
    connect( m_mainCtx, &MainCtx::windowExtendedMarginChanged, this, updateMinimumSize );
    connect( m_mainCtx, &MainCtx::minimalViewChanged, this, updateMinimumSize );
    m_mainCtx->updateIntfScaleFactor();
    updateMinimumSize();

    m_mainCtx->onWindowVisibilityChanged(m_window->visibility());
    connect( m_window, &QWindow::visibilityChanged,
             m_mainCtx, &MainCtx::onWindowVisibilityChanged);

    connect( m_mainCtx, &MainCtx::askBoss,
             this, &InterfaceWindowHandler::setBoss, Qt::QueuedConnection  );
    connect( m_mainCtx, &MainCtx::askRaise,
             this, &InterfaceWindowHandler::setRaise, Qt::QueuedConnection  );

    connect( m_mainCtx, &MainCtx::interfaceAlwaysOnTopChanged,
             this, &InterfaceWindowHandler::setInterfaceAlwaysOnTop);

    connect( m_mainCtx, &MainCtx::setInterfaceFullScreen,
             this, &InterfaceWindowHandler::setInterfaceFullScreen);

    connect( m_mainCtx, &MainCtx::toggleWindowVisibility,
             this, &InterfaceWindowHandler::toggleWindowVisibility);

    connect( m_mainCtx, &MainCtx::setInterfaceVisibible,
             this, &InterfaceWindowHandler::setInterfaceVisible);

    connect(this, &InterfaceWindowHandler::incrementIntfUserScaleFactor,
            m_mainCtx, &MainCtx::incrementIntfUserScaleFactor);

    connect( m_mainCtx, &MainCtx::useClientSideDecorationChanged,
             this, &InterfaceWindowHandler::updateCSDWindowSettings );

    connect(m_mainCtx, &MainCtx::requestInterfaceMaximized,
            this, &InterfaceWindowHandler::setInterfaceMaximized);

    connect(m_mainCtx, &MainCtx::requestInterfaceNormal,
            this, &InterfaceWindowHandler::setInterfaceNormal);

    connect(m_mainCtx, &MainCtx::requestInterfaceMinimized,
            this, &InterfaceWindowHandler::setInterfaceMinimized);

    connect(this, &InterfaceWindowHandler::kc_pressed,
            m_mainCtx, &MainCtx::kc_pressed);

    const auto dem = DialogErrorModel::getInstance<false>(); // expected to be already created
    assert(dem);
    connect(dem, &DialogErrorModel::rowsInserted, this, [w = QPointer(m_window)]() {
        if (Q_LIKELY(w))
            w->alert(0);
    });

    connect(
        &m_wheelAccumulator, &WheelToVLCConverter::wheelUpDown,
        this, [this] (int steps, Qt::KeyboardModifiers modifiers) {
            if (modifiers != Qt::ControlModifier)
                return;
            emit incrementIntfUserScaleFactor(steps > 0);
    });

    m_window->installEventFilter(this);
}

InterfaceWindowHandler::~InterfaceWindowHandler()
{
    m_window->removeEventFilter(this);
    /* Save this size */
    QVLCTools::saveWindowPosition(getSettings(), m_window);
    assert(qApp);
    if (!qApp->property("isDying").toBool())
    {
        // If application is dying, we don't need to adjust the state
        WindowStateHolder::holdOnTop( m_window,  WindowStateHolder::INTERFACE, false );
        WindowStateHolder::holdFullscreen( m_window,  WindowStateHolder::INTERFACE, false );
    }
}

void InterfaceWindowHandler::updateCSDWindowSettings()
{
    m_window->setFlag(Qt::FramelessWindowHint, m_mainCtx->useClientSideDecoration());
}

bool InterfaceWindowHandler::eventFilter(QObject*, QEvent* event)
{
    switch ( event->type() )
    {
    case QEvent::WindowStateChange:
    {
        QWindowStateChangeEvent *windowStateChangeEvent = static_cast<QWindowStateChangeEvent*>(event);
        Qt::WindowStates newState = m_window->windowStates();
        Qt::WindowStates oldState = windowStateChangeEvent->oldState();

        /* b_maximizedView stores if the window was maximized before entering fullscreen.
         * It is set when entering maximized mode, unset when leaving it to normal mode.
         * Upon leaving full screen, if b_maximizedView is set,
         * the window should be maximized again. */
        if( newState & Qt::WindowMaximized &&
            !( oldState & Qt::WindowMaximized ) )
            m_maximizedView = true;

        if( !( newState & Qt::WindowMaximized ) &&
            oldState & Qt::WindowMaximized ) //FIXME && !b_videoFullScreen )
            m_maximizedView = false;

        if( !( newState & Qt::WindowFullScreen ) &&
            oldState & Qt::WindowFullScreen &&
            m_maximizedView )
        {
            setInterfaceMaximized();
            return false;
        }

        if( newState & Qt::WindowMinimized )
        {

            m_hasPausedWhenMinimized = false;

            if( THEMIM->getPlayingState() == PlayerController::PLAYING_STATE_PLAYING &&
                THEMIM->hasVideoOutput() && !THEMIM->hasAudioVisualization() &&
                m_pauseOnMinimize )
            {
                m_hasPausedWhenMinimized = true;
                THEMPL->pause();
            }
        }
        else if( oldState & Qt::WindowMinimized && !( newState & Qt::WindowMinimized ) )
        {
            if( m_hasPausedWhenMinimized )
            {
                THEMPL->play();
            }
        }
        break;
    }
    case QEvent::KeyPress:
    {
        QKeyEvent * keyEvent = static_cast<QKeyEvent *> (event);

        /* easter eggs sequence handling */
        if ( keyEvent->key() == kc[ i_kc_offset ] )
            i_kc_offset++;
        else
            i_kc_offset = 0;

        if ( i_kc_offset == ARRAY_SIZE( kc ) )
        {
            i_kc_offset = 0;
            emit kc_pressed();
        }

        if (applyKeyEvent(keyEvent) == false)
            return false;

        m_mainCtx->sendHotkey(static_cast<Qt::Key> (keyEvent->key()), keyEvent->modifiers());

        return true;
    }
    case QEvent::KeyRelease:
    {
        return applyKeyEvent(static_cast<QKeyEvent *> (event));
    }
    case QEvent::Wheel:
    {
        QWheelEvent* wheelEvent = static_cast<QWheelEvent*>(event);
        if (wheelEvent->modifiers() == Qt::ControlModifier)
        {
            m_wheelAccumulator.wheelEvent(wheelEvent);
            wheelEvent->accept();
            return true;
        }
        break;
    }
    case QEvent::Close:
    {
#if QT_VERSION < QT_VERSION_CHECK(6, 9, 0)
        // Before Qt 6.9.0, `QWindow`'s `wl_surface` gets deleted when `QWindow::hide()` is called.
        static bool platformIsWayland = []() {
            assert(qGuiApp);
            return qGuiApp->platformName().startsWith(QLatin1String("wayland"));
        }();

        if (!platformIsWayland)
            setInterfaceHiden();
#else
        setInterfaceHiden();
#endif

        if (var_InheritBool(p_intf, "qt-close-to-system-tray"))
        {
            if (const QSystemTrayIcon* const sysTrayIcon = m_mainCtx->getSysTray())
            {
                if (sysTrayIcon->isSystemTrayAvailable() && sysTrayIcon->isVisible())
                {
#if QT_VERSION < QT_VERSION_CHECK(6, 9, 0)
                    if (platformIsWayland)
                        setInterfaceHiden();
#endif
                    event->ignore();
                    return true;
                }
            }
        }

        m_mainCtx->onWindowClose(m_window);
        event->ignore();
        return true;
    }
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
    case QEvent::DevicePixelRatioChange:
    {
        m_mainCtx->intfDevicePixelRatioChanged();
    }
#endif

    default:
        break;
    }

    return false;
}

void InterfaceWindowHandler::onVideoEmbedChanged(bool embed)
{
    if (embed)
    {
        m_interfaceGeometry = m_window->geometry();
    }
    else if (!m_interfaceGeometry.isNull())
    {
        m_window->setGeometry(m_interfaceGeometry);
        m_interfaceGeometry = QRect();
    }
}


void InterfaceWindowHandler::toggleWindowVisibility()
{
    switch ( m_window->visibility() )
    {
    case QWindow::Hidden:
        /* If hidden, show it */
        setInterfaceShown();
        requestActivate();
        break;
    case QWindow::Minimized:
        setInterfaceNormal();
        requestActivate();
        break;
    default:
        setInterfaceHiden();
        break;
    }
}


void InterfaceWindowHandler::setInterfaceVisible(bool visible)
{
    if (visible)
    {
        switch ( m_window->visibility() )
        {
        case QWindow::Hidden:
            setInterfaceShown();
            break;
        case QWindow::Minimized:
            setInterfaceNormal();
            break;
        default:
            break;
        }
        requestActivate();
    }
    else
    {
        setInterfaceHiden();
    }
}


void InterfaceWindowHandler::setFullScreen( bool fs )
{
    WindowStateHolder::holdFullscreen(m_window, WindowStateHolder::INTERFACE, fs);
}

void InterfaceWindowHandler::setInterfaceFullScreen( bool fs )
{
    setFullScreen(fs);
    emit interfaceFullScreenChanged( fs );
}

void InterfaceWindowHandler::setRaise()
{
    requestActivate();
    m_window->raise();
}

void InterfaceWindowHandler::setBoss()
{
    THEMPL->pause();
    if( m_mainCtx->getSysTray() )
    {
        setInterfaceHiden();
    }
    else
    {
        setInterfaceMinimized();
    }
}

void InterfaceWindowHandler::setInterfaceHiden()
{
#if QT_VERSION < QT_VERSION_CHECK(6, 9, 0)
    // `CompositorWayland` depends on the promise that the window resources are not deleted.
    // Window's `wl_surface` gets deleted when the window gets hidden before Qt 6.9.0. So,
    // here we should inhibit hiding the window. Note that intercepting `QEvent::hide` does
    // not make it possible to inhibit hiding the window.
    assert(p_intf);
    assert(p_intf->p_compositor);
    static bool inhibitHide = [intf = p_intf]() {
        assert(qGuiApp);
        // type() == WaylandCompositor is not used because any video embedding methodology may
        // depend on window resources:
        const bool ret = qGuiApp->platformName().startsWith(QLatin1String("wayland")) &&
                         dynamic_cast<vlc::CompositorVideo*>(intf->p_compositor.get());
        if (ret)
            msg_Warn(intf, "In this configuration, the interface window can not get hidden.");
        return ret;
    }();

    if (inhibitHide)
    {
        // Instead of doing nothing, minimize the window:
        setInterfaceMinimized();
        return;
    }
#endif

    m_window->hide();
}

void InterfaceWindowHandler::setInterfaceShown()
{
    m_window->setVisible(true);
}

void InterfaceWindowHandler::setInterfaceMinimized()
{
    setWindowState(m_window, Qt::WindowMinimized);
}

void InterfaceWindowHandler::setInterfaceMaximized()
{
    setWindowState(m_window, Qt::WindowMaximized);
}

void InterfaceWindowHandler::setInterfaceNormal()
{
    m_window->showNormal();
}


void InterfaceWindowHandler::requestActivate()
{
    m_window->requestActivate();
}

void InterfaceWindowHandler::setInterfaceAlwaysOnTop( bool on_top )
{
    WindowStateHolder::holdOnTop(m_window, WindowStateHolder::INTERFACE, on_top);
}

// Functions private

bool InterfaceWindowHandler::applyKeyEvent(QKeyEvent * event) const
{
    int key = event->key();

    // NOTE: We have to make sure tab and backtab are never used as hotkeys. When browsing the
    //       MediaLibrary, we let the view handle the navigation keys.
    if (key == Qt::Key_Tab || key == Qt::Key_Backtab || m_mainCtx->preferHotkeys() == false)
    {
        return false;
    }

    QQuickItem * item = p_intf->p_compositor->activeFocusItem();

    // NOTE: When the item has visual focus we let it handle the key. When the item does not
    //       inherit from QQuickControl we have to declare the 'visualFocus' property ourselves.
    if (item)
    {

        QVariant visualFocus = QQmlProperty::read(item, "visualFocus", qmlContext(item));
        if (visualFocus.isValid())
        {
            if (visualFocus.toBool())
                return false;
        }
        //while being QuickControls TextField and TextArea don't provide visualFocus property
        //here we check their (non control) parent class, this should cover all text input widgets
        else if (item->inherits("QQuickTextInput") || item->inherits("QQuickTextEdit"))
        {
            QVariant activeFocus = QQmlProperty::read(item, "activeFocus", qmlContext(item));
            if (activeFocus.isValid())
            {
                if (activeFocus.toBool())
                    return false;
            }
        }
    }

    event->accept();

    return true;
}
