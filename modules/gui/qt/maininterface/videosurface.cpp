/*****************************************************************************
 * Copyright (C) 2019 VLC authors and VideoLAN
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
#include "videosurface.hpp"

#include <QSGRectangleNode>
#include <QThreadPool>
#include <vlc_window.h>

#include <QQuickRenderControl>
#ifdef QT_DECLARATIVE_PRIVATE
#  include <QtGui/qpa/qplatformwindow.h>
#endif

#include "maininterface/mainctx.hpp"

VideoSurfaceProvider::VideoSurfaceProvider(bool threadedSurfaceUpdates, QObject* parent)
    : QObject(parent)
    , m_threadedSurfaceUpdates(threadedSurfaceUpdates)
{
}

bool VideoSurfaceProvider::isEnabled()
{
    return m_voutWindow != nullptr;
}

bool VideoSurfaceProvider::hasVideoEmbed() const
{
    return m_videoEmbed;
}

void VideoSurfaceProvider::enable(vlc_window_t* voutWindow)
{
    assert(voutWindow);
    m_voutWindow = voutWindow;
    emit videoEnabledChanged(true);
}

void VideoSurfaceProvider::disable()
{
    setVideoEmbed(false);
    m_voutWindow = nullptr;
    emit videoEnabledChanged(false);
}

void VideoSurfaceProvider::setVideoEmbed(bool embed)
{
    m_videoEmbed = embed;
    emit hasVideoEmbedChanged(embed);
}

void VideoSurfaceProvider::onWindowClosed()
{
    if (m_voutWindow)
        vlc_window_ReportClose(m_voutWindow);
}

void VideoSurfaceProvider::onMousePressed(int vlcButton)
{
    if (m_voutWindow)
        vlc_window_ReportMousePressed(m_voutWindow, vlcButton);
}

void VideoSurfaceProvider::onMouseReleased(int vlcButton)
{
    if (m_voutWindow)
        vlc_window_ReportMouseReleased(m_voutWindow, vlcButton);
}

void VideoSurfaceProvider::onMouseDoubleClick(int vlcButton)
{
    if (m_voutWindow)
        vlc_window_ReportMouseDoubleClick(m_voutWindow, vlcButton);
}

void VideoSurfaceProvider::onMouseMoved(float x, float y)
{
    if (m_voutWindow)
        vlc_window_ReportMouseMoved(m_voutWindow, x, y);
}

void VideoSurfaceProvider::onMouseWheeled(int vlcButton)
{
    if (m_voutWindow)
        vlc_window_ReportKeyPress(m_voutWindow, vlcButton);
}

void VideoSurfaceProvider::onKeyPressed(int key, Qt::KeyboardModifiers modifiers)
{
    QKeyEvent event(QEvent::KeyPress, key, modifiers);
    int vlckey = qtEventToVLCKey(&event);
    if (m_voutWindow)
        vlc_window_ReportKeyPress(m_voutWindow, vlckey);
}

void VideoSurfaceProvider::onSurfaceSizeChanged(QSizeF size)
{
    emit surfaceSizeChanged(size);
    if (m_voutWindow)
        vlc_window_ReportSize(m_voutWindow, std::ceil(size.width()), std::ceil(size.height()));
}


VideoSurface::VideoSurface(QQuickItem* parent)
    : ViewBlockingRectangle(parent)
{
    setAcceptHoverEvents(true);
    setAcceptedMouseButtons(Qt::AllButtons);
    setFlag(ItemAcceptsInputMethod, true);
}

int VideoSurface::qtMouseButton2VLC( Qt::MouseButton qtButton )
{
    switch( qtButton )
    {
        case Qt::LeftButton:
            return 0;
        case Qt::RightButton:
            return 2;
        case Qt::MiddleButton:
            return 1;
        default:
            return -1;
    }
}

void VideoSurface::mousePressEvent(QMouseEvent* event)
{
    int vlc_button = qtMouseButton2VLC( event->button() );
    if( vlc_button >= 0 )
    {
        emit mousePressed(vlc_button);
        event->accept();
    }
    else
        event->ignore();
}

void VideoSurface::mouseReleaseEvent(QMouseEvent* event)
{
    int vlc_button = qtMouseButton2VLC( event->button() );
    if( vlc_button >= 0 )
    {
        emit mouseReleased(vlc_button);
        event->accept();
    }
    else
        event->ignore();
}

void VideoSurface::mouseMoveEvent(QMouseEvent* event)
{
    QPointF current_pos = event->position();
    QQuickWindow* window = this->window();
    if (!window)
        return;
    qreal dpr = window->effectiveDevicePixelRatio();
    emit mouseMoved(current_pos.x() * dpr, current_pos.y() * dpr);
    event->accept();
}

void VideoSurface::hoverMoveEvent(QHoverEvent* event)
{
    QPointF current_pos = event->position();
    if (current_pos != m_oldHoverPos)
    {
        QQuickWindow* window = this->window();
        if (!window)
            return;
        qreal dpr = window->effectiveDevicePixelRatio();
        emit mouseMoved(current_pos.x() * dpr, current_pos.y()  * dpr);
        m_oldHoverPos = current_pos;
    }
    event->accept();
}

void VideoSurface::mouseDoubleClickEvent(QMouseEvent* event)
{
    int vlc_button = qtMouseButton2VLC( event->button() );
    if( vlc_button >= 0 )
    {
        emit mouseDblClicked(vlc_button);
        event->accept();
    }
    else
        event->ignore();
}

void VideoSurface::keyPressEvent(QKeyEvent* event)
{
    emit keyPressed(event->key(), event->modifiers());
    event->ignore();
}

#if QT_CONFIG(wheelevent)
void VideoSurface::wheelEvent(QWheelEvent *event)
{
    m_wheelEventConverter.wheelEvent(event);
    event->accept();
}
#endif

Qt::CursorShape VideoSurface::getCursorShape() const
{
    return cursor().shape();
}

void VideoSurface::setCursorShape(Qt::CursorShape shape)
{
    setCursor(shape);
}

void VideoSurface::synchronize()
{
    // This may be called from the rendering thread, not necessarily
    // during synchronization (GUI thread is not blocked). Try to
    // be very careful.

    QSizeF size;
    QPointF position;

    static const bool isObjectThread = QThread::currentThread() == thread();
    if (isObjectThread)
    {
        assert(QThread::currentThread() == thread());
        // Item's thread (GUI thread):
        size = this->size();
        position = this->mapToScene(QPointF(0,0));
    }
    else
    {
        assert(QThread::currentThread() != thread());
        // Render thread:
        size = renderSize();
        position = renderPosition();
    }

    if (m_allDirty || m_dprDirty || m_oldRenderSize != size)
    {
        if (!size.isEmpty())
        {
            emit surfaceSizeChanged(size * m_dpr);
            m_oldRenderSize = size;
        }
    }

    if (m_allDirty || m_dprDirty || m_oldRenderPosition != position)
    {
        if (position.x() >= 0.0 && position.y() >= 0.0)
        {
            emit surfacePositionChanged(position * m_dpr); // render position is relative to scene/viewport
            m_oldRenderPosition = position;
        }
    }

    if (m_allDirty || m_dprDirty)
    {
        emit surfaceScaleChanged(m_dpr);
        m_dprDirty = false;
    }

    m_allDirty = false;
}

void VideoSurface::itemChange(ItemChange change, const ItemChangeData &value)
{
    if (change == ItemDevicePixelRatioHasChanged || change == ItemSceneChange)
    {
        m_dprChanged = true;
        // Request update, so that `updatePaintNode()` gets called which updates the DPR for `::synchronize()`:
        update();
    }

    QQuickItem::itemChange(change, value);
}

void VideoSurface::setVideoSurfaceProvider(VideoSurfaceProvider *newVideoSurfaceProvider)
{
    if (m_provider == newVideoSurfaceProvider)
        return;

    if (m_provider)
    {
        disconnect(this, nullptr, m_provider, nullptr);
        disconnect(&m_wheelEventConverter, nullptr, m_provider, nullptr);
        disconnect(m_provider, nullptr, this, nullptr);
    }

    m_provider = newVideoSurfaceProvider;

    if (m_provider)
    {
        connect(this, &VideoSurface::mouseMoved, m_provider, &VideoSurfaceProvider::onMouseMoved);
        connect(this, &VideoSurface::mousePressed, m_provider, &VideoSurfaceProvider::onMousePressed);
        connect(this, &VideoSurface::mouseDblClicked, m_provider, &VideoSurfaceProvider::onMouseDoubleClick);
        connect(this, &VideoSurface::mouseReleased, m_provider, &VideoSurfaceProvider::onMouseReleased);
        connect(this, &VideoSurface::keyPressed, m_provider, &VideoSurfaceProvider::onKeyPressed);
        connect(this, &VideoSurface::surfaceSizeChanged, m_provider, &VideoSurfaceProvider::onSurfaceSizeChanged, Qt::DirectConnection);
        connect(this, &VideoSurface::surfacePositionChanged, m_provider, &VideoSurfaceProvider::surfacePositionChanged, Qt::DirectConnection);
        connect(this, &VideoSurface::surfaceScaleChanged, m_provider, &VideoSurfaceProvider::surfaceScaleChanged, Qt::DirectConnection);

        // With auto connection, this should be queued if the signal was emitted from vout thread,
        // so that the slot is executed in item's thread:
        connect(m_provider, &VideoSurfaceProvider::videoEnabledChanged, this, [this](bool enabled) {
            if (enabled)
            {
                m_videoEnabledChanged = true;
                if (flags().testFlag(ItemHasContents)) // "Only items which specify QQuickItem::ItemHasContents are allowed to call QQuickItem::update()."
                    update();
            }
        });

        connect(&m_wheelEventConverter, &WheelToVLCConverter::vlcWheelKey, m_provider, &VideoSurfaceProvider::onMouseWheeled);

        setFlag(ItemHasContents, true);
    }
    else
    {
        setFlag(ItemHasContents, false);
    }

    emit videoSurfaceProviderChanged();
}

QSGNode *VideoSurface::updatePaintNode(QSGNode *node, UpdatePaintNodeData *data)
{
    // This is called from the render thread, but during synchronization.
    // So the GUI thread is blocked here. This makes it safer to access the window
    // to get the effective DPR, rather than doing it outside the synchronization
    // stage.

    const auto w = window();
    assert (w);

    if (Q_UNLIKELY(!m_provider))
        return node;

    if (w != m_oldWindow)
    {
        if (m_oldWindow)
            disconnect(m_synchConnection);

        m_oldWindow = w;

        if (w)
        {
            // This is constant:
            if (m_provider->supportsThreadedSurfaceUpdates())
            {
                // Synchronize just before swapping the frame for better synchronization:
                m_synchConnection = connect(w, &QQuickWindow::afterRendering, this, &VideoSurface::synchronize, Qt::DirectConnection);
            }
            else
            {
                m_synchConnection = connect(w, &QQuickWindow::afterAnimating, this, &VideoSurface::synchronize);
            }
        }
    }

    if (m_dprChanged)
    {
        m_dpr = w->effectiveDevicePixelRatio();
        m_dprDirty = true;
        m_dprChanged = false;
    }

    if (m_videoEnabledChanged)
    {
        m_allDirty = true;
        m_videoEnabledChanged = false;
    }

    return ViewBlockingRectangle::updatePaintNode(node, data);
}
