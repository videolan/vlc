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
#include "maininterface/main_interface.hpp"
#include "widgets/native/customwidgets.hpp" //for qtEventToVLCKey
#include <QSGRectangleNode>

VideoSurfaceProvider::VideoSurfaceProvider(QObject* parent)
    : QObject(parent)
{
}

bool VideoSurfaceProvider::hasVideo()
{
    QMutexLocker lock(&m_voutlock);
    return m_voutWindow != nullptr;
}

void VideoSurfaceProvider::enable(vout_window_t* voutWindow)
{
    assert(voutWindow);
    {
        QMutexLocker lock(&m_voutlock);
        m_voutWindow = voutWindow;
    }
    emit hasVideoChanged(true);
}

void VideoSurfaceProvider::disable()
{
    {
        QMutexLocker lock(&m_voutlock);
        m_voutWindow = nullptr;
    }
    emit hasVideoChanged(false);
}

void VideoSurfaceProvider::onWindowClosed()
{
    QMutexLocker lock(&m_voutlock);
    if (m_voutWindow)
        vout_window_ReportClose(m_voutWindow);
}

void VideoSurfaceProvider::onMousePressed(int vlcButton)
{
    QMutexLocker lock(&m_voutlock);
    if (m_voutWindow)
        vout_window_ReportMousePressed(m_voutWindow, vlcButton);
}

void VideoSurfaceProvider::onMouseReleased(int vlcButton)
{
    QMutexLocker lock(&m_voutlock);
    if (m_voutWindow)
        vout_window_ReportMouseReleased(m_voutWindow, vlcButton);
}

void VideoSurfaceProvider::onMouseDoubleClick(int vlcButton)
{
    QMutexLocker lock(&m_voutlock);
    if (m_voutWindow)
        vout_window_ReportMouseDoubleClick(m_voutWindow, vlcButton);
}

void VideoSurfaceProvider::onMouseMoved(float x, float y)
{
    QMutexLocker lock(&m_voutlock);
    if (m_voutWindow)
        vout_window_ReportMouseMoved(m_voutWindow, x, y);
}

void VideoSurfaceProvider::onMouseWheeled(const QPointF& pos, int delta, Qt::MouseButtons buttons,  Qt::KeyboardModifiers modifiers, Qt::Orientation orient)
{
    QWheelEvent event(pos, delta, buttons, modifiers, orient);
    int vlckey = qtWheelEventToVLCKey(&event);
    QMutexLocker lock(&m_voutlock);
    if (m_voutWindow)
        vout_window_ReportKeyPress(m_voutWindow, vlckey);
}

void VideoSurfaceProvider::onKeyPressed(int key, Qt::KeyboardModifiers modifiers)
{
    QKeyEvent event(QEvent::KeyPress, key, modifiers);
    int vlckey = qtEventToVLCKey(&event);
    QMutexLocker lock(&m_voutlock);
    if (m_voutWindow)
        vout_window_ReportKeyPress(m_voutWindow, vlckey);

}

void VideoSurfaceProvider::onSurfaceSizeChanged(QSizeF size)
{
    QMutexLocker lock(&m_voutlock);
    if (m_voutWindow)
        vout_window_ReportSize(m_voutWindow, size.width(), size.height());
}


VideoSurface::VideoSurface(QQuickItem* parent)
    : QQuickItem(parent)
{
    setAcceptHoverEvents(true);
    setAcceptedMouseButtons(Qt::AllButtons);
    setFlag(ItemAcceptsInputMethod, true);
    setFlag(ItemHasContents, true);

    connect(this, &QQuickItem::widthChanged, this, &VideoSurface::onSurfaceSizeChanged);
    connect(this, &QQuickItem::heightChanged, this, &VideoSurface::onSurfaceSizeChanged);
}

QmlMainContext*VideoSurface::getCtx()
{
    return m_mainCtx;
}

void VideoSurface::setCtx(QmlMainContext* mainctx)
{
    m_mainCtx = mainctx;
    emit ctxChanged(mainctx);
}

QSize VideoSurface::getSourceSize() const
{
    return m_sourceSize;
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
    QPointF current_pos = event->localPos();
    emit mouseMoved(current_pos.x() , current_pos.y());
    event->accept();
}

void VideoSurface::hoverMoveEvent(QHoverEvent* event)
{
    QPointF current_pos = event->posF();
    if (current_pos != m_oldHoverPos)
    {
        float scaleW = m_sourceSize.width() / width();
        float scaleH = m_sourceSize.height() / height();
        emit mouseMoved(current_pos.x() * scaleW, current_pos.y() * scaleH);
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

void VideoSurface::geometryChanged(const QRectF& newGeometry, const QRectF& oldGeometry)
{
    QQuickItem::geometryChanged(newGeometry, oldGeometry);
    onSurfaceSizeChanged();
}

#if QT_CONFIG(wheelevent)
void VideoSurface::wheelEvent(QWheelEvent *event)
{
    emit mouseWheeled(event->posF(), event->delta(), event->buttons(), event->modifiers(), event->orientation());
    event->ignore();
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

QSGNode*VideoSurface::updatePaintNode(QSGNode* oldNode, QQuickItem::UpdatePaintNodeData*)
{
    QSGRectangleNode* node = static_cast<QSGRectangleNode*>(oldNode);

    if (!node)
    {
        node = this->window()->createRectangleNode();
        node->setColor(Qt::transparent);
    }
    node->setRect(this->boundingRect());

    if (m_provider == nullptr)
    {
        if (m_mainCtx == nullptr)
            return node;
        m_provider =  m_mainCtx->getMainInterface()->getVideoSurfaceProvider();
        if (!m_provider)
            return node;

        //forward signal to the provider
        connect(this, &VideoSurface::mouseMoved, m_provider, &VideoSurfaceProvider::onMouseMoved);
        connect(this, &VideoSurface::mousePressed, m_provider, &VideoSurfaceProvider::onMousePressed);
        connect(this, &VideoSurface::mouseDblClicked, m_provider, &VideoSurfaceProvider::onMouseDoubleClick);
        connect(this, &VideoSurface::mouseReleased, m_provider, &VideoSurfaceProvider::onMouseReleased);
        connect(this, &VideoSurface::mouseWheeled, m_provider, &VideoSurfaceProvider::onMouseWheeled);
        connect(this, &VideoSurface::keyPressed, m_provider, &VideoSurfaceProvider::onKeyPressed);
        connect(this, &VideoSurface::surfaceSizeChanged, m_provider, &VideoSurfaceProvider::onSurfaceSizeChanged);

        onSurfaceSizeChanged();
    }
    return node;
}

void VideoSurface::onSurfaceSizeChanged()
{
    emit surfaceSizeChanged(size() * this->window()->effectiveDevicePixelRatio());
}
