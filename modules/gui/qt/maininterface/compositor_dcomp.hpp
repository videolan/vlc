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
#ifndef VLC_COMPOSITOR_DIRECT_COMPOSITION
#define VLC_COMPOSITOR_DIRECT_COMPOSITION

#include "compositor.hpp"

#include "../maininterface/mainui.hpp"
#include "interface_window_handler.hpp"
#include "video_window_handler.hpp"

#include <QPointer>
#include <QWaitCondition>
#include <QMutex>

#include <memory>

#include <wrl.h>

class MainCtx;
class WinTaskbarWidget;
class QQuickView;

class IDCompositionVisual;
class IDCompositionDevice;
class IDCompositionTarget;

namespace vlc {

class CompositorDCompositionAcrylicSurface;

class CompositorDirectComposition : public CompositorVideo
{
    Q_OBJECT
public:
    CompositorDirectComposition(qt_intf_t *p_intf, QObject* parent = nullptr);
    ~CompositorDirectComposition();

    bool init() override;

    bool makeMainInterface(MainCtx*, std::function<void (QQuickWindow *)> aboutToShowQuickWindowCallback = {}) override;
    void destroyMainInterface() override;
    void unloadGUI() override;

    bool setupVoutWindow(vlc_window_t *p_wnd, VoutDestroyCb destroyCb) override;
    QWindow* interfaceMainWindow() const override;

    Type type() const override;

    void addVisual(IDCompositionVisual *visual);
    void removeVisual(IDCompositionVisual *visual);

    QQuickItem * activeFocusItem() const override;

    bool eventFilter(QObject *watched, QEvent *event) override;

    bool canDoThreadedSurfaceUpdates() const override { return true; };

protected:
    bool canDoCombinedSurfaceUpdates() const override { return true; };
    void commitSurface() override;

private slots:
    void onSurfacePositionChanged(const QPointF& position) override;
    void onSurfaceSizeChanged(const QSizeF& size) override;

    void setup();

protected:
    int windowEnable(const vlc_window_cfg_t *) override;
    void windowDisable() override;
    void windowDestroy() override;

private:
    enum class SetupState {
        Uninitialized,
        Success,
        Fail
    };
    SetupState m_setupState = SetupState::Uninitialized;
    QWaitCondition m_setupStateCond;
    QMutex m_setupStateLock;

    std::unique_ptr<QQuickView> m_quickView;

    IDCompositionDevice *m_dcompDevice = nullptr;
    Microsoft::WRL::ComPtr<IDCompositionVisual> m_rootVisual;
    Microsoft::WRL::ComPtr<IDCompositionVisual> m_videoVisual;
    IDCompositionVisual *m_uiVisual = nullptr;

    std::unique_ptr<CompositorDCompositionAcrylicSurface> m_acrylicSurface;
};

}

#endif /* VLC_COMPOSITOR_DIRECT_COMPOSITION */
