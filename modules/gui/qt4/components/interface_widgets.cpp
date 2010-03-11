/*****************************************************************************
 * interface_widgets.cpp : Custom widgets for the main interface
 ****************************************************************************
 * Copyright (C) 2006-2010 the VideoLAN team
 * $Id$
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

#include "components/interface_widgets.hpp"

#include "menus.hpp"             /* Popup menu on bgWidget */

#include <vlc_vout.h>

#include <QLabel>
#include <QToolButton>
#include <QPalette>
#include <QResizeEvent>
#include <QDate>
#include <QMenu>
#include <QWidgetAction>
#include <QDesktopWidget>
#include <QPainter>
#include <QTimer>
#include <QSlider>
#include <QBitmap>

#ifdef Q_WS_X11
# include <X11/Xlib.h>
# include <qx11info_x11.h>
static void videoSync( void )
{
    /* Make sure the X server has processed all requests.
     * This protects other threads using distinct connections from getting
     * the video widget window in an inconsistent states. */
    XSync( QX11Info::display(), False );
}
#else
# define videoSync() (void)0
#endif

#include <math.h>
#include <assert.h>

class ReparentableWidget : public QWidget
{
private:
    VideoWidget *owner;
public:
    ReparentableWidget( VideoWidget *owner ) : owner( owner )
    {
    }

protected:
    void keyPressEvent( QKeyEvent *e )
    {
        emit owner->keyPressed( e );
    }
};

/**********************************************************************
 * Video Widget. A simple frame on which video is drawn
 * This class handles resize issues
 **********************************************************************/

VideoWidget::VideoWidget( intf_thread_t *_p_i )
    : QFrame( NULL )
      , p_intf( _p_i )
      , reparentable( NULL )
{
    /* Set the policy to expand in both directions */
    // setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding );

    layout = new QHBoxLayout( this );
    layout->setContentsMargins( 0, 0, 0, 0 );
    setLayout( layout );
}

VideoWidget::~VideoWidget()
{
    /* Ensure we are not leaking the video output. This would crash. */
    assert( reparentable == NULL );
}

/**
 * Request the video to avoid the conflicts
 **/
WId VideoWidget::request( int *pi_x, int *pi_y,
                          unsigned int *pi_width, unsigned int *pi_height,
                          bool b_keep_size )
{
    msg_Dbg( p_intf, "Video was requested %i, %i", *pi_x, *pi_y );

    if( reparentable != NULL )
    {
        msg_Dbg( p_intf, "embedded video already in use" );
        return NULL;
    }
    if( b_keep_size )
    {
        *pi_width  = size().width();
        *pi_height = size().height();
    }

    /* The Qt4 UI needs a fixed a widget ("this"), so that the parent layout is
     * not messed up when we the video is reparented. Hence, we create an extra
     * reparentable widget, that will be within the VideoWidget in windowed
     * mode, and within the root window (NULL parent) in full-screen mode.
     */
    reparentable = new ReparentableWidget( this );
    QLayout *innerLayout = new QHBoxLayout( reparentable );
    innerLayout->setContentsMargins( 0, 0, 0, 0 );

    /* The owner of the video window needs a stable handle (WinId). Reparenting
     * in Qt4-X11 changes the WinId of the widget, so we need to create another
     * dummy widget that stays within the reparentable widget. */
    QWidget *stable = new QWidget();
    QPalette plt = palette();
    plt.setColor( QPalette::Window, Qt::black );
    stable->setPalette( plt );
    stable->setAutoFillBackground(true);
    /* Indicates that the widget wants to draw directly onto the screen.
       Widgets with this attribute set do not participate in composition
       management */
    stable->setAttribute( Qt::WA_PaintOnScreen, true );

    innerLayout->addWidget( stable );

    layout->addWidget( reparentable );

#ifdef Q_WS_X11
    /* HACK: Only one X11 client can subscribe to mouse button press events.
     * VLC currently handles those in the video display.
     * Force Qt4 to unsubscribe from mouse press and release events. */
    Display *dpy = QX11Info::display();
    Window w = stable->winId();
    XWindowAttributes attr;

    XGetWindowAttributes( dpy, w, &attr );
    attr.your_event_mask &= ~(ButtonPressMask|ButtonReleaseMask);
    XSelectInput( dpy, w, attr.your_event_mask );
#endif
    videoSync();
#ifndef NDEBUG
    msg_Dbg( p_intf, "embedded video ready (handle %p)",
             (void *)stable->winId() );
#endif
    return stable->winId();
}

