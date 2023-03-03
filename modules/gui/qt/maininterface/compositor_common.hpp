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

#ifndef COMPOSITOR_COMMON_HPP
#define COMPOSITOR_COMMON_HPP

#include <QWindow>
#include <QQuickWindow>

namespace vlc {

/**
 * a minimal window with no content
 * this may be useful on linux platform to provide a
 * window which can be drawn into, using a bare QWindow
 * usually freeze on resize
 */
class DummyRenderWindow : public QWindow
{
    Q_OBJECT
public:
    explicit DummyRenderWindow(QWindow* parent = nullptr);

    virtual QAccessibleInterface *accessibleRoot() const override;

protected:
    bool event(QEvent *event) override;

    void resizeEvent(QResizeEvent *resizeEvent) override;

private:
    void init();
    void render();

    QBackingStore* m_backingStore = nullptr;
    bool m_initialized = false;;
};


/**
 * @brief The CompositorOffscreenWindow class allows to fake the visiblilty
 * of the the QQuickWindow, note that this feature will only work if QT5_DECLARATIVE_PRIVATE
 * are available
 */
class CompositorOffscreenWindow : public QQuickWindow
{
    Q_OBJECT
public:
    explicit CompositorOffscreenWindow(QQuickRenderControl* renderControl);

    void setWindowStateExt(Qt::WindowState);

    void setPseudoVisible(bool visible);
};


}

#endif /* COMPOSITOR_COMMON_HPP */
