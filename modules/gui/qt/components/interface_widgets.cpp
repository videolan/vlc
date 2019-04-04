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
#include "components/interface_widgets.hpp"
#include "dialogs_provider.hpp"
#include "util/customwidgets.hpp"               // qtEventToVLCKey, QVLCStackedWidget

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

/**********************************************************************
 * Background Widget. Show a simple image background. Currently,
 * it's album art if present or cone.
 **********************************************************************/

BackgroundWidget::BackgroundWidget( intf_thread_t *_p_i )
    :QWidget( NULL ), p_intf( _p_i ), b_expandPixmap( false ), b_withart( true )
{
    /* A dark background */
    setAutoFillBackground( true );
    QPalette plt = palette();
    plt.setColor( QPalette::Active, QPalette::Window , Qt::black );
    plt.setColor( QPalette::Inactive, QPalette::Window , Qt::black );
    setPalette( plt );

    /* Init the cone art */
    updateDefaultArt( ":/logo/vlc128.png" );
    updateArt( "" );

    /* fade in animator */
    setProperty( "opacity", 1.0 );
    fadeAnimation = new QPropertyAnimation( this, "opacity", this );
    fadeAnimation->setDuration( 1000 );
    fadeAnimation->setStartValue( 0.0 );
    fadeAnimation->setEndValue( 1.0 );
    fadeAnimation->setEasingCurve( QEasingCurve::OutSine );
    CONNECT( fadeAnimation, valueChanged( const QVariant & ),
             this, update() );

    CONNECT( THEMIM->getIM(), artChanged( QString ),
             this, updateArt( const QString& ) );
    CONNECT( THEMIM->getIM(), nameChanged( const QString& ),
             this, titleUpdated( const QString & ) );
}

void BackgroundWidget::updateArt( const QString& url )
{
    if ( !url.isEmpty() )
        pixmapUrl = url;
    else
        pixmapUrl = defaultArt;
    update();
}

void BackgroundWidget::updateDefaultArt( const QString& url )
{
    if ( !url.isEmpty() )
        defaultArt = url;
    update();
}

void BackgroundWidget::titleUpdated( const QString& title )
{
    /* don't ask */
    if( var_InheritBool( p_intf, "qt-icon-change" ) && !title.isEmpty() )
    {
        int i_pos = title.indexOf( "Ki" /* Bps */ "ll", 0, Qt::CaseInsensitive );
        if( i_pos != -1 &&
            i_pos + 5 == title.indexOf( "Bi" /* directional */ "ll",
                                       i_pos, Qt::CaseInsensitive ) )
                updateDefaultArt( ":/logo/vlc128-kb.png" );
        else
                updateDefaultArt( ":/logo/vlc128.png" );
    }
}

void BackgroundWidget::showEvent( QShowEvent * e )
{
    Q_UNUSED( e );
    if ( b_withart ) fadeAnimation->start();
}

