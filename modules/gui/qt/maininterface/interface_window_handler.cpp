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
#include "main_interface.hpp"
#include <player/player_controller.hpp>
#include <playlist/playlist_controller.hpp>

InterfaceWindowHandler::InterfaceWindowHandler(intf_thread_t *_p_intf, MainInterface* mainInterface, QWindow* window, QObject *parent)
    : QObject(parent)
    , p_intf(_p_intf)
    , m_window(window)
    , m_mainInterface(mainInterface)
{
    assert(m_window);
    assert(m_mainInterface);

    /* */
    m_pauseOnMinimize = var_InheritBool( p_intf, "qt-pause-minimized" );

    m_window->setIcon( QApplication::windowIcon() );
    m_window->setOpacity( var_InheritFloat( p_intf, "qt-opacity" ) );

    WindowStateHolder::holdOnTop( m_window,  WindowStateHolder::INTERFACE, m_mainInterface->isInterfaceAlwaysOnTop() );
    WindowStateHolder::holdFullscreen( m_window,  WindowStateHolder::INTERFACE, m_mainInterface->isInterfaceFullScreen() );

    if (m_mainInterface->isHideAfterCreation())
    {
        //this needs to be called asynchronously
        //otherwise QQuickWidget won't initialize properly
        QMetaObject::invokeMethod(this, [this]() {
                m_window->hide();
            }, Qt::QueuedConnection, nullptr);
    }


    m_window->setTitle("");

    if( var_InheritBool( p_intf, "qt-name-in-title" ) )
    {
        connect( THEMIM, &PlayerController::nameChanged, m_window, &QWindow::setTitle );
    }

    connect( m_mainInterface, &MainInterface::askBoss,
             this, &InterfaceWindowHandler::setBoss, Qt::QueuedConnection  );
    connect( m_mainInterface, &MainInterface::askRaise,
             this, &InterfaceWindowHandler::setRaise, Qt::QueuedConnection  );

    connect( m_mainInterface, &MainInterface::interfaceAlwaysOnTopChanged,
             this, &InterfaceWindowHandler::setInterfaceAlwaysOnTop);

    connect( m_mainInterface, &MainInterface::interfaceFullScreenChanged,
             this, &InterfaceWindowHandler::setInterfaceFullScreen);

    connect( m_mainInterface, &MainInterface::toggleWindowVisibility,
             this, &InterfaceWindowHandler::toggleWindowVisiblity);

    connect( m_mainInterface, &MainInterface::setInterfaceVisibible,
             this, &InterfaceWindowHandler::setInterfaceVisible);

    connect(this, &InterfaceWindowHandler::incrementIntfUserScaleFactor,
            m_mainInterface, &MainInterface::incrementIntfUserScaleFactor);

    m_window->installEventFilter(this);
}

InterfaceWindowHandler::~InterfaceWindowHandler()
{
    m_window->removeEventFilter(this);
    WindowStateHolder::holdOnTop( m_window,  WindowStateHolder::INTERFACE, false );
    WindowStateHolder::holdFullscreen( m_window,  WindowStateHolder::INTERFACE, false );
}

#if QT_VERSION >= QT_VERSION_CHECK(5,15,0)
bool InterfaceWindowHandler::CSDSetCursor(QMouseEvent* mouseEvent)
{
    if (!m_mainInterface->useClientSideDecoration())
        return false;
    if ((m_window->visibility() & QWindow::Maximized) != 0)
        return false;
    Qt::CursorShape shape;
    const int x = mouseEvent->x();
    const int y = mouseEvent->y();
    const int winHeight = m_window->height();
    const int winWidth = m_window->width();
    const int b = 5 * m_mainInterface->getIntfScaleFactor();

    if (x < b && y < b) shape = Qt::SizeFDiagCursor;
    else if (x >= winWidth - b && y >= winHeight - b) shape = Qt::SizeFDiagCursor;
    else if (x >= winWidth - b && y < b) shape = Qt::SizeBDiagCursor;
    else if (x < b && y >= winHeight - b) shape = Qt::SizeBDiagCursor;
    else if (x < b || x >= winWidth - b) shape = Qt::SizeHorCursor;
    else if (y < b || y >= winHeight - b) shape = Qt::SizeVerCursor;
    else if (m_hasResizeCursor) {
        m_window->unsetCursor();
        m_hasResizeCursor = false;
        return false;
    } else {
        return false;
    }
    m_hasResizeCursor = true;
    m_window->setCursor(shape);
    return false;
}

bool InterfaceWindowHandler::CSDHandleClick(QMouseEvent* mouseEvent)
{
    if (!m_mainInterface->useClientSideDecoration())
        return false;
    const int b = 5 * m_mainInterface->getIntfScaleFactor();
    if( mouseEvent->buttons() != Qt::LeftButton)
        return false;
    if ((m_window->visibility() & QWindow::Maximized) != 0)
        return false;
    Qt::Edges edge;
    if (mouseEvent->x() < b) { edge |= Qt::LeftEdge; }
    if (mouseEvent->x() > m_window->width() - b) { edge |= Qt::RightEdge; }
    if (mouseEvent->y() < b) { edge |= Qt::TopEdge; }
    if (mouseEvent->y() > m_window->height() - b) { edge |= Qt::BottomEdge; }
    if (edge != 0) {
        m_window->startSystemResize(edge);
        return true;
    }
    return false;
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
            m_window->showMaximized();
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
#if QT_VERSION >= QT_VERSION_CHECK(5,15,0)
    //Handle CSD edge behaviors
    case QEvent::MouseMove:
    {
        QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
        return CSDSetCursor(mouseEvent);
    }
    case QEvent::MouseButtonPress:
    {
        QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
        return CSDHandleClick(mouseEvent);
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


void InterfaceWindowHandler::toggleWindowVisiblity()
{
    switch ( m_window->visibility() )
    {
    case QWindow::Hidden:
        /* If hidden, show it */
        m_window->show();
        m_window->requestActivate();
        break;
    case QWindow::Minimized:
        m_window->showNormal();
        m_window->requestActivate();
        break;
    default:
        m_window->hide();
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
            m_window->show();
            break;
        case QWindow::Minimized:
            m_window->showNormal();
            break;
        default:
            break;
        }
        m_window->requestActivate();
    }
    else
    {
        m_window->hide();
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
    m_window->requestActivate();
    m_window->raise();
}

void InterfaceWindowHandler::setBoss()
{
    THEMPL->pause();
    if( m_mainInterface->getSysTray() )
    {
        m_window->hide();
    }
    else
    {
        m_window->showMinimized();
    }
}

void InterfaceWindowHandler::setInterfaceAlwaysOnTop( bool on_top )
{
    WindowStateHolder::holdOnTop(m_window, WindowStateHolder::INTERFACE, on_top);
}
