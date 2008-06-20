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

#include "dialogs_provider.hpp"
#include "components/interface_widgets.hpp"
#include "main_interface.hpp"
#include "input_manager.hpp"
#include "menus.hpp"
#include "util/input_slider.hpp"
#include "util/customwidgets.hpp"
#include <vlc_vout.h>

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
#include <QMutexLocker>

/**********************************************************************
 * Video Widget. A simple frame on which video is drawn
 * This class handles resize issues
 **********************************************************************/

VideoWidget::VideoWidget( intf_thread_t *_p_i ) : QFrame( NULL ), p_intf( _p_i )
{
    /* Init */
    vlc_mutex_init( &lock );
    p_vout = NULL;
    handleReady = false;
    hide(); setMinimumSize( 16, 16 );
    videoSize.rwidth() = -1;
    videoSize.rheight() = -1;

    /* Black background is more coherent for a Video Widget IMVHO */
    QPalette plt =  palette();
    plt.setColor( QPalette::Active, QPalette::Window , Qt::black );
    plt.setColor( QPalette::Inactive, QPalette::Window , Qt::black );
    setPalette( plt );

    /* The core can ask through a callback to show the video.
     * NOTE: We need to block the video output core until the window handle
     * is ready for use (otherwise an X11 invalid handle failure may occur).
     * As a side effect, it is illegal to emit askVideoWidgetToShow from
     * the same thread as the Qt4 thread that owns this. */
    QObject::connect( this, SIGNAL(askVideoWidgetToShow()), this, SLOT(show()),
                      Qt::BlockingQueuedConnection );

    /* The core can ask through a callback to resize the video */
   // CONNECT( this, askResize( int, int ), this, SetSizing( int, int ) );
}

void VideoWidget::paintEvent(QPaintEvent *ev)
{
    QFrame::paintEvent(ev);
    handleReady = true;
    handleWait.wakeAll();
}

VideoWidget::~VideoWidget()
{
    vlc_mutex_lock( &lock );
    if( p_vout )
    {
        if( !p_intf->psz_switch_intf )
        {
            if( vout_Control( p_vout, VOUT_CLOSE ) != VLC_SUCCESS )
                vout_Control( p_vout, VOUT_REPARENT );
        }
        else
        {
            if( vout_Control( p_vout, VOUT_REPARENT ) != VLC_SUCCESS )
                vout_Control( p_vout, VOUT_CLOSE );
        }
    }
    vlc_mutex_unlock( &lock );
    vlc_mutex_destroy( &lock );
}

/**
 * Request the video to avoid the conflicts
 **/
void *VideoWidget::request( vout_thread_t *p_nvout, int *pi_x, int *pi_y,
                           unsigned int *pi_width, unsigned int *pi_height )
{
    QMutexLocker locker( &handleLock );
    msg_Dbg( p_intf, "Video was requested %i, %i", *pi_x, *pi_y );
    emit askVideoWidgetToShow();
    if( p_vout )
    {
        msg_Dbg( p_intf, "embedded video already in use" );
        return NULL;
    }
    p_vout = p_nvout;
    while( !handleReady )
    {
        msg_Dbg( p_intf, "embedded video pending (handle %p)", winId() );
        handleWait.wait( &handleLock );
    }
    msg_Dbg( p_intf, "embedded video ready (handle %p)", winId() );
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
    updateGeometry(); // Needed for deinterlace
}

void VideoWidget::release( void *p_win )
{
    msg_Dbg( p_intf, "Video is non needed anymore" );
    p_vout = NULL;
    videoSize.rwidth() = 0;
    videoSize.rheight() = 0;
    hide();
    updateGeometry(); // Needed for deinterlace
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
#define MIN_BG_SIZE 64

BackgroundWidget::BackgroundWidget( intf_thread_t *_p_i )
                 :QWidget( NULL ), p_intf( _p_i )
{
    /* We should use that one to take the more size it can */
//    setSizePolicy( QSizePolicy::Preferred, QSizePolicy::Preferred );

    /* A dark background */
    setAutoFillBackground( true );
    plt =  palette();
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

    CONNECT( THEMIM->getIM(), artChanged( QString ), this, updateArt( QString ) );
}

BackgroundWidget::~BackgroundWidget()
{
}

void BackgroundWidget::resizeEvent( QResizeEvent * event )
{
    if( event->size().height() <= MIN_BG_SIZE )
        label->hide();
    else
        label->show();
}

void BackgroundWidget::updateArt( QString url )
{
    if( url.isEmpty() )
    {
        if( QDate::currentDate().dayOfYear() >= 354 )
            label->setPixmap( QPixmap( ":/vlc128-christmas.png" ) );
        else
            label->setPixmap( QPixmap( ":/vlc128.png" ) );
        return;
    }
    else
    {
        label->setPixmap( QPixmap( url ) );
    }
}

void BackgroundWidget::contextMenuEvent( QContextMenuEvent *event )
{
    QVLCMenu::PopupMenu( p_intf, true );
}

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

    layout->addItem( new QSpacerItem( 40,20,
                              QSizePolicy::Expanding, QSizePolicy::Minimum ) );
    layout->addWidget( new QLabel( qtr( "Current visualization:" ) ) );

    current = new QLabel( qtr( "None" ) );
    layout->addWidget( current );

    BUTTONACT( prevButton, prev() );
    BUTTONACT( nextButton, next() );

    setLayout( layout );
    setMaximumHeight( 35 );
}

