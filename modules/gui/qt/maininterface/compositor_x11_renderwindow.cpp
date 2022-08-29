/*****************************************************************************
 * Copyright (C) 2021 VLC authors and VideoLAN
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
#include <math.h>

#include <QtEvents>
#include <QWindow>
#include <QX11Info>
#include <QMainWindow>
#include <QThread>
#include <QSocketNotifier>

#include <xcb/composite.h>

#include "compositor_x11_renderwindow.hpp"
#include "compositor_x11_renderclient.hpp"
#include "compositor_x11_uisurface.hpp"
#include "compositor_x11_utils.hpp"

#include <vlc_cxx_helpers.hpp>

#include "qt.hpp"

//blur behind for KDE
#define _KDE_NET_WM_BLUR_BEHIND_REGION_NAME "_KDE_NET_WM_BLUR_BEHIND_REGION"

using namespace vlc;

RenderTask::RenderTask(qt_intf_t *intf, xcb_connection_t* conn, xcb_drawable_t wid,
                       QMutex& pictureLock, QObject *parent)
    : QObject(parent)
    , m_intf(intf)
    , m_conn(conn)
    , m_pictureLock(pictureLock)
    , m_drawingarea(m_conn)
    , m_wid(wid)
{
    assert(conn);
    assert(m_intf);
    assert(m_wid);
    connect(this, &RenderTask::requestRefreshInternal, this, &RenderTask::render);
}

RenderTask::~RenderTask()
{
}

void RenderTask::render(unsigned int requestId)
{
    if (requestId != m_refreshRequestId)
        return;
    if (!m_visible)
        return;

    assert(m_interfaceClient != nullptr);

    xcb_flush(m_conn);
    xcb_render_picture_t drawingarea = getBackTexture();

    if (m_hasAcrylic)
    {
        //clear screen
        xcb_render_color_t clear = { 0x0000, 0x0000, 0x0000, 0x0000 };
        xcb_rectangle_t rect = {0, 0, 0xFFFF, 0xFFFF};
        xcb_render_fill_rectangles(m_conn, XCB_RENDER_PICT_OP_SRC, drawingarea,
                                   clear, 1, &rect);
    }


    {
        QMutexLocker lock(&m_pictureLock);
        if (m_videoEmbed)
        {
            assert(m_videoClient);
            xcb_render_picture_t pic = m_videoClient->getPicture();
            if (pic)
            {
                xcb_render_composite(m_conn, XCB_RENDER_PICT_OP_SRC,
                                     pic, 0, drawingarea,
                                     0,0,0,0,
                                     m_videoPosition.x(),m_videoPosition.y(),
                                     m_videoPosition.width(), m_videoPosition.height());
            }
        }

        xcb_render_picture_t pic = m_interfaceClient->getPicture();
        if (pic)
        {
            xcb_render_composite(m_conn, XCB_RENDER_PICT_OP_OVER,
                                 pic, 0, drawingarea,
                                 0,0,0,0,
                                 0, 0, m_interfaceSize.width(), m_interfaceSize.height());
        }

    } //picture lock scope

    xcb_clear_area(m_conn, 0, m_wid,
                   0, 0, 0, 0);

    m_refreshRequestId++;
}

void RenderTask::onWindowSizeChanged(const QSize& newSize)
{
    if (m_renderSize.isValid()
            && newSize.width() <= m_renderSize.width()
            && newSize.height() <= m_renderSize.height())
        return;

    if (!m_renderSize.isValid())
    {
        m_renderSize = newSize;
    }
    else
    {
        m_renderSize.setWidth(vlc_align(newSize.width(), 128));
        m_renderSize.setHeight(vlc_align(newSize.height(), 128));
    }
    m_resizeRequested = true;
}

void RenderTask::requestRefresh()
{
    emit requestRefreshInternal(m_refreshRequestId, {});
}

void RenderTask::onInterfaceSurfaceChanged(CompositorX11RenderClient* surface)
{
    m_interfaceClient = surface;
}

void RenderTask::onVideoSurfaceChanged(CompositorX11RenderClient* surface)
{
    m_videoClient = surface;
}

void RenderTask::onRegisterVideoWindow(unsigned int surface)
{
    m_videoEmbed = (surface != 0);
}

void RenderTask::onVideoPositionChanged(const QRect& position)
{
    if (m_videoPosition == position)
        return;
    m_videoPosition = position;
    emit requestRefreshInternal(m_refreshRequestId, {});
}

void RenderTask::onInterfaceSizeChanged(const QSize& size)
{
    if (m_interfaceSize == size)
        return;
    m_interfaceSize = size;
}

void RenderTask::onVisibilityChanged(bool visible)
{
    m_visible = visible;
}

void RenderTask::onAcrylicChanged(bool enabled)
{
    m_hasAcrylic = enabled;
}

xcb_render_picture_t RenderTask::getBackTexture()
{
    if (m_drawingarea && !m_resizeRequested)
        return m_drawingarea.get();

    xcb_void_cookie_t voidCookie;
    auto err = wrap_cptr<xcb_generic_error_t>(nullptr);
    xcb_generic_error_t* rawerror = NULL;

    xcb_get_window_attributes_cookie_t attrCookie = xcb_get_window_attributes(m_conn, m_wid);

    auto attrReply = wrap_cptr(xcb_get_window_attributes_reply(m_conn, attrCookie, &rawerror));
    if (rawerror)
    {
        msg_Warn(m_intf, " error: getting window attributes xcb_get_window_attributes_reply %u", rawerror->error_code);
        free(rawerror);
        return 0;
    }

    xcb_visualid_t visual = attrReply->visual;

    uint8_t depth;
    xcb_render_pictformat_t fmt;
    findVisualFormat(m_conn, visual, &fmt, &depth);

    PixmapPtr  background{ m_conn};
    background.generateId();
    voidCookie =  xcb_create_pixmap_checked(m_conn, depth, background.get(), m_wid, m_renderSize.width(), m_renderSize.height());
    err.reset(xcb_request_check(m_conn, voidCookie));
    if (err)
    {
        msg_Warn(m_intf, " error: creating xcb_create_pixmap %u", err->error_code);
        return 0;
    }

    uint32_t attributeList[] = {background.get()};
    voidCookie = xcb_change_window_attributes_checked(m_conn, m_wid, XCB_CW_BACK_PIXMAP, attributeList);
    err.reset(xcb_request_check(m_conn, voidCookie));
    if (err)
    {
        msg_Warn(m_intf, " error: xcb_change_window_attributes_checked %u", err->error_code);
        return 0;
    }

    m_drawingarea.generateId();
    xcb_render_create_picture_checked(m_conn, m_drawingarea.get(), background.get(), fmt, 0, nullptr);
    err.reset(xcb_request_check(m_conn, voidCookie));
    if (err)
    {
        msg_Warn(m_intf, " error: xcb_change_window_attributes_checked %u", err->error_code);
        return 0;
    }

    m_resizeRequested = false;
    return m_drawingarea.get();
}

//X11 damage listenner

X11DamageObserver::X11DamageObserver(qt_intf_t* intf, xcb_connection_t* conn, QObject* parent)
    : QObject(parent)
    , m_intf(intf)
    , m_conn(conn)
{
}

bool X11DamageObserver::init()
{
    const xcb_query_extension_reply_t *reply = xcb_get_extension_data(m_conn, &xcb_damage_id);
    if (!reply || !reply->present)
        return false;
    m_xdamageBaseEvent = reply->first_event;
    m_connFd = xcb_get_file_descriptor(m_conn);
    return true;
}

#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
//can't use QOverload with private signals
template<class T>
static auto privateOverload(void (QSocketNotifier::* s)( QSocketDescriptor,QSocketNotifier::Type, T) )
{
    return s;
}
#endif

void X11DamageObserver::start()
{
    //listen to the x11 socket instead of blocking
    m_socketNotifier = new QSocketNotifier(m_connFd, QSocketNotifier::Read, this);
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    connect(m_socketNotifier, privateOverload(&QSocketNotifier::activated),
            this, &X11DamageObserver::onEvent);
#else
    connect(m_socketNotifier, &QSocketNotifier::activated, this, &X11DamageObserver::onEvent);
#endif
}

bool X11DamageObserver::onRegisterSurfaceDamage(unsigned int wid)
{
    if (m_dammage != 0)
    {
        xcb_damage_destroy(m_conn, m_dammage);
        m_dammage = 0;
    }
    if (wid != 0)
    {
        m_dammage = xcb_generate_id(m_conn);
        xcb_void_cookie_t cookie = xcb_damage_create_checked(m_conn, m_dammage, wid, XCB_DAMAGE_REPORT_LEVEL_RAW_RECTANGLES);
        auto err = wrap_cptr(xcb_request_check(m_conn, cookie));
        if (err)
        {
            msg_Warn(m_intf, "error while registering damage on surface");
            return false;
        }
    }

    return true;
}

void X11DamageObserver::onEvent()
{
    bool isRefreshNeeded = false;
    auto event = wrap_cptr<xcb_generic_event_t>(nullptr);
    while ((event = wrap_cptr(xcb_poll_for_event(m_conn))) != nullptr)
    {
        if (event->response_type == m_xdamageBaseEvent + XCB_DAMAGE_NOTIFY)
        {
            xcb_damage_notify_event_t* damageEvent = reinterpret_cast<xcb_damage_notify_event_t*>(event.get());
            if (damageEvent->damage != m_dammage)
                continue;
            isRefreshNeeded = true;
        }
    }

    if (isRefreshNeeded)
        emit needRefresh();
}

//// CompositorX11RenderWindow

CompositorX11RenderWindow::CompositorX11RenderWindow(qt_intf_t* p_intf, xcb_connection_t* conn, bool useCSD, QWidget* parent)
    : QMainWindow(parent)
    , m_intf(p_intf)
    , m_conn(conn)
{
    setAcceptDrops(true);
    setAttribute(Qt::WA_NativeWindow);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_MouseTracking);

    if (useCSD)
        setWindowFlag(Qt::FramelessWindowHint);

    m_stable = new DummyNativeWidget(this);
    m_stable->winId();

    setCentralWidget(m_stable);

    winId();
    show();

    m_window = window()->windowHandle();
    m_wid = winId();
}

CompositorX11RenderWindow::~CompositorX11RenderWindow()
{
    stopRendering();

    removeEventFilter(this);
    m_window->removeEventFilter(this);
}

bool CompositorX11RenderWindow::init()
{
    m_damageObserver = new X11DamageObserver(m_intf, m_conn);
    bool ret = m_damageObserver->init();
    if (!ret)
    {
        delete m_damageObserver;
        m_damageObserver = nullptr;
        msg_Warn(m_intf, "can't initialize X11 damage");
        return false;
    }

    xcb_connection_t* qtConn = QX11Info::connection();

    //check if KDE "acrylic" effect is available
    xcb_atom_t blurBehindAtom = getInternAtom(qtConn, _KDE_NET_WM_BLUR_BEHIND_REGION_NAME);
    if (blurBehindAtom != XCB_ATOM_NONE)
    {
        uint32_t val = 0;
        xcb_change_property(qtConn, XCB_PROP_MODE_REPLACE, m_wid,
                            blurBehindAtom, XCB_ATOM_CARDINAL, 32, 1, &val);
        m_hasAcrylic = true;
    }

    //install event filters
    installEventFilter(this);
    m_window->installEventFilter(this);

    return true;
}

bool CompositorX11RenderWindow::startRendering()
{
    assert(m_interfaceWindow);

    //Rendering thread
    m_renderTask = new RenderTask(m_intf, m_conn, m_stable->effectiveWinId(), m_pictureLock);
    m_renderThread = new QThread(this);

    m_renderTask->moveToThread(m_renderThread);
    connect(m_renderThread, &QThread::finished, m_renderTask, &QObject::deleteLater);

    connect(m_interfaceWindow, &CompositorX11UISurface::updated, m_renderTask, &RenderTask::requestRefresh);
    connect(m_interfaceWindow, &CompositorX11UISurface::sizeChanged, m_renderTask, &RenderTask::onInterfaceSizeChanged);

    connect(this, &CompositorX11RenderWindow::windowSizeChanged, m_renderTask, &RenderTask::onWindowSizeChanged);
    connect(this, &CompositorX11RenderWindow::requestUIRefresh, m_renderTask, &RenderTask::requestRefresh);
    connect(this, &CompositorX11RenderWindow::visiblityChanged, m_renderTask, &RenderTask::onVisibilityChanged);
    connect(this, &CompositorX11RenderWindow::videoPositionChanged, m_renderTask, &RenderTask::onVideoPositionChanged);
    connect(this, &CompositorX11RenderWindow::registerVideoWindow, m_renderTask, &RenderTask::onRegisterVideoWindow);
    connect(this, &CompositorX11RenderWindow::videoSurfaceChanged, m_renderTask, &RenderTask::onVideoSurfaceChanged, Qt::BlockingQueuedConnection);

    //pass initial values
    m_renderTask->onInterfaceSurfaceChanged(m_interfaceClient.get());
    m_renderTask->onVideoSurfaceChanged(m_videoClient.get());
    m_renderTask->onWindowSizeChanged(size() * devicePixelRatioF());
    m_renderTask->onAcrylicChanged(m_hasAcrylic);

    //use the same thread as the rendering thread, neither tasks are blocking.
    m_damageObserver->moveToThread(m_renderThread);
    connect(m_renderThread, &QThread::started, m_damageObserver, &X11DamageObserver::start);
    connect(this, &CompositorX11RenderWindow::registerVideoWindow, m_damageObserver,  &X11DamageObserver::onRegisterSurfaceDamage);
    connect(m_damageObserver, &X11DamageObserver::needRefresh, m_renderTask, &RenderTask::requestRefresh);
    connect(m_renderThread, &QThread::finished, m_damageObserver, &QObject::deleteLater);

    //start the rendering thread
    m_renderThread->start();

    return true;
}

void CompositorX11RenderWindow::stopRendering()
{
    if (m_renderThread)
    {
        m_renderThread->quit();
        m_renderThread->wait();
        delete m_renderThread;
        m_renderThread = nullptr;
    }
    m_videoClient.reset();
    m_videoWindow = nullptr;
    m_interfaceClient.reset();
    m_interfaceWindow = nullptr;
}

void CompositorX11RenderWindow::resetClientPixmaps()
{
    QMutexLocker lock(&m_pictureLock);
    xcb_flush(QX11Info::connection());
    //reset and recreate the clients surfaces
    if (m_interfaceClient)
    {
        m_interfaceClient->resetPixmap();
        m_interfaceClient->getPicture();
    }
    if (m_videoClient)
    {
        m_videoClient->resetPixmap();
        m_interfaceClient->getPicture();
    }
}

bool CompositorX11RenderWindow::eventFilter(QObject* obj, QEvent* event)
{
    bool ret = false;
    bool needRefresh = false;

    //event on the window
    if (obj == m_window)
    {
        //window may get resized without the widget knowing about it
        if (event->type() == QEvent::Resize)
        {
            auto resizeEvent = static_cast<QResizeEvent*>(event);
            if (m_interfaceWindow)
                m_interfaceWindow->handleWindowEvent(event);
            resetClientPixmaps();
            emit windowSizeChanged(resizeEvent->size() * devicePixelRatioF());
            needRefresh = true;
        }
    }
    else
    {
        assert(obj == this);
        if (event->type() == QEvent::Resize)
            return false;

        if (m_interfaceWindow)
            ret =  m_interfaceWindow->handleWindowEvent(event);

        switch (event->type())
        {
        case QEvent::Expose:
        {
            resetClientPixmaps();
            needRefresh = true;
            break;
        }
        case QEvent::Show:
        {
            resetClientPixmaps();
            needRefresh = true;
            emit visiblityChanged(true);
            break;
        }
        case QEvent::Hide:
            emit visiblityChanged(false);
            break;
        default:
            break;
        }
    }

    if (needRefresh)
        emit requestUIRefresh();

    return ret;
}

void CompositorX11RenderWindow::setVideoPosition(const QPoint& position)
{
    if (m_videoWindow && m_videoClient)
    {
        m_videoPosition.moveTopLeft(position);
        emit videoPositionChanged(m_videoPosition);
    }
}

void CompositorX11RenderWindow::setVideoSize(const QSize& size)
{
    if (m_videoWindow && m_videoClient)
    {
        m_videoWindow->resize(size);
        {
            QMutexLocker lock(&m_pictureLock);
            xcb_flush(QX11Info::connection());
            //reset and recreate the clients surfaces
            m_videoClient->resetPixmap();
            m_videoClient->getPicture();
        }
        m_videoPosition.setSize(size * devicePixelRatioF());
        emit videoPositionChanged(m_videoPosition);
    }
}

void CompositorX11RenderWindow::setVideoWindow( QWindow* window)
{
    //ensure Qt x11 pending operation have been forwarded to the server
    xcb_flush(QX11Info::connection());
    m_videoClient = std::make_unique<CompositorX11RenderClient>(m_intf, m_conn, window);
    m_videoPosition = QRect(0,0,0,0);
    m_videoWindow = window;
    emit videoSurfaceChanged(m_videoClient.get());
}

void CompositorX11RenderWindow::enableVideoWindow()
{
    emit registerVideoWindow(m_videoWindow->winId());
}

void CompositorX11RenderWindow::disableVideoWindow()
{
    emit registerVideoWindow(0);
}

void CompositorX11RenderWindow::setInterfaceWindow(CompositorX11UISurface* window)
{
    //ensure Qt x11 pending operation have been forwarded to the server
    xcb_flush(QX11Info::connection());
    m_interfaceClient = std::make_unique<CompositorX11RenderClient>(m_intf, m_conn, window);
    m_interfaceWindow = window;

}
