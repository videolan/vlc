/*****************************************************************************
 * Copyright (C) 2023 VLC authors and VideoLAN
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


#include "compositor_common.hpp"

#include <QBackingStore>
#ifndef QT_NO_ACCESSIBILITY
#include <QAccessible>
#endif

#ifdef QT5_DECLARATIVE_PRIVATE
#include <private/qquickwindow_p.h>
#endif

using namespace vlc;

DummyRenderWindow::DummyRenderWindow(QWindow* parent)
    : QWindow(parent)
{
    setSurfaceType(RasterSurface);
    QSurfaceFormat fmt = format();
    fmt.setAlphaBufferSize(8);
    setFormat(fmt);
}

QAccessibleInterface* DummyRenderWindow::accessibleRoot() const
{
#ifndef QT_NO_ACCESSIBILITY
    QAccessibleInterface* iface = QAccessible::queryAccessibleInterface(
                const_cast<DummyRenderWindow*>(this));
    return iface;
#else
    return nullptr;
#endif
}

bool DummyRenderWindow::event(QEvent* event)
{
    switch (event->type() )
    {
    case QEvent::UpdateRequest:
        render();
        return true;
    case QEvent::Expose:
        if (isExposed())
            requestUpdate();
        return true;
    default:
        break;
    }

    return QWindow::event(event);
}

void DummyRenderWindow::resizeEvent(QResizeEvent* resizeEvent)
{
    if (!m_backingStore)
        return;
    if (!m_initialized)
        init();
    m_backingStore->resize(resizeEvent->size());
}

void DummyRenderWindow::init()
{
    if (m_initialized)
        return;
    m_initialized = true;
    m_backingStore = new QBackingStore(this);
}

void DummyRenderWindow::render()
{
    if (!m_initialized)
        init();
    if (!isExposed())
        return;
    QRect rect(0, 0, width(), height());
    if (m_backingStore->size() != size())
    {
        m_backingStore->resize(size());
    }

    //note that we don't need to actually use our backing store
    //drawing anything would just lead to flickering
    m_backingStore->flush(QRect(0, 0, 1, 1));
    return;
}

#ifdef QT5_DECLARATIVE_PRIVATE

class OffscreenWindowPrivate: public QQuickWindowPrivate
{
public:
    void setVisible(bool newVisible) override {
        Q_Q(QWindow);
        if (visible != newVisible)
        {
            visible = newVisible;
            q->visibleChanged(newVisible);
            // this stays always invisible
            visibility = newVisible ? QWindow::Windowed : QWindow::Hidden;
            q->visibilityChanged(visibility); // workaround for QTBUG-49054
        }
    }
};

CompositorOffscreenWindow::CompositorOffscreenWindow(QQuickRenderControl* renderControl)
    : QQuickWindow(* (new OffscreenWindowPrivate()), renderControl)
{
}

static Qt::WindowState resolveWindowState(Qt::WindowStates states)
{
    // No more than one of these 3 can be set
    if (states & Qt::WindowMinimized)
        return Qt::WindowMinimized;
    if (states & Qt::WindowMaximized)
        return Qt::WindowMaximized;
    if (states & Qt::WindowFullScreen)
        return Qt::WindowFullScreen;
    // No state means "windowed" - we ignore Qt::WindowActive
    return Qt::WindowNoState;
}

void CompositorOffscreenWindow::setWindowStateExt(Qt::WindowState state)
{
    QWindow::setWindowState(resolveWindowState(state));
}

void CompositorOffscreenWindow::setPseudoVisible(bool visible)
{
    setVisible(visible);
}

#else

CompositorOffscreenWindow::CompositorOffscreenWindow(QQuickRenderControl* renderControl)
: QQuickWindow(renderControl)
{
}

void CompositorOffscreenWindow::setWindowStateExt(Qt::WindowState state)
{
    QWindow::setWindowState(state);
}

//don't set the window visible, this would create the window, and show make it actually visible
void CompositorOffscreenWindow::setPseudoVisible(bool)
{
}

#endif



