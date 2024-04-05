/*****************************************************************************
 * Copyright (C) 2021 the VideoLAN team
 *
 * Authors: Prince Gupta <guptaprince8832@gmail.com>
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

#ifndef COMPOSITOR_DCOMP_ACRYLICSURFACE_HPP
#define COMPOSITOR_DCOMP_ACRYLICSURFACE_HPP

#include <QAbstractNativeEventFilter>
#include <QBasicTimer>

#include "mainctx.hpp"

#include <wrl.h>
#include <dwmapi.h>

#include "compositor_dcomp_error.hpp"

class IDCompositionVisual2;

// Windows Private APIs, taken from https://blog.adeltax.com/dwm-thumbnails-but-with-idcompositionvisual/

enum THUMBNAIL_TYPE {
    TT_DEFAULT = 0x0,
    TT_SNAPSHOT = 0x1,
    TT_ICONIC = 0x2,
    TT_BITMAPPENDING = 0x3,
    TT_BITMAP = 0x4
};

typedef HRESULT(WINAPI* DwmpCreateSharedThumbnailVisual)(
    IN HWND hwndDestination,
    IN HWND hwndSource,
    IN DWORD dwThumbnailFlags,
    IN DWM_THUMBNAIL_PROPERTIES* pThumbnailProperties,
    IN VOID* pDCompDevice,
    OUT VOID** ppVisual,
    OUT PHTHUMBNAIL phThumbnailId);

typedef HRESULT(WINAPI* DwmpQueryWindowThumbnailSourceSize)(
    IN HWND hwndSource,
    IN BOOL fSourceClientAreaOnly,
    OUT SIZE* pSize);

typedef HRESULT(WINAPI* DwmpQueryThumbnailType)(
    IN HTHUMBNAIL hThumbnailId,
    OUT THUMBNAIL_TYPE* thumbType);

typedef HRESULT(WINAPI* DwmpCreateSharedMultiWindowVisual)(
    IN HWND hwndDestination,
    IN VOID* pDCompDevice,
    OUT VOID** ppVisual,
    OUT PHTHUMBNAIL phThumbnailId);

//pre-cobalt/pre-iron
typedef HRESULT(WINAPI* DwmpUpdateSharedVirtualDesktopVisual)(
    IN HTHUMBNAIL hThumbnailId,
    IN HWND* phwndsInclude,
    IN DWORD chwndsInclude,
    IN HWND* phwndsExclude,
    IN DWORD chwndsExclude,
    OUT RECT* prcSource,
    OUT SIZE* pDestinationSize);


//cobalt/iron (20xxx+)
//Change: function name + new DWORD parameter.
//Pass "1" in dwFlags. Feel free to explore other flags.
typedef HRESULT(WINAPI* DwmpUpdateSharedMultiWindowVisual)(
    IN HTHUMBNAIL hThumbnailId,
    IN HWND* phwndsInclude,
    IN DWORD chwndsInclude,
    IN HWND* phwndsExclude,
    IN DWORD chwndsExclude,
    OUT RECT* prcSource,
    OUT SIZE* pDestinationSize,
    IN DWORD dwFlags);

#define DWM_TNP_FREEZE 0x100000
#define DWM_TNP_ENABLE3D 0x4000000
#define DWM_TNP_DISABLE3D 0x8000000
#define DWM_TNP_FORCECVI 0x40000000
#define DWM_TNP_DISABLEFORCECVI 0x80000000

enum WINDOWCOMPOSITIONATTRIB {
    WCA_UNDEFINED = 0x0,
    WCA_NCRENDERING_ENABLED = 0x1,
    WCA_NCRENDERING_POLICY = 0x2,
    WCA_TRANSITIONS_FORCEDISABLED = 0x3,
    WCA_ALLOW_NCPAINT = 0x4,
    WCA_CAPTION_BUTTON_BOUNDS = 0x5,
    WCA_NONCLIENT_RTL_LAYOUT = 0x6,
    WCA_FORCE_ICONIC_REPRESENTATION = 0x7,
    WCA_EXTENDED_FRAME_BOUNDS = 0x8,
    WCA_HAS_ICONIC_BITMAP = 0x9,
    WCA_THEME_ATTRIBUTES = 0xA,
    WCA_NCRENDERING_EXILED = 0xB,
    WCA_NCADORNMENTINFO = 0xC,
    WCA_EXCLUDED_FROM_LIVEPREVIEW = 0xD,
    WCA_VIDEO_OVERLAY_ACTIVE = 0xE,
    WCA_FORCE_ACTIVEWINDOW_APPEARANCE = 0xF,
    WCA_DISALLOW_PEEK = 0x10,
    WCA_CLOAK = 0x11,
    WCA_CLOAKED = 0x12,
    WCA_ACCENT_POLICY = 0x13,
    WCA_FREEZE_REPRESENTATION = 0x14,
    WCA_EVER_UNCLOAKED = 0x15,
    WCA_VISUAL_OWNER = 0x16,
    WCA_HOLOGRAPHIC = 0x17,
    WCA_EXCLUDED_FROM_DDA = 0x18,
    WCA_PASSIVEUPDATEMODE = 0x19,
    WCA_LAST = 0x1A,
};

struct WINDOWCOMPOSITIONATTRIBDATA {
    WINDOWCOMPOSITIONATTRIB Attrib;
    void* pvData;
    DWORD cbData;
};

typedef BOOL(WINAPI* SetWindowCompositionAttribute)(
    IN HWND hwnd,
    IN WINDOWCOMPOSITIONATTRIBDATA* pwcad);

typedef BOOL(WINAPI* GetWindowCompositionAttribute)(
    IN HWND hwnd,
    OUT WINDOWCOMPOSITIONATTRIBDATA* pAttrData
);

namespace vlc
{

class CompositorDirectComposition;

/**
 * @brief The CompositorDCompositionAcrylicSurface class
 * Adds acrylic surface to the compositor_dcomp when the main window becomes active
 * This acrylic surface is only valid for screen configuration at the time of initialization
 */