void BackgroundWidget::paintEvent( QPaintEvent *e )
{
    if ( !b_withart )
    {
        /* we just want background autofill */
        QWidget::paintEvent( e );
        return;
    }

    int i_maxwidth, i_maxheight;
    QPixmap pixmap = QPixmap( pixmapUrl );
    QPainter painter(this);

#if HAS_QT56
    qreal dpr = devicePixelRatioF();
#else
    qreal dpr = devicePixelRatio();
#endif
    pixmap.setDevicePixelRatio( dpr );

    i_maxwidth  = __MIN( maximumWidth(), width() ) - MARGIN * 2;
    i_maxheight = __MIN( maximumHeight(), height() ) - MARGIN * 2;

    painter.setOpacity( property( "opacity" ).toFloat() );

    if ( height() > MARGIN * 2 )
    {
        /* Scale down the pixmap if the widget is too small */
        if( pixmap.width() > i_maxwidth || pixmap.height() > i_maxheight )
        {
            pixmap = pixmap.scaled( i_maxwidth * dpr, i_maxheight * dpr ,
                            Qt::KeepAspectRatio, Qt::SmoothTransformation );
        }
        else
        if ( b_expandPixmap &&
             pixmap.width() < width() && pixmap.height() < height() )
        {
            pixmap = pixmap.scaled(
                    (width() - MARGIN * 2) * dpr,
                    (height() - MARGIN * 2) * dpr ,
                    Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }
        else if (dpr != 1.0)
        {
            pixmap = pixmap.scaled( pixmap.width() * dpr, pixmap.height() * dpr,
                                    Qt::KeepAspectRatio, Qt::SmoothTransformation );
        }

        painter.drawPixmap(
                MARGIN + ( i_maxwidth - ( pixmap.width() / dpr ) ) / 2,
                MARGIN + ( i_maxheight - ( pixmap.height() / dpr ) ) / 2,
                pixmap);
    }
    QWidget::paintEvent( e );
}

void BackgroundWidget::contextMenuEvent( QContextMenuEvent *event )
{
    THEDP->setPopupMenu();
    event->accept();
}

EasterEggBackgroundWidget::EasterEggBackgroundWidget( intf_thread_t *p_intf )
    : BackgroundWidget( p_intf )
{
    flakes = new QLinkedList<flake *>();
    i_rate = 2;
    i_speed = 1;
    b_enabled = false;
    timer = new QTimer( this );
    timer->setInterval( 100 );
    CONNECT( timer, timeout(), this, spawnFlakes() );
    if ( isVisible() && b_enabled ) timer->start();
    defaultArt = QString( ":/logo/vlc128-xmas.png" );
    updateArt( "" );
}

EasterEggBackgroundWidget::~EasterEggBackgroundWidget()
{
    timer->stop();
    delete timer;
    reset();
    delete flakes;
}

void EasterEggBackgroundWidget::showEvent( QShowEvent *e )
{
    if ( b_enabled ) timer->start();
    BackgroundWidget::showEvent( e );
}

void EasterEggBackgroundWidget::hideEvent( QHideEvent *e )
{
    timer->stop();
    reset();
    BackgroundWidget::hideEvent( e );
}

void EasterEggBackgroundWidget::resizeEvent( QResizeEvent *e )
{
    reset();
    BackgroundWidget::resizeEvent( e );
}

void EasterEggBackgroundWidget::animate()
{
    b_enabled = true;
    if ( isVisible() ) timer->start();
}

void EasterEggBackgroundWidget::spawnFlakes()
{
    if ( ! isVisible() ) return;

    double w = (double) width() / RAND_MAX;

    int i_spawn = ( (double) qrand() / RAND_MAX ) * i_rate;

    QLinkedList<flake *>::iterator it = flakes->begin();
    while( it != flakes->end() )
    {
        flake *current = *it;
        current->point.setY( current->point.y() + i_speed );
        if ( current->point.y() + i_speed >= height() )
        {
            delete current;
            it = flakes->erase( it );
        }
        else
            ++it;
    }

    if ( flakes->size() < MAX_FLAKES )
    for ( int i=0; i<i_spawn; i++ )
    {
        flake *f = new flake;
        f->point.setX( qrand() * w );
        f->b_fat = ( qrand() < ( RAND_MAX * .33 ) );
        flakes->append( f );
    }
    update();
}

void EasterEggBackgroundWidget::reset()
{
    while ( !flakes->isEmpty() )
        delete flakes->takeFirst();
}

void EasterEggBackgroundWidget::paintEvent( QPaintEvent *e )
{
    QPainter painter(this);

    painter.setBrush( QBrush( QColor(Qt::white) ) );
    painter.setPen( QPen(Qt::white) );

    QLinkedList<flake *>::const_iterator it = flakes->constBegin();
    while( it != flakes->constEnd() )
    {
        const flake * const f = *(it++);
        if ( f->b_fat )
        {
            /* Xsnow like :p */
            painter.drawPoint( f->point.x(), f->point.y() -1 );
            painter.drawPoint( f->point.x() + 1, f->point.y() );
            painter.drawPoint( f->point.x(), f->point.y() +1 );
            painter.drawPoint( f->point.x() - 1, f->point.y() );
        }
        else
        {
            painter.drawPoint( f->point );
        }
    }

    BackgroundWidget::paintEvent( e );
}

SpeedLabel::SpeedLabel( intf_thread_t *_p_intf, QWidget *parent )
           : QLabel( parent ), p_intf( _p_intf )
{
    tooltipStringPattern = qtr( "Current playback speed: %1\nClick to adjust" );

    /* Create the Speed Control Widget */
    speedControl = new SpeedControlWidget( p_intf, this );
    speedControlMenu = new QMenu( this );

    widgetAction = new QWidgetAction( speedControl );
    widgetAction->setDefaultWidget( speedControl );
    speedControlMenu->addAction( widgetAction );

    /* Change the SpeedRate in the Label */
    CONNECT( THEMIM->getIM(), rateChanged( float ), this, setRate( float ) );

    DCONNECT( THEMIM, inputChanged( bool ),
              speedControl, activateOnState() );

    setContentsMargins(4, 0, 4, 0);
    setRate( var_InheritFloat( THEPL, "rate" ) );
}

SpeedLabel::~SpeedLabel()
{
    widgetAction->setParent( this );
    delete speedControlMenu;
}

/****************************************************************************
 * Small right-click menu for rate control
 ****************************************************************************/

void SpeedLabel::showSpeedMenu( QPoint pos )
{
    speedControlMenu->exec( QCursor::pos() - pos
                            + QPoint( -70 + width()/2, height() ) );
}

void SpeedLabel::setRate( float rate )
{
    QString str;
    str.setNum( rate, 'f', 2 );
    str.append( "x" );
    setText( str );
    setToolTip( tooltipStringPattern.arg( str ) );
    speedControl->updateControls( rate );
}

/**********************************************************************
 * Speed control widget
 **********************************************************************/
SpeedControlWidget::SpeedControlWidget( intf_thread_t *_p_i, QWidget *_parent )
                    : QFrame( _parent ), p_intf( _p_i )
{
    QSizePolicy sizePolicy( QSizePolicy::Fixed, QSizePolicy::Maximum );
    sizePolicy.setHorizontalStretch( 0 );
    sizePolicy.setVerticalStretch( 0 );

    speedSlider = new QSlider( this );
    speedSlider->setSizePolicy( sizePolicy );
    speedSlider->setMinimumSize( QSize( 140, 20 ) );
    speedSlider->setOrientation( Qt::Horizontal );
    speedSlider->setTickPosition( QSlider::TicksBelow );

    speedSlider->setRange( -34, 34 );
    speedSlider->setSingleStep( 1 );
    speedSlider->setPageStep( 1 );
    speedSlider->setTickInterval( 17 );

    CONNECT( speedSlider, valueChanged( int ), this, updateRate( int ) );

    QToolButton *normalSpeedButton = new QToolButton( this );
    normalSpeedButton->setMaximumSize( QSize( 26, 16 ) );
    normalSpeedButton->setAutoRaise( true );
    normalSpeedButton->setText( "1x" );
    normalSpeedButton->setToolTip( qtr( "Revert to normal play speed" ) );

    CONNECT( normalSpeedButton, clicked(), this, resetRate() );

    QToolButton *slowerButton = new QToolButton( this );
    slowerButton->setMaximumSize( QSize( 26, 16 ) );
    slowerButton->setAutoRaise( true );
    slowerButton->setToolTip( tooltipL[SLOWER_BUTTON] );
    slowerButton->setIcon( QIcon( iconL[SLOWER_BUTTON] ) );
    CONNECT( slowerButton, clicked(), THEMIM->getIM(), slower() );

    QToolButton *fasterButton = new QToolButton( this );
    fasterButton->setMaximumSize( QSize( 26, 16 ) );
    fasterButton->setAutoRaise( true );
    fasterButton->setToolTip( tooltipL[FASTER_BUTTON] );
    fasterButton->setIcon( QIcon( iconL[FASTER_BUTTON] ) );
    CONNECT( fasterButton, clicked(), THEMIM->getIM(), faster() );

/*    spinBox = new QDoubleSpinBox();
    spinBox->setDecimals( 2 );
    spinBox->setMaximum( 32 );
    spinBox->setMinimum( 0.03F );
    spinBox->setSingleStep( 0.10F );
    spinBox->setAlignment( Qt::AlignRight );

    CONNECT( spinBox, valueChanged( double ), this, updateSpinBoxRate( double ) ); */

    QGridLayout* speedControlLayout = new QGridLayout( this );
    speedControlLayout->addWidget( speedSlider, 0, 0, 1, 3 );
    speedControlLayout->addWidget( slowerButton, 1, 0 );
    speedControlLayout->addWidget( normalSpeedButton, 1, 1, 1, 1, Qt::AlignRight );
    speedControlLayout->addWidget( fasterButton, 1, 2, 1, 1, Qt::AlignRight );
    //speedControlLayout->addWidget( spinBox );
    speedControlLayout->setContentsMargins( 0, 0, 0, 0 );
    speedControlLayout->setSpacing( 0 );

    lastValue = 0;

    activateOnState();
}

void SpeedControlWidget::activateOnState()
{
    speedSlider->setEnabled( THEMIM->getIM()->hasInput() );
    //spinBox->setEnabled( THEMIM->getIM()->hasInput() );
}

void SpeedControlWidget::updateControls( float rate )
{
    if( speedSlider->isSliderDown() )
    {
        //We don't want to change anything if the user is using the slider
        return;
    }

    double value = 17 * log( rate ) / log( 2. );
    int sliderValue = (int) ( ( value > 0 ) ? value + .5 : value - .5 );

    if( sliderValue < speedSlider->minimum() )
    {
        sliderValue = speedSlider->minimum();
    }
    else if( sliderValue > speedSlider->maximum() )
    {
        sliderValue = speedSlider->maximum();
    }
    lastValue = sliderValue;

    speedSlider->setValue( sliderValue );
    //spinBox->setValue( rate );
}

void SpeedControlWidget::updateRate( int sliderValue )
{
    if( sliderValue == lastValue )
        return;
    lastValue = sliderValue;

    double speed = pow( 2, (double)sliderValue / 17 );

    THEMIM->getIM()->setRate(speed);
    //spinBox->setValue( var_InheritFloat( THEPL, "rate" ) );
}

void SpeedControlWidget::updateSpinBoxRate( double r )
{
    var_SetFloat( THEPL, "rate", r );
}

void SpeedControlWidget::resetRate()
{
    THEMIM->getIM()->setRate( 1.0f );
}

CoverArtLabel::CoverArtLabel( QWidget *parent, intf_thread_t *_p_i )
    : QLabel( parent ), p_intf( _p_i ), p_item( NULL )
{
    setContextMenuPolicy( Qt::ActionsContextMenu );
    CONNECT( THEMIM->getIM(), artChanged( input_item_t * ),
             this, showArtUpdate( input_item_t * ) );

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

    p_item = THEMIM->currentInputItem();
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
    if ( _p_item ) url = THEMIM->getIM()->decodeArtURL( _p_item );
    showArtUpdate( url );
}

void CoverArtLabel::askForUpdate()
{
    THEMIM->getIM()->requestArtUpdate( p_item, true );
}

void CoverArtLabel::setArtFromFile()
{
    if( !p_item )
        return;

    QUrl fileUrl = QFileDialog::getOpenFileUrl( this, qtr( "Choose Cover Art" ),
        p_intf->p_sys->filepath, qtr( "Image Files (*.gif *.jpg *.jpeg *.png)" ) );

    if( fileUrl.isEmpty() )
        return;

    THEMIM->getIM()->setArt( p_item, fileUrl.toString() );
}

void CoverArtLabel::clear()
{
    showArtUpdate( "" );
}

TimeLabel::TimeLabel( intf_thread_t *_p_intf, TimeLabel::Display _displayType  )
    : ClickableQLabel()
    , p_intf( _p_intf )
    , cachedPos( -1 )
    , cachedTime( 0 )
    , cachedLength( 0 )
    , displayType( _displayType )
{
    b_remainingTime = false;
    if( _displayType != TimeLabel::Elapsed )
        b_remainingTime = getSettings()->value( "MainWindow/ShowRemainingTime", false ).toBool();
    switch( _displayType ) {
        case TimeLabel::Elapsed:
            setText( " --:-- " );
            setToolTip( qtr("Elapsed time") );
            break;
        case TimeLabel::Remaining:
            setText( " --:-- " );
            setToolTip( qtr("Total/Remaining time")
                        + QString("\n-")
                        + qtr("Click to toggle between total and remaining time")
                      );
            break;
        case TimeLabel::Both:
            setText( " --:--/--:-- " );
            setToolTip( QString( "- " )
                + qtr( "Click to toggle between elapsed and remaining time" )
                + QString( "\n- " )
                + qtr( "Double click to jump to a chosen time position" ) );
            break;
    }
    setAlignment( Qt::AlignRight | Qt::AlignVCenter );

    CONNECT( THEMIM->getIM(), seekRequested( float ),
             this, setDisplayPosition( float ) );

    CONNECT( THEMIM->getIM(), positionUpdated( float, vlc_tick_t, int ),
              this, setDisplayPosition( float, vlc_tick_t, int ) );

    connect( this, SIGNAL( broadcastRemainingTime( bool ) ),
         THEMIM->getIM(), SIGNAL( remainingTimeChanged( bool ) ) );

    CONNECT( THEMIM->getIM(), remainingTimeChanged( bool ),
              this, setRemainingTime( bool ) );

    setStyleSheet( "QLabel { padding-left: 4px; padding-right: 4px; }" );
}

void TimeLabel::setRemainingTime( bool remainingTime )
{
    if( displayType != TimeLabel::Elapsed )
    {
        b_remainingTime = remainingTime;
        refresh();
    }
}

void TimeLabel::refresh()
{
    setDisplayPosition( cachedPos, cachedTime, cachedLength );
}

void TimeLabel::setDisplayPosition( float pos, vlc_tick_t t, int length )
{
    cachedPos = pos;
    if( pos == -1.f )
    {
        setMinimumSize( QSize( 0, 0 ) );
        if( displayType == TimeLabel::Both )
            setText( "--:--/--:--" );
        else
            setText( "--:--" );
        return;
    }

    int time = SEC_FROM_VLC_TICK(t);

    secstotimestr( psz_length, length );
    secstotimestr( psz_time, ( b_remainingTime && length ) ? length - time
                                                           : time );

    // compute the minimum size that will be required for the psz_length
    // and use it to enforce a minimal size to avoid "dancing" widgets
    QSize minsize( 0, 0 );
    if ( length > 0 )
    {
        QMargins margins = contentsMargins();
        minsize += QSize(
                  fontMetrics().size( 0, QString( psz_length ), 0, 0 ).width(),
                  sizeHint().height()
                );
        minsize += QSize( margins.left() + margins.right() + 8, 0 ); /* +padding */

        if ( b_remainingTime )
            minsize += QSize( fontMetrics().size( 0, "-", 0, 0 ).width(), 0 );
    }

    switch( displayType )
    {
        case TimeLabel::Elapsed:
            setMinimumSize( minsize );
            setText( QString( psz_time ) );
            break;
        case TimeLabel::Remaining:
            if( b_remainingTime )
            {
                setMinimumSize( minsize );
                setText( QString("-") + QString( psz_time ) );
            }
            else
            {
                setMinimumSize( QSize( 0, 0 ) );
                setText( QString( psz_length ) );
            }
            break;
        case TimeLabel::Both:
        default:
            QString timestr = QString( "%1%2/%3" )
            .arg( QString( (b_remainingTime && length) ? "-" : "" ) )
            .arg( QString( psz_time ) )
            .arg( QString( ( !length && time ) ? "--:--" : psz_length ) );

            setText( timestr );
            break;
    }
    cachedLength = length;
    cachedTime = t;
}

void TimeLabel::setDisplayPosition( float pos )
{
    vlc_tick_t time = vlc_tick_from_sec(pos * cachedLength);
    setDisplayPosition( pos, time, cachedLength );
}

void TimeLabel::toggleTimeDisplay()
{
    b_remainingTime = !b_remainingTime;
    getSettings()->setValue( "MainWindow/ShowRemainingTime", b_remainingTime );
    emit broadcastRemainingTime( b_remainingTime );
}

