/*****************************************************************************
 * interface_widgets.cpp : Custom widgets for the main interface
 ****************************************************************************
 * Copyright ( C ) 2006 the VideoLAN team
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

#include <vlc_vout.h>

#include "dialogs_provider.hpp"
#include "components/interface_widgets.hpp"
#include "main_interface.hpp"
#include "input_manager.hpp"
#include "menus.hpp"
#include "util/input_slider.hpp"
#include "util/customwidgets.hpp"

#include <QLabel>
#include <QSpacerItem>
#include <QCursor>
#include <QPushButton>
#include <QToolButton>
#include <QHBoxLayout>
#include <QMenu>
#include <QPalette>
#include <QResizeEvent>
#include <QDate>

#ifdef Q_WS_X11
# include <X11/Xlib.h>
# include <qx11info_x11.h>
#endif

#include <math.h>

#define I_PLAY_TOOLTIP N_("Play\nIf the playlist is empty, open a media")

/**********************************************************************
 * Video Widget. A simple frame on which video is drawn
 * This class handles resize issues
 **********************************************************************/

VideoWidget::VideoWidget( intf_thread_t *_p_i ) : QFrame( NULL ), p_intf( _p_i )
{
    /* Init */
    p_vout = NULL;
    videoSize.rwidth() = -1;
    videoSize.rheight() = -1;

    hide();

    /* Set the policy to expand in both directions */
//    setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding );

    /* Black background is more coherent for a Video Widget */
    QPalette plt =  palette();
    plt.setColor( QPalette::Window, Qt::black );
    setPalette( plt );
    setAutoFillBackground(true);

    /* Indicates that the widget wants to draw directly onto the screen.
       Widgets with this attribute set do not participate in composition
       management */
    setAttribute( Qt::WA_PaintOnScreen, true );

    /* The core can ask through a callback to show the video. */
    connect( this, SIGNAL(askVideoWidgetToShow( unsigned int, unsigned int)),
             this, SLOT(SetSizing(unsigned int, unsigned int )),
             Qt::BlockingQueuedConnection );
}

void VideoWidget::paintEvent(QPaintEvent *ev)
{
    QFrame::paintEvent(ev);
#ifdef Q_WS_X11
    XFlush( QX11Info::display() );
#endif
}

VideoWidget::~VideoWidget()
{
    /* Ensure we are not leaking the video output. This would crash. */
    assert( !p_vout );
}

/**
 * Request the video to avoid the conflicts
 **/
void *VideoWidget::request( vout_thread_t *p_nvout, int *pi_x, int *pi_y,
                            unsigned int *pi_width, unsigned int *pi_height )
{
    msg_Dbg( p_intf, "Video was requested %i, %i", *pi_x, *pi_y );
    emit askVideoWidgetToShow( *pi_width, *pi_height );
    if( p_vout )
    {
        msg_Dbg( p_intf, "embedded video already in use" );
        return NULL;
    }
    p_vout = p_nvout;
#ifndef NDEBUG
    msg_Dbg( p_intf, "embedded video ready (handle %p)", winId() );
#endif
    return ( void* )winId();
}

/* Set the Widget to the correct Size */
/* Function has to be called by the parent
   Parent has to care about resizing himself*/
void VideoWidget::SetSizing( unsigned int w, unsigned int h )
{
    msg_Dbg( p_intf, "Video is resizing to: %i %i", w, h );
    videoSize.rwidth() = w;
    videoSize.rheight() = h;
    if( isHidden() ) show();
    updateGeometry(); // Needed for deinterlace
}

void VideoWidget::release( void )
{
    msg_Dbg( p_intf, "Video is not needed anymore" );
    p_vout = NULL;
    videoSize.rwidth() = 0;
    videoSize.rheight() = 0;
    updateGeometry();
    hide();
}

QSize VideoWidget::sizeHint() const
{
    return videoSize;
}

/**********************************************************************
 * Background Widget. Show a simple image background. Currently,
 * it's album art if present or cone.
 **********************************************************************/
#define ICON_SIZE 128
#define MAX_BG_SIZE 400
#define MIN_BG_SIZE 128

BackgroundWidget::BackgroundWidget( intf_thread_t *_p_i )
                 :QWidget( NULL ), p_intf( _p_i )
{
    /* We should use that one to take the more size it can */
    setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding);

    /* A dark background */
    setAutoFillBackground( true );
    plt = palette();
    plt.setColor( QPalette::Active, QPalette::Window , Qt::black );
    plt.setColor( QPalette::Inactive, QPalette::Window , Qt::black );
    setPalette( plt );

    /* A cone in the middle */
    label = new QLabel;
    label->setMargin( 5 );
    label->setMaximumHeight( MAX_BG_SIZE );
    label->setMaximumWidth( MAX_BG_SIZE );
    label->setMinimumHeight( MIN_BG_SIZE );
    label->setMinimumWidth( MIN_BG_SIZE );
    if( QDate::currentDate().dayOfYear() >= 354 )
        label->setPixmap( QPixmap( ":/vlc128-christmas.png" ) );
    else
        label->setPixmap( QPixmap( ":/vlc128.png" ) );

    QGridLayout *backgroundLayout = new QGridLayout( this );
    backgroundLayout->addWidget( label, 0, 1 );
    backgroundLayout->setColumnStretch( 0, 1 );
    backgroundLayout->setColumnStretch( 2, 1 );

    CONNECT( THEMIM->getIM(), artChanged( input_item_t* ),
             this, updateArt( input_item_t* ) );
}