/* Set the Widget to the correct Size */
/* Function has to be called by the parent
   Parent has to care about resizing itself */
void VideoWidget::SetSizing( unsigned int w, unsigned int h )
{
    if (reparentable->windowState() & Qt::WindowFullScreen )
        return;
    if( !isVisible() ) show();
    resize( w, h );
    emit sizeChanged( w, h );
    videoSync();
}

void VideoWidget::SetFullScreen( bool b_fs )
{
    const Qt::WindowStates curstate = reparentable->windowState();
    Qt::WindowStates newstate = curstate;
    Qt::WindowFlags  newflags = reparentable->windowFlags();


    if( b_fs )
    {
        newstate |= Qt::WindowFullScreen;
        newflags |= Qt::WindowStaysOnTopHint;
    }
    else
    {
        newstate &= ~Qt::WindowFullScreen;
        newflags &= ~Qt::WindowStaysOnTopHint;
    }
    if( newstate == curstate )
        return; /* no changes needed */

    if( b_fs )
    {   /* Go full-screen */
        int numscreen = var_InheritInteger( p_intf, "qt-fullscreen-screennumber" );
        /* if user hasn't defined screennumber, or screennumber that is bigger
         * than current number of screens, take screennumber where current interface
         * is
         */
        if( numscreen == -1 || numscreen > QApplication::desktop()->numScreens() )
            numscreen = QApplication::desktop()->screenNumber( p_intf->p_sys->p_mi );

        QRect screenres = QApplication::desktop()->screenGeometry( numscreen );

        reparentable->setParent( NULL, newflags );
        reparentable->setWindowState( newstate );
        /* To be sure window is on proper-screen in xinerama */
        if( !screenres.contains( reparentable->pos() ) )
        {
            msg_Dbg( p_intf, "Moving video to correct screen");
            reparentable->move( QPoint( screenres.x(), screenres.y() ) );
        }
        reparentable->show();
    }
    else
    {   /* Go windowed */
        reparentable->setWindowFlags( newflags );
        reparentable->setWindowState( newstate );
        layout->addWidget( reparentable );
    }
    videoSync();
}

void VideoWidget::release( void )
{
    msg_Dbg( p_intf, "Video is not needed anymore" );
    //layout->removeWidget( reparentable );

#ifdef WIN32
    /* Come back to default thumbnail for Windows 7 taskbar */
    LPTASKBARLIST3 p_taskbl;

    CoInitialize( 0 );

    if( S_OK == CoCreateInstance( &clsid_ITaskbarList,
                NULL, CLSCTX_INPROC_SERVER,
                &IID_ITaskbarList3,
                (void **)&p_taskbl) )
    {
        p_taskbl->vt->HrInit(p_taskbl);

        HWND hroot = GetAncestor(reparentable->winId(),GA_ROOT);

        if (S_OK != p_taskbl->vt->SetThumbnailClip(p_taskbl, hroot, NULL))
            msg_Err(p_intf, "SetThumbNailClip failed");
        msg_Err(p_intf, "Releasing taskbar | root handle = %08x", hroot);
        p_taskbl->vt->Release(p_taskbl);
    }
    CoUninitialize();

#endif

    delete reparentable;
    reparentable = NULL;
    updateGeometry();
    hide();
}

/**********************************************************************
 * Background Widget. Show a simple image background. Currently,
 * it's album art if present or cone.
 **********************************************************************/

BackgroundWidget::BackgroundWidget( intf_thread_t *_p_i )
                 :QWidget( NULL ), p_intf( _p_i ), b_expandPixmap( false )
{
    /* A dark background */
    setAutoFillBackground( true );
    QPalette plt = palette();
    plt.setColor( QPalette::Active, QPalette::Window , Qt::black );
    plt.setColor( QPalette::Inactive, QPalette::Window , Qt::black );
    setPalette( plt );

    /* Init the cone art */
    updateArt( "" );

    CONNECT( THEMIM->getIM(), artChanged( QString ),
             this, updateArt( const QString& ) );
}

