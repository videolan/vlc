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

#include "qt4.hpp"
#include "components/interface_widgets.hpp"
#include "dialogs_provider.hpp"
#include "util/customwidgets.hpp"               // qtEventToVLCKey, QVLCStackedWidget

#include "menus.hpp"             /* Popup menu on bgWidget */

#include <vlc_vout.h>

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

#ifdef Q_WS_X11
#   include <X11/Xlib.h>
#   include <qx11info_x11.h>
#endif

#include <math.h>
#include <assert.h>

/**********************************************************************
 * Video Widget. A simple frame on which video is drawn
 * This class handles resize issues
 **********************************************************************/

VideoWidget::VideoWidget( intf_thread_t *_p_i )
            : QFrame( NULL ) , p_intf( _p_i )
{
    /* Set the policy to expand in both directions */
    // setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding );

    layout = new QHBoxLayout( this );
    layout->setContentsMargins( 0, 0, 0, 0 );
    stable = NULL;
    show();
}

VideoWidget::~VideoWidget()
{
    /* Ensure we are not leaking the video output. This would crash. */
    assert( !stable );
}

void VideoWidget::sync( void )
{
#ifdef Q_WS_X11
    /* Make sure the X server has processed all requests.
     * This protects other threads using distinct connections from getting
     * the video widget window in an inconsistent states. */
    XSync( QX11Info::display(), False );
#endif
}

/**
 * Request the video to avoid the conflicts
 **/
WId VideoWidget::request( int *pi_x, int *pi_y,
                          unsigned int *pi_width, unsigned int *pi_height,
                          bool b_keep_size )
{
    msg_Dbg( p_intf, "Video was requested %i, %i", *pi_x, *pi_y );

    if( stable )
    {
        msg_Dbg( p_intf, "embedded video already in use" );
        return 0;
    }
    if( b_keep_size )
    {
        *pi_width  = size().width();
        *pi_height = size().height();
    }

    /* The owner of the video window needs a stable handle (WinId). Reparenting
     * in Qt4-X11 changes the WinId of the widget, so we need to create another
     * dummy widget that stays within the reparentable widget. */
    stable = new QWidget();
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
#if !defined (Q_WS_X11) && !defined (Q_WS_QPA)
    stable->setAttribute( Qt::WA_PaintOnScreen, true );
#endif

    layout->addWidget( stable );

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
    sync();
    return stable->winId();
}

/* Set the Widget to the correct Size */
/* Function has to be called by the parent
   Parent has to care about resizing itself */
void VideoWidget::SetSizing( unsigned int w, unsigned int h )
{
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

void VideoWidget::release( void )
{
    msg_Dbg( p_intf, "Video is not needed anymore" );

    if( stable )
    {
        layout->removeWidget( stable );
        stable->deleteLater();
        stable = NULL;
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
    defaultArt = QString( ":/logo/vlc128.png" );
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
}

void BackgroundWidget::updateArt( const QString& url )
{
    if ( !url.isEmpty() )
        pixmapUrl = url;
    else
        pixmapUrl = defaultArt;
    update();
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
    QBitmap pMask;
    float f_alpha = 1.0;

    i_maxwidth  = __MIN( maximumWidth(), width() ) - MARGIN * 2;
    i_maxheight = __MIN( maximumHeight(), height() ) - MARGIN * 2;

    painter.setOpacity( property( "opacity" ).toFloat() );

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
    VLCMenuBar::PopupMenu( p_intf, true );
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
            it++;
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
    tooltipStringPattern = qtr( "Current playback speed: %1\nClick to adjust" );

    /* Create the Speed Control Widget */
    speedControl = new SpeedControlWidget( p_intf, this );
    speedControlMenu = new QMenu( this );

    QWidgetAction *widgetAction = new QWidgetAction( speedControl );
    widgetAction->setDefaultWidget( speedControl );
    speedControlMenu->addAction( widgetAction );

    /* Change the SpeedRate in the Label */
    CONNECT( THEMIM->getIM(), rateChanged( float ), this, setRate( float ) );

    DCONNECT( THEMIM, inputChanged( input_thread_t * ),
              speedControl, activateOnState() );

    setFrameStyle( QFrame::StyledPanel | QFrame::Raised );
    setLineWidth( 1 );

    setRate( var_InheritFloat( THEPL, "rate" ) );
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

    double speed = pow( 2, (double)sliderValue / 17 );
    int rate = INPUT_RATE_DEFAULT / speed;

    THEMIM->getIM()->setRate(rate);
    //spinBox->setValue( var_InheritFloat( THEPL, "rate" ) );
}

void SpeedControlWidget::updateSpinBoxRate( double r )
{
    var_SetFloat( THEPL, "rate", r );
}

void SpeedControlWidget::resetRate()
{
    THEMIM->getIM()->setRate( INPUT_RATE_DEFAULT );
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
        vlc_gc_incref( p_item );
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
    if ( p_item ) vlc_gc_decref( p_item );
}

void CoverArtLabel::setItem( input_item_t *_p_item )
{
    if ( p_item ) vlc_gc_decref( p_item );
    p_item = _p_item;
    if ( p_item ) vlc_gc_incref( p_item );
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
    THEMIM->getIM()->requestArtUpdate( p_item );
}

void CoverArtLabel::setArtFromFile()
{
    if( !p_item )
        return;

    QString filePath = QFileDialog::getOpenFileName( this, qtr( "Choose Cover Art" ),
        p_intf->p_sys->filepath, qtr( "Image Files (*.gif *.jpg *.jpeg *.png)" ) );

    if( filePath.isEmpty() )
        return;

    QString fileUrl = QUrl::fromLocalFile( filePath ).toString();

    THEMIM->getIM()->setArt( p_item, fileUrl );
}

void CoverArtLabel::clear()
{
    showArtUpdate( "" );
}

TimeLabel::TimeLabel( intf_thread_t *_p_intf, TimeLabel::Display _displayType  )
    : ClickableQLabel(), p_intf( _p_intf ), bufTimer( new QTimer(this) ),
      buffering( false ), showBuffering(false), bufVal( -1 ), displayType( _displayType )
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

    bufTimer->setSingleShot( true );

    CONNECT( THEMIM->getIM(), positionUpdated( float, int64_t, int ),
              this, setDisplayPosition( float, int64_t, int ) );
    CONNECT( THEMIM->getIM(), cachingChanged( float ),
              this, updateBuffering( float ) );
    CONNECT( bufTimer, timeout(), this, updateBuffering() );

    setStyleSheet( "padding-left: 4px; padding-right: 4px;" );
}

void TimeLabel::setDisplayPosition( float pos, int64_t t, int length )
{
    showBuffering = false;
    bufTimer->stop();

    if( pos == -1.f )
    {
        setMinimumSize( QSize( 0, 0 ) );
        if( displayType == TimeLabel::Both )
            setText( "--:--/--:--" );
        else
            setText( "--:--" );
        return;
    }

    int time = t / 1000000;

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
    QString timestr = QString( "%1%2/%3" )
        .arg( QString( (b_remainingTime && cachedLength) ? "-" : "" ) )
        .arg( QString( psz_time ) )
        .arg( QString( ( !cachedLength && time ) ? "--:--" : psz_length ) );

    setText( timestr );
}


void TimeLabel::toggleTimeDisplay()
{
    b_remainingTime = !b_remainingTime;
    getSettings()->setValue( "MainWindow/ShowRemainingTime", b_remainingTime );
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