BackgroundWidget::~BackgroundWidget()
{}

void BackgroundWidget::resizeEvent( QResizeEvent * event )
{
    if( event->size().height() <= MIN_BG_SIZE )
        label->hide();
    else
        label->show();
}

void BackgroundWidget::updateArt( input_item_t *p_item )
{
    QString url;
    if( p_item )
    {
        char *psz_art = input_item_GetArtURL( p_item );
        url = psz_art;
        free( psz_art );
    }

    if( url.isEmpty() )
    {
        if( QDate::currentDate().dayOfYear() >= 354 )
            label->setPixmap( QPixmap( ":/vlc128-christmas.png" ) );
        else
            label->setPixmap( QPixmap( ":/vlc128.png" ) );
    }
    else
    {
        url = url.replace( "file://", QString("" ) );
        /* Taglib seems to define a attachment://, It won't work yet */
        url = url.replace( "attachment://", QString("" ) );
        label->setPixmap( QPixmap( url ) );
    }
}

void BackgroundWidget::contextMenuEvent( QContextMenuEvent *event )
{
    QVLCMenu::PopupMenu( p_intf, true );
}

#if 0
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

/**********************************************************************
 * Speed control widget
 **********************************************************************/
SpeedControlWidget::SpeedControlWidget( intf_thread_t *_p_i ) :
                             QFrame( NULL ), p_intf( _p_i )
{
    QSizePolicy sizePolicy( QSizePolicy::Maximum, QSizePolicy::Fixed );
    sizePolicy.setHorizontalStretch( 0 );
    sizePolicy.setVerticalStretch( 0 );

    speedSlider = new QSlider;
    speedSlider->setSizePolicy( sizePolicy );
    speedSlider->setMaximumSize( QSize( 80, 200 ) );
    speedSlider->setOrientation( Qt::Vertical );
    speedSlider->setTickPosition( QSlider::TicksRight );

    speedSlider->setRange( -24, 24 );
    speedSlider->setSingleStep( 1 );
    speedSlider->setPageStep( 1 );
    speedSlider->setTickInterval( 12 );

    CONNECT( speedSlider, valueChanged( int ), this, updateRate( int ) );

    QToolButton *normalSpeedButton = new QToolButton( this );
    normalSpeedButton->setMaximumSize( QSize( 26, 20 ) );
    normalSpeedButton->setAutoRaise( true );
    normalSpeedButton->setText( "1x" );
    normalSpeedButton->setToolTip( qtr( "Revert to normal play speed" ) );

    CONNECT( normalSpeedButton, clicked(), this, resetRate() );

    QVBoxLayout *speedControlLayout = new QVBoxLayout;
    speedControlLayout->setLayoutMargins( 4, 4, 4, 4, 4 );
    speedControlLayout->setSpacing( 4 );
    speedControlLayout->addWidget( speedSlider );
    speedControlLayout->addWidget( normalSpeedButton );
    setLayout( speedControlLayout );
}

SpeedControlWidget::~SpeedControlWidget()
{}

void SpeedControlWidget::setEnable( bool b_enable )
{
    speedSlider->setEnabled( b_enable );
}

void SpeedControlWidget::updateControls( int rate )
{
    if( speedSlider->isSliderDown() )
    {
        //We don't want to change anything if the user is using the slider
        return;
    }

    double value = 12 * log( (double)INPUT_RATE_DEFAULT / rate ) / log( 2 );
    int sliderValue = (int) ( ( value > 0 ) ? value + .5 : value - .5 );

    if( sliderValue < speedSlider->minimum() )
    {
        sliderValue = speedSlider->minimum();
    }
    else if( sliderValue > speedSlider->maximum() )
    {
        sliderValue = speedSlider->maximum();
    }

    //Block signals to avoid feedback loop
    speedSlider->blockSignals( true );
    speedSlider->setValue( sliderValue );
    speedSlider->blockSignals( false );
}

void SpeedControlWidget::updateRate( int sliderValue )
{
    double speed = pow( 2, (double)sliderValue / 12 );
    int rate = INPUT_RATE_DEFAULT / speed;

    THEMIM->getIM()->setRate(rate);
}

void SpeedControlWidget::resetRate()
{
    THEMIM->getIM()->setRate(INPUT_RATE_DEFAULT);
}



