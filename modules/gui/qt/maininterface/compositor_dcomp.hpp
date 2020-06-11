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
#include <dcomp.h>
#include <d3d11.h>
#include <wrl.h>
#include <dwmapi.h>

#include "maininterface/mainui.hpp"
#include "compositor_dcomp_uisurface.hpp"
#include "videosurface.hpp"

#include <QOpenGLContext>

class MainInterface;

namespace vlc {

class CompositorDirectComposition : public QObject, public Compositor
{
    Q_OBJECT
public:
    CompositorDirectComposition(intf_thread_t *p_intf, QObject* parent = nullptr);
    ~CompositorDirectComposition();

    bool init();

    MainInterface *makeMainInterface() override;
    void destroyMainInterface() override;

    bool setupVoutWindow(vout_window_t *p_wnd) override;

private:
    static int window_enable(struct vout_window_t *, const vout_window_cfg_t *);
    static void window_disable(struct vout_window_t *);
    static void window_resize(struct vout_window_t *, unsigned width, unsigned height);
    static void window_destroy(struct vout_window_t *);
    static void window_set_state(struct vout_window_t *, unsigned state);
    static void window_unset_fullscreen(struct vout_window_t *);
    static void window_set_fullscreen(struct vout_window_t *, const char *id);

    intf_thread_t *m_intf = nullptr;

    MainInterface* m_rootWindow = nullptr;
    std::unique_ptr<CompositorDCompositionUISurface> m_uiSurface;
    vout_window_t *m_window = nullptr;
    std::unique_ptr<MainUI> m_ui;
    std::unique_ptr<VideoSurfaceProvider> m_qmlVideoSurfaceProvider;

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
