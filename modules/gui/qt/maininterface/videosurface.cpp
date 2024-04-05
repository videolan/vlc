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
#include "maininterface/mainctx.hpp"
#include "widgets/native/customwidgets.hpp" //for qtEventToVLCKey
#include <QSGRectangleNode>
#include <QThreadPool>
#include <vlc_window.h>

#include <QQuickRenderControl>
#ifdef QT_DECLARATIVE_PRIVATE
#  include <QtGui/qpa/qplatformwindow.h>
#endif

WindowResizer::WindowResizer(vlc_window_t* window):
    m_requestedWidth(0),
    m_requestedHeight(0),
    m_currentWidth(0),
    m_currentHeight(0),
    m_running(false),
    m_voutWindow(window)
{
    vlc_mutex_init(&m_lock);
    vlc_cond_init(&m_cond);
    setAutoDelete(false);
}

WindowResizer::~WindowResizer()
{
}

void WindowResizer::run()
{
    vlc_mutex_lock(&m_lock);
    while (m_requestedWidth != m_currentWidth ||
           m_requestedHeight != m_currentHeight)
    {
        unsigned width = m_requestedWidth;
        unsigned height = m_requestedHeight;
        vlc_mutex_unlock(&m_lock);

        vlc_window_ReportSize(m_voutWindow, width, height);

        vlc_mutex_lock(&m_lock);
        m_currentWidth = width;
        m_currentHeight = height;
    }
    m_running = false;
    vlc_cond_signal(&m_cond);
    vlc_mutex_unlock(&m_lock);
}

void WindowResizer::reportSize(float width, float height)
{
    if (width < 0 || height < 0)
        return;

    vlc_mutex_locker locker(&m_lock);
    m_requestedWidth = static_cast<unsigned>(width);
    m_requestedHeight = static_cast<unsigned>(height);
    if (m_running == false)
    {
        m_running = true;
        QThreadPool::globalInstance()->start(this);
    }
}

/* Must called under m_voutlock before deletion */
void WindowResizer::waitForCompletion()
{
    vlc_mutex_locker locker(&m_lock);
    while (m_running)
        vlc_cond_wait(&m_cond, &m_lock);
}

VideoSurfaceProvider::VideoSurfaceProvider(QObject* parent)
    : QObject(parent)
{
}

bool VideoSurfaceProvider::isEnabled()
{
    QMutexLocker lock(&m_voutlock);
    return m_voutWindow != nullptr;
}

bool VideoSurfaceProvider::hasVideoEmbed() const
{
    return m_videoEmbed;
}

void VideoSurfaceProvider::enable(vlc_window_t* voutWindow)
{
    assert(voutWindow);
    {
        QMutexLocker lock(&m_voutlock);
        m_voutWindow = voutWindow;
        m_resizer = new (std::nothrow) WindowResizer(voutWindow);
    }
    emit videoEnabledChanged(true);
}

void VideoSurfaceProvider::disable()
{
    setVideoEmbed(false);
    {
        QMutexLocker lock(&m_voutlock);
        if (m_resizer != nullptr)
        {
            m_resizer->waitForCompletion();
            delete m_resizer;
            m_resizer = nullptr;
        }
        m_voutWindow = nullptr;
    }
    emit videoEnabledChanged(false);
}

void VideoSurfaceProvider::setVideoEmbed(bool embed)
{
    m_videoEmbed = embed;
    emit hasVideoEmbedChanged(embed);
}

void VideoSurfaceProvider::onWindowClosed()
{
    QMutexLocker lock(&m_voutlock);
    if (m_resizer != nullptr)
        m_resizer->waitForCompletion();
    if (m_voutWindow)
        vlc_window_ReportClose(m_voutWindow);
}

void VideoSurfaceProvider::onMousePressed(int vlcButton)
{
    QMutexLocker lock(&m_voutlock);
    if (m_voutWindow)
        vlc_window_ReportMousePressed(m_voutWindow, vlcButton);
}

void VideoSurfaceProvider::onMouseReleased(int vlcButton)
{
    QMutexLocker lock(&m_voutlock);
    if (m_voutWindow)
        vlc_window_ReportMouseReleased(m_voutWindow, vlcButton);
}

void VideoSurfaceProvider::onMouseDoubleClick(int vlcButton)
{
    QMutexLocker lock(&m_voutlock);
    if (m_voutWindow)
        vlc_window_ReportMouseDoubleClick(m_voutWindow, vlcButton);
}

void VideoSurfaceProvider::onMouseMoved(float x, float y)
{
    QMutexLocker lock(&m_voutlock);
    if (m_voutWindow)
        vlc_window_ReportMouseMoved(m_voutWindow, x, y);
}

