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
    i_vout = 0;
    videoSize.rwidth() = -1;
    videoSize.rheight() = -1;

    hide();

    /* Set the policy to expand in both directions */
    setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding );

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
#if HAS_QT43
    connect( this, SIGNAL(askVideoWidgetToShow( unsigned int, unsigned int)),
             this, SLOT(SetSizing(unsigned int, unsigned int )),
             Qt::BlockingQueuedConnection );
#else
#warning This is broken. Fix it with a QEventLoop with a processEvents ()
    connect( this, SIGNAL(askVideoWidgetToShow( unsigned int, unsigned int)),
             this, SLOT(SetSizing(unsigned int, unsigned int )) );
#endif
}

void VideoWidget::paintEvent(QPaintEvent *ev)
{
    QFrame::paintEvent(ev);
#ifdef Q_WS_X11
    XFlush( QX11Info::display() );
#endif
}

/* Kill the vout at Destruction */
VideoWidget::~VideoWidget()
{
    vout_thread_t *p_vout = i_vout ?
        (vout_thread_t *)vlc_object_get( i_vout ) : NULL;

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
        vlc_object_release( p_vout );
    }
}

/**
 * Request the video to avoid the conflicts
 **/
void *VideoWidget::request( vout_thread_t *p_nvout, int *pi_x, int *pi_y,
                            unsigned int *pi_width, unsigned int *pi_height )
{
    msg_Dbg( p_intf, "Video was requested %i, %i", *pi_x, *pi_y );
    emit askVideoWidgetToShow( *pi_width, *pi_height );
    if( i_vout )
    {
        msg_Dbg( p_intf, "embedded video already in use" );
        return NULL;
    }
    i_vout = p_nvout->i_object_id;
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

void VideoWidget::release( void *p_win )
{
    msg_Dbg( p_intf, "Video is not needed anymore" );
    i_vout = 0;
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
#define MIN_BG_SIZE 64

BackgroundWidget::BackgroundWidget( intf_thread_t *_p_i )
                 :QWidget( NULL ), p_intf( _p_i )
{
    /* We should use that one to take the more size it can */
    setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding);

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
{}

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
 * TEH controls
 **********************************************************************/

#define setupSmallButton( aButton ){  \
    aButton->setMaximumSize( QSize( 26, 26 ) ); \
    aButton->setMinimumSize( QSize( 26, 26 ) ); \
    aButton->setIconSize( QSize( 20, 20 ) ); }

/* init static variables in advanced controls */
mtime_t AdvControlsWidget::timeA = 0;
mtime_t AdvControlsWidget::timeB = 0;

AdvControlsWidget::AdvControlsWidget( intf_thread_t *_p_i, bool b_fsCreation = false ) :
                                           QFrame( NULL ), p_intf( _p_i )
{
    QHBoxLayout *advLayout = new QHBoxLayout( this );
    advLayout->setMargin( 0 );
    advLayout->setSpacing( 0 );
    advLayout->setAlignment( Qt::AlignBottom );

    /* A to B Button */
    ABButton = new QPushButton;
    setupSmallButton( ABButton );
    advLayout->addWidget( ABButton );
    BUTTON_SET_ACT_I( ABButton, "", atob_nob,
      qtr( "Loop from point A to point B continuously.\nClick to set point A" ),
      fromAtoB() );
    timeA = timeB = 0;
    i_last_input_id = 0;
    /* in FS controller we skip this, because we dont want to have it double
       controlled */
    if( !b_fsCreation )
        CONNECT( THEMIM->getIM(), positionUpdated( float, int, int ),
                 this, AtoBLoop( float, int, int ) );
    /* set up synchronization between main controller and fs controller */
    CONNECT( THEMIM->getIM(), advControlsSetIcon(), this, setIcon() );
    connect( this, SIGNAL( timeChanged() ),
        THEMIM->getIM(), SIGNAL( advControlsSetIcon()));
#if 0
    frameButton = new QPushButton( "Fr" );
    frameButton->setMaximumSize( QSize( 26, 26 ) );
    frameButton->setIconSize( QSize( 20, 20 ) );
    advLayout->addWidget( frameButton );
    BUTTON_SET_ACT( frameButton, "Fr", qtr( "Frame by frame" ), frame() );
#endif

    /* Record Button */
    recordButton = new QPushButton;
    setupSmallButton( recordButton );
    advLayout->addWidget( recordButton );
    BUTTON_SET_ACT_I( recordButton, "", record,
            qtr( "Record" ), record() );

    /* Snapshot Button */
    snapshotButton = new QPushButton;
    setupSmallButton( snapshotButton );
    advLayout->addWidget( snapshotButton );
    BUTTON_SET_ACT_I( snapshotButton, "", snapshot,
            qtr( "Take a snapshot" ), snapshot() );
}

AdvControlsWidget::~AdvControlsWidget()
{}

void AdvControlsWidget::enableInput( bool enable )
{
    int i_input_id = 0;
    if( THEMIM->getInput() != NULL )
    {
        input_item_t *p_item = input_GetItem( THEMIM->getInput() );
        i_input_id = p_item->i_id;

        if( var_Type( THEMIM->getInput(), "record-toggle" ) == VLC_VAR_VOID )
            recordButton->setVisible( true );
        else
            recordButton->setVisible( false );
    }
    else
        recordButton->setVisible( false );

    ABButton->setEnabled( enable );
    recordButton->setEnabled( enable );

    if( enable && ( i_last_input_id != i_input_id ) )
    {
        timeA = timeB = 0;
        i_last_input_id = i_input_id;
        emit timeChanged();
    }
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
        emit timeChanged();
        return;
    }
    if( !timeB )
    {
        timeB = var_GetTime( THEMIM->getInput(), "time"  );
        var_SetTime( THEMIM->getInput(), "time" , timeA );
        emit timeChanged();
        return;
    }
    timeA = 0;
    timeB = 0;
    emit timeChanged();
}

