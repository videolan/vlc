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

#include <QtQuick/QQuickItem>
#include <QCursor>
#include <QMutex>
#include <util/qml_main_context.hpp>
#include "qt.hpp"
#include "vlc_vout_window.h"

class VideoSurfaceProvider : public QObject
{
    Q_OBJECT
public:
    VideoSurfaceProvider(QObject* parent = nullptr);
    virtual ~VideoSurfaceProvider() {}

    bool hasVideo();

    void enable(vout_window_t* voutWindow);
    void disable();


signals:
    void ctxChanged(QmlMainContext*);
    bool hasVideoChanged(bool);

public slots:
    void onWindowClosed();
    void onMousePressed( int vlcButton );
    void onMouseReleased( int vlcButton );
    void onMouseDoubleClick( int vlcButton );
    void onMouseMoved( float x, float y );
    void onMouseWheeled(const QPointF &pos, int delta, Qt::MouseButtons buttons, Qt::KeyboardModifiers modifiers, Qt::Orientation orient);
    void onKeyPressed(int key, Qt::KeyboardModifiers modifiers);
    void onSurfaceSizeChanged(QSizeF size);

protected:
    QMutex m_voutlock;
    vout_window_t* m_voutWindow = nullptr;
};


class VideoSurface : public QQuickItem
{
    Q_OBJECT
    Q_PROPERTY(QmlMainContext* ctx READ getCtx WRITE setCtx NOTIFY ctxChanged)
    Q_PROPERTY(QSize sourceSize READ getSourceSize NOTIFY sourceSizeChanged)
    Q_PROPERTY(Qt::CursorShape cursorShape READ getCursorShape WRITE setCursorShape RESET unsetCursor)

public:
    VideoSurface( QQuickItem* parent = nullptr );

    QmlMainContext* getCtx();
    void setCtx(QmlMainContext* mainctx);

    QSize getSourceSize() const;

protected:
    int qtMouseButton2VLC( Qt::MouseButton qtButton );

    virtual void mousePressEvent(QMouseEvent *event) override;
    virtual void mouseReleaseEvent(QMouseEvent *event) override;
    virtual void mouseMoveEvent(QMouseEvent *event) override;
    virtual void hoverMoveEvent(QHoverEvent *event) override;
    virtual void mouseDoubleClickEvent(QMouseEvent *event) override;
    virtual void keyPressEvent(QKeyEvent *event) override;
#if QT_CONFIG(wheelevent)
    virtual void wheelEvent(QWheelEvent *event) override;
#endif

    virtual void geometryChanged(const QRectF &newGeometry,
                                 const QRectF &oldGeometry) override;

    Qt::CursorShape getCursorShape() const;
    void setCursorShape(Qt::CursorShape);

    virtual QSGNode* updatePaintNode(QSGNode *, QQuickItem::UpdatePaintNodeData *) override;

signals:
    void ctxChanged(QmlMainContext*);
    void sourceSizeChanged(QSize);
    void surfaceSizeChanged(QSizeF);

    void mousePressed( int vlcButton );
    void mouseReleased( int vlcButton );
    void mouseDblClicked( int vlcButton );
    void mouseMoved( float x, float y );
    void keyPressed(int key, Qt::KeyboardModifiers modifier);
    void mouseWheeled(const QPointF& pos, int delta, Qt::MouseButtons buttons,  Qt::KeyboardModifiers modifiers, Qt::Orientation orient);

private:
    void onSurfaceSizeChanged();

    QmlMainContext* m_mainCtx = nullptr;

    bool m_sourceSizeChanged = false;
    QSize m_sourceSize;
    QPointF m_oldHoverPos;

    VideoSurfaceProvider* m_provider = nullptr;
};

#endif // VIDEOSURFACE_HPP