void VideoSurfaceProvider::onMouseWheeled(const QWheelEvent& event)
{
    int vlckey = qtWheelEventToVLCKey(event);
    QMutexLocker lock(&m_voutlock);
    if (m_voutWindow)
        vlc_window_ReportKeyPress(m_voutWindow, vlckey);
}

void VideoSurfaceProvider::onKeyPressed(int key, Qt::KeyboardModifiers modifiers)
{
    QKeyEvent event(QEvent::KeyPress, key, modifiers);
    int vlckey = qtEventToVLCKey(&event);
    QMutexLocker lock(&m_voutlock);
    if (m_voutWindow)
        vlc_window_ReportKeyPress(m_voutWindow, vlckey);

}

void VideoSurfaceProvider::onSurfaceSizeChanged(QSizeF size)
{
    emit surfaceSizeChanged(size);
    QMutexLocker lock(&m_voutlock);
    if (m_resizer)
        m_resizer->reportSize(size.width(), size.height());
    else if (m_voutWindow)
        vlc_window_ReportSize(m_voutWindow, size.width(), size.height());
}


VideoSurface::VideoSurface(QQuickItem* parent)
    : ViewBlockingRectangle(parent)
{
    setAcceptHoverEvents(true);
    setAcceptedMouseButtons(Qt::AllButtons);
    setFlag(ItemAcceptsInputMethod, true);
    setFlag(ItemHasContents, true);

    connect(this, &QQuickItem::xChanged, this, &VideoSurface::onSurfacePositionChanged);
    connect(this, &QQuickItem::yChanged, this, &VideoSurface::onSurfacePositionChanged);
    connect(this, &QQuickItem::widthChanged, this, &VideoSurface::onSurfaceSizeChanged);
    connect(this, &QQuickItem::heightChanged, this, &VideoSurface::onSurfaceSizeChanged);
    connect(this, &VideoSurface::enabledChanged, this, &VideoSurface::updatePositionAndSize);
}

MainCtx* VideoSurface::getCtx()
{
    return m_ctx;
}

void VideoSurface::setCtx(MainCtx* ctx)
{
    m_ctx = ctx;
    emit ctxChanged(ctx);
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
    QQuickWindow* window = this->window();
    if (!window)
        return;
    qreal dpr = window->effectiveDevicePixelRatio();
    emit mouseMoved(current_pos.x() * dpr, current_pos.y() * dpr);
    event->accept();
}

void VideoSurface::hoverMoveEvent(QHoverEvent* event)
{
    QPointF current_pos = event->posF();
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

void VideoSurface::geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry)
{
    QQuickItem::geometryChange(newGeometry, oldGeometry);
    onSurfaceSizeChanged();
}

#if QT_CONFIG(wheelevent)
void VideoSurface::wheelEvent(QWheelEvent *event)
{
    emit mouseWheeled(*event);
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

QSGNode*VideoSurface::updatePaintNode(QSGNode* oldNode, QQuickItem::UpdatePaintNodeData* data)
{
    const auto node = ViewBlockingRectangle::updatePaintNode(oldNode, data);

    if (m_provider == nullptr)
    {
        if (m_ctx == nullptr)
            return node;
        m_provider =  m_ctx->getVideoSurfaceProvider();
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
        connect(this, &VideoSurface::surfacePositionChanged, m_provider, &VideoSurfaceProvider::surfacePositionChanged);

        connect(m_provider, &VideoSurfaceProvider::hasVideoEmbedChanged, this, &VideoSurface::onProviderVideoChanged);

    }
    updatePositionAndSize();
    return node;
}

void VideoSurface::onProviderVideoChanged(bool hasVideo)
{
    if (!hasVideo)
        return;
    updatePositionAndSize();
}

static qreal dprForWindow(QQuickWindow* quickWindow)
{
    if (!quickWindow)
        return 1.0;

    QWindow* window = QQuickRenderControl::renderWindowFor(quickWindow);
    if (!window)
        window = quickWindow;

    return window->devicePixelRatio();
}

void VideoSurface::onSurfaceSizeChanged()
{
    if (!isEnabled())
        return;

    qreal dpr = dprForWindow(window());

    emit surfaceSizeChanged(size() * dpr);
}

void VideoSurface::onSurfacePositionChanged()
{
    if (!isEnabled())
        return;

    qreal dpr = dprForWindow(window());

    QPointF scenePosition = this->mapToScene(QPointF(0,0));

    emit surfacePositionChanged(scenePosition * dpr);
}

void VideoSurface::updatePositionAndSize()
{
    if (!isEnabled())
        return;

    qreal dpr = dprForWindow(window());

    emit surfaceSizeChanged(size() * dpr);
    QPointF scenePosition = this->mapToScene(QPointF(0, 0));
    emit surfacePositionChanged(scenePosition * dpr);
}