class CompositorDCompositionAcrylicSurface
        : public QObject
        , public QAbstractNativeEventFilter
{
    Q_OBJECT

public:
    CompositorDCompositionAcrylicSurface(qt_intf_t * intf, CompositorDirectComposition *compositor, MainCtx *mainctx, class IDCompositionDevice *device, QObject *parent = nullptr);

    ~CompositorDCompositionAcrylicSurface();

protected:
    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override;

private:
    bool init();
    bool loadFunctions();
    bool initializeEffects();
    bool createDesktopVisual();
    bool createBackHostVisual();

    void updateTransparencyState();

    void sync();
    void updateVisual();
    void commitChanges();

    void setActive(bool newActive);

    QWindow *window();

    HWND hwnd();

    DwmpCreateSharedThumbnailVisual lDwmpCreateSharedThumbnailVisual {};

    DwmpCreateSharedMultiWindowVisual lDwmpCreateSharedMultiWindowVisual {};

    // use to update visual created with lDwmpCreateSharedMultiWindowVisual
    //PRE-IRON
    DwmpUpdateSharedVirtualDesktopVisual lDwmpUpdateSharedVirtualDesktopVisual {};

    //20xxx+
    DwmpUpdateSharedMultiWindowVisual lDwmpUpdateSharedMultiWindowVisual {};

    SetWindowCompositionAttribute lSetWindowCompositionAttribute {};
    GetWindowCompositionAttribute lGetWindowCompositionAttribute {};

    HTHUMBNAIL m_backHostThumbnail = NULL;
    HWND m_dummyWindow {};

    class IDCompositionDevice3 *m_dcompDevice;
    Microsoft::WRL::ComPtr<IDCompositionVisual2> m_rootVisual;
    Microsoft::WRL::ComPtr<IDCompositionVisual2> m_backHostVisual;
    Microsoft::WRL::ComPtr<IDCompositionVisual2> m_desktopVisual;
    Microsoft::WRL::ComPtr<class IDCompositionRectangleClip> m_rootClip;
    Microsoft::WRL::ComPtr<class IDCompositionTranslateTransform> m_translateTransform;
    Microsoft::WRL::ComPtr<class IDCompositionSaturationEffect> m_saturationEffect;
    Microsoft::WRL::ComPtr<class IDCompositionGaussianBlurEffect> m_gaussianBlur;

    qt_intf_t *m_intf = nullptr;
    CompositorDirectComposition *m_compositor = nullptr;
    MainCtx *m_mainCtx = nullptr;
    bool m_resetPending = false;
    bool m_active = false;
    bool m_transparencyEnabled = false;
    int m_leftMostScreenX = 0;
    int m_topMostScreenY = 0;
};

}

#endif
