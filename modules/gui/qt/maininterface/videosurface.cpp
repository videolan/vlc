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

void VideoSurfaceProvider::onMouseWheeled(int vlcButton)
{
    QMutexLocker lock(&m_voutlock);
    if (m_voutWindow)
        vlc_window_ReportKeyPress(m_voutWindow, vlcButton);
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
        m_resizer->reportSize(std::ceil(size.width()), std::ceil(size.height()));
    else if (m_voutWindow)
        vlc_window_ReportSize(m_voutWindow, std::ceil(size.width()), std::ceil(size.height()));
}


VideoSurface::VideoSurface(QQuickItem* parent)
    : ViewBlockingRectangle(parent)
{
    setAcceptHoverEvents(true);
    setAcceptedMouseButtons(Qt::AllButtons);
    setFlag(ItemAcceptsInputMethod, true);

    {
        connect(this, &QQuickItem::widthChanged, this, &VideoSurface::updateSurfaceSize);
        connect(this, &QQuickItem::heightChanged, this, &VideoSurface::updateSurfaceSize);

        connect(this, &QQuickItem::xChanged, this, &VideoSurface::updateSurfacePosition);
        connect(this, &QQuickItem::yChanged, this, &VideoSurface::updateSurfacePosition);
        connect(this, &QQuickItem::parentChanged, this, &VideoSurface::updateParentChanged);
        updateParentChanged();
    }
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

void VideoSurface::updatePolish()
{
    QQuickItem::updatePolish();

    assert(window());

    if (m_sizeDirty && !size().isEmpty())
    {
        emit surfaceSizeChanged(size() * window()->effectiveDevicePixelRatio());
        m_sizeDirty = false;
    }

    if (m_positionDirty)
    {
        QPointF scenePosition = this->mapToScene(QPointF(0,0));

        emit surfacePositionChanged(scenePosition * window()->effectiveDevicePixelRatio());
        m_positionDirty = false;
    }

    if (m_scaleDirty)
    {
        emit surfaceScaleChanged(window()->effectiveDevicePixelRatio());
        m_scaleDirty = false;
    }
}

void VideoSurface::itemChange(ItemChange change, const ItemChangeData &value)
{
    if (change == ItemDevicePixelRatioHasChanged || change == ItemSceneChange)
    {
        updateSurfaceScale();
    }

    QQuickItem::itemChange(change, value);
}

void VideoSurface::updateSurfacePosition()
{
    m_positionDirty = true;
    polish();
}

void VideoSurface::updateSurfaceSize()
{
    m_sizeDirty = true;
    polish();
}

void VideoSurface::updateSurfaceScale()
{
    m_scaleDirty = true;
    polish();
}

void VideoSurface::updateSurface()
{
    updateSurfacePosition();
    updateSurfaceSize();
    updateSurfaceScale();
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
        connect(this, &VideoSurface::surfaceSizeChanged, m_provider, &VideoSurfaceProvider::onSurfaceSizeChanged);
        connect(this, &VideoSurface::surfacePositionChanged, m_provider, &VideoSurfaceProvider::surfacePositionChanged);
        connect(this, &VideoSurface::surfaceScaleChanged, m_provider, &VideoSurfaceProvider::surfaceScaleChanged);

        connect(&m_wheelEventConverter, &WheelToVLCConverter::vlcWheelKey, m_provider, &VideoSurfaceProvider::onMouseWheeled);
        connect(m_provider, &VideoSurfaceProvider::videoEnabledChanged, this, &VideoSurface::updateSurface);

        setFlag(ItemHasContents, true);
        updateSurface(); // Polish is queued anyway, updatePolish() should be called when the initial size is set.
    }
    else
    {
        setFlag(ItemHasContents, false);
    }

    emit videoSurfaceProviderChanged();
}

void VideoSurface::updateParentChanged()
{
    //we need to track the global position of the VideoSurface within the scene
    //it depends on the position of the VideoSurface itself and all its parents

    for (const QPointer<QQuickItem>& p : m_parentList)
    {
        if (!p)
            continue;
        disconnect(p, &QQuickItem::xChanged, this, &VideoSurface::updateSurfacePosition);
        disconnect(p, &QQuickItem::yChanged, this, &VideoSurface::updateSurfacePosition);
        disconnect(p, &QQuickItem::parentChanged, this, &VideoSurface::updateParentChanged);
    }
    m_parentList.clear();

    for (QQuickItem* p = parentItem(); p != nullptr; p = p->parentItem())
    {
        connect(p, &QQuickItem::xChanged, this, &VideoSurface::updateSurfacePosition);
        connect(p, &QQuickItem::yChanged, this, &VideoSurface::updateSurfacePosition);
        connect(p, &QQuickItem::parentChanged, this, &VideoSurface::updateParentChanged);
        m_parentList.push_back(p);
    }
}
