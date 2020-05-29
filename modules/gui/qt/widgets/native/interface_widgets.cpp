/*****************************************************************************
 * interface_widgets.cpp : Custom widgets for the main interface
 ****************************************************************************
 * Copyright (C) 2006-2010 the VideoLAN team
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Jean-Baptiste Kempf <jb@videolan.org>
 *          Rafaël Carré <funman@videolanorg>
 *          Ilkka Ollakka <ileoo@videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * ( at your option ) any later version.
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "qt.hpp"
#include "interface_widgets.hpp"
#include "dialogs/dialogs_provider.hpp"
#include "widgets/native/customwidgets.hpp"               // qtEventToVLCKey, QVLCStackedWidget

#include <QLabel>
#include <QToolButton>
#include <QPalette>
#include <QEvent>
#include <QResizeEvent>
#include <QDate>
#include <QMenu>
#include <QWidgetAction>
#include <QDesktopWidget>
#include <QPainter>
#include <QTimer>
#include <QSlider>
#include <QBitmap>
#include <QUrl>

#if defined (QT5_HAS_X11)
# include <X11/Xlib.h>
# include <QX11Info>
# if defined(QT5_HAS_XCB)
#  include <xcb/xproto.h>
# endif
#endif
#ifdef QT5_HAS_WAYLAND
# include QPNI_HEADER
# include <QWindow>
#endif

#if defined(_WIN32)
#include <QWindow>
#include <qpa/qplatformnativeinterface.h>
#endif

#include <math.h>
#include <assert.h>

#include <vlc_vout_window.h>
#include <vlc_player.h>

/**********************************************************************
 * Video Widget. A simple frame on which video is drawn
 * This class handles resize issues
 **********************************************************************/

VideoWidget::VideoWidget( intf_thread_t *_p_i, QWidget* p_parent )
            : QFrame( p_parent ) , p_intf( _p_i )
{
    /* Set the policy to expand in both directions */
    // setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding );

    layout = new QHBoxLayout( this );
    layout->setContentsMargins( 0, 0, 0, 0 );
    stable = NULL;
    p_window = NULL;

    cursorTimer = new QTimer( this );
    cursorTimer->setSingleShot( true );
    connect( cursorTimer, SIGNAL(timeout()), this, SLOT(hideCursor()) );
    cursorTimeout = var_InheritInteger( _p_i, "mouse-hide-timeout" );

    show();
}

VideoWidget::~VideoWidget()
{
    /* Ensure we are not leaking the video output. This would crash. */
    assert( !stable );
    assert( !p_window );
}

void VideoWidget::sync( void )
{
    /* Make sure the X server has processed all requests.
     * This protects other threads using distinct connections from getting
     * the video widget window in an inconsistent states. */
#ifdef QT5_HAS_X11
    if( QX11Info::isPlatformX11() )
        XSync( QX11Info::display(), False );
#endif
}

/**
 * Request the video to avoid the conflicts
 **/
void VideoWidget::request( struct vout_window_t *p_wnd )
{
    assert( stable == NULL );
    assert( !p_window );

    /* The owner of the video window needs a stable handle (WinId). Reparenting
     * in Qt4-X11 changes the WinId of the widget, so we need to create another
     * dummy widget that stays within the reparentable widget. */
    stable = new QWidget();
    stable->setContextMenuPolicy( Qt::PreventContextMenu );
    QPalette plt = palette();
    plt.setColor( QPalette::Window, Qt::black );
    stable->setPalette( plt );
    stable->setAutoFillBackground(true);
    /* Force the widget to be native so that it gets a winId() */
    stable->setAttribute( Qt::WA_NativeWindow, true );
    /* Indicates that the widget wants to draw directly onto the screen.
       Widgets with this attribute set do not participate in composition
       management */
    /* This is currently disabled on X11 as it does not seem to improve
     * performance, but causes the video widget to be transparent... */
#if defined (QT5_HAS_X11)
    stable->setMouseTracking( true );
    setMouseTracking( true );
#elif defined(_WIN32)
    stable->setAttribute( Qt::WA_PaintOnScreen, true );
    stable->setMouseTracking( true );
    setMouseTracking( true );
    stable->setWindowFlags( Qt::Tool | Qt::FramelessWindowHint | Qt::WindowDoesNotAcceptFocus );
    stable->setAttribute( Qt::WA_ShowWithoutActivating );
#else
    stable->setAttribute( Qt::WA_PaintOnScreen, true );
#endif
    layout->addWidget( stable );

    sync();
    p_window = p_wnd;

    p_wnd->type = p_intf->p_sys->voutWindowType;
    switch( p_wnd->type )
    {
        case VOUT_WINDOW_TYPE_XID:
            p_wnd->handle.xid = stable->winId();
            p_wnd->display.x11 = NULL;
            break;
        case VOUT_WINDOW_TYPE_HWND:
            p_wnd->handle.hwnd = (void *)stable->winId();
            break;
        case VOUT_WINDOW_TYPE_NSOBJECT:
            p_wnd->handle.nsobject = (void *)stable->winId();
            break;
#ifdef QT5_HAS_WAYLAND
        case VOUT_WINDOW_TYPE_WAYLAND:
        {
            /* Ensure only the video widget is native (needed for Wayland) */
            stable->setAttribute( Qt::WA_DontCreateNativeAncestors, true);

            QWindow *window = stable->windowHandle();
            assert(window != NULL);
            window->create();

            QPlatformNativeInterface *qni = qApp->platformNativeInterface();
            assert(qni != NULL);

            p_wnd->handle.wl = static_cast<wl_surface*>(
                qni->nativeResourceForWindow(QByteArrayLiteral("surface"),
                                             window));
            p_wnd->display.wl = static_cast<wl_display*>(
                qni->nativeResourceForIntegration(QByteArrayLiteral("wl_display")));
            break;
        }
#endif
        default:
            vlc_assert_unreachable();
    }
}