void BackgroundWidget::updateArt( const QString& url )
{
    if ( !url.isEmpty() )
    {
        pixmapUrl = url;
    }
    else
    {   /* Xmas joke */
        if( QDate::currentDate().dayOfYear() >= 354 )
            pixmapUrl = QString( ":/logo/vlc128-christmas.png" );
        else
            pixmapUrl = QString( ":/logo/vlc128.png" );
    }
    update();
}

void BackgroundWidget::paintEvent( QPaintEvent *e )
{
    int i_maxwidth, i_maxheight;
    QPixmap pixmap = QPixmap( pixmapUrl );
    QPainter painter(this);
    QBitmap pMask;
    float f_alpha = 1.0;

    i_maxwidth = std::min( maximumWidth(), width() ) - MARGIN * 2;
    i_maxheight = std::min( maximumHeight(), height() ) - MARGIN * 2;

    if ( height() > MARGIN * 2 )
    {
        /* Scale down the pixmap if the widget is too small */
        if( pixmap.width() > i_maxwidth || pixmap.height() > i_maxheight )
        {
            pixmap = pixmap.scaled( i_maxwidth, i_maxheight,
                            Qt::KeepAspectRatio, Qt::SmoothTransformation );
        }
        else
        if ( b_expandPixmap &&
             pixmap.width() < width() && pixmap.height() < height() )
        {
            /* Scale up the pixmap to fill widget's size */
            f_alpha = ( (float) pixmap.height() / (float) height() );
            pixmap = pixmap.scaled(
                    width() - MARGIN * 2,
                    height() - MARGIN * 2,
                    Qt::KeepAspectRatio,
                    ( f_alpha < .2 )? /* Don't waste cpu when not visible */
                        Qt::SmoothTransformation:
                        Qt::FastTransformation
                    );
            /* Non agressive alpha compositing when sizing up */
            pMask = QBitmap( pixmap.width(), pixmap.height() );
            pMask.fill( QColor::fromRgbF( 1.0, 1.0, 1.0, f_alpha ) );
            pixmap.setMask( pMask );
        }

        painter.drawPixmap(
                MARGIN + ( i_maxwidth - pixmap.width() ) /2,
                MARGIN + ( i_maxheight - pixmap.height() ) /2,
                pixmap);
    }
    QWidget::paintEvent( e );
}

void BackgroundWidget::contextMenuEvent( QContextMenuEvent *event )
{
    QVLCMenu::PopupMenu( p_intf, true );
    event->accept();
}

#if 0
#include <QPushButton>
#include <QHBoxLayout>

/**********************************************************************
 * Visualization selector panel
 **********************************************************************/
VisualSelector::VisualSelector( intf_thread_t *_p_i ) :
                                QFrame( NULL ), p_intf( _p_i )
{
    QHBoxLayout *layout = new QHBoxLayout( this );
    layout->setMargin( 0 );
    QPushButton *prevButton = new QPushButton( "Prev" );
    QPushButton *nextButton = new QPushButton( "Next" );
    layout->addWidget( prevButton );
    layout->addWidget( nextButton );

    layout->addStretch( 10 );
    layout->addWidget( new QLabel( qtr( "Current visualization" ) ) );

    current = new QLabel( qtr( "None" ) );
    layout->addWidget( current );

    BUTTONACT( prevButton, prev() );
    BUTTONACT( nextButton, next() );

    setLayout( layout );
    setMaximumHeight( 35 );
}

VisualSelector::~VisualSelector()
{}

void VisualSelector::prev()
{
    char *psz_new = aout_VisualPrev( p_intf );
    if( psz_new )
    {
        current->setText( qfu( psz_new ) );
        free( psz_new );
    }
}

void VisualSelector::next()
{
    char *psz_new = aout_VisualNext( p_intf );
    if( psz_new )
    {
        current->setText( qfu( psz_new ) );
        free( psz_new );
    }
}
#endif