/* setting/synchro icons after click on main or fs controller */
void AdvControlsWidget::setIcon()
{
    if( !timeA && !timeB)
    {
        ABButton->setIcon( QIcon( ":/atob_nob" ) );
        ABButton->setToolTip( qtr( "Loop from point A to point B continuously\nClick to set point A" ) );
    }
    else if( timeA && !timeB )
    {
        ABButton->setIcon( QIcon( ":/atob_noa" ) );
        ABButton->setToolTip( qtr( "Click to set point B" ) );
    }
    else if( timeA && timeB )
    {
        ABButton->setIcon( QIcon( ":/atob" ) );
        ABButton->setToolTip( qtr( "Stop the A to B loop" ) );
    }
}

/* Function called regularly when in an AtoB loop */
void AdvControlsWidget::AtoBLoop( float f_pos, int i_time, int i_length )
{
    if( timeB )
    {
        if( ( i_time >= (int)( timeB/1000000 ) )
            || ( i_time < (int)( timeA/1000000 ) ) )
            var_SetTime( THEMIM->getInput(), "time" , timeA );
    }
}

void AdvControlsWidget::record()
{
    input_thread_t *p_input = THEMIM->getInput();
    if( p_input )
    {
        /* This method won't work fine if the stream can't be cut anywhere */
        if( var_Type( p_input, "record-toggle" ) == VLC_VAR_VOID )
            var_TriggerCallback( p_input, "record-toggle" );
#if 0
        else
        {
            /* 'record' access-filter is not loaded, we open Save dialog */
            input_item_t *p_item = input_GetItem( p_input );
            if( !p_item )
                return;

            char *psz = input_item_GetURI( p_item );
            if( psz )
                THEDP->streamingDialog( NULL, psz, true );
        }
#endif
    }
}

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
    setSizePolicy( QSizePolicy::Preferred , QSizePolicy::Maximum );

    /** The main Slider **/
    slider = new InputSlider( Qt::Horizontal, NULL );
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

    fasterButton = new QToolButton;
    fasterButton->setAutoRaise( true );
    fasterButton->setMaximumSize( QSize( 26, 20 ) );

    BUTTON_SET_ACT( fasterButton, "+", qtr( "Faster" ), faster() );

    /* advanced Controls handling */
    b_advancedVisible = b_advControls;

    advControls = new AdvControlsWidget( p_intf, b_fsCreation );
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

    BUTTON_SET_IMG( prevSectionButton, "", dvd_prev, "" );
    BUTTON_SET_IMG( nextSectionButton, "", dvd_next, "" );
    BUTTON_SET_IMG( menuButton, "", dvd_menu, qtr( "Menu" ) );

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

    telexOn = new QPushButton;
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

    telexFrame->hide(); /* default hidden */

    CONNECT( telexPage, valueChanged( int ), THEMIM->getIM(),
             telexGotoPage( int ) );
    CONNECT( THEMIM->getIM(), setNewTelexPage( int ),
              telexPage, setValue( int ) );

    BUTTON_SET_IMG( telexOn, "", tv, qtr( "Teletext on" ) );

    CONNECT( telexOn, clicked(), THEMIM->getIM(),
             telexToggleButtons() );
    CONNECT( telexOn, clicked( bool ), THEMIM->getIM(),
             telexToggle( bool ) );
    CONNECT( THEMIM->getIM(), toggleTelexButtons(),
              this, toggleTeletext() );
    b_telexEnabled = false;
    telexTransparent->setEnabled( false );
    telexPage->setEnabled( false );

    BUTTON_SET_IMG( telexTransparent, "", tvtelx, qtr( "Teletext" ) );
    CONNECT( telexTransparent, clicked( bool ),
             THEMIM->getIM(), telexSetTransparency() );
    CONNECT( THEMIM->getIM(), toggleTelexTransparency(),
              this, toggleTeletextTransparency() );
    CONNECT( THEMIM->getIM(), teletextEnabled( bool ),
             this, enableTeletext( bool ) );

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

    BUTTON_SET_ACT_I( playButton, "", play_b, qtr( I_PLAY_TOOLTIP ), play() );
    BUTTON_SET_ACT_I( prevButton, "" , previous_b,
                      qtr( "Previous media in the playlist" ), prev() );
    BUTTON_SET_ACT_I( nextButton, "", next_b,
                      qtr( "Next media in the playlist" ), next() );
    BUTTON_SET_ACT_I( stopButton, "", stop_b, qtr( "Stop playback" ), stop() );

    /*
     * Other first Line buttons
     */
    /** Fullscreen/Visualisation **/
    fullscreenButton = new QPushButton;
    BUTTON_SET_ACT_I( fullscreenButton, "", fullscreen,
            qtr( "Toggle the video in fullscreen" ), fullscreen() );
    setupSmallButton( fullscreenButton );

    if( !b_fsCreation )
    {
        /** Playlist Button **/
        playlistButton = new QPushButton;
        setupSmallButton( playlistButton );
        BUTTON_SET_IMG( playlistButton, "" , playlist, qtr( "Show playlist" ) );
        CONNECT( playlistButton, clicked(), _p_mi, togglePlaylist() );

        /** extended Settings **/
        extSettingsButton = new QPushButton;
        BUTTON_SET_ACT_I( extSettingsButton, "", extended,
                qtr( "Show extended settings" ), extSettings() );
        setupSmallButton( extSettingsButton );
    }

    /* Volume */
    hVolLabel = new VolumeClickHandler( p_intf, this );

    volMuteLabel = new QLabel;
    volMuteLabel->setPixmap( QPixmap( ":/volume-medium" ) );
    volMuteLabel->installEventFilter( hVolLabel );

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
    volumeSlider->setMinimumSize( QSize( 85, 30 ) );
    volumeSlider->setFocusPolicy( Qt::NoFocus );

    /* Set the volume from the config */
    volumeSlider->setValue( ( config_GetInt( p_intf, "volume" ) ) *
                              VOLUME_MAX / (AOUT_VOLUME_MAX/2) );

    /* Force the update at build time in order to have a muted icon if needed */
    updateVolume( volumeSlider->value() );

    /* Volume control connection */
    CONNECT( volumeSlider, valueChanged( int ), this, updateVolume( int ) );
    CONNECT( THEMIM, volumeChanged( void ), this, updateVolume( void ) );

    if( !b_fsCreation )
    {
        controlLayout = new QGridLayout( this );

        controlLayout->setSpacing( 0 );
        controlLayout->setLayoutMargins( 7, 5, 7, 3, 6 );

        controlLayout->addWidget( slider, 0, 1, 1, 18 );
        controlLayout->addWidget( slowerButton, 0, 0 );
        controlLayout->addWidget( fasterButton, 0, 19 );

        controlLayout->addWidget( discFrame, 1, 8, 2, 3, Qt::AlignBottom );
        controlLayout->addWidget( telexFrame, 1, 8, 2, 5, Qt::AlignBottom );

        controlLayout->addWidget( playButton, 2, 0, 2, 2, Qt::AlignBottom );
        controlLayout->setColumnMinimumWidth( 2, 10 );
        controlLayout->setColumnStretch( 2, 0 );

        controlLayout->addLayout( controlButLayout, 3, 3, 1, 3, Qt::AlignBottom );
        /* Column 6 is unused */
        controlLayout->setColumnStretch( 6, 0 );
        controlLayout->setColumnStretch( 7, 0 );
        controlLayout->setColumnMinimumWidth( 7, 10 );

        controlLayout->addWidget( fullscreenButton, 3, 8, Qt::AlignBottom );
        controlLayout->addWidget( playlistButton, 3, 9, Qt::AlignBottom );
        controlLayout->addWidget( extSettingsButton, 3, 10, Qt::AlignBottom );
        controlLayout->setColumnStretch( 11, 0 ); /* telex alignment */

        controlLayout->setColumnStretch( 12, 0 );
        controlLayout->setColumnMinimumWidth( 12, 10 );

        controlLayout->addWidget( advControls, 3, 13, 1, 3, Qt::AlignBottom );

        controlLayout->setColumnStretch( 16, 10 );
        controlLayout->setColumnMinimumWidth( 16, 10 );

        controlLayout->addWidget( volMuteLabel, 3, 17, Qt::AlignBottom );
        controlLayout->addWidget( volumeSlider, 3, 18, 1 , 2, Qt::AlignBottom );
    }

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