VisualSelector::~VisualSelector()
{
}

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

/**********************************************************************
 * TEH controls
 **********************************************************************/

#define setupSmallButton( aButton ){  \
    aButton->setMaximumSize( QSize( 26, 26 ) ); \
    aButton->setMinimumSize( QSize( 26, 26 ) ); \
    aButton->setIconSize( QSize( 20, 20 ) ); }

AdvControlsWidget::AdvControlsWidget( intf_thread_t *_p_i ) :
                                           QFrame( NULL ), p_intf( _p_i )
{
    QHBoxLayout *advLayout = new QHBoxLayout( this );
    advLayout->setMargin( 0 );
    advLayout->setSpacing( 0 );
    advLayout->setAlignment( Qt::AlignBottom );

    /* A to B Button */
    ABButton = new QPushButton( "AB" );
    setupSmallButton( ABButton );
    advLayout->addWidget( ABButton );
    BUTTON_SET_ACT( ABButton, "AB", qtr( "A to B" ), fromAtoB() );
    timeA = timeB = 0;
    CONNECT( THEMIM->getIM(), positionUpdated( float, int, int ),
             this, AtoBLoop( float, int, int ) );
#if 0
    frameButton = new QPushButton( "Fr" );
    frameButton->setMaximumSize( QSize( 26, 26 ) );
    frameButton->setIconSize( QSize( 20, 20 ) );
    advLayout->addWidget( frameButton );
    BUTTON_SET_ACT( frameButton, "Fr", qtr( "Frame by Frame" ), frame() );
#endif

    recordButton = new QPushButton( "R" );
    setupSmallButton( recordButton );
    advLayout->addWidget( recordButton );
    BUTTON_SET_ACT_I( recordButton, "", record_16px.png,
            qtr( "Record" ), record() );

    /* Snapshot Button */
    snapshotButton = new QPushButton( "S" );
    setupSmallButton( snapshotButton );
    advLayout->addWidget( snapshotButton );
    BUTTON_SET_ACT( snapshotButton, "S", qtr( "Take a snapshot" ), snapshot() );
}

AdvControlsWidget::~AdvControlsWidget()
{}

void AdvControlsWidget::enableInput( bool enable )
{
    ABButton->setEnabled( enable );
    recordButton->setEnabled( enable );
}

void AdvControlsWidget::enableVideo( bool enable )
{
    snapshotButton->setEnabled( enable );
#if 0
    frameButton->setEnabled( enable );
#endif
}

void AdvControlsWidget::snapshot()
{
    vout_thread_t *p_vout =
        (vout_thread_t *)vlc_object_find( p_intf, VLC_OBJECT_VOUT, FIND_ANYWHERE );
    if( p_vout ) vout_Control( p_vout, VOUT_SNAPSHOT );
}

/* Function called when the button is clicked() */
void AdvControlsWidget::fromAtoB()
{
    if( !timeA )
    {
        timeA = var_GetTime( THEMIM->getInput(), "time"  );
        ABButton->setText( "A->..." );
        return;
    }
    if( !timeB )
    {
        timeB = var_GetTime( THEMIM->getInput(), "time"  );
        var_SetTime( THEMIM->getInput(), "time" , timeA );
        ABButton->setText( "A<=>B" );
        return;
    }
    timeA = 0;
    timeB = 0;
    ABButton->setText( "AB" );
}

/* Function called regularly when in an AtoB loop */
void AdvControlsWidget::AtoBLoop( float f_pos, int i_time, int i_length )
{
    if( timeB )
    {
        if( i_time >= (int)(timeB/1000000) )
            var_SetTime( THEMIM->getInput(), "time" , timeA );
    }
}

/* FIXME Record function */
void AdvControlsWidget::record(){}

#if 0
//FIXME Frame by frame function
void AdvControlsWidget::frame(){}
#endif

/*****************************
 * DA Control Widget !
 *****************************/
