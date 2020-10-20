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
#include "compositor_win7.hpp"
#include "main_interface_win32.hpp"
#include "mainui.hpp"

#include <d3d11.h>

#include <dwmapi.h>
#include <QLibrary>

using namespace vlc;

int CompositorWin7::window_enable(struct vout_window_t * p_wnd, const vout_window_cfg_t *)
{
    CompositorWin7* that = static_cast<CompositorWin7*>(p_wnd->sys);
    msg_Dbg(that->m_intf, "window_enable");
    that->m_qmlVideoSurfaceProvider->enable(p_wnd);
    that->m_qmlVideoSurfaceProvider->setVideoEmbed(true);
    return VLC_SUCCESS;
}

void CompositorWin7::window_disable(struct vout_window_t * p_wnd)
{
    CompositorWin7* that = static_cast<CompositorWin7*>(p_wnd->sys);
    that->m_qmlVideoSurfaceProvider->setVideoEmbed(false);
    that->m_qmlVideoSurfaceProvider->disable();
    that->m_videoWindowHandler->disable();
    msg_Dbg(that->m_intf, "window_disable");
}

void CompositorWin7::window_resize(struct vout_window_t * p_wnd, unsigned width, unsigned height)
{
    CompositorWin7* that = static_cast<CompositorWin7*>(p_wnd->sys);
    msg_Dbg(that->m_intf, "window_resize %ux%u", width, height);
    that->m_videoWindowHandler->requestResizeVideo(width, height);
}

void CompositorWin7::window_destroy(struct vout_window_t * p_wnd)
{
    CompositorWin7* that = static_cast<CompositorWin7*>(p_wnd->sys);
    msg_Dbg(that->m_intf, "window_destroy");
}

void CompositorWin7::window_set_state(struct vout_window_t * p_wnd, unsigned state)
{
    CompositorWin7* that = static_cast<CompositorWin7*>(p_wnd->sys);
    msg_Dbg(that->m_intf, "window_set_state");
    that->m_videoWindowHandler->requestVideoState(static_cast<vout_window_state>(state));
}

void CompositorWin7::window_unset_fullscreen(struct vout_window_t * p_wnd)
{
    CompositorWin7* that = static_cast<CompositorWin7*>(p_wnd->sys);
    msg_Dbg(that->m_intf, "window_unset_fullscreen");
    that->m_videoWindowHandler->requestVideoWindowed();
}

void CompositorWin7::window_set_fullscreen(struct vout_window_t * p_wnd, const char *id)
{
    CompositorWin7* that = static_cast<CompositorWin7*>(p_wnd->sys);
    msg_Dbg(that->m_intf, "window_set_fullscreen");
    that->m_videoWindowHandler->requestVideoFullScreen(id);
}


CompositorWin7::CompositorWin7(intf_thread_t *p_intf, QObject* parent)
    : QObject(parent)
    , m_intf(p_intf)
{
}

CompositorWin7::~CompositorWin7()
{
    if (m_taskbarWidget)
        qApp->removeNativeEventFilter(m_taskbarWidget);
    if (m_nativeEventFilter)
        qApp->removeNativeEventFilter(m_nativeEventFilter);
    if (m_stable)
        delete m_stable;
}

bool CompositorWin7::init()
{
    //check whether D3DCompiler is available. whitout it Angle won't work
    QLibrary d3dCompilerDll;
    for (int i = 47; i > 41; --i)
    {
        d3dCompilerDll.setFileName(QString("D3DCOMPILER_%1.dll").arg(i));
        if (d3dCompilerDll.load())
            break;
    }

    D3D_FEATURE_LEVEL requestedFeatureLevels[] = {
        D3D_FEATURE_LEVEL_9_1,
        D3D_FEATURE_LEVEL_9_2,
        D3D_FEATURE_LEVEL_9_3,
        D3D_FEATURE_LEVEL_10_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
    };

    HRESULT hr = D3D11CreateDevice(
        nullptr,    // Adapter
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,    // Module
        D3D11_CREATE_DEVICE_BGRA_SUPPORT,
        requestedFeatureLevels,
        ARRAY_SIZE(requestedFeatureLevels),
        D3D11_SDK_VERSION,
        nullptr, //D3D device
        nullptr,    // Actual feature level
        nullptr //D3D context
        );

    //no hw acceleration, manually select the software backend
    //otherwise Qt will load angle and fail.
    if (!d3dCompilerDll.isLoaded() || FAILED(hr))
    {
        msg_Info(m_intf, "no D3D support, use software backend");
        QQuickWindow::setSceneGraphBackend(QSGRendererInterface::Software);
    }

    return true;
}

