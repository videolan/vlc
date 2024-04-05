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

// Win 8.1 for IDCompositionDevice3/IDCompositionVisual2
# if !defined(_WIN32_WINNT) || (_WIN32_WINNT < 0x0603) // _WIN32_WINNT_WINBLUE
#  undef _WIN32_WINNT
#  define _WIN32_WINNT 0x0603
# endif

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dcomp.h>

#include "compositor_dcomp_acrylicsurface.hpp"

#include <QWindow>
#include <QScreen>
#include <QLibrary>
#include <versionhelpers.h>

#include "compositor_dcomp.hpp"

#include <windows.h>
#include <ntdef.h>

namespace
{

bool isTransparencyEnabled()
{
    HIGHCONTRAST constrastInfo;
    constrastInfo.cbSize = sizeof(HIGHCONTRAST);

    bool ret = SystemParametersInfoA(SPI_GETHIGHCONTRAST, constrastInfo.cbSize, &constrastInfo, 0);

    bool useHighContrast = ret && ((constrastInfo.dwFlags & HCF_HIGHCONTRASTON) == HCF_HIGHCONTRASTON);
    if (useHighContrast)
        return false;

    static const char *TRANSPARENCY_SETTING_PATH = "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize";
    static const char *TRANSPARENCY_SETTING_KEY = "EnableTransparency";

    QSettings settings(QLatin1String {TRANSPARENCY_SETTING_PATH}, QSettings::NativeFormat);
    return settings.value(TRANSPARENCY_SETTING_KEY).toBool();
}

template <typename F>
F loadFunction(QLibrary &library, const char *symbol)
{
    vlc_assert(library.isLoaded());

    auto f = library.resolve(symbol);
    if (!f)
    {
        const auto err = GetLastError();
        throw std::runtime_error(QString("failed to load %1, code %2").arg(QString(symbol), QString::number(err)).toStdString());
    }

    return reinterpret_cast<F>(f);
}

bool isWinPreIron()
{
    typedef NTSTATUS(WINAPI* RtlGetVersionPtr)(PRTL_OSVERSIONINFOW);

    auto ntdll = GetModuleHandleW(L"ntdll.dll");
    auto GetVersionInfo = reinterpret_cast<RtlGetVersionPtr>(GetProcAddress(ntdll, "RtlGetVersion"));

    if (GetVersionInfo)
    {
        RTL_OSVERSIONINFOW versionInfo = { };
        versionInfo.dwOSVersionInfoSize = sizeof(versionInfo);
        if (!GetVersionInfo(&versionInfo))
            return versionInfo.dwMajorVersion <= 10
                    && versionInfo.dwBuildNumber < 20000;
    }

    return false;
}

}

