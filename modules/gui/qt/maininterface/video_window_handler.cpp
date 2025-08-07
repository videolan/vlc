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
#include "mainctx.hpp"

#include <QApplication>
#include <QScreen>

#include <vlc_window.h>
#include "compositor.hpp"

VideoWindowHandler::VideoWindowHandler(qt_intf_t* intf, QObject *parent)
    : QObject(parent)
    , m_intf(intf)
{
    /* Does the interface resize to video size or the opposite */
    m_autoresize = var_InheritBool( m_intf, "qt-video-autoresize" );

    assert(qGuiApp);
    m_hasWayland = qGuiApp->platformName().startsWith(QLatin1String("wayland"));

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
}

void VideoWindowHandler::disable()
{
    emit askVideoSetFullScreen( false );
    emit askVideoOnTop( false );
}

void VideoWindowHandler::requestResizeVideo( unsigned i_width, unsigned i_height )
{
    if (!m_window)
        return;
    emit askVideoToResize( i_width, i_height, m_window->windowStates() );
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
    bool on_top = (i_arg & VLC_WINDOW_STATE_ABOVE) != 0;
    emit askVideoOnTop( on_top );
}

void VideoWindowHandler::setVideoSize(unsigned int w, unsigned int h, Qt::WindowStates currentStates)
{
    if (!m_window)
        return;
    Qt::WindowStates states = m_window->windowStates();

    // This slot is queued, so by the time it is called
    // the window states reflect the final states.
    // `qt-video-autoresize` should not apply during state
    // transition, that was also the behavior in VLC 3.
    // Otherwise, when getting disengaged from fullscreen
    // the interface/video would forget its old (overridden)
    // size.

    // Do not resize when transitioning between states:
    if (currentStates != states)
        return;

    if ((states & (Qt::WindowFullScreen | Qt::WindowMaximized)) == 0)
    {
        /* Resize video widget to video size, or keep it at the same
         * size. Call setSize() either way so that vlc_window_ReportSize
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
                {
                    // Convert the size in logical pixels
                    w = qRound( (float)w / factor );
                    h = qRound( (float)h / factor );
                    msg_Dbg( m_intf, "Logical video size: %ux%u", w, h );
                }

                {
                    // Video window is anchored to the interface window, this is
                    // expected because video window is "embedded". It is not the
                    // other way around, where the window would adjust its size
                    // depending on the size of the video window. However here,
                    // with auto resize, the size is solely for the video window.
                    // So, with the current approach, we need to compensate non-
                    // video area for setting the window size to make the video
                    // window have the expected size. If in the future we change
                    // the approach, we can get rid of this workaround and rather
                    // set the size of video surface item in the Qt Quick scene.
                    // Currently this is mostly a case with pinned controls in
                    // the player page, where the controls do not overlay the
                    // video window/surface.

                    assert(m_intf);
                    assert(m_intf->p_compositor); // this slot should not be executed otherwise
                    const auto quickWindow = m_intf->p_compositor->quickWindow();
                    assert(quickWindow);
                    const auto contentItem = quickWindow->contentItem();
                    assert(contentItem);

                    // I initially wanted to probe the QML video surface item, but
                    // we can not do that because it is not available when this slot
                    // is executed. In fact, due to late `MainCtx.hasEmbededVideo`
                    // adjustment, it still remains "false" with a video input and
                    // player state is in playing state. Worse, `hasVideoOutput`
                    // is also "false" in the same situation (player reports playing
                    // state). Relying on a delay there would be fragile, but we can
                    // simply probe the loader instead, where the video surface item
                    // is expected to fill it (so has the same size).
                    if (const auto playerSpecializationLoader = contentItem->findChild<QQuickItem*>(QStringLiteral("playerSpecializationLoader")))
                    {
                        // Capture the current sizes to calculate the compensation, we
                        // can do this because the compensation is not expected to change
                        // with the change in the window size:
                        const auto pslSize = playerSpecializationLoader->size().toSize(); // QML items have logical size too
                        const auto windowSize = m_window->size();

                        const auto dW = (windowSize.width() - pslSize.width());
                        const auto dH = (windowSize.height() - pslSize.height());
                        assert(dW >= 0 && dH >= 0); // psl can not be bigger than the window

                        msg_Dbg( m_intf, "Autoresize interface window size compensation: %ix%i", dW, dH );

                        w += dW;
                        h += dH;
                    }
                }
            }

            m_window->resize(std::clamp<int>(w, m_window->minimumWidth(), m_window->maximumWidth()),
                             std::clamp<int>(h, m_window->minimumHeight(), m_window->maximumHeight()));
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
            if( !m_hasWayland )
                m_window->setScreen(screenList[numscreen]);

            /* To be sure window is on proper-screen in xinerama */
            if( !screenres.contains( m_window->position() ) )
            {
                m_window->setPosition(screenres.x(), screenres.y() );
            }
        }
        WindowStateHolder::holdFullscreen(m_window,  WindowStateHolder::VIDEO, true);
    }
    else
    {
        WindowStateHolder::holdFullscreen(m_window,  WindowStateHolder::VIDEO, false);

        if( m_lastWinScreen != NULL && !m_hasWayland )
            m_window->setScreen(m_lastWinScreen);
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
