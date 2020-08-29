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
#include "video_window_handler.hpp"
#include "main_interface.hpp"

#include <QApplication>
#include <QScreen>

#include <vlc_vout_window.h>

VideoWindowHandler::VideoWindowHandler(intf_thread_t* intf, MainInterface* mainInterace,QObject *parent)
    : QObject(parent)
    , m_intf(intf)
    , m_interface(mainInterace)
{
    /* Does the interface resize to video size or the opposite */
    m_autoresize = var_InheritBool( m_intf, "qt-video-autoresize" );

    connect( this, &VideoWindowHandler::askVideoToResize,
             this, &VideoWindowHandler::setVideoSize, Qt::QueuedConnection );
    connect( this, &VideoWindowHandler::askVideoSetFullScreen,
             this, &VideoWindowHandler::setVideoFullScreen, Qt::QueuedConnection );
    connect( this, &VideoWindowHandler::askVideoOnTop,
             this, &VideoWindowHandler::setVideoOnTop, Qt::QueuedConnection );
}

void VideoWindowHandler::setWindow(QWindow* window)
{
    if (m_window == window)
        return;
    if (m_window)
    {
        WindowStateHolder::holdOnTop(m_window, WindowStateHolder::VIDEO, false);
        WindowStateHolder::holdFullscreen(m_window, WindowStateHolder::VIDEO, false);
    }
    m_window = window;
    if (m_window)
    {
        m_lastWinGeometry = m_window->geometry();
    }
    else
        m_lastWinGeometry = QRect{};
}

void VideoWindowHandler::disable()
{
    emit askVideoSetFullScreen( false );
    emit askVideoOnTop( false );
}

void VideoWindowHandler::requestResizeVideo( unsigned i_width, unsigned i_height )
{
    emit askVideoToResize( i_width, i_height );
}

void VideoWindowHandler::requestVideoWindowed( )
{
   emit askVideoSetFullScreen( false );
}

void VideoWindowHandler::requestVideoFullScreen(const char * )
{
    emit askVideoSetFullScreen( true );
}

void VideoWindowHandler::requestVideoState(  unsigned i_arg )
{
    bool on_top = (i_arg & VOUT_WINDOW_STATE_ABOVE) != 0;
    emit askVideoOnTop( on_top );
}

void VideoWindowHandler::setVideoSize(unsigned int w, unsigned int h)
{
    if (!m_window)
        return;
    Qt::WindowStates states = m_window->windowStates();
    if ((states & (Qt::WindowFullScreen | Qt::WindowMaximized)) == 0)
    {
        /* Resize video widget to video size, or keep it at the same
         * size. Call setSize() either way so that vout_window_ReportSize
         * will always get called.
         * If the video size is too large for the screen, resize it
         * to the screen size.
         */
        if (m_autoresize)
        {
            QRect screen = m_window->screen()->availableGeometry();
            qreal factor = m_window->devicePixelRatio();
            if( (float)h / factor > screen.height() )
            {
                w = screen.width();
                h = screen.height();
            }
            else
            {
                // Convert the size in logical pixels
                w = qRound( (float)w / factor );
                h = qRound( (float)h / factor );
                msg_Dbg( m_intf, "Logical video size: %ux%u", w, h );
            }
            m_window->resize(w, h);
        }
    }
}

void VideoWindowHandler::setVideoFullScreen( bool fs )
{
    if (!m_window)
        return;
    m_videoFullScreen = fs;
    if( fs )
    {
        int numscreen = var_InheritInteger( m_intf, "qt-fullscreen-screennumber" );

        auto screenList = QApplication::screens();
        if ( numscreen >= 0 && numscreen < screenList.count() )
        {
            QRect screenres = screenList[numscreen]->geometry();
            m_lastWinScreen = m_window->screen();
#ifdef QT5_HAS_WAYLAND
            if( !m_hasWayland )
                m_window->setScreen(screenList[numscreen]);
#endif
            m_window->setScreen(screenList[numscreen]);

            /* To be sure window is on proper-screen in xinerama */
            if( !screenres.contains( m_window->position() ) )
            {
                m_lastWinGeometry = m_window->geometry();
                m_window->setPosition(screenres.x(), screenres.y() );
            }
        }
        WindowStateHolder::holdFullscreen(m_window,  WindowStateHolder::VIDEO, true);
    }
    else
    {
        bool hold = WindowStateHolder::holdFullscreen(m_window,  WindowStateHolder::VIDEO, false);

#ifdef QT5_HAS_WAYLAND
        if( m_lastWinScreen != NULL && !m_hasWayland )
            m_window->setScreen(m_lastWinScreen);
#else
        if( m_lastWinScreen != NULL )
            m_window->setScreen(m_lastWinScreen);
#endif
        if( !hold && m_lastWinGeometry.isNull() == false )
        {
            m_window->setGeometry( m_lastWinGeometry );
            m_lastWinGeometry = QRect();
        }
    }
}


/* Slot to change the video always-on-top flag.
 * Emit askVideoOnTop() to invoke this from other thread. */
void VideoWindowHandler::setVideoOnTop( bool on_top )
{
    if (!m_window)
        return;
    WindowStateHolder::holdOnTop(m_window, WindowStateHolder::VIDEO, on_top);
}