void ControlsWidget::enableTeletext( bool b_enable )
{
    telexFrame->setVisible( b_enable );
    bool b_on = THEMIM->teletextState();

    telexOn->setChecked( b_on );
    telexTransparent->setEnabled( b_on );
    telexPage->setEnabled( b_on );
    b_telexEnabled = b_on;
}

void ControlsWidget::toggleTeletextTransparency()
{
    if( b_telexTransparent )
    {
        telexTransparent->setIcon( QIcon( ":/tvtelx" ) );
        telexTransparent->setToolTip( qtr( "Teletext" ) );
        b_telexTransparent = false;
    }
    else
    {
        telexTransparent->setIcon( QIcon( ":/tvtelx-transparent" ) );
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
#define HELP_PCH N_( "Previous chapter" )
#define HELP_NCH N_( "Next chapter" )

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
    {
        volMuteLabel->setPixmap( QPixmap(":/volume-muted" ) );
        volMuteLabel->setToolTip( qtr( "Unmute" ) );
        return;
    }

    if( i_sliderVolume < VOLUME_MAX / 3 )
        volMuteLabel->setPixmap( QPixmap( ":/volume-low" ) );
    else if( i_sliderVolume > (VOLUME_MAX * 2 / 3 ) )
        volMuteLabel->setPixmap( QPixmap( ":/volume-high" ) );
    else volMuteLabel->setPixmap( QPixmap( ":/volume-medium" ) );
    volMuteLabel->setToolTip( qtr( "Mute" ) );
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
        playButton->setIcon( QIcon( ":/pause_b" ) );
        playButton->setToolTip( qtr( "Pause the playback" ) );
    }
    else
    {
        playButton->setIcon( QIcon( ":/play_b" ) );
        playButton->setToolTip( qtr( I_PLAY_TOOLTIP ) );
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
    slider->setSliderPosition ( 0 );
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
    if( advControls && !b_advancedVisible )
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
          i_mouse_last_x( -1 ), i_mouse_last_y( -1 ), b_mouse_over(false),
          b_slow_hide_begin(false), i_slow_hide_timeout(1),
          b_fullscreen( false ), i_hide_timeout( 1 ), p_vout(NULL)
{
    setWindowFlags( Qt::ToolTip );

    setFrameShape( QFrame::StyledPanel );
    setFrameStyle( QFrame::Sunken );
    setSizePolicy( QSizePolicy::Minimum, QSizePolicy::Minimum );

    QGridLayout *fsLayout = new QGridLayout( this );
    fsLayout->setLayoutMargins( 5, 2, 5, 2, 5 );

    /* First line */
    slider->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Minimum);
    slider->setMinimumWidth( 220 );
    fsLayout->addWidget( slowerButton, 0, 0 );
    fsLayout->addWidget( slider, 0, 1, 1, 9 );
    fsLayout->addWidget( fasterButton, 0, 10 );

    fsLayout->addWidget( playButton, 1, 0, 1, 2 );
    fsLayout->addLayout( controlButLayout, 1, 2 );

    fsLayout->addWidget( discFrame, 1, 3 );
    fsLayout->addWidget( telexFrame, 1, 4 );
    fsLayout->addWidget( fullscreenButton, 1, 5 );
    fsLayout->addWidget( advControls, 1, 6, Qt::AlignVCenter );

    fsLayout->setColumnStretch( 7, 10 );
    fsLayout->addWidget( volMuteLabel, 1, 8 );
    fsLayout->addWidget( volumeSlider, 1, 9, 1, 2 );

    /* hiding timer */
    p_hideTimer = new QTimer( this );
    CONNECT( p_hideTimer, timeout(), this, hideFSC() );
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
    b_fscHidden = true;
    adjustSize();
    show();
#endif

    fullscreenButton->setIcon( QIcon( ":/defullscreen" ) );

    vlc_mutex_init_recursive( &lock );
}

FullscreenControllerWidget::~FullscreenControllerWidget()
{
    detachVout();
    vlc_mutex_destroy( &lock );
}

/**
 * Show fullscreen controller
 */
void FullscreenControllerWidget::showFSC()
{
    adjustSize();
#ifdef WIN32TRICK
    // after quiting and going to fs, we need to call show()
    if( isHidden() )
        show();

    if( b_fscHidden )
    {
        b_fscHidden = false;
        setWindowOpacity( 1.0 );
    }
#else
    show();
#endif

#if HAVE_TRANSPARENCY
    setWindowOpacity( DEFAULT_OPACITY );
#endif
}

/**
 * Hide fullscreen controller
 * FIXME: under windows it have to be done by moving out of screen
 *        because hide() doesnt work
 */
void FullscreenControllerWidget::hideFSC()
{
#ifdef WIN32TRICK
    b_fscHidden = true;
    setWindowOpacity( 0.0 );    // simulate hidding
#else
    hide();
#endif
}

/**
 * Plane to hide fullscreen controller
 */
void FullscreenControllerWidget::planHideFSC()
{
    vlc_mutex_lock( &lock );
    int i_timeout = i_hide_timeout;
    vlc_mutex_unlock( &lock );

    p_hideTimer->start( i_timeout );

#if HAVE_TRANSPARENCY
    b_slow_hide_begin = true;
    i_slow_hide_timeout = i_timeout;
    p_slowHideTimer->start( i_slow_hide_timeout / 2 );
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
    if( b_slow_hide_begin )
    {
        b_slow_hide_begin = false;

        p_slowHideTimer->stop();
        /* the last part of time divided to 100 pieces */
        p_slowHideTimer->start( (int)( i_slow_hide_timeout / 2 / ( windowOpacity() * 100 ) ) );

    }
    else
    {
#ifdef WIN32TRICK
         if ( windowOpacity() > 0.0 && !b_fscHidden )
#else
         if ( windowOpacity() > 0.0 )
#endif
         {
             /* we should use 0.01 because of 100 pieces ^^^
                but than it cannt be done in time */
             setWindowOpacity( windowOpacity() - 0.02 );
         }

         if ( windowOpacity() <= 0.0 )
             p_slowHideTimer->stop();
    }
#endif
}

/**
 * event handling
 * events: show, hide, start timer for hidding
 */
void FullscreenControllerWidget::customEvent( QEvent *event )
{
    bool b_fs;

    switch( event->type() )
    {
        case FullscreenControlToggle_Type:
            vlc_mutex_lock( &lock );
            b_fs = b_fullscreen;
            vlc_mutex_unlock( &lock );
            if( b_fs )
#ifdef WIN32TRICK
                if( b_fscHidden )
#else
                if( isHidden() )
#endif
                {
                    p_hideTimer->stop();
                    showFSC();
                }
                else
                    hideFSC();
            break;
        case FullscreenControlShow_Type:
            vlc_mutex_lock( &lock );
            b_fs = b_fullscreen;
            vlc_mutex_unlock( &lock );

            if( b_fs )  // FIXME I am not sure about that one
                showFSC();
            break;
        case FullscreenControlHide_Type:
            hideFSC();
            break;
        case FullscreenControlPlanHide_Type:
            if( !b_mouse_over ) // Only if the mouse is not over FSC
                planHideFSC();
            break;
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
        int i_moveX = event->globalX() - i_mouse_last_x;
        int i_moveY = event->globalY() - i_mouse_last_y;

        move( x() + i_moveX, y() + i_moveY );

        i_mouse_last_x = event->globalX();
        i_mouse_last_y = event->globalY();
    }
}

/**
 * On mouse press
 * store position of cursor
 */
void FullscreenControllerWidget::mousePressEvent( QMouseEvent *event )
{
    i_mouse_last_x = event->globalX();
    i_mouse_last_y = event->globalY();
}

/**
 * On mouse go above FSC
 */
void FullscreenControllerWidget::enterEvent( QEvent *event )
{
    b_mouse_over = true;

    p_hideTimer->stop();
#if HAVE_TRANSPARENCY
    p_slowHideTimer->stop();
#endif
}

/**
 * On mouse go out from FSC
 */
void FullscreenControllerWidget::leaveEvent( QEvent *event )
{
    planHideFSC();

    b_mouse_over = false;
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

/* */
static int FullscreenControllerWidgetFullscreenChanged( vlc_object_t *vlc_object, const char *variable,
                                                        vlc_value_t old_val, vlc_value_t new_val,
                                                        void *data )
{
    vout_thread_t *p_vout = (vout_thread_t *) vlc_object;
    FullscreenControllerWidget *p_fs = (FullscreenControllerWidget *)data;

    p_fs->fullscreenChanged( p_vout, new_val.b_bool, var_GetInteger( p_vout, "mouse-hide-timeout" ) );

    return VLC_SUCCESS;
}
/* */
static int FullscreenControllerWidgetMouseMoved( vlc_object_t *vlc_object, const char *variable,
                                                 vlc_value_t old_val, vlc_value_t new_val,
                                                 void *data )
{
    FullscreenControllerWidget *p_fs = (FullscreenControllerWidget *)data;

    /* Show event */
    IMEvent *eShow = new IMEvent( FullscreenControlShow_Type, 0 );
    QApplication::postEvent( p_fs, static_cast<QEvent *>(eShow) );

    /* Plan hide event */
    IMEvent *eHide = new IMEvent( FullscreenControlPlanHide_Type, 0 );
    QApplication::postEvent( p_fs, static_cast<QEvent *>(eHide) );

    return VLC_SUCCESS;
}


/**
 * It is called when video start
 */
void FullscreenControllerWidget::attachVout( vout_thread_t *p_nvout )
{
    assert( p_nvout && !p_vout );

    p_vout = p_nvout;

    vlc_mutex_lock( &lock );
    var_AddCallback( p_vout, "fullscreen", FullscreenControllerWidgetFullscreenChanged, this ); /* I miss a add and fire */
    fullscreenChanged( p_vout, var_GetBool( p_vout, "fullscreen" ), var_GetInteger( p_vout, "mouse-hide-timeout" ) );
    vlc_mutex_unlock( &lock );
}
/**
 * It is called after turn off video.
 */
void FullscreenControllerWidget::detachVout()
{
    if( p_vout )
    {
        var_DelCallback( p_vout, "fullscreen", FullscreenControllerWidgetFullscreenChanged, this );
        vlc_mutex_lock( &lock );
        fullscreenChanged( p_vout, false, 0 );
        vlc_mutex_unlock( &lock );
        p_vout = NULL;
    }
}

/**
 * Register and unregister callback for mouse moving
 */
void FullscreenControllerWidget::fullscreenChanged( vout_thread_t *p_vout, bool b_fs, int i_timeout )
{
    vlc_mutex_lock( &lock );
    if( b_fs && !b_fullscreen )
    {
        b_fullscreen = true;
        i_hide_timeout = i_timeout;
        var_AddCallback( p_vout, "mouse-moved", FullscreenControllerWidgetMouseMoved, this );
    }
    else if( !b_fs && b_fullscreen )
    {
        b_fullscreen = false;
        i_hide_timeout = i_timeout;
        var_DelCallback( p_vout, "mouse-moved", FullscreenControllerWidgetMouseMoved, this );

        /* Force fs hidding */
        IMEvent *eHide = new IMEvent( FullscreenControlHide_Type, 0 );
        QApplication::postEvent( this, static_cast<QEvent *>(eHide) );
    }
    vlc_mutex_unlock( &lock );
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
