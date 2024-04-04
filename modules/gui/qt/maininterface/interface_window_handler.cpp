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


InterfaceWindowHandler::InterfaceWindowHandler(qt_intf_t *_p_intf, MainCtx* mainCtx, QWindow* window, QObject *parent)
    : QObject(parent)
    , p_intf(_p_intf)
    , m_window(window)
    , m_mainCtx(mainCtx)
{
    assert(m_window);
    assert(m_mainCtx);

    /* */
    m_pauseOnMinimize = var_InheritBool( p_intf, "qt-pause-minimized" );

    m_window->setIcon( QApplication::windowIcon() );
    m_window->setOpacity( var_InheritFloat( p_intf, "qt-opacity" ) );

    // this needs to be called asynchronously
    // otherwise QQuickWidget won't initialize properly
    QMetaObject::invokeMethod(this, [this]()
    {
        QVLCTools::restoreWindowPosition( getSettings(), m_window, QSize(600, 420) );

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
        int margin = m_mainCtx->windowExtendedMargin() * 2;
        int width = 320 + margin;
        int height = 300 + margin;

        double intfScaleFactor = m_mainCtx->getIntfScaleFactor();
        int scaledWidth = std::ceil( intfScaleFactor * width );
        int scaledHeight = std::ceil( intfScaleFactor * height );

        m_window->setMinimumSize( QSize(scaledWidth, scaledHeight) );
    };
    connect( m_mainCtx, &MainCtx::intfScaleFactorChanged, this, updateMinimumSize );
    connect( m_mainCtx, &MainCtx::windowExtendedMarginChanged, this, updateMinimumSize );
    m_mainCtx->updateIntfScaleFactor();

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

#if QT_CLIENT_SIDE_DECORATION_AVAILABLE
    connect( m_mainCtx, &MainCtx::useClientSideDecorationChanged,
             this, &InterfaceWindowHandler::updateCSDWindowSettings );
#endif

    connect(m_mainCtx, &MainCtx::requestInterfaceMaximized,
            this, &InterfaceWindowHandler::setInterfaceMaximized);

    connect(m_mainCtx, &MainCtx::requestInterfaceNormal,
            this, &InterfaceWindowHandler::setInterfaceNormal);

    connect(m_mainCtx, &MainCtx::requestInterfaceMinimized,
            this, &InterfaceWindowHandler::setInterfaceMinimized);

    m_window->installEventFilter(this);
}

InterfaceWindowHandler::~InterfaceWindowHandler()
{
    m_window->removeEventFilter(this);
    WindowStateHolder::holdOnTop( m_window,  WindowStateHolder::INTERFACE, false );
    WindowStateHolder::holdFullscreen( m_window,  WindowStateHolder::INTERFACE, false );
    /* Save this size */
    QVLCTools::saveWindowPosition(getSettings(), m_window);
}

#if QT_CLIENT_SIDE_DECORATION_AVAILABLE
void InterfaceWindowHandler::updateCSDWindowSettings()
{
    m_window->hide(); // some window managers don't like to change frame window hint on visible window
    m_window->setFlag(Qt::FramelessWindowHint, m_mainCtx->useClientSideDecoration());
    m_window->show();
}
#endif

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
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
            emit incrementIntfUserScaleFactor(wheelEvent->angleDelta().y() > 0);
#else
            emit incrementIntfUserScaleFactor(wheelEvent->delta() > 0);
#endif
            wheelEvent->accept();
            return true;
        }
        break;
    }
    case QEvent::Close:
    {
        bool ret = m_mainCtx->onWindowClose(m_window);
        if (ret)
        {
            /* Accept session quit. Otherwise we break the desktop mamager. */
            event->accept();
            return false;
        }
        else
        {
            event->ignore();
            return true;
        }
    }
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
    m_window->hide();
}

void InterfaceWindowHandler::setInterfaceShown()
{
    m_window->show();
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
    if (item && QQmlProperty(item, "visualFocus", qmlContext(item)).read().toBool())
    {
        return false;
    }

    event->accept();

    return true;
}