MainInterface* CompositorWin7::makeMainInterface()
{
    //Tool flag needs to be passed in the window constructor otherwise the
    //window will still appears int the taskbar
    MainInterfaceWin32* rootWindowW32 =  new MainInterfaceWin32(m_intf, nullptr, Qt::Tool | Qt::FramelessWindowHint );
    m_rootWindow = rootWindowW32;
    //m_rootWindow6>show() is not called on purpose

    /*
     * m_stable is not attached to the main interface because dialogs are attached to the mainInterface
     * and showing them would raise the video widget above the interface
     */
    m_stable = new QWidget(nullptr, Qt::Tool | Qt::FramelessWindowHint);
    m_stable->setContextMenuPolicy( Qt::PreventContextMenu );

    QPalette plt = m_rootWindow->palette();
    plt.setColor( QPalette::Window, Qt::black );
    m_stable->setPalette( plt );
    m_stable->setAutoFillBackground(true);
    /* Force the widget to be native so that it gets a winId() */
    m_stable->setAttribute( Qt::WA_NativeWindow, true );
    m_stable->setAttribute( Qt::WA_PaintOnScreen, true );
    m_stable->setMouseTracking( true );
    m_stable->setWindowFlags( Qt::Tool | Qt::FramelessWindowHint | Qt::WindowDoesNotAcceptFocus );
    m_stable->setAttribute( Qt::WA_ShowWithoutActivating );
    m_stable->show();

    m_videoWindowHWND = (HWND)m_stable->winId();

    BOOL excluseFromPeek = TRUE;
    DwmSetWindowAttribute(m_videoWindowHWND, DWMWA_EXCLUDED_FROM_PEEK, &excluseFromPeek, sizeof(excluseFromPeek));
    DwmSetWindowAttribute(m_videoWindowHWND, DWMWA_DISALLOW_PEEK, &excluseFromPeek, sizeof(excluseFromPeek));

    m_qmlVideoSurfaceProvider = std::make_unique<VideoSurfaceProvider>();
    m_rootWindow->setVideoSurfaceProvider(m_qmlVideoSurfaceProvider.get());

    m_qmlView = std::make_unique<QQuickView>();
    m_qmlView->setResizeMode(QQuickView::SizeRootObjectToView);
    m_qmlView->setClearBeforeRendering(true);
    m_qmlView->setColor(QColor(Qt::transparent));
    m_qmlView->setGeometry(m_rootWindow->geometry());
    m_qmlView->setMinimumSize( m_rootWindow->minimumSize() );
    if (m_rootWindow->useClientSideDecoration())
        m_qmlView->setFlag(Qt::FramelessWindowHint);

    m_qmlView->installEventFilter(this);
    Win7NativeEventFilter* m_nativeEventFilter = new Win7NativeEventFilter(this);
    qApp->installNativeEventFilter(m_nativeEventFilter);
    connect(m_nativeEventFilter, &Win7NativeEventFilter::windowStyleChanged,
            this, &CompositorWin7::resetVideoZOrder);

    m_qmlView->show();

    m_qmlWindowHWND = (HWND)m_qmlView->winId();

    m_videoWindowHandler = std::make_unique<VideoWindowHandler>(m_intf, m_rootWindow);
    m_videoWindowHandler->setWindow( m_qmlView.get() );

    new InterfaceWindowHandlerWin32(m_intf, m_rootWindow, m_qmlView.get(), m_qmlView.get());

    m_taskbarWidget = new WinTaskbarWidget(m_intf, m_qmlView.get(), this);
    qApp->installNativeEventFilter(m_taskbarWidget);

    MainUI* m_ui = new MainUI(m_intf, m_rootWindow, m_qmlView.get(), this);
    m_ui->setup(m_qmlView->engine());


    m_qmlView->setContent(QUrl(), m_ui->getComponent(), m_ui->createRootItem());

    connect(m_rootWindow, &MainInterface::windowTitleChanged,
            m_qmlView.get(), &QQuickView::setTitle);
    connect(m_rootWindow, &MainInterface::windowIconChanged,
            m_qmlView.get(), &QQuickView::setIcon);
    connect(m_rootWindow, &MainInterface::requestInterfaceMaximized,
            m_qmlView.get(), &QWindow::showMaximized);
    connect(m_rootWindow, &MainInterface::requestInterfaceNormal,
            m_qmlView.get(), &QWindow::showNormal);

    return m_rootWindow;
}

