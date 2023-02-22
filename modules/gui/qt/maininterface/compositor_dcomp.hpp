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

#include <windows.h>

#include "maininterface/mainui.hpp"
#include "compositor_dcomp_acrylicsurface.hpp"
#include "compositor_dcomp_uisurface.hpp"
#include "videosurface.hpp"
#include "interface_window_handler.hpp"
#include "video_window_handler.hpp"

#include <QOpenGLContext>

class MainCtx;
class WinTaskbarWidget;

namespace vlc {

class CompositorDirectComposition : public CompositorVideo
{
    Q_OBJECT
public:
    CompositorDirectComposition(qt_intf_t *p_intf, QObject* parent = nullptr);
    ~CompositorDirectComposition();

    static bool preInit(qt_intf_t *);
    bool init() override;

    bool makeMainInterface(MainCtx*) override;
    void destroyMainInterface() override;
    void unloadGUI() override;

    bool setupVoutWindow(vlc_window_t *p_wnd, VoutDestroyCb destroyCb) override;
    virtual QWindow* interfaceMainWindow() const override;

    Type type() const override;

    void addVisual(Microsoft::WRL::ComPtr<IDCompositionVisual> visual);
    void removeVisual(Microsoft::WRL::ComPtr<IDCompositionVisual> visual);

    QQuickItem * activeFocusItem() const override;

private slots:
    void onSurfacePositionChanged(const QPointF& position) override;
    void onSurfaceSizeChanged(const QSizeF& size) override;

protected:
    int windowEnable(const vlc_window_cfg_t *) override;
    void windowDisable() override;
    void windowDestroy() override;

private:
    DCompRenderWindow* m_rootWindow = nullptr;

    std::unique_ptr<WinTaskbarWidget> m_taskbarWidget;

    std::unique_ptr<CompositorDCompositionUISurface> m_uiSurface;
    std::unique_ptr<CompositorDCompositionAcrylicSurface> m_acrylicSurface;

    //main window composition
    HINSTANCE m_dcomp_dll = nullptr;
    Microsoft::WRL::ComPtr<ID3D11Device> m_d3d11Device;
    Microsoft::WRL::ComPtr<IDCompositionDevice> m_dcompDevice;
    Microsoft::WRL::ComPtr<IDCompositionTarget> m_dcompTarget;
    Microsoft::WRL::ComPtr<IDCompositionVisual> m_rootVisual;
    Microsoft::WRL::ComPtr<IDCompositionVisual> m_uiVisual;
    Microsoft::WRL::ComPtr<IDCompositionVisual> m_videoVisual;
};

}

#endif /* VLC_COMPOSITOR_DIRECT_COMPOSITION */