ControlsWidget::ControlsWidget( intf_thread_t *_p_i,
                                MainInterface *_p_mi,
                                bool b_advControls,
                                bool b_shiny,
                                bool b_fsCreation) :
                                QFrame( _p_mi ), p_intf( _p_i )
{
    controlLayout = new QGridLayout( );

    controlLayout->setSpacing( 0 );
    controlLayout->setLayoutMargins( 7, 5, 7, 3, 6 );

    if( !b_fsCreation )
        setLayout( controlLayout );

    setSizePolicy( QSizePolicy::Preferred , QSizePolicy::Maximum );

    /** The main Slider **/
    slider = new InputSlider( Qt::Horizontal, NULL );
    controlLayout->addWidget( slider, 0, 1, 1, 16 );
    /* Update the position when the IM has changed */
    CONNECT( THEMIM->getIM(), positionUpdated( float, int, int ),
             slider, setPosition( float, int, int ) );
    /* And update the IM, when the position has changed */
    CONNECT( slider, sliderDragged( float ),
             THEMIM->getIM(), sliderUpdate( float ) );

    /** Slower and faster Buttons **/
    slowerButton = new QToolButton;
    slowerButton->setAutoRaise( true );
    slowerButton->setMaximumSize( QSize( 26, 20 ) );

    BUTTON_SET_ACT( slowerButton, "-", qtr( "Slower" ), slower() );
    controlLayout->addWidget( slowerButton, 0, 0 );

    fasterButton = new QToolButton;
    fasterButton->setAutoRaise( true );
    fasterButton->setMaximumSize( QSize( 26, 20 ) );

    BUTTON_SET_ACT( fasterButton, "+", qtr( "Faster" ), faster() );
    controlLayout->addWidget( fasterButton, 0, 17 );

    /* advanced Controls handling */
    b_advancedVisible = b_advControls;

    advControls = new AdvControlsWidget( p_intf );
    controlLayout->addWidget( advControls, 1, 3, 2, 4, Qt::AlignBottom );
    if( !b_advancedVisible ) advControls->hide();

    /** Disc and Menus handling */
    discFrame = new QWidget( this );

    QHBoxLayout *discLayout = new QHBoxLayout( discFrame );
    discLayout->setSpacing( 0 );
    discLayout->setMargin( 0 );

    prevSectionButton = new QPushButton( discFrame );
    setupSmallButton( prevSectionButton );
    discLayout->addWidget( prevSectionButton );

    menuButton = new QPushButton( discFrame );
    setupSmallButton( menuButton );
    discLayout->addWidget( menuButton );

    nextSectionButton = new QPushButton( discFrame );
    setupSmallButton( nextSectionButton );
    discLayout->addWidget( nextSectionButton );

    controlLayout->addWidget( discFrame, 1, 10, 2, 3, Qt::AlignBottom );

    BUTTON_SET_IMG( prevSectionButton, "", previous.png, "" );
    BUTTON_SET_IMG( nextSectionButton, "", next.png, "" );
    BUTTON_SET_IMG( menuButton, "", previous.png, "" );

    discFrame->hide();

    /* Change the navigation button display when the IM navigation changes */
    CONNECT( THEMIM->getIM(), navigationChanged( int ),
             this, setNavigation( int ) );
    /* Changes the IM navigation when triggered on the nav buttons */
    CONNECT( prevSectionButton, clicked(), THEMIM->getIM(),
             sectionPrev() );
    CONNECT( nextSectionButton, clicked(), THEMIM->getIM(),
             sectionNext() );
    CONNECT( menuButton, clicked(), THEMIM->getIM(),
             sectionMenu() );

    /**
     * Telextext QFrame
     * TODO: Merge with upper menu in a StackLayout
     **/
    telexFrame = new QWidget( this );
    QHBoxLayout *telexLayout = new QHBoxLayout( telexFrame );
    telexLayout->setSpacing( 0 );
    telexLayout->setMargin( 0 );

    QPushButton  *telexOn = new QPushButton;
    setupSmallButton( telexOn );
    telexLayout->addWidget( telexOn );

    telexTransparent = new QPushButton;
    setupSmallButton( telexTransparent );
    telexLayout->addWidget( telexTransparent );
    b_telexTransparent = false;

    telexPage = new QSpinBox;
    telexPage->setRange( 0, 999 );
    telexPage->setValue( 100 );
    telexPage->setAccelerated( true );
    telexPage->setWrapping( true );
    telexPage->setAlignment( Qt::AlignRight );
    telexPage->setSizePolicy( QSizePolicy::Preferred, QSizePolicy::Minimum );
    telexLayout->addWidget( telexPage );

    controlLayout->addWidget( telexFrame, 1, 10, 2, 3, Qt::AlignBottom );
    telexFrame->hide(); /* default hidden */

    CONNECT( telexPage, valueChanged( int ), THEMIM->getIM(),
             telexGotoPage( int ) );

    BUTTON_SET_ACT_I( telexOn, "", tv.png, qtr( "Teletext on" ),
                      toggleTeletext() );
    CONNECT( telexOn, clicked( bool ), THEMIM->getIM(),
             telexToggle( bool ) );
    telexTransparent->setEnabled( false );
    telexPage->setEnabled( false );

    BUTTON_SET_ACT_I( telexTransparent, "", tvtelx.png, qtr( "Teletext" ),
                      toggleTeletextTransparency() );
    CONNECT( telexTransparent, clicked( bool ),
             THEMIM->getIM(), telexSetTransparency() );
    CONNECT( THEMIM->getIM(), teletextEnabled( bool ),
             telexFrame, setVisible( bool ) );

    /** Play Buttons **/
    QSizePolicy sizePolicy( QSizePolicy::Fixed, QSizePolicy::Fixed );
    sizePolicy.setHorizontalStretch( 0 );
    sizePolicy.setVerticalStretch( 0 );

    /* Play */
    playButton = new QPushButton;
    playButton->setSizePolicy( sizePolicy );
    playButton->setMaximumSize( QSize( 36, 36 ) );
    playButton->setMinimumSize( QSize( 36, 36 ) );
    playButton->setIconSize( QSize( 30, 30 ) );

    controlLayout->addWidget( playButton, 2, 0, 2, 2 );

    controlLayout->setColumnMinimumWidth( 2, 20 );
    controlLayout->setColumnStretch( 2, 0 );

    /** Prev + Stop + Next Block **/
    controlButLayout = new QHBoxLayout;
    controlButLayout->setSpacing( 0 ); /* Don't remove that, will be useful */

    /* Prev */
    QPushButton *prevButton = new QPushButton;
    prevButton->setSizePolicy( sizePolicy );
    setupSmallButton( prevButton );

    controlButLayout->addWidget( prevButton );

    /* Stop */
    QPushButton *stopButton = new QPushButton;
    stopButton->setSizePolicy( sizePolicy );
    setupSmallButton( stopButton );

    controlButLayout->addWidget( stopButton );

    /* next */
    QPushButton *nextButton = new QPushButton;
    nextButton->setSizePolicy( sizePolicy );
    setupSmallButton( nextButton );

    controlButLayout->addWidget( nextButton );

    /* Add this block to the main layout */
    if( !b_fsCreation )
        controlLayout->addLayout( controlButLayout, 3, 3, 1, 3 );

    BUTTON_SET_ACT_I( playButton, "", play.png, qtr( "Play" ), play() );
    BUTTON_SET_ACT_I( prevButton, "" , previous.png,
                      qtr( "Previous" ), prev() );
    BUTTON_SET_ACT_I( nextButton, "", next.png, qtr( "Next" ), next() );
    BUTTON_SET_ACT_I( stopButton, "", stop.png, qtr( "Stop" ), stop() );

    controlLayout->setColumnMinimumWidth( 7, 20 );
    controlLayout->setColumnStretch( 7, 0 );
    controlLayout->setColumnStretch( 8, 0 );
    controlLayout->setColumnStretch( 9, 0 );

    /*
     * Other first Line buttons
     */
    /** Fullscreen/Visualisation **/
    fullscreenButton = new QPushButton( "F" );
    BUTTON_SET_ACT( fullscreenButton, "F", qtr( "Fullscreen" ), fullscreen() );
    setupSmallButton( fullscreenButton );
    controlLayout->addWidget( fullscreenButton, 3, 10, Qt::AlignBottom );

    /** Playlist Button **/
    playlistButton = new QPushButton;
    setupSmallButton( playlistButton );
    controlLayout->addWidget( playlistButton, 3, 11, Qt::AlignBottom );
    BUTTON_SET_IMG( playlistButton, "" , playlist.png, qtr( "Show playlist" ) );
    CONNECT( playlistButton, clicked(), _p_mi, togglePlaylist() );

    /** extended Settings **/
    extSettingsButton = new QPushButton;
    BUTTON_SET_ACT( extSettingsButton, "Ex", qtr( "Extended Settings" ),
            extSettings() );
    setupSmallButton( extSettingsButton );
    controlLayout->addWidget( extSettingsButton, 3, 12, Qt::AlignBottom );

    controlLayout->setColumnStretch( 13, 0 );
    controlLayout->setColumnMinimumWidth( 13, 24 );
    controlLayout->setColumnStretch( 14, 5 );

    /* Volume */
    hVolLabel = new VolumeClickHandler( p_intf, this );

    volMuteLabel = new QLabel;
    volMuteLabel->setPixmap( QPixmap( ":/pixmaps/volume-medium.png" ) );
    volMuteLabel->setToolTip( qtr( "Mute" ) );
    volMuteLabel->installEventFilter( hVolLabel );
    controlLayout->addWidget( volMuteLabel, 3, 15, Qt::AlignBottom );

    if( b_shiny )
    {
        volumeSlider = new SoundSlider( this,
            config_GetInt( p_intf, "volume-step" ),
            config_GetInt( p_intf, "qt-volume-complete" ),
            config_GetPsz( p_intf, "qt-slider-colours" ) );
    }
    else
    {
        volumeSlider = new QSlider( this );
        volumeSlider->setOrientation( Qt::Horizontal );
    }
    volumeSlider->setMaximumSize( QSize( 200, 40 ) );
    volumeSlider->setMinimumSize( QSize( 106, 30 ) );
    volumeSlider->setFocusPolicy( Qt::NoFocus );
    controlLayout->addWidget( volumeSlider, 2, 16, 2 , 2, Qt::AlignBottom );

    /* Set the volume from the config */
    volumeSlider->setValue( ( config_GetInt( p_intf, "volume" ) ) *
                              VOLUME_MAX / (AOUT_VOLUME_MAX/2) );

    /* Force the update at build time in order to have a muted icon if needed */
    updateVolume( volumeSlider->value() );

    /* Volume control connection */
    CONNECT( volumeSlider, valueChanged( int ), this, updateVolume( int ) );
    CONNECT( THEMIM, volumeChanged( void ), this, updateVolume( void ) );

    updateInput();
}