void CompositorWin7::destroyMainInterface()
{
    m_qmlVideoSurfaceProvider.reset();
    m_videoWindowHandler.reset();
    m_qmlView.reset();
    if (m_rootWindow)
    {
        delete m_rootWindow;
        m_rootWindow = nullptr;
    }
}

bool CompositorWin7::setupVoutWindow(vout_window_t *p_wnd)
{
    BOOL isCompositionEnabled;
    HRESULT hr = DwmIsCompositionEnabled(&isCompositionEnabled);

    //composition is disabled, video can't be seen through the interface,
    //so we fallback to a separate window.
    if (FAILED(hr) || !isCompositionEnabled)
        return false;

    static const struct vout_window_operations ops = {
        CompositorWin7::window_enable,
        CompositorWin7::window_disable,
        CompositorWin7::window_resize,
        CompositorWin7::window_destroy,
        CompositorWin7::window_set_state,
        CompositorWin7::window_unset_fullscreen,
        CompositorWin7::window_set_fullscreen,
        nullptr, //window_set_title
    };

    p_wnd->sys = this;
    p_wnd->type = VOUT_WINDOW_TYPE_HWND;
    p_wnd->handle.hwnd = (HWND)m_stable->winId();
    p_wnd->display.x11 = nullptr;
    p_wnd->ops = &ops;
    p_wnd->info.has_double_click = true;
    return true;
}

bool CompositorWin7::eventFilter(QObject*, QEvent* ev)
{
    switch (ev->type())
    {
    case QEvent::Close:
        m_rootWindow->close();
        break;
    case QEvent::Move:
    case QEvent::Resize:
    case QEvent::ApplicationStateChange:
        m_stable->setGeometry(m_qmlView->geometry());
        resetVideoZOrder();
        break;
    case QEvent::WindowStateChange:
        if (m_qmlView->windowStates() & Qt::WindowMinimized)
            m_stable->hide();
        else
        {
            m_stable->show();
            m_stable->setGeometry(m_qmlView->geometry());
            resetVideoZOrder();
        }
        break;

    case QEvent::FocusIn:
        resetVideoZOrder();
        break;
    case QEvent::Show:
        m_stable->show();
        resetVideoZOrder();
        break;
    case QEvent::Hide:
        m_stable->hide();
        break;
    default:
        break;
    }

    return false;
}

void CompositorWin7::resetVideoZOrder()
{
    //Place the video wdiget right behind the interface
    HWND bottomHWND = m_qmlWindowHWND;
    HWND currentHWND = bottomHWND;
    while (currentHWND != nullptr)
    {
        bottomHWND = currentHWND;
        currentHWND = GetWindow(bottomHWND, GW_OWNER);
    }

    SetWindowPos(
        m_videoWindowHWND,
        bottomHWND,
        0,0,0,0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE
    );
}

Win7NativeEventFilter::Win7NativeEventFilter(QObject* parent)
    : QObject(parent)
{
}

//parse native events that are not reported by Qt
bool Win7NativeEventFilter::nativeEventFilter(const QByteArray&, void* message, long*)
{
    MSG * msg = static_cast<MSG*>( message );

    switch( msg->message )
    {
        //style like "always on top" changed
        case WM_STYLECHANGED:
            emit windowStyleChanged();
            break;
    }
    return false;
}