static int downloadCoverCallback( vlc_object_t *p_this,
                                  char const *psz_var,
                                  vlc_value_t oldvar, vlc_value_t newvar,
                                  void *data )
{
    if( !strcmp( psz_var, "item-change" ) )
    {
        CoverArtLabel *art = static_cast< CoverArtLabel* >( data );
        if( art )
            art->requestUpdate();
    }
    return VLC_SUCCESS;
}

CoverArtLabel::CoverArtLabel( QWidget *parent,
                              vlc_object_t *_p_this,
                              input_item_t *_p_input )
        : QLabel( parent ), p_this( _p_this), p_input( _p_input ), prevArt()
{
    setContextMenuPolicy( Qt::ActionsContextMenu );
    CONNECT( this, updateRequested(), this, doUpdate() );

    playlist_t *p_playlist = pl_Hold( p_this );
    var_AddCallback( p_playlist, "item-change",
                     downloadCoverCallback, this );
    pl_Release( p_this );

    setMinimumHeight( 128 );
    setMinimumWidth( 128 );
    setMaximumHeight( 128 );
    setMaximumWidth( 128 );
    setScaledContents( true );

    doUpdate();
}

void CoverArtLabel::downloadCover()
{
    if( p_input )
    {
        playlist_t *p_playlist = pl_Hold( p_this );
        playlist_AskForArtEnqueue( p_playlist, p_input );
        pl_Release( p_this );
    }
}

void CoverArtLabel::doUpdate()
{
    if( !p_input )
    {
        setPixmap( QPixmap( ":/noart.png" ) );
        QList< QAction* > artActions = actions();
        if( !artActions.isEmpty() )
            foreach( QAction *act, artActions )
                removeAction( act );
        prevArt = "";
    }
    else
    {
        char *psz_meta = input_item_GetArtURL( p_input );
        if( psz_meta && !strncmp( psz_meta, "file://", 7 ) )
        {
            QString artUrl = qfu( psz_meta ).replace( "file://", "" );
            if( artUrl != prevArt )
            {
                QPixmap pix;
                if( pix.load( artUrl ) )
                    setPixmap( pix );
                else
                {
                    msg_Dbg( p_this, "Qt could not load image '%s'",
                             qtu( artUrl ) );
                    setPixmap( QPixmap( ":/noart.png" ) );
                }
            }
            QList< QAction* > artActions = actions();
            if( !artActions.isEmpty() )
            {
                foreach( QAction *act, artActions )
                    removeAction( act );
            }
            prevArt = artUrl;
        }
        else
        {
            if( prevArt != "" )
                setPixmap( QPixmap( ":/noart.png" ) );
            prevArt = "";
            QList< QAction* > artActions = actions();
            if( artActions.isEmpty() )
            {
                QAction *action = new QAction( qtr( "Download cover art" ),
                                               this );
                addAction( action );
                CONNECT( action, triggered(),
                         this, downloadCover() );
            }
        }
        free( psz_meta );
    }
}

TimeLabel::TimeLabel( intf_thread_t *_p_intf  ) :QLabel(), p_intf( _p_intf )
{
   b_remainingTime = false;
   setText( " --:--/--:-- " );
   setAlignment( Qt::AlignRight | Qt::AlignVCenter );
   setToolTip( qtr( "Toggle between elapsed and remaining time" ) );


   CONNECT( THEMIM->getIM(), statusChanged( int ),
            this, setStatus( int ) );
   CONNECT( THEMIM->getIM(), positionUpdated( float, int, int ),
             this, setDisplayPosition( float, int, int ) );
}

void TimeLabel::setDisplayPosition( float pos, int time, int length )
{
    char psz_length[MSTRTIME_MAX_SIZE], psz_time[MSTRTIME_MAX_SIZE];
    secstotimestr( psz_length, length );
    secstotimestr( psz_time, ( b_remainingTime && length ) ? length - time
                                                           : time );

    QString timestr;
    timestr.sprintf( "%s/%s", psz_time,
                            ( !length && time ) ? "--:--" : psz_length );

    /* Add a minus to remaining time*/
    if( b_remainingTime && length ) setText( " -"+timestr+" " );
    else setText( " "+timestr+" " );
}

void TimeLabel::toggleTimeDisplay()
{
    b_remainingTime = !b_remainingTime;
}

void TimeLabel::setStatus( int i_status )
{
    msg_Warn( p_intf, "Status: %i", i_status );

    if( i_status == OPENING_S )
        setText( "Buffering" );
}

bool VolumeClickHandler::eventFilter( QObject *obj, QEvent *e )
{
    if (e->type() == QEvent::MouseButtonPress  )
    {
        aout_VolumeMute( p_intf, NULL );
        audio_volume_t i_volume;
        aout_VolumeGet( p_intf, &i_volume );
//        m->updateVolume( i_volume *  VOLUME_MAX / (AOUT_VOLUME_MAX/2) );
        return true;
    }
    return false;
}