ControlsWidget::~ControlsWidget()
{}

void ControlsWidget::toggleTeletext()
{
    bool b_enabled = THEMIM->teletextState();
    if( b_telexEnabled )
    {
        telexTransparent->setEnabled( false );
        telexPage->setEnabled( false );
        b_telexEnabled = false;
    }
    else if( b_enabled )
    {
        telexTransparent->setEnabled( true );
        telexPage->setEnabled( true );
        b_telexEnabled = true;
    }
}

void ControlsWidget::toggleTeletextTransparency()
{
    if( b_telexTransparent )
    {
        telexTransparent->setIcon( QIcon( ":/pixmaps/tvtelx.png" ) );
        telexTransparent->setToolTip( qtr( "Teletext" ) );
        b_telexTransparent = false;
    }
    else
    {
        telexTransparent->setIcon( QIcon( ":/pixmaps/tvtelx-transparent.png" ) );
        telexTransparent->setToolTip( qtr( "Transparent" ) );
        b_telexTransparent = true;
    }
}

void ControlsWidget::stop()
{
    THEMIM->stop();
}

void ControlsWidget::play()
{
    if( THEPL->current.i_size == 0 )
    {
        /* The playlist is empty, open a file requester */
        THEDP->openFileDialog();
        setStatus( 0 );
        return;
    }
    THEMIM->togglePlayPause();
}

