/*****************************************************************************
 * Copyright (C) 2021 VLC authors and VideoLAN
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
#ifndef COMPOSITOR_X11_UISURFACE_HPP
#define COMPOSITOR_X11_UISURFACE_HPP

#include <QWindow>
#include <QQuickWindow>
#include <QQuickRenderControl>
#include "compositor.hpp"

class QQuickWindow;
class QQmlEngine;
class QQmlComponent;
class QQuickItem;

namespace vlc {

class CompositorX11RenderControl : public QQuickRenderControl {
    Q_OBJECT
public:
    CompositorX11RenderControl(QWindow* window, QObject* parent = nullptr)
        : QQuickRenderControl(parent)
        , m_window(window)
    {}

    QWindow *renderWindow(QPoint * offset) override;

private:
    QWindow* m_window = nullptr;
};

class CompositorX11UISurface : public QWindow , public CompositorVideo::QmlUISurface
{
    Q_OBJECT
public:
    explicit CompositorX11UISurface(QWindow* window, QScreen *screen = nullptr);
    ~CompositorX11UISurface();

    virtual void render();

    bool handleWindowEvent(QEvent *event);

    //QmlUISurface API
    void setContent(QQmlComponent*,  QQuickItem* rootItem) override;
    QQmlEngine* engine() const override { return m_qmlEngine; }

    QQuickItem * activeFocusItem() const override;

    QQuickWindow* getOffscreenWindow() const;

signals:
    void beforeRendering();
    void afterRendering();
    void sizeChanged(const QSize& size);
    void updated();

protected:
    bool eventFilter(QObject* object, QEvent *event) override;

    bool event(QEvent *event) override;

    void resizeEvent(QResizeEvent *) override;
    void exposeEvent(QExposeEvent *) override;
    void handleScreenChange();


    void updateSizes();

    void createFbo();
    void destroyFbo();
    void resizeFbo();

private:
    QQuickItem* m_rootItem = nullptr;
    QOpenGLContext *m_context = nullptr;
    QQuickWindow* m_uiWindow = nullptr;
    QQmlEngine* m_qmlEngine = nullptr;
    QWindow* m_renderWindow = nullptr;
    CompositorX11RenderControl* m_uiRenderControl = nullptr;

    QSize m_onscreenSize;
};

}

#endif // COMPOSITOR_X11_UISURFACE_HPP