namespace vlc
{

CompositorDCompositionAcrylicSurface::CompositorDCompositionAcrylicSurface(qt_intf_t *intf, CompositorDirectComposition *compositor, MainCtx *mainCtx, IDCompositionDevice *device, QObject *parent)
    : QObject(parent)
    , m_intf {intf}
    , m_compositor {compositor}
    , m_mainCtx {mainCtx}
{
    assert(device);
    device->QueryInterface(__uuidof(IDCompositionDevice3), (void**)&m_dcompDevice);

    if (!m_dcompDevice)
        throw std::runtime_error("DCompositionDevice is not DCompositionDevice3.");

    if (!init())
        throw std::exception();

    qApp->installNativeEventFilter(this);

    setActive(m_transparencyEnabled && m_mainCtx->acrylicActive());
    connect(m_mainCtx, &MainCtx::acrylicActiveChanged, this, [this]()
    {
        setActive(m_transparencyEnabled && m_mainCtx->acrylicActive());
    });

    // CSDWin32EventHandler updates frame when window is maximized
    connect(window(), &QWindow::windowStateChanged, this, [this]()
    {
        sync();
        commitChanges();
    }
    // CSDWin32EventHandler changes client rect on window state change
    // use queued connection so that we can get correct state.
    , Qt::QueuedConnection);
}

CompositorDCompositionAcrylicSurface::~CompositorDCompositionAcrylicSurface()
{
    m_mainCtx->setHasAcrylicSurface(false);

    if (m_dummyWindow)
        DestroyWindow(m_dummyWindow);
}

bool CompositorDCompositionAcrylicSurface::nativeEventFilter(const QByteArray &, void *message, qintptr *)
{
    MSG* msg = static_cast<MSG*>( message );

    if (msg->hwnd != hwnd())
        return false;

    switch (msg->message)
    {
    case WM_WINDOWPOSCHANGED:
    {
        if (!m_active)
            break;

        sync();
        commitChanges();
        break;
    }
    case WM_SETTINGCHANGE:
    {
        if (!lstrcmpW(LPCWSTR(msg->lParam), L"ImmersiveColorSet"))
        {
            updateTransparencyState();
        }
        break;
    }
    case WM_SYSCOLORCHANGE:
    {
        updateTransparencyState();
        break;
    }
    }

    return false;
}


bool CompositorDCompositionAcrylicSurface::init()
{
    if (!loadFunctions())
        return false;

    if (!initializeEffects())
        return false;

    if (!createDesktopVisual())
        return false;

    if (!createBackHostVisual())
        return false;

    m_leftMostScreenX = 0;
    m_topMostScreenY = 0;
    for (const auto screen : qGuiApp->screens())
    {
        const auto geometry = screen->geometry();
        m_leftMostScreenX = std::min<int>(geometry.left(), m_leftMostScreenX);
        m_topMostScreenY = std::min<int>(geometry.top(), m_topMostScreenY);
    }

    m_transparencyEnabled = isTransparencyEnabled();
    m_mainCtx->setHasAcrylicSurface(m_transparencyEnabled);

    return true;
}

void CompositorDCompositionAcrylicSurface::updateTransparencyState()
{
    const auto transparencyEnabled = isTransparencyEnabled();
    if (m_transparencyEnabled == transparencyEnabled)
        return;

    m_transparencyEnabled = transparencyEnabled;
    m_mainCtx->setHasAcrylicSurface(m_transparencyEnabled);
    setActive(m_transparencyEnabled && m_mainCtx->acrylicActive());
}

bool CompositorDCompositionAcrylicSurface::loadFunctions()
try
{
    QLibrary dwmapi("dwmapi.dll");
    if (!dwmapi.load())
        throw std::runtime_error("failed to dwmapi.dll, reason: " + dwmapi.errorString().toStdString());

    lDwmpCreateSharedThumbnailVisual = loadFunction<DwmpCreateSharedThumbnailVisual>(dwmapi, MAKEINTRESOURCEA(147));
    lDwmpCreateSharedMultiWindowVisual = loadFunction<DwmpCreateSharedMultiWindowVisual>(dwmapi, MAKEINTRESOURCEA(163));

    if (isWinPreIron())
        lDwmpUpdateSharedVirtualDesktopVisual = loadFunction<DwmpUpdateSharedVirtualDesktopVisual>(dwmapi, MAKEINTRESOURCEA(164)); //PRE-IRON
    else
        lDwmpUpdateSharedMultiWindowVisual = loadFunction<DwmpUpdateSharedMultiWindowVisual>(dwmapi, MAKEINTRESOURCEA(164)); //20xxx+


    QLibrary user32("user32.dll");
    if (!user32.load())
        throw std::runtime_error("failed to user32.dll, reason: " + user32.errorString().toStdString());

    lSetWindowCompositionAttribute = loadFunction<SetWindowCompositionAttribute>(user32, "SetWindowCompositionAttribute");
    lGetWindowCompositionAttribute = loadFunction<GetWindowCompositionAttribute>(user32, "GetWindowCompositionAttribute");

    return true;
}
catch (std::exception &err)
{
    msg_Err(m_intf, "%s", err.what());
    return false;
}

bool CompositorDCompositionAcrylicSurface::initializeEffects()
try
{
    assert(m_dcompDevice);

    HR(m_dcompDevice->CreateVisual(&m_rootVisual), "create root visual");

    HR(m_dcompDevice->CreateRectangleClip(&m_rootClip), "create root clip");

    HR(m_dcompDevice->CreateTranslateTransform(&m_translateTransform), "create translate transform");

    HR(m_dcompDevice->CreateSaturationEffect(&m_saturationEffect), "create saturation effect");

    HR(m_dcompDevice->CreateGaussianBlurEffect(&m_gaussianBlur), "create gaussian effect");

    m_saturationEffect->SetSaturation(2);

    m_gaussianBlur->SetBorderMode(D2D1_BORDER_MODE_HARD);
    m_gaussianBlur->SetStandardDeviation(40);
    m_gaussianBlur->SetInput(0, m_saturationEffect.Get(), 0);
    m_rootVisual->SetEffect(m_gaussianBlur.Get());

    return true;
}
catch (const DXError &err)
{
    msg_Err(m_intf, "failed to initialise compositor acrylic surface: '%s' code: 0x%lX", err.what(), err.code());
    return false;
}


bool CompositorDCompositionAcrylicSurface::createDesktopVisual()
try
{
    vlc_assert(!m_desktopVisual);
    auto desktopWindow = GetShellWindow();
    if (!desktopWindow)
        throw DXError("failed to get desktop window",  static_cast<HRESULT>(GetLastError()));

    const int desktopWidth = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    const int desktopHeight = GetSystemMetrics(SM_CYVIRTUALSCREEN);

    DWM_THUMBNAIL_PROPERTIES thumbnail;
    thumbnail.dwFlags = DWM_TNP_SOURCECLIENTAREAONLY | DWM_TNP_VISIBLE | DWM_TNP_RECTDESTINATION | DWM_TNP_RECTSOURCE | DWM_TNP_OPACITY | DWM_TNP_ENABLE3D;
    thumbnail.opacity = 255;
    thumbnail.fVisible = TRUE;
    thumbnail.fSourceClientAreaOnly = FALSE;
    thumbnail.rcDestination = RECT{ 0, 0, desktopWidth, desktopHeight };
    thumbnail.rcSource = RECT{ 0, 0, desktopWidth, desktopHeight };

    HTHUMBNAIL desktopThumbnail;
    HR(lDwmpCreateSharedThumbnailVisual(hwnd(), desktopWindow, 2, &thumbnail, m_dcompDevice, &m_desktopVisual, &desktopThumbnail), "create desktop visual");
    HR(m_rootVisual->AddVisual(m_desktopVisual.Get(), FALSE, nullptr), "Add desktop visual");

    return true;
}
catch (const DXError &err)
{
    msg_Err(m_intf, "failed to create desktop visual: '%s' code: 0x%lX", err.what(), err.code());
    return false;
}

bool CompositorDCompositionAcrylicSurface::createBackHostVisual()
try
{
    vlc_assert(!m_dummyWindow);
    // lDwmpCreateSharedMultiWindowVisual requires a window with disabled live (thumbnail) preview
    // use a hidden dummy window to avoid disabling live preview of main window
    m_dummyWindow = ::CreateWindowExA(WS_EX_TOOLWINDOW, "STATIC", "dummy", WS_VISIBLE, 0, 0, 0, 0, NULL, NULL, NULL, NULL);
    if (!m_dummyWindow)
        throw DXError("failed to create dummy window",  static_cast<HRESULT>(GetLastError()));

    int attr = DWM_CLOAKED_APP;
    DwmSetWindowAttribute(m_dummyWindow, DWMWA_CLOAK, &attr, sizeof attr);

    BOOL enable = TRUE;
    WINDOWCOMPOSITIONATTRIBDATA CompositionAttribute{};
    CompositionAttribute.Attrib = WCA_EXCLUDED_FROM_LIVEPREVIEW;
    CompositionAttribute.pvData = &enable;
    CompositionAttribute.cbData = sizeof(BOOL);
    lSetWindowCompositionAttribute(m_dummyWindow, &CompositionAttribute);

    vlc_assert(!m_backHostVisual);
    HR(lDwmpCreateSharedMultiWindowVisual(m_dummyWindow, m_dcompDevice, &m_backHostVisual, &m_backHostThumbnail)
       , "failed to create shared multi visual");

    updateVisual();

    HR(m_rootVisual->AddVisual(m_backHostVisual.Get(), TRUE, m_desktopVisual.Get()), "Add backhost visual");

    return true;
}
catch (const DXError &err)
{
    msg_Err(m_intf, "failed to create acrylic back host visual: '%s' code: 0x%lX", err.what(), err.code());
    return false;
}

void CompositorDCompositionAcrylicSurface::sync()
{
    if (!hwnd())
        return;

    const int dx = std::abs(m_leftMostScreenX);
    const int dy = std::abs(m_topMostScreenY);

    // window()->geometry()/frameGeometry() returns incorrect rect with CSD
    RECT rect;
    GetWindowRect(hwnd(), &rect);
    m_rootClip->SetLeft((float)rect.left + dx);
    m_rootClip->SetRight((float)rect.right + dx);
    m_rootClip->SetTop((float)rect.top + dy);
    m_rootClip->SetBottom((float)rect.bottom + dy);
    m_rootVisual->SetClip(m_rootClip.Get());

    int frameX = 0;
    int frameY = 0;

    if (!m_mainCtx->useClientSideDecoration())
    {
        frameX = GetSystemMetrics(SM_CXFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
        frameY = GetSystemMetrics(SM_CYFRAME) + GetSystemMetrics(SM_CYCAPTION)
                    + GetSystemMetrics(SM_CXPADDEDBORDER);
    }
    else if (window()->visibility() & QWindow::Maximized)
    {
        // in maximized state CSDWin32EventHandler re-adds border
        frameX = GetSystemMetrics(SM_CXSIZEFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
        frameY = GetSystemMetrics(SM_CYSIZEFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
    }

    m_translateTransform->SetOffsetX(-1 * ((float)rect.left + frameX + dx));
    m_translateTransform->SetOffsetY(-1 * ((float)rect.top + frameY + dy));
    m_rootVisual->SetTransform(m_translateTransform.Get());
}

void CompositorDCompositionAcrylicSurface::updateVisual()
{
    const auto w = window();
    if (!w || !w->screen())
        return;

    RECT sourceRect {};
    GetWindowRect(GetShellWindow(), &sourceRect);

    const int desktopWidth = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    const int desktopHeight = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    SIZE destinationSize {desktopWidth, desktopHeight};

    HWND hwndExclusionList[2];
    hwndExclusionList[0] = hwnd();
    hwndExclusionList[1] = m_dummyWindow;

    HRESULT hr = S_FALSE;

    if (lDwmpUpdateSharedVirtualDesktopVisual)
        hr = lDwmpUpdateSharedVirtualDesktopVisual(m_backHostThumbnail, NULL, 0, hwndExclusionList, 2, &sourceRect, &destinationSize);
    else if (lDwmpUpdateSharedMultiWindowVisual)
        hr = lDwmpUpdateSharedMultiWindowVisual(m_backHostThumbnail, NULL, 0, hwndExclusionList, 2, &sourceRect, &destinationSize, 1);
    else
        vlc_assert_unreachable();

    if (FAILED(hr))
        qDebug("failed to update shared multi window visual");
}

void CompositorDCompositionAcrylicSurface::commitChanges()
{
    m_dcompDevice->Commit();
    DwmFlush();
}

void CompositorDCompositionAcrylicSurface::setActive(const bool newActive)
{
    if (newActive == m_active)
        return;

    m_active = newActive;
    if (m_active)
    {
        m_compositor->addVisual(m_rootVisual.Get());

        updateVisual();
        sync();
        commitChanges();
    }
    else
    {
        m_compositor->removeVisual(m_rootVisual.Get());
    }
}

QWindow *CompositorDCompositionAcrylicSurface::window()
{
    return m_compositor->interfaceMainWindow();
}

HWND CompositorDCompositionAcrylicSurface::hwnd()
{
    if (auto w = window())
        return w->handle() ? (HWND)w->winId() : nullptr;

    return nullptr;
}

}