void ControlsWidget::prev()
{
    THEMIM->prev();
}

void ControlsWidget::next()
{
    THEMIM->next();
}

void ControlsWidget::setNavigation( int navigation )
{
#define HELP_MENU N_( "Menu" )
#define HELP_PCH N_( "Previous chapter" )
#define HELP_NCH N_( "Next chapter" )
#define HELP_PTR N_( "Previous track" )
#define HELP_NTR N_( "Next track" )

    // 1 = chapter, 2 = title, 0 = no
    if( navigation == 0 )
    {
        discFrame->hide();
    } else if( navigation == 1 ) {
        prevSectionButton->setToolTip( qfu( HELP_PCH ) );
        nextSectionButton->setToolTip( qfu( HELP_NCH ) );
        menuButton->show();
        discFrame->show();
    } else {
        prevSectionButton->setToolTip( qfu( HELP_PCH ) );
        nextSectionButton->setToolTip( qfu( HELP_NCH ) );
        menuButton->hide();
        discFrame->show();
    }
}

static bool b_my_volume;
void ControlsWidget::updateVolume( int i_sliderVolume )
{
    if( !b_my_volume )
    {
        int i_res = i_sliderVolume  * (AOUT_VOLUME_MAX / 2) / VOLUME_MAX;
        aout_VolumeSet( p_intf, i_res );
    }
    if( i_sliderVolume == 0 )
        volMuteLabel->setPixmap( QPixmap(":/pixmaps/volume-muted.png" ) );
    else if( i_sliderVolume < VOLUME_MAX / 3 )
        volMuteLabel->setPixmap( QPixmap( ":/pixmaps/volume-low.png" ) );
    else if( i_sliderVolume > (VOLUME_MAX * 2 / 3 ) )
        volMuteLabel->setPixmap( QPixmap( ":/pixmaps/volume-high.png" ) );
    else volMuteLabel->setPixmap( QPixmap( ":/pixmaps/volume-medium.png" ) );
}

void ControlsWidget::updateVolume()
{
    /* Audio part */
    audio_volume_t i_volume;
    aout_VolumeGet( p_intf, &i_volume );
    i_volume = ( i_volume *  VOLUME_MAX )/ (AOUT_VOLUME_MAX/2);
    int i_gauge = volumeSlider->value();
    b_my_volume = false;
    if( i_volume - i_gauge > 1 || i_gauge - i_volume > 1 )
    {
        b_my_volume = true;
        volumeSlider->setValue( i_volume );
        b_my_volume = false;
    }
}