QSize VideoWidget::physicalSize() const
{
#ifdef QT5_HAS_X11
    if ( QX11Info::isPlatformX11() )
    {
        Display *p_x_display = QX11Info::display();
        Window x_window = stable->winId();
        XWindowAttributes x_attributes;

        XGetWindowAttributes( p_x_display, x_window, &x_attributes );

        return QSize( x_attributes.width, x_attributes.height );
    }
#endif
#if defined(_WIN32)
    HWND hwnd;
    RECT rect;

    QWindow *window = windowHandle();
    hwnd = static_cast<HWND>(QGuiApplication::platformNativeInterface()->nativeResourceForWindow("handle", window));

    GetClientRect(hwnd, &rect);

    return QSize( rect.right, rect.bottom );
#endif

    QSize current_size = size();

#   if HAS_QT56
    /* Android-like scaling */
    current_size *= devicePixelRatioF();
#   else
    /* OSX-like scaling */
    current_size *= devicePixelRatio();
#   endif

    return current_size;
}

void VideoWidget::reportSize()
{
    if( !p_window )
        return;

    QSize size = physicalSize();
    vout_window_ReportSize( p_window, size.width(), size.height() );
}

/* Set the Widget to the correct Size */
/* Function has to be called by the parent
   Parent has to care about resizing itself */
void VideoWidget::setSize( unsigned int w, unsigned int h )
{
    /* If the size changed, resizeEvent will be called, otherwise not,
     * in which case we need to tell the vout what the size actually is
     */
    if( (unsigned)size().width() == w && (unsigned)size().height() == h )
    {
        reportSize();
        return;
    }

    resize( w, h );
    emit sizeChanged( w, h );
    /* Work-around a bug?misconception? that would happen when vout core resize
       twice to the same size and would make the vout not centered.
       This cause a small flicker.
       See #3621
     */
    if( (unsigned)size().width() == w && (unsigned)size().height() == h )
        updateGeometry();
    sync();
}

bool VideoWidget::nativeEvent( const QByteArray& eventType, void* message, long* )
{
#if defined(QT5_HAS_X11)
# if defined(QT5_HAS_XCB)
    if ( eventType == "xcb_generic_event_t" )
    {
        const xcb_generic_event_t* xev = static_cast<const xcb_generic_event_t*>( message );

        if ( xev->response_type == XCB_CONFIGURE_NOTIFY )
            reportSize();
    }
# endif
#endif
#ifdef _WIN32
    if ( eventType == "windows_generic_MSG" )
    {
        MSG* msg = static_cast<MSG*>( message );
        if ( msg->message == WM_SIZE )
            reportSize();
    }
#endif
    // Let Qt handle that event in any case
    return false;
}

void VideoWidget::resizeEvent( QResizeEvent *event )
{
    QWidget::resizeEvent( event );

    if ( p_intf->p_sys->voutWindowType == VOUT_WINDOW_TYPE_XID ||
        p_intf->p_sys->voutWindowType == VOUT_WINDOW_TYPE_HWND )
        return;
    reportSize();
}

void VideoWidget::hideCursor()
{
    setCursor( Qt::BlankCursor );
}

void VideoWidget::showCursor()
{
    setCursor( Qt::ArrowCursor );
    cursorTimer->start( cursorTimeout );
}

int VideoWidget::qtMouseButton2VLC( Qt::MouseButton qtButton )
{
    if( p_window == NULL )
        return -1;
    switch( qtButton )
    {
        case Qt::LeftButton:
            return 0;
        case Qt::RightButton:
            return 2;
        case Qt::MiddleButton:
            return 1;
        default:
            return -1;
    }
}

