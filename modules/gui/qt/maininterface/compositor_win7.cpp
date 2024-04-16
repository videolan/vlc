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

#include <QApplication>
#include <QLibrary>

#include "mainctx_win32.hpp"
#include "mainui.hpp"

#include <vlc_window.h>

#include <dwmapi.h>

using namespace vlc;

int CompositorWin7::windowEnable(const vlc_window_cfg_t *)
{
    commonWindowEnable();
    return VLC_SUCCESS;
}

void CompositorWin7::windowDisable()
{
    commonWindowDisable();
}


CompositorWin7::CompositorWin7(qt_intf_t *p_intf, QObject* parent)
    : CompositorVideo(p_intf, parent)
{
}

CompositorWin7::~CompositorWin7()
{
}

bool CompositorWin7::preInit(qt_intf_t *p_intf)
{
    return true;
}

bool CompositorWin7::init()
{
    {
        const QString& platformName = qApp->platformName();
        if (!(platformName == QLatin1String("windows") || platformName == QLatin1String("direct2d")))
            return false;
    }

    return true;
}

bool CompositorWin7::makeMainInterface(MainCtx* mainCtx)
{
    m_mainCtx = mainCtx;

    /*
     * m_stable is not attached to the main interface because dialogs are attached to the mainCtx
     * and showing them would raise the video widget above the interface
     */
    m_videoWidget = new QWidget(nullptr, Qt::Tool | Qt::FramelessWindowHint);
    m_stable = new QWidget(m_videoWidget);
    m_stable->setContextMenuPolicy( Qt::PreventContextMenu );

    QPalette plt = m_stable->palette();
    plt.setColor( QPalette::Window, Qt::black );
    m_stable->setPalette( plt );
    m_stable->setAutoFillBackground(true);
    /* Force the widget to be native so that it gets a winId() */
    m_stable->setAttribute( Qt::WA_NativeWindow, true );
    m_stable->setAttribute( Qt::WA_PaintOnScreen, true );
    m_stable->setMouseTracking( true );
    m_stable->setAttribute( Qt::WA_ShowWithoutActivating );
    m_stable->setVisible(true);

    m_videoWidget->show();
    m_videoWindowHWND = (HWND)m_videoWidget->winId();

    BOOL excluseFromPeek = TRUE;
    DwmSetWindowAttribute(m_videoWindowHWND, DWMWA_EXCLUDED_FROM_PEEK, &excluseFromPeek, sizeof(excluseFromPeek));
    DwmSetWindowAttribute(m_videoWindowHWND, DWMWA_DISALLOW_PEEK, &excluseFromPeek, sizeof(excluseFromPeek));

    m_qmlView = std::make_unique<QQuickView>();
    m_qmlView->setResizeMode(QQuickView::SizeRootObjectToView);
    m_qmlView->setColor(QColor(Qt::transparent));

    m_qmlView->installEventFilter(this);
    m_nativeEventFilter = std::make_unique<Win7NativeEventFilter>();
    qApp->installNativeEventFilter(m_nativeEventFilter.get());
    connect(m_nativeEventFilter.get(), &Win7NativeEventFilter::windowStyleChanged,
            this, &CompositorWin7::resetVideoZOrder);

    m_qmlView->show();

    m_qmlWindowHWND = (HWND)m_qmlView->winId();

    return commonGUICreate(m_qmlView.get(), m_qmlView.get(), CompositorVideo::CAN_SHOW_PIP);
}

void CompositorWin7::destroyMainInterface()
{
    commonIntfDestroy();
    if (m_videoWidget)
    {
        delete m_videoWidget;
        m_videoWidget = nullptr;
    }
}

void CompositorWin7::unloadGUI()
{
    m_qmlView.reset();
    commonGUIDestroy();
}

bool CompositorWin7::setupVoutWindow(vlc_window_t *p_wnd, VoutDestroyCb destroyCb)
{
    BOOL isCompositionEnabled;
    HRESULT hr = DwmIsCompositionEnabled(&isCompositionEnabled);

    //composition is disabled, video can't be seen through the interface,
    //so we fallback to a separate window.
    if (FAILED(hr) || !isCompositionEnabled)
        return false;

    commonSetupVoutWindow(p_wnd, destroyCb);
    p_wnd->type = VLC_WINDOW_TYPE_HWND;
    p_wnd->handle.hwnd = (HWND)m_stable->winId();
    p_wnd->display.x11 = nullptr;
    return true;
}

QWindow *CompositorWin7::interfaceMainWindow() const
{
    return m_qmlView.get();
}

Compositor::Type CompositorWin7::type() const
{
    return Compositor::Win7Compositor;
}

QQuickItem * CompositorWin7::activeFocusItem() const /* override */
{
    return m_qmlView->activeFocusItem();
}

bool CompositorWin7::eventFilter(QObject*, QEvent* ev)
{
    if (!m_videoWidget || !m_qmlView)
        return false;

    switch (ev->type())
    {
    case QEvent::Move:
    case QEvent::Resize:
    case QEvent::ApplicationStateChange:
        m_videoWidget->setGeometry(m_qmlView->geometry());
        resetVideoZOrder();
        break;
    case QEvent::WindowStateChange:
        if (m_qmlView->windowStates() & Qt::WindowMinimized)
            m_videoWidget->hide();
        else
        {
            m_videoWidget->show();
            m_videoWidget->setGeometry(m_qmlView->geometry());
            resetVideoZOrder();
        }
        break;

    case QEvent::FocusIn:
        resetVideoZOrder();
        break;
    case QEvent::Show:
        m_videoWidget->show();
        resetVideoZOrder();
        break;
    case QEvent::Hide:
        m_videoWidget->hide();
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

void CompositorWin7::onSurfacePositionChanged(const QPointF& position)
{
    m_stable->move((position / m_stable->window()->devicePixelRatioF()).toPoint());
}

void CompositorWin7::onSurfaceSizeChanged(const QSizeF& size)
{
    m_stable->resize((size / m_stable->window()->devicePixelRatioF()).toSize());
}


Win7NativeEventFilter::Win7NativeEventFilter(QObject* parent)
    : QObject(parent)
{
}

//parse native events that are not reported by Qt
bool Win7NativeEventFilter::nativeEventFilter(const QByteArray&, void* message, qintptr*)
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
