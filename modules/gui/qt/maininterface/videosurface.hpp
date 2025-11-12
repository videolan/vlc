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
#ifndef VIDEOSURFACE_HPP
#define VIDEOSURFACE_HPP

#include "widgets/native/viewblockingrectangle.hpp"
#include <QMutex>
#include <QRunnable>
#include <QPointer>
#include "qt.hpp"
#include "util/vlchotkeyconverter.hpp"

#include <vlc_threads.h>

extern "C" {
    typedef struct vlc_window vlc_window_t;
}

Q_MOC_INCLUDE( "maininterface/mainctx.hpp")

class MainCtx;
class VideoSurface;

class VideoSurfaceProvider : public QObject
{
    Q_OBJECT

    QPointer<VideoSurface> m_videoSurface;

public:
    VideoSurfaceProvider(bool threadedSurfaceUpdates = false, QObject* parent = nullptr);
    virtual ~VideoSurfaceProvider() {}

    void enable(vlc_window_t* voutWindow);
    void disable();
    bool isEnabled();

    void setVideoEmbed(bool embed);
    bool hasVideoEmbed() const;

    QPointer<VideoSurface> videoSurface() { return m_videoSurface; }
    void setVideoSurface(QPointer<VideoSurface> videoSurface) { m_videoSurface = videoSurface; }

    bool supportsThreadedSurfaceUpdates() const { return m_threadedSurfaceUpdates; };

signals:
    void ctxChanged(MainCtx*);
    bool videoEnabledChanged(bool);
    bool hasVideoEmbedChanged(bool);
    void surfacePropertiesChanged(const std::optional<QSizeF>& size,
                                  const std::optional<QPointF>& position,
                                  const std::optional<qreal>& scale);

public slots:
    void onWindowClosed();
    void onMousePressed( int vlcButton );
    void onMouseReleased( int vlcButton );
    void onMouseDoubleClick( int vlcButton );
    void onMouseMoved( float x, float y );
    void onMouseWheeled(int vlcButton);
    void onKeyPressed(int key, Qt::KeyboardModifiers modifiers);
    void onSurfacePropertiesChanged(const std::optional<QSizeF>& size,
                                    const std::optional<QPointF>& position,
                                    const std::optional<qreal>& scale);

protected:
    vlc_window_t* m_voutWindow = nullptr;
    bool m_videoEmbed = false;
    bool m_threadedSurfaceUpdates = false;
};


class VideoSurface : public ViewBlockingRectangle
{
    Q_OBJECT
    Q_PROPERTY(VideoSurfaceProvider* videoSurfaceProvider READ videoSurfaceProvider WRITE setVideoSurfaceProvider NOTIFY videoSurfaceProviderChanged FINAL)
    Q_PROPERTY(Qt::CursorShape cursorShape READ getCursorShape WRITE setCursorShape RESET unsetCursor FINAL)

public:
    VideoSurface( QQuickItem* parent = nullptr );

    VideoSurfaceProvider* videoSurfaceProvider() const { return m_provider; };
    void setVideoSurfaceProvider(VideoSurfaceProvider *newVideoSurfaceProvider);

protected:
    QSGNode *updatePaintNode(QSGNode *, UpdatePaintNodeData *) override;

    int qtMouseButton2VLC( Qt::MouseButton qtButton );

    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void hoverMoveEvent(QHoverEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
#if QT_CONFIG(wheelevent)
    void wheelEvent(QWheelEvent *event) override;
#endif

    Qt::CursorShape getCursorShape() const;
    void setCursorShape(Qt::CursorShape);

    void itemChange(QQuickItem::ItemChange change, const QQuickItem::ItemChangeData &value) override;
signals:
    void surfacePropertiesChanged(const std::optional<QSizeF>& size,
                                  const std::optional<QPointF>& position,
                                  const std::optional<qreal>& scale);

    void mousePressed( int vlcButton );
    void mouseReleased( int vlcButton );
    void mouseDblClicked( int vlcButton );
    void mouseMoved( float x, float y );
    void keyPressed(int key, Qt::KeyboardModifiers modifier);

    void videoSurfaceProviderChanged();

protected slots:
    void synchronize();

private:
    QPointF m_oldHoverPos;

    WheelToVLCConverter m_wheelEventConverter;

    QPointer<VideoSurfaceProvider> m_provider;

    QMetaObject::Connection m_synchConnection;

    // These are updated and read from different threads, but during synchronization stage so explicit synchronization
    // such as atomic boolean or locking is not necessary:
    bool m_dprChanged = false; // itemChange() <-> updatePaintNode() (different threads, but GUI thread is blocked)
    bool m_videoEnabledChanged = false; // we need to enforce a full fledged synchronization in this case

    // These are updated and read from either the item/GUI thread or the render thread:
    QSizeF m_oldRenderSize;
    QPointF m_oldRenderPosition {-1., -1.};

    // m_dpr, m_dprDirty and m_allDirty are updated in render thread when the GUI thread is blocked (updatePaintNode()).
    // m_dprDirty and m_allDirty may be updated in GUI thread when threaded updates is not possible. Since they can
    // not be updated both in render thread and GUI thread concurrently (as GUI thread is blocked during
    // updatePaintNode() call), data synchronization should not be necessary:
    qreal m_dpr = 1.0;
    bool m_dprDirty = false; // causes synchronizing dpr-related properties
    bool m_allDirty = false; // causes synchronizing everything
};

#endif // VIDEOSURFACE_HPP