void ControlsWidget::updateInput()
{
    /* Activate the interface buttons according to the presence of the input */
    enableInput( THEMIM->getIM()->hasInput() );
    enableVideo( THEMIM->getIM()->hasVideo() && THEMIM->getIM()->hasInput() );
}

void ControlsWidget::setStatus( int status )
{
    if( status == PLAYING_S ) /* Playing */
    {
        playButton->setIcon( QIcon( ":/pixmaps/pause.png" ) );
        playButton->setToolTip( qtr( "Pause" ) );
    }
    else
    {
        playButton->setIcon( QIcon( ":/pixmaps/play.png" ) );
        playButton->setToolTip( qtr( "Play" ) );
    }
}

/**
 * TODO
 * This functions toggle the fullscreen mode
 * If there is no video, it should first activate Visualisations...
 *  This has also to be fixed in enableVideo()
 */
void ControlsWidget::fullscreen()
{
    vout_thread_t *p_vout =
        (vout_thread_t *)vlc_object_find( p_intf, VLC_OBJECT_VOUT, FIND_ANYWHERE );
    if( p_vout)
    {
        var_SetBool( p_vout, "fullscreen", !var_GetBool( p_vout, "fullscreen" ) );
        vlc_object_release( p_vout );
    }
}

void ControlsWidget::extSettings()
{
    THEDP->extendedDialog();
}

void ControlsWidget::slower()
{
    THEMIM->getIM()->slower();
}

void ControlsWidget::faster()
{
    THEMIM->getIM()->faster();
}

void ControlsWidget::enableInput( bool enable )
{
    slowerButton->setEnabled( enable );
    slider->setEnabled( enable );
    fasterButton->setEnabled( enable );

    /* Advanced Buttons too */
    advControls->enableInput( enable );
}

void ControlsWidget::enableVideo( bool enable )
{
    // TODO Later make the fullscreenButton toggle Visualisation and so on.
    fullscreenButton->setEnabled( enable );

    /* Advanced Buttons too */
    advControls->enableVideo( enable );
}

void ControlsWidget::toggleAdvanced()
{
    if( !VISIBLE( advControls ) )
    {
        advControls->show();
        b_advancedVisible = true;
    }
    else
    {
        advControls->hide();
        b_advancedVisible = false;
    }
    emit advancedControlsToggled( b_advancedVisible );
}


/**********************************************************************
 * Fullscrenn control widget
 **********************************************************************/
FullscreenControllerWidget::FullscreenControllerWidget( intf_thread_t *_p_i,
        MainInterface *_p_mi, bool b_advControls, bool b_shiny )
        : ControlsWidget( _p_i, _p_mi, b_advControls, b_shiny, true ),
        i_lastPosX( -1 ), i_lastPosY( -1 ), i_hideTimeout( 1 ),
        b_mouseIsOver( false )
{
    setWindowFlags( Qt::ToolTip );

    setFrameShape( QFrame::StyledPanel );
    setFrameStyle( QFrame::Sunken );
    setSizePolicy( QSizePolicy::Minimum, QSizePolicy::Minimum );

    QGridLayout *fsLayout = new QGridLayout( this );
    controlLayout->setSpacing( 0 );
    controlLayout->setLayoutMargins( 5, 1, 5, 1, 5 );

    fsLayout->addWidget( slowerButton, 0, 0 );
    slider->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Minimum);
    fsLayout->addWidget( slider, 0, 1, 1, 6 );
    fsLayout->addWidget( fasterButton, 0, 7 );

    fsLayout->addWidget( volMuteLabel, 1, 0);
    fsLayout->addWidget( volumeSlider, 1, 1 );

    fsLayout->addLayout( controlButLayout, 1, 2 );

    fsLayout->addWidget( playButton, 1, 3 );

    fsLayout->addWidget( discFrame, 1, 4 );

    fsLayout->addWidget( telexFrame, 1, 5 );

    fsLayout->addWidget( advControls, 1, 6, Qt::AlignVCenter );

    fsLayout->addWidget( fullscreenButton, 1, 7 );

    /* hiding timer */
    p_hideTimer = new QTimer( this );
    CONNECT( p_hideTimer, timeout(), this, hideFSControllerWidget() );
    p_hideTimer->setSingleShot( true );

    /* slow hiding timer */
#if HAVE_TRANSPARENCY
    p_slowHideTimer = new QTimer( this );
    CONNECT( p_slowHideTimer, timeout(), this, slowHideFSC() );
#endif

    adjustSize ();  /* need to get real width and height for moving */

    /* center down */
    QDesktopWidget * p_desktop = QApplication::desktop();

    move( p_desktop->width() / 2 - width() / 2,
          p_desktop->height() - height() );

    #ifdef WIN32TRICK
    setWindowOpacity( 0.0 );
    fscHidden = true;
    show();
    #endif
}

FullscreenControllerWidget::~FullscreenControllerWidget()
{
}

