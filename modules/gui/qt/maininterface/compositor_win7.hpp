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

namespace vlc {

class Win7NativeEventFilter : public QObject, public QAbstractNativeEventFilter {
    Q_OBJECT
public:
    Win7NativeEventFilter( QObject* parent = nullptr );

    bool nativeEventFilter(const QByteArray &, void *message, long* /* result */);
signals:
    void windowStyleChanged();
};

class CompositorWin7 : public CompositorDummy
{
    Q_OBJECT
public:
    CompositorWin7(intf_thread_t *p_intf, QObject* parent = nullptr);

    virtual ~CompositorWin7();

    bool init();

    virtual MainInterface *makeMainInterface() override;
    virtual bool setupVoutWindow(vout_window_t*) override;

protected:
    bool eventFilter(QObject *obj, QEvent *ev) override;

private:
    static int window_enable(struct vout_window_t *, const vout_window_cfg_t *);
    static void window_disable(struct vout_window_t *);
    static void window_resize(struct vout_window_t *, unsigned width, unsigned height);
    static void window_destroy(struct vout_window_t *);
    static void window_set_state(struct vout_window_t *, unsigned state);
    static void window_unset_fullscreen(struct vout_window_t *);
    static void window_set_fullscreen(struct vout_window_t *, const char *id);

private slots:
    void resetVideoZOrder();

private:
    QWidget* m_stable = nullptr;
    std::unique_ptr<QQuickView> m_qmlView;
    std::unique_ptr<VideoWindowHandler> m_videoWindowHandler;
    std::unique_ptr<VideoSurfaceProvider> m_qmlVideoSurfaceProvider;
    WinTaskbarWidget* m_taskbarWidget = nullptr;
    Win7NativeEventFilter* m_nativeEventFilter = nullptr;

    HWND m_qmlWindowHWND = nullptr;
    HWND m_videoWindowHWND = nullptr;

};

}


#endif // COMPOSITORDUMMYWIN32_H