SpeedLabel::SpeedLabel( intf_thread_t *_p_intf, QWidget *parent )
           : QLabel( parent ), p_intf( _p_intf )
{
    setToolTip( qtr( "Current playback speed.\nClick to adjust" ) );

    /* Create the Speed Control Widget */
    speedControl = new SpeedControlWidget( p_intf, this );
    speedControlMenu = new QMenu( this );

    QWidgetAction *widgetAction = new QWidgetAction( speedControl );
    widgetAction->setDefaultWidget( speedControl );
    speedControlMenu->addAction( widgetAction );

    /* Change the SpeedRate in the Status Bar */
    CONNECT( THEMIM->getIM(), rateChanged( float ), this, setRate( float ) );

    DCONNECT( THEMIM, inputChanged( input_thread_t * ),
              speedControl, activateOnState() );
    setRate( var_InheritFloat( p_intf, "rate" ) );
}

SpeedLabel::~SpeedLabel()
{
    delete speedControl;
    delete speedControlMenu;
}

/****************************************************************************
 * Small right-click menu for rate control
 ****************************************************************************/

void SpeedLabel::showSpeedMenu( QPoint pos )
{
    speedControlMenu->exec( QCursor::pos() - pos
                          + QPoint( 0, height() ) );
}

void SpeedLabel::setRate( float rate )
{
    QString str;
    str.setNum( rate, 'f', 2 );
    str.append( "x" );
    setText( str );
    setToolTip( str );
    speedControl->updateControls( rate );
}

/**********************************************************************
 * Speed control widget
 **********************************************************************/
SpeedControlWidget::SpeedControlWidget( intf_thread_t *_p_i, QWidget *_parent )
                    : QFrame( _parent ), p_intf( _p_i )
{
    QSizePolicy sizePolicy( QSizePolicy::Maximum, QSizePolicy::Fixed );
    sizePolicy.setHorizontalStretch( 0 );
    sizePolicy.setVerticalStretch( 0 );

    speedSlider = new QSlider( this );
    speedSlider->setSizePolicy( sizePolicy );
    speedSlider->setMaximumSize( QSize( 80, 200 ) );
    speedSlider->setOrientation( Qt::Vertical );
    speedSlider->setTickPosition( QSlider::TicksRight );

    speedSlider->setRange( -34, 34 );
    speedSlider->setSingleStep( 1 );
    speedSlider->setPageStep( 1 );
    speedSlider->setTickInterval( 17 );

    CONNECT( speedSlider, valueChanged( int ), this, updateRate( int ) );

    QToolButton *normalSpeedButton = new QToolButton( this );
    normalSpeedButton->setMaximumSize( QSize( 26, 20 ) );
    normalSpeedButton->setAutoRaise( true );
    normalSpeedButton->setText( "1x" );
    normalSpeedButton->setToolTip( qtr( "Revert to normal play speed" ) );

    CONNECT( normalSpeedButton, clicked(), this, resetRate() );

    QVBoxLayout *speedControlLayout = new QVBoxLayout( this );
    speedControlLayout->setContentsMargins( 4, 4, 4, 4 );
    speedControlLayout->setSpacing( 4 );
    speedControlLayout->addWidget( speedSlider );
    speedControlLayout->addWidget( normalSpeedButton );

    lastValue = 0;

    activateOnState();
}

void SpeedControlWidget::activateOnState()
{
    speedSlider->setEnabled( THEMIM->getIM()->hasInput() );
}

void SpeedControlWidget::updateControls( float rate )
{
    if( speedSlider->isSliderDown() )
    {
        //We don't want to change anything if the user is using the slider
        return;
    }

    double value = 17 * log( rate ) / log( 2 );
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
}

void SpeedControlWidget::updateRate( int sliderValue )
{
    if( sliderValue == lastValue )
        return;

    double speed = pow( 2, (double)sliderValue / 17 );
    int rate = INPUT_RATE_DEFAULT / speed;

    THEMIM->getIM()->setRate(rate);
}

void SpeedControlWidget::resetRate()
{
    THEMIM->getIM()->setRate( INPUT_RATE_DEFAULT );
}