void VideoWidget::mouseReleaseEvent( QMouseEvent *event )
{
    int vlc_button = qtMouseButton2VLC( event->button() );
    if( vlc_button >= 0 )
    {
        vout_window_ReportMouseReleased( p_window, vlc_button );
        showCursor();
        event->accept();
    }
    else
        event->ignore();
}

void VideoWidget::mousePressEvent( QMouseEvent* event )
{
    int vlc_button = qtMouseButton2VLC( event->button() );
    if( vlc_button >= 0 )
    {
        vout_window_ReportMousePressed( p_window, vlc_button );
        showCursor();
        event->accept();
    }
    else
        event->ignore();
}

void VideoWidget::mouseMoveEvent( QMouseEvent *event )
{
    if( p_window != NULL )
    {
        QPointF current_pos = event->localPos();

#if HAS_QT56
        current_pos *= devicePixelRatioF();
#else
        current_pos *= devicePixelRatio();
#endif
        vout_window_ReportMouseMoved( p_window, current_pos.x(), current_pos.y() );
        showCursor();
        event->accept();
    }
    else
        event->ignore();
}

void VideoWidget::mouseDoubleClickEvent( QMouseEvent *event )
{
    int vlc_button = qtMouseButton2VLC( event->button() );
    if( vlc_button >= 0 )
    {
        vout_window_ReportMouseDoubleClick( p_window, vlc_button );
        showCursor();
        event->accept();
    }
    else
        event->ignore();
}


void VideoWidget::release( void )
{
    msg_Dbg( p_intf, "Video is not needed anymore" );

    if( stable )
    {
        layout->removeWidget( stable );
        stable->deleteLater();
        stable = NULL;
        p_window = NULL;
    }

    updateGeometry();
}


CoverArtLabel::CoverArtLabel( QWidget *parent, intf_thread_t *_p_i )
    : QLabel( parent ), p_intf( _p_i ), p_item( NULL )
{
    setContextMenuPolicy( Qt::ActionsContextMenu );
    connect( THEMIM, QOverload<QString>::of(&PlayerController::artChanged),
             this, QOverload<const QString&>::of(&CoverArtLabel::showArtUpdate) );

    setMinimumHeight( 128 );
    setMinimumWidth( 128 );
    setScaledContents( false );
    setAlignment( Qt::AlignCenter );

    QAction *action = new QAction( qtr( "Download cover art" ), this );
    CONNECT( action, triggered(), this, askForUpdate() );
    addAction( action );

    action = new QAction( qtr( "Add cover art from file" ), this );
    CONNECT( action, triggered(), this, setArtFromFile() );
    addAction( action );

    p_item = THEMIM->getInput();
    if( p_item )
    {
        input_item_Hold( p_item );
        showArtUpdate( p_item );
    }
    else
        showArtUpdate( "" );
}

CoverArtLabel::~CoverArtLabel()
{
    QList< QAction* > artActions = actions();
    foreach( QAction *act, artActions )
        removeAction( act );
    if ( p_item ) input_item_Release( p_item );
}

void CoverArtLabel::setItem( input_item_t *_p_item )
{
    if ( p_item ) input_item_Release( p_item );
    p_item = _p_item;
    if ( p_item ) input_item_Hold( p_item );
}

void CoverArtLabel::showArtUpdate( const QString& url )
{
    QPixmap pix;
    if( !url.isEmpty() && pix.load( url ) )
    {
        pix = pix.scaled( minimumWidth(), minimumHeight(),
                          Qt::KeepAspectRatioByExpanding,
                          Qt::SmoothTransformation );
    }
    else
    {
        pix = QPixmap( ":/noart.png" );
    }
    setPixmap( pix );
}

void CoverArtLabel::showArtUpdate( input_item_t *_p_item )
{
    /* not for me */
    if ( _p_item != p_item )
        return;

    QString url;
    if ( _p_item ) url = THEMIM->decodeArtURL( _p_item );
    showArtUpdate( url );
}

void CoverArtLabel::askForUpdate()
{
    THEMIM->requestArtUpdate( p_item, true );
}

void CoverArtLabel::setArtFromFile()
{
    if( !p_item )
        return;

    QUrl fileUrl = QFileDialog::getOpenFileUrl( this, qtr( "Choose Cover Art" ),
        p_intf->p_sys->filepath, qtr( "Image Files (*.gif *.jpg *.jpeg *.png)" ) );

    if( fileUrl.isEmpty() )
        return;

    THEMIM->setArt( p_item, fileUrl.toString() );
}

void CoverArtLabel::clear()
{
    showArtUpdate( "" );
}
