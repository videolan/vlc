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
#ifndef COMPOSITORDUMMYWIN32_H
#define COMPOSITORDUMMYWIN32_H

#include "compositor_dummy.hpp"
#include "videosurface.hpp"
#include "video_window_handler.hpp"
#include <QAbstractNativeEventFilter>
#include <memory>

class WinTaskbarWidget;
class InterfaceWindowHandlerWin32;

namespace vlc {

class Win7NativeEventFilter : public QObject, public QAbstractNativeEventFilter {
    Q_OBJECT
public:
    Win7NativeEventFilter( QObject* parent = nullptr );

    bool nativeEventFilter(const QByteArray &, void *message, qintptr* /* result */);
signals:
    void windowStyleChanged();
};

class CompositorWin7 : public CompositorVideo
{
    Q_OBJECT
public:
    CompositorWin7(qt_intf_t *p_intf, QObject* parent = nullptr);

    virtual ~CompositorWin7();

    static bool preInit(qt_intf_t *p_intf);
    bool init() override;

    bool makeMainInterface(MainCtx*) override;
    void destroyMainInterface() override;
    void unloadGUI() override;
    bool setupVoutWindow(vlc_window_t*, VoutDestroyCb destroyCb) override;
    QWindow* interfaceMainWindow() const override;

    Type type() const override;

    QQuickItem * activeFocusItem() const override;

protected:
    bool eventFilter(QObject *obj, QEvent *ev) override;

private:
    int windowEnable(const vlc_window_cfg_t *) override;
    void windowDisable() override;

private slots:
    void resetVideoZOrder();
    void onSurfacePositionChanged(const QPointF& position) override;
    void onSurfaceSizeChanged(const QSizeF& size) override;

private:
    QWidget* m_videoWidget = nullptr;
    QWidget* m_stable = nullptr;
    std::unique_ptr<QQuickView> m_qmlView;
    std::unique_ptr<Win7NativeEventFilter> m_nativeEventFilter;

    HWND m_qmlWindowHWND = nullptr;
    HWND m_videoWindowHWND = nullptr;

};

}


#endif // COMPOSITORDUMMYWIN32_H