/**
 * Hide fullscreen controller
 * FIXME: under windows it have to be done by moving out of screen
 *        because hide() doesnt work
 */
void FullscreenControllerWidget::hideFSControllerWidget()
{
    #ifdef WIN32TRICK
    fscHidden = true;
    setWindowOpacity( 0.0 );    // simulate hidding
    #else
    hide();
    #endif
}

/**
 * Hidding fullscreen controller slowly
 * Linux: need composite manager
 * Windows: it is blinking, so it can be enabled by define TRASPARENCY
 */
void FullscreenControllerWidget::slowHideFSC()
{
#if HAVE_TRANSPARENCY
    static bool first_call = true;

    if ( first_call )
    {
        first_call = false;

        p_slowHideTimer->stop();
        /* the last part of time divided to 100 pieces */
        p_slowHideTimer->start(
            (int) ( i_hideTimeout / 2 / ( windowOpacity() * 100 ) ) );
    }
    else
    {
         if ( windowOpacity() > 0.0 )
         {
             /* we should use 0.01 because of 100 pieces ^^^
                but than it cannt be done in time */
             setWindowOpacity( windowOpacity() - 0.02 );
         }

         if ( windowOpacity() == 0.0 )
         {
             first_call = true;
             p_slowHideTimer->stop();
         }
    }
#endif
}

/**
 * Get state of visibility of FS controller on screen
 * On windows control if it is on hidden position
 */
bool FullscreenControllerWidget::isFSCHidden()
{
    #ifdef WIN32TRICK
    return fscHidden;
    #endif

    return isHidden();
}

/**
 * event handling
 * events: show, hide, start timer for hidding
 */
void FullscreenControllerWidget::customEvent( QEvent *event )
{
    int type = event->type();

    if ( type == FullscreenControlShow_Type )
    {
        #ifdef WIN32TRICK
        // after quiting and going to fs, we need to call show()
        if ( isHidden() )
            show();

        if ( fscHidden )
        {
            fscHidden = false;
            setWindowOpacity( 1.0 );
        }
        #else
        show();
        #endif

#if HAVE_TRANSPARENCY
        setWindowOpacity( DEFAULT_OPACITY );
#endif
    }
    else if ( type == FullscreenControlHide_Type )
    {
        hideFSControllerWidget();
    }
    else if ( type == FullscreenControlPlanHide_Type && !b_mouseIsOver )
    {
        p_hideTimer->start( i_hideTimeout );
#if HAVE_TRANSPARENCY
        p_slowHideTimer->start( i_hideTimeout / 2 );
#endif
    }
}

/**
 * On mouse move
 * moving with FSC
 */
void FullscreenControllerWidget::mouseMoveEvent( QMouseEvent *event )
{
    if ( event->buttons() == Qt::LeftButton )
    {
        int i_moveX = event->globalX() - i_lastPosX;
        int i_moveY = event->globalY() - i_lastPosY;

        move( x() + i_moveX, y() + i_moveY );

        i_lastPosX = event->globalX();
        i_lastPosY = event->globalY();
    }
}

/**
 * On mouse press
 * store position of cursor
 */
void FullscreenControllerWidget::mousePressEvent( QMouseEvent *event )
{
    i_lastPosX = event->globalX();
    i_lastPosY = event->globalY();
}

/**
 * On mouse go above FSC
 */
void FullscreenControllerWidget::enterEvent( QEvent *event )
{
    p_hideTimer->stop();
#if HAVE_TRANSPARENCY
    p_slowHideTimer->stop();
#endif
    b_mouseIsOver = true;
}

/**
 * On mouse go out from FSC
 */
void FullscreenControllerWidget::leaveEvent( QEvent *event )
{
    p_hideTimer->start( i_hideTimeout );
#if HAVE_TRANSPARENCY
    p_slowHideTimer->start( i_hideTimeout / 2 );
#endif
    b_mouseIsOver = false;
}

/**
 * When you get pressed key, send it to video output
 * FIXME: clearing focus by clearFocus() to not getting
 * key press events didnt work
 */
void FullscreenControllerWidget::keyPressEvent( QKeyEvent *event )
{
    int i_vlck = qtEventToVLCKey( event );
    if( i_vlck > 0 )
    {
        var_SetInteger( p_intf->p_libvlc, "key-pressed", i_vlck );
        event->accept();
    }
    else
        event->ignore();
}

/**
 * It is called when video start
 */
void FullscreenControllerWidget::regFullscreenCallback( vout_thread_t *p_vout )
{
    if ( p_vout )
    {
        var_AddCallback( p_vout, "fullscreen", regMouseMoveCallback, this );
    }
}

/**
 * It is called after turn off video, because p_vout is NULL now
 * we cannt delete callback, just hide if FScontroller is visible
 */
void FullscreenControllerWidget::unregFullscreenCallback()
{
    if ( isVisible() )
        hide();
}

/**
 * Register and unregister callback for mouse moving
 */