CoverArtLabel::CoverArtLabel( QWidget *parent, intf_thread_t *_p_i )
              : QLabel( parent ), p_intf( _p_i )
{
    setContextMenuPolicy( Qt::ActionsContextMenu );
    CONNECT( this, updateRequested(), this, askForUpdate() );

    setMinimumHeight( 128 );
    setMinimumWidth( 128 );
    setMaximumHeight( 128 );
    setMaximumWidth( 128 );
    setScaledContents( false );
    setAlignment( Qt::AlignCenter );

    QList< QAction* > artActions = actions();
    QAction *action = new QAction( qtr( "Download cover art" ), this );
    CONNECT( action, triggered(), this, askForUpdate() );
    addAction( action );

    showArtUpdate( "" );
}

CoverArtLabel::~CoverArtLabel()
{
    QList< QAction* > artActions = actions();
    foreach( QAction *act, artActions )
        removeAction( act );
}

void CoverArtLabel::showArtUpdate( const QString& url )
{
    QPixmap pix;
    if( !url.isEmpty() && pix.load( url ) )
    {
        pix = pix.scaled( maximumWidth(), maximumHeight(),
                          Qt::KeepAspectRatioByExpanding );
    }
    else
    {
        pix = QPixmap( ":/noart.png" );
    }
    setPixmap( pix );
}

void CoverArtLabel::askForUpdate()
{
    THEMIM->getIM()->requestArtUpdate();
}

TimeLabel::TimeLabel( intf_thread_t *_p_intf  )
    : QLabel(), p_intf( _p_intf ), bufTimer( new QTimer(this) ),
      buffering( false ), showBuffering(false), bufVal( -1 )
{
    b_remainingTime = false;
    setText( " --:--/--:-- " );
    setAlignment( Qt::AlignRight | Qt::AlignVCenter );
    setToolTip( qtr( "Toggle between elapsed and remaining time" ) );
    bufTimer->setSingleShot( true );

    CONNECT( THEMIM->getIM(), positionUpdated( float, int64_t, int ),
              this, setDisplayPosition( float, int64_t, int ) );
    CONNECT( THEMIM->getIM(), cachingChanged( float ),
              this, updateBuffering( float ) );
    CONNECT( bufTimer, timeout(), this, updateBuffering() );
}

void TimeLabel::setDisplayPosition( float pos, int64_t t, int length )
{
    showBuffering = false;
    bufTimer->stop();

    if( pos == -1.f )
    {
        setText( " --:--/--:-- " );
        return;
    }

    int time = t / 1000000;

    secstotimestr( psz_length, length );
    secstotimestr( psz_time, ( b_remainingTime && length ) ? length - time
                                                           : time );

    QString timestr = QString( " %1%2/%3 " )
            .arg( QString( (b_remainingTime && length) ? "-" : "" ) )
            .arg( QString( psz_time ) )
            .arg( QString( ( !length && time ) ? "--:--" : psz_length ) );

    setText( timestr );

    cachedLength = length;
}

void TimeLabel::setDisplayPosition( float pos )
{
    if( pos == -1.f || cachedLength == 0 )
    {
        setText( " --:--/--:-- " );
        return;
    }

    int time = pos * cachedLength;
    secstotimestr( psz_time,
                   ( b_remainingTime && cachedLength ?
                   cachedLength - time : time ) );
    QString timestr = QString( " %1%2/%3 " )
        .arg( QString( (b_remainingTime && cachedLength) ? "-" : "" ) )
        .arg( QString( psz_time ) )
        .arg( QString( ( !cachedLength && time ) ? "--:--" : psz_length ) );

    setText( timestr );
}


void TimeLabel::toggleTimeDisplay()
{
    b_remainingTime = !b_remainingTime;
}


void TimeLabel::updateBuffering( float _buffered )
{
    bufVal = _buffered;
    if( !buffering || bufVal == 0 )
    {
        showBuffering = false;
        buffering = true;
        bufTimer->start(200);
    }
    else if( bufVal == 1 )
    {
        showBuffering = buffering = false;
        bufTimer->stop();
    }
    update();
}

void TimeLabel::updateBuffering()
{
    showBuffering = true;
    update();
}

void TimeLabel::paintEvent( QPaintEvent* event )
{
    if( showBuffering )
    {
        QRect r( rect() );
        r.setLeft( r.width() * bufVal );
        QPainter p( this );
        p.setOpacity( 0.4 );
        p.fillRect( r, palette().color( QPalette::Highlight ) );
    }
    QLabel::paintEvent( event );
}