static int regMouseMoveCallback( vlc_object_t *vlc_object, const char *variable,
                                 vlc_value_t old_val, vlc_value_t new_val,
                                 void *data )
{
    vout_thread_t *p_vout = (vout_thread_t *) vlc_object;

    static bool b_registered = false;
    FullscreenControllerWidget *p_fs = (FullscreenControllerWidget *) data;

    if ( var_GetBool( p_vout, "fullscreen" ) && !b_registered )
    {
        p_fs->SetHideTimeout( var_GetInteger( p_vout, "mouse-hide-timeout" ) );
        var_AddCallback( p_vout, "mouse-moved",
                        showFullscreenControllCallback, (void *) p_fs );
        b_registered = true;
    }

    if ( !var_GetBool( p_vout, "fullscreen" ) && b_registered )
    {
        var_DelCallback( p_vout, "mouse-moved",
                        showFullscreenControllCallback, (void *) p_fs );
        b_registered = false;
        p_fs->hide();
    }

    return VLC_SUCCESS;
}

/**
 * Show fullscreen controller after mouse move
 * after show immediately plan hide event
 */
static int showFullscreenControllCallback( vlc_object_t *vlc_object, const char *variable,
                                           vlc_value_t old_val, vlc_value_t new_val,
                                           void *data )
{
    FullscreenControllerWidget *p_fs = (FullscreenControllerWidget *) data;

    if ( p_fs->isFSCHidden() || p_fs->windowOpacity() < DEFAULT_OPACITY )
    {
        IMEvent *event = new IMEvent( FullscreenControlShow_Type, 0 );
        QApplication::postEvent( p_fs, static_cast<QEvent *>(event) );
    }

    IMEvent *e = new IMEvent( FullscreenControlPlanHide_Type, 0 );
    QApplication::postEvent( p_fs, static_cast<QEvent *>(e) );

    return VLC_SUCCESS;
}

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

    speedSlider->setRange( -100, 100 );
    speedSlider->setSingleStep( 10 );
    speedSlider->setPageStep( 20 );
    speedSlider->setTickInterval( 20 );

    CONNECT( speedSlider, valueChanged( int ), this, updateRate( int ) );

    QToolButton *normalSpeedButton = new QToolButton( this );
    normalSpeedButton->setMaximumSize( QSize( 26, 20 ) );
    normalSpeedButton->setAutoRaise( true );
    normalSpeedButton->setText( "N" );
    normalSpeedButton->setToolTip( qtr( "Revert to normal play speed" ) );

    CONNECT( normalSpeedButton, clicked(), this, resetRate() );

    QVBoxLayout *speedControlLayout = new QVBoxLayout;
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

#define RATE_SLIDER_MAXIMUM 3.0
#define RATE_SLIDER_MINIMUM 0.3
#define RATE_SLIDER_LENGTH 100.0

void SpeedControlWidget::updateControls( int rate )
{
    if( speedSlider->isSliderDown() )
    {
        //We don't want to change anything if the user is using the slider
        return;
    }

    int sliderValue;
    double speed = INPUT_RATE_DEFAULT / (double)rate;

    if( rate >= INPUT_RATE_DEFAULT )
    {
        if( speed < RATE_SLIDER_MINIMUM )
        {
            sliderValue = speedSlider->minimum();
        }
        else
        {
            sliderValue = (int)( ( speed - 1.0 ) * RATE_SLIDER_LENGTH
                                        / ( 1.0 - RATE_SLIDER_MAXIMUM ) );
        }
    }
    else
    {
        if( speed > RATE_SLIDER_MAXIMUM )
        {
            sliderValue = speedSlider->maximum();
        }
        else
        {
            sliderValue = (int)( ( speed - 1.0 ) * RATE_SLIDER_LENGTH
                                        / ( RATE_SLIDER_MAXIMUM - 1.0 ) );
        }
    }

    //Block signals to avoid feedback loop
    speedSlider->blockSignals( true );
    speedSlider->setValue( sliderValue );
    speedSlider->blockSignals( false );
}

void SpeedControlWidget::updateRate( int sliderValue )
{
    int rate;

    if( sliderValue < 0.0 )
    {
        rate = (int)(INPUT_RATE_DEFAULT* RATE_SLIDER_LENGTH /
            ( sliderValue * ( 1.0 - RATE_SLIDER_MINIMUM ) + RATE_SLIDER_LENGTH ));
    }
    else
    {
        rate = (int)(INPUT_RATE_DEFAULT* RATE_SLIDER_LENGTH /
            ( sliderValue * ( RATE_SLIDER_MAXIMUM - 1.0 ) + RATE_SLIDER_LENGTH ));
    }

    THEMIM->getIM()->setRate(rate);
}

void SpeedControlWidget::resetRate()
{
    THEMIM->getIM()->setRate(INPUT_RATE_DEFAULT);
}
