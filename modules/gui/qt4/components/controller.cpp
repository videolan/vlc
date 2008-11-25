/*****************************************************************************
 * Controller.cpp : Controller for the main interface
 ****************************************************************************
 * Copyright ( C ) 2006-2008 the VideoLAN team
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
#include <QToolButton>
#include <QHBoxLayout>
#include <QMenu>
#include <QPalette>
#include <QResizeEvent>
#include <QDate>
#include <QSignalMapper>
#include <QTimer>

#define I_PLAY_TOOLTIP N_("Play\nIf the playlist is empty, open a media")

/**********************************************************************
 * TEH controls
 **********************************************************************/

/******
  * This is an abstract Toolbar/Controller
  * This has helper to create any toolbar, any buttons and to manage the actions
  *
  *****/
AbstractController::AbstractController( intf_thread_t * _p_i ) : QFrame( NULL )
{
    p_intf = _p_i;

    /* We need one layout. An controller without layout is stupid with 
       current architecture */
    controlLayout = new QGridLayout( this );

    /* Main action provider */
    toolbarActionsMapper = new QSignalMapper( this );
    CONNECT( toolbarActionsMapper, mapped( int ),
             this, doAction( int ) );
    CONNECT( THEMIM->getIM(), statusChanged( int ), this, setStatus( int ) );
}

/* Reemit some signals on status Change to activate some buttons */
void AbstractController::setStatus( int status )
{
    bool b_hasInput = THEMIM->getIM()->hasInput();
    /* Activate the interface buttons according to the presence of the input */
    emit inputExists( b_hasInput );

    emit inputPlaying( status == PLAYING_S );

    emit inputIsRecordable( b_hasInput &&
                            var_GetBool( THEMIM->getInput(), "can-record" ) );
}

/* Generic button setup */
void AbstractController::setupButton( QAbstractButton *aButton )
{
    static QSizePolicy sizePolicy( QSizePolicy::Fixed, QSizePolicy::Fixed );
    sizePolicy.setHorizontalStretch( 0 );
    sizePolicy.setVerticalStretch( 0 );

    aButton->setSizePolicy( sizePolicy );
    aButton->setFixedSize( QSize( 26, 26 ) );
    aButton->setIconSize( QSize( 20, 20 ) );
    aButton->setFocusPolicy( Qt::NoFocus );
}

/* Open the generic config line for the toolbar, parse it
 * and create the widgets accordingly */
void AbstractController::parseAndCreateLine( QString config, int i_line )
{
    int i_column = 0;
    QStringList list = config.split( ";" ) ;
    for( int i = 0; i < list.size(); i++ )
    {
         QStringList list2 = list.at( i ).split( "-" );
         if( list2.size() < 1 )
         {
             msg_Warn( p_intf, "Parsing error. Report this" );
             continue;
         }

         bool ok;
         int i_option = WIDGET_NORMAL;
         buttonType_e i_type = (buttonType_e)list2.at( 0 ).toInt( &ok );
         if( !ok )
         {
             msg_Warn( p_intf, "Parsing error 0. Please report this" );
             continue;
         }
         /* Special case for SPACERS, who aren't QWidgets */
         if( i_type == WIDGET_SPACER || i_type == WIDGET_SPACER_EXTEND )
         {
             controlLayout->setColumnMinimumWidth( i, 10 );
             i_column++;
             controlLayout->setColumnStretch( i,
                     ( i_type == WIDGET_SPACER ) ? 0 : 10 );
             continue;
         }

         if( list2.size() > 1 )
         {
             i_option = list2.at( 1 ).toInt( &ok );
             if( !ok )
             {
                 msg_Warn( p_intf, "Parsing error 1. Please report this" );
                 continue;
             }
         }

         int i_size;
         QWidget *widg = createWidget( i_type, &i_size, i_option );
         if( !widg ) continue;

         if( list2.size() > 2 )
         {
             i_size = list.at( 2 ).toInt( &ok );

             if( !ok )
             {
                 msg_Warn( p_intf, "Parsing error 2. Please report this" );
                 continue;
             }
         }

         controlLayout->addWidget( widg, i_line, i_column, 1, i_size );
         i_column += i_size;
    }
}

#define CONNECT_MAP( a ) CONNECT( a, clicked(),  toolbarActionsMapper, map() )
#define SET_MAPPING( a, b ) toolbarActionsMapper->setMapping( a , b )
#define CONNECT_MAP_SET( a, b ) \
    CONNECT_MAP( a ); \
    SET_MAPPING( a, b );
#define BUTTON_SET_BAR( button, image, tooltip ) \
    button->setToolTip( tooltip );          \
    button->setIcon( QIcon( ":/"#image ) );

#define ENABLE_ON_VIDEO( a ) \
    CONNECT( THEMIM->getIM(), voutChanged( bool ), a, setEnabled( bool ) ); \
    a->setEnabled( THEMIM->getIM()->hasVideo() ); /* TODO: is this necessary? when input is started before the interface? */

#define ENABLE_ON_INPUT( a ) \
    CONNECT( this, inputExists( bool ), a, setEnabled( bool ) ); \
    a->setEnabled( THEMIM->getIM()->hasInput() ); /* TODO: is this necessary? when input is started before the interface? */

QWidget *AbstractController::createWidget( buttonType_e button, int* i_size,
        int options )
{
    assert( i_size );

    bool b_flat = options & WIDGET_FLAT;
    bool b_big = options & WIDGET_BIG;
    bool b_shiny = options & WIDGET_SHINY;
    *i_size = 1; 

    QWidget *widget = NULL;
    switch( button )
    {
    case PLAY_BUTTON: {
        PlayButton *playButton = new PlayButton;
        setupButton( playButton );
        BUTTON_SET_BAR( playButton, play_b, qtr( I_PLAY_TOOLTIP ) );
        CONNECT_MAP_SET( playButton, PLAY_ACTION );
        CONNECT( this, inputPlaying( bool ),
                 playButton, updateButton( bool ));
        widget = playButton;
        }
        break;
    case STOP_BUTTON:{
        QToolButton *stopButton = new QToolButton;
        setupButton( stopButton );
        CONNECT_MAP_SET( stopButton, STOP_ACTION );
        BUTTON_SET_BAR( stopButton, stop_b, qtr( "Stop playback" ) );
        widget = stopButton;
        }
        break;
    case PREVIOUS_BUTTON:{
        QToolButton *prevButton = new QToolButton;
        setupButton( prevButton );
        CONNECT_MAP_SET( prevButton, PREVIOUS_ACTION );
        BUTTON_SET_BAR( prevButton, previous_b,
                  qtr( "Previous media in the playlist" ) );
        widget = prevButton;
        }
        break;
    case NEXT_BUTTON:
        {
        QToolButton *nextButton = new QToolButton;
        setupButton( nextButton );
        CONNECT_MAP_SET( nextButton, NEXT_ACTION );
        BUTTON_SET_BAR( nextButton, next_b,
                qtr( "Next media in the playlist" ) );
        widget = nextButton;
        }
        break;
    case SLOWER_BUTTON:{
        QToolButton *slowerButton = new QToolButton;
        setupButton( slowerButton );
        CONNECT_MAP_SET( slowerButton, SLOWER_ACTION );
        BUTTON_SET_BAR( slowerButton, slower, qtr( "Slower" ) );
        ENABLE_ON_INPUT( slowerButton );
        widget = slowerButton;
        }
        break;
    case FASTER_BUTTON:{
        QToolButton *fasterButton = new QToolButton;
        setupButton( fasterButton );
        CONNECT_MAP_SET( fasterButton, FASTER_ACTION );
        BUTTON_SET_BAR( fasterButton, faster, qtr( "Faster" ) );
        ENABLE_ON_INPUT( fasterButton );
        widget = fasterButton;
        }
        break;
    case FRAME_BUTTON: {
        QToolButton *frameButton = new QToolButton;
        setupButton( frameButton );
        CONNECT_MAP_SET( frameButton, FRAME_ACTION );
        BUTTON_SET_BAR( frameButton, frame, qtr( "Frame by frame" ) );
        ENABLE_ON_VIDEO( frameButton );
        widget = frameButton;
        }
        break;
    case FULLSCREEN_BUTTON:{
        QToolButton *fullscreenButton = new QToolButton;
        setupButton( fullscreenButton );
        CONNECT_MAP_SET( fullscreenButton, FULLSCREEN_ACTION );
        BUTTON_SET_BAR( fullscreenButton, fullscreen,
                qtr( "Toggle the video in fullscreen" ) );
        ENABLE_ON_VIDEO( fullscreenButton );
        widget = fullscreenButton;
        }
        break;
    case DEFULLSCREEN_BUTTON:{
        QToolButton *fullscreenButton = new QToolButton;
        setupButton( fullscreenButton );
        CONNECT_MAP_SET( fullscreenButton, FULLSCREEN_ACTION );
        BUTTON_SET_BAR( fullscreenButton, defullscreen,
                qtr( "Toggle the video in fullscreen" ) );
        ENABLE_ON_VIDEO( fullscreenButton );
        widget = fullscreenButton;
        }
        break;
    case EXTENDED_BUTTON:{
        QToolButton *extSettingsButton = new QToolButton;
        setupButton( extSettingsButton );
        CONNECT_MAP_SET( extSettingsButton, EXTENDED_ACTION );
        BUTTON_SET_BAR( extSettingsButton, extended,
                qtr( "Show extended settings" ) );
        widget = extSettingsButton;
        }
        break;
    case PLAYLIST_BUTTON:{
        QToolButton *playlistButton = new QToolButton;
        setupButton( playlistButton );
        CONNECT_MAP_SET( playlistButton, PLAYLIST_ACTION );
        BUTTON_SET_BAR( playlistButton, playlist,
                qtr( "Show playlist" ) );
        widget = playlistButton;
        }
        break;
    case SNAPSHOT_BUTTON:{
        QToolButton *snapshotButton = new QToolButton;
        setupButton( snapshotButton );
        CONNECT_MAP_SET( snapshotButton, SNAPSHOT_ACTION );
        BUTTON_SET_BAR( snapshotButton, snapshot, qtr( "Take a snapshot" ) );
        ENABLE_ON_VIDEO( snapshotButton );
        widget = snapshotButton;
        }
        break;
    case RECORD_BUTTON:{
        QToolButton *recordButton = new QToolButton;
        setupButton( recordButton );
        CONNECT_MAP_SET( recordButton, RECORD_ACTION );
        BUTTON_SET_BAR( recordButton, record, qtr( "Record" ) );
        ENABLE_ON_INPUT( recordButton );
        widget = recordButton;
        }
        break;
    case ATOB_BUTTON: {
        AtoB_Button *ABButton = new AtoB_Button;
        setupButton( ABButton );
        BUTTON_SET_BAR( ABButton, atob_nob, qtr( "Loop from point A to point "
                    "B continuously.\nClick to set point A" ) );
        ENABLE_ON_INPUT( ABButton );
        CONNECT_MAP_SET( ABButton, ATOB_ACTION );
        CONNECT( THEMIM->getIM(), AtoBchanged( bool, bool),
                 ABButton, setIcons( bool, bool ) );
        widget = ABButton;
        }
        break;
    case INPUT_SLIDER: {
        InputSlider *slider = new InputSlider( Qt::Horizontal, NULL );
        /* Update the position when the IM has changed */
        CONNECT( THEMIM->getIM(), positionUpdated( float, int, int ),
                slider, setPosition( float, int, int ) );
        /* And update the IM, when the position has changed */
        CONNECT( slider, sliderDragged( float ),
                THEMIM->getIM(), sliderUpdate( float ) );
        /* *i_size = -1; */
        widget = slider;
        }
        break;
    case MENU_BUTTONS:
        widget = discFrame();
        widget->hide();
        *i_size = 3;
        break;
    case TELETEXT_BUTTONS:
        widget = telexFrame();
        widget->hide();
        *i_size = 4;
        break;
    case VOLUME:
        {
            SoundWidget *snd = new SoundWidget( p_intf, b_shiny );
            widget = snd;
            *i_size = 4;
        }
        break;
    case TIME_LABEL:
        {
            TimeLabel *timeLabel = new TimeLabel( p_intf );
            widget = timeLabel;
        }
        break;
    case SPLITTER:
        {
            QFrame *line = new QFrame;
            line->setFrameShape( QFrame::VLine );
            line->setFrameShadow( QFrame::Raised );
            line->setLineWidth( 0 );
            line->setMidLineWidth( 1 );
            widget = line;
        }
        break;
    case ADVANCED_CONTROLLER:
        {
            advControls = new AdvControlsWidget( p_intf );
            widget = advControls;
            *i_size = advControls->getWidth();
        }
        break;
    case REVERSE_BUTTON:{
        QToolButton *reverseButton = new QToolButton;
        setupButton( reverseButton );
        CONNECT_MAP_SET( reverseButton, REVERSE_ACTION );
        BUTTON_SET_BAR( reverseButton, reverse, qtr( "Reverse" ) );
        ENABLE_ON_INPUT( reverseButton );
        widget = reverseButton;
        }
        break;
    default:
        msg_Warn( p_intf, "This should not happen" );
        break;
    }

    /* Customize Buttons */
    if( b_flat || b_big )
    {
        QToolButton *tmpButton = qobject_cast<QToolButton *>(widget);
        if( tmpButton )
        {
            if( b_flat )
                tmpButton->setAutoRaise( b_flat );
            if( b_big )
            {
                tmpButton->setFixedSize( QSize( 32, 32 ) );
                tmpButton->setIconSize( QSize( 26, 26 ) );
                *i_size = 2;
            }
        }
    }
    return widget;
}

QWidget *AbstractController::discFrame()
{
    /** Disc and Menus handling */
    QWidget *discFrame = new QWidget( this );

    QHBoxLayout *discLayout = new QHBoxLayout( discFrame );
    discLayout->setSpacing( 0 ); discLayout->setMargin( 0 );

    QToolButton *prevSectionButton = new QToolButton( discFrame );
    setupButton( prevSectionButton );
    BUTTON_SET_BAR( prevSectionButton, dvd_prev,
            qtr("Previous Chapter/Title" ) );
    discLayout->addWidget( prevSectionButton );

    QToolButton *menuButton = new QToolButton( discFrame );
    setupButton( menuButton );
    discLayout->addWidget( menuButton );
    BUTTON_SET_BAR( menuButton, dvd_menu, qtr( "Menu" ) );

    QToolButton *nextSectionButton = new QToolButton( discFrame );
    setupButton( nextSectionButton );
    discLayout->addWidget( nextSectionButton );
    BUTTON_SET_BAR( nextSectionButton, dvd_next,
            qtr("Next Chapter/Title" ) );


    /* Change the navigation button display when the IM
       navigation changes */
    CONNECT( THEMIM->getIM(), titleChanged( bool ),
            discFrame, setVisible( bool ) );
    CONNECT( THEMIM->getIM(), chapterChanged( bool ),
            menuButton, setVisible( bool ) );
    /* Changes the IM navigation when triggered on the nav buttons */
    CONNECT( prevSectionButton, clicked(), THEMIM->getIM(),
            sectionPrev() );
    CONNECT( nextSectionButton, clicked(), THEMIM->getIM(),
            sectionNext() );
    CONNECT( menuButton, clicked(), THEMIM->getIM(),
            sectionMenu() );

    return discFrame;
}

QWidget *AbstractController::telexFrame()
{
    /**
     * Telextext QFrame
     **/
    TeletextController *telexFrame = new TeletextController;
    QHBoxLayout *telexLayout = new QHBoxLayout( telexFrame );
    telexLayout->setSpacing( 0 ); telexLayout->setMargin( 0 );
    CONNECT( THEMIM->getIM(), teletextPossible( bool ),
             telexFrame, setVisible( bool ) );

    /* On/Off button */
    QToolButton *telexOn = new QToolButton;
    telexFrame->telexOn = telexOn;
    setupButton( telexOn );
    BUTTON_SET_BAR( telexOn, tv, qtr( "Teletext Activation" ) );
    telexLayout->addWidget( telexOn );

    /* Teletext Activation and set */
    CONNECT( telexOn, clicked( bool ),
             THEMIM->getIM(), activateTeletext( bool ) );
    CONNECT( THEMIM->getIM(), teletextActivated( bool ),
             telexFrame, enableTeletextButtons( bool ) );


    /* Transparency button */
    QToolButton *telexTransparent = new QToolButton;
    telexFrame->telexTransparent = telexTransparent;
    setupButton( telexTransparent );
    BUTTON_SET_BAR( telexTransparent, tvtelx,
            qtr( "Toggle Transparency " ) );
    telexTransparent->setEnabled( false );
    telexLayout->addWidget( telexTransparent );

    /* Transparency change and set */
    CONNECT( telexTransparent, clicked( bool ),
            THEMIM->getIM(), telexSetTransparency( bool ) );
    CONNECT( THEMIM->getIM(), teletextTransparencyActivated( bool ),
            telexFrame, toggleTeletextTransparency( bool ) );


    /* Page setting */
    QSpinBox *telexPage = new QSpinBox;
    telexFrame->telexPage = telexPage;
    telexPage->setRange( 0, 999 );
    telexPage->setValue( 100 );
    telexPage->setAccelerated( true );
    telexPage->setWrapping( true );
    telexPage->setAlignment( Qt::AlignRight );
    telexPage->setSizePolicy( QSizePolicy::Preferred, QSizePolicy::Minimum );
    telexPage->setEnabled( false );
    telexLayout->addWidget( telexPage );

    /* Page change and set */
    CONNECT( telexPage, valueChanged( int ),
            THEMIM->getIM(), telexSetPage( int ) );
    CONNECT( THEMIM->getIM(), newTelexPageSet( int ),
            telexPage, setValue( int ) );

    return telexFrame;
}
#undef CONNECT_MAP
#undef SET_MAPPING
#undef CONNECT_MAP_SET
#undef BUTTON_SET_BAR
#undef ENABLE_ON_VIDEO
#undef ENABLE_ON_INPUT

SoundWidget::SoundWidget( intf_thread_t * _p_intf, bool b_shiny )
                         : b_my_volume( false )
{
    p_intf = _p_intf;
    QHBoxLayout *layout = new QHBoxLayout( this );
    layout->setSpacing( 0 ); layout->setMargin( 0 );
    hVolLabel = new VolumeClickHandler( p_intf, this );

    volMuteLabel = new QLabel;
    volMuteLabel->setPixmap( QPixmap( ":/volume-medium" ) );
    volMuteLabel->installEventFilter( hVolLabel );
    layout->addWidget( volMuteLabel );

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
    layout->addWidget( volumeSlider );

    /* Set the volume from the config */
    volumeSlider->setValue( ( config_GetInt( p_intf, "volume" ) ) *
                              VOLUME_MAX / (AOUT_VOLUME_MAX/2) );

    /* Force the update at build time in order to have a muted icon if needed */
    updateVolume( volumeSlider->value() );

    /* Volume control connection */
    CONNECT( volumeSlider, valueChanged( int ), this, updateVolume( int ) );
    CONNECT( THEMIM, volumeChanged( void ), this, updateVolume( void ) );
}

void SoundWidget::updateVolume( int i_sliderVolume )
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

void SoundWidget::updateVolume()
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

void TeletextController::toggleTeletextTransparency( bool b_transparent )
{
    telexTransparent->setIcon( b_transparent ? QIcon( ":/tvtelx" )
                                             : QIcon( ":/tvtelx-trans" ) );
}

void TeletextController::enableTeletextButtons( bool b_enabled )
{
    telexOn->setChecked( b_enabled );
    telexTransparent->setEnabled( b_enabled );
    telexPage->setEnabled( b_enabled );
}

void PlayButton::updateButton( bool b_playing )
{
    setIcon( b_playing ? QIcon( ":/pause_b" ) : QIcon( ":/play_b" ) );
    setToolTip( b_playing ? qtr( "Pause the playback" )
                          : qtr( I_PLAY_TOOLTIP ) );
}

void AtoB_Button::setIcons( bool timeA, bool timeB )
{
    if( !timeA && !timeB)
    {
        setIcon( QIcon( ":/atob_nob" ) );
        setToolTip( qtr( "Loop from point A to point B continuously\n"
                         "Click to set point A" ) );
    }
    else if( timeA && !timeB )
    {
        setIcon( QIcon( ":/atob_noa" ) );
        setToolTip( qtr( "Click to set point B" ) );
    }
    else if( timeA && timeB )
    {
        setIcon( QIcon( ":/atob" ) );
        setToolTip( qtr( "Stop the A to B loop" ) );
    }
}


//* Actions */
void AbstractController::doAction( int id_action )
{
    switch( id_action )
    {
        case PLAY_ACTION:
            play(); break;
        case PREVIOUS_ACTION:
            prev(); break;
        case NEXT_ACTION:
            next(); break;
        case STOP_ACTION:
            stop(); break;
        case SLOWER_ACTION:
            slower(); break;
        case FASTER_ACTION:
            faster(); break;
        case FULLSCREEN_ACTION:
            fullscreen(); break;
        case EXTENDED_ACTION:
            extSettings(); break;
        case PLAYLIST_ACTION:
            playlist(); break;
        case SNAPSHOT_ACTION:
            snapshot(); break;
        case RECORD_ACTION:
            record(); break;
        case ATOB_ACTION:
            THEMIM->getIM()->setAtoB(); break;
        case FRAME_ACTION:
            frame(); break;
        case REVERSE_ACTION:
            reverse(); break;
        default:
            msg_Dbg( p_intf, "Action: %i", id_action );
            break;
    }
}

void AbstractController::stop()
{
    THEMIM->stop();
}

void AbstractController::play()
{
    if( THEPL->current.i_size == 0 )
    {
        /* The playlist is empty, open a file requester */
        THEDP->openFileDialog();
        return;
    }
    THEMIM->togglePlayPause();
}

void AbstractController::prev()
{
    THEMIM->prev();
}

void AbstractController::next()
{
    THEMIM->next();
}

/**
  * TODO
 * This functions toggle the fullscreen mode
 * If there is no video, it should first activate Visualisations...
 *  This has also to be fixed in enableVideo()
 */
void AbstractController::fullscreen()
{
    vout_thread_t *p_vout =
      (vout_thread_t *)vlc_object_find( p_intf, VLC_OBJECT_VOUT, FIND_ANYWHERE );
    if( p_vout)
    {
        var_SetBool( p_vout, "fullscreen", !var_GetBool( p_vout, "fullscreen" ) );
        vlc_object_release( p_vout );
    }
}

void AbstractController::snapshot()
{
    vout_thread_t *p_vout =
      (vout_thread_t *)vlc_object_find( p_intf, VLC_OBJECT_VOUT, FIND_ANYWHERE );
    if( p_vout )
    {
        vout_Control( p_vout, VOUT_SNAPSHOT );
        vlc_object_release( p_vout );
    }
}

void AbstractController::extSettings()
{
    THEDP->extendedDialog();
}

void AbstractController::reverse()
{
    THEMIM->getIM()->reverse();
}

void AbstractController::slower()
{
    THEMIM->getIM()->slower();
}

void AbstractController::faster()
{
    THEMIM->getIM()->faster();
}

void AbstractController::playlist()
{
    if( p_intf->p_sys->p_mi ) p_intf->p_sys->p_mi->togglePlaylist();
}

void AbstractController::record()
{
    input_thread_t *p_input = THEMIM->getInput();
    if( p_input )
    {
        /* This method won't work fine if the stream can't be cut anywhere */
        const bool b_recording = var_GetBool( p_input, "record" );
        var_SetBool( p_input, "record", !b_recording );
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

void AbstractController::frame()
{
    input_thread_t *p_input = THEMIM->getInput();
    if( p_input )
        var_SetVoid( p_input, "frame-next" );
}

/*****************************
 * DA Control Widget !
 *****************************/
ControlsWidget::ControlsWidget( intf_thread_t *_p_i,
                                bool b_advControls ) :
                                AbstractController( _p_i )
{
    setSizePolicy( QSizePolicy::Preferred , QSizePolicy::Maximum );

    /* advanced Controls handling */
    b_advancedVisible = b_advControls;
    advControls = NULL;

    controlLayout->setSpacing( 0 );
    controlLayout->setLayoutMargins( 6, 4, 6, 2, 5 );

    QString line1 = getSettings()->value( "MainWindow/Controls2",
            "18;19;25" ).toString();
    parseAndCreateLine( line1, 0 );

/*    QString line2 = QString( "%1-2;%2;%3;%4;%5;%6;%6;%7;%8;%9;%6;%10;%11-4")
        .arg( PLAY_BUTTON )         .arg( WIDGET_SPACER )
        .arg( PREVIOUS_BUTTON )         .arg( STOP_BUTTON )
        .arg( NEXT_BUTTON )        .arg( WIDGET_SPACER )
        .arg( FULLSCREEN_BUTTON )        .arg( PLAYLIST_BUTTON )
        .arg( EXTENDED_BUTTON )        .arg( WIDGET_SPACER_EXTEND )
        .arg( VOLUME ); */

    QString line2 = getSettings()->value( "MainWindow/Controls2",
            "0-2;21;21;4;2;5;21;21;8;11;10;21;22;20-4" ).toString();
    parseAndCreateLine( line2, 1 );

    if( !b_advancedVisible && advControls ) advControls->hide();
}

ControlsWidget::~ControlsWidget()
{}

void ControlsWidget::toggleAdvanced()
{
    if( !advControls ) return;

    if( !b_advancedVisible )
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

AdvControlsWidget::AdvControlsWidget( intf_thread_t *_p_i ) :
                                     AbstractController( _p_i )
{
    controlLayout->setMargin( 0 );
    controlLayout->setSpacing( 0 );

    /* QString line = QString( "%1;%2;%3").arg( RECORD_BUTTON )
        .arg( SNAPSHOT_BUTTON )
        .arg( ATOB_BUTTON )
        .arg( FRAME_BUTTON ); */

    QString line = getSettings()->value( "MainWindow/AdvControl",
            "12;13;14;15" ).toString();
    parseAndCreateLine( line, 0 );
}

InputControlsWidget::InputControlsWidget( intf_thread_t *_p_i ) :
                                     AbstractController( _p_i )
{
    controlLayout->setMargin( 0 );
    controlLayout->setSpacing( 0 );

    /*    QString line = QString( "%1-%2;%3;%4-%2")
        .arg( SLOWER_BUTTON ).arg( WIDGET_FLAT )
        .arg( INPUT_SLIDER )
        .arg( FASTER_BUTTON ); */
    QString line = getSettings()->value( "MainWindow/InputControl",
                   "6-1;16;7-1" ).toString();
    parseAndCreateLine( line, 0 );
}
/**********************************************************************
 * Fullscrenn control widget
 **********************************************************************/
FullscreenControllerWidget::FullscreenControllerWidget( intf_thread_t *_p_i )
                           : AbstractController( _p_i )
{
    i_mouse_last_x      = -1;
    i_mouse_last_y      = -1;
    b_mouse_over        = false;
    i_mouse_last_move_x = -1;
    i_mouse_last_move_y = -1;
#if HAVE_TRANSPARENCY
    b_slow_hide_begin   = false;
    i_slow_hide_timeout = 1;
#endif
    b_fullscreen        = false;
    i_hide_timeout      = 1;
    p_vout              = NULL;
    i_screennumber      = -1;

    setWindowFlags( Qt::ToolTip );
    setMinimumWidth( 600 );

    setFrameShape( QFrame::StyledPanel );
    setFrameStyle( QFrame::Sunken );
    setSizePolicy( QSizePolicy::Minimum, QSizePolicy::Minimum );

    controlLayout->setLayoutMargins( 5, 2, 5, 2, 5 );

    /* First line */
    InputControlsWidget *inputC = new InputControlsWidget( p_intf );
    controlLayout->addWidget( inputC, 0, 0, 1, -1 );

    /* Second line */
    /* QString line2 = QString( "%1-2;%2;%3;%4;%5;%2;%6;%2;%7;%2;%8;%9;%10-4")
        .arg( PLAY_BUTTON )
        .arg( WIDGET_SPACER )
        .arg( PREVIOUS_BUTTON )
        .arg( STOP_BUTTON )
        .arg( NEXT_BUTTON )
        .arg( MENU_BUTTONS )
        .arg( TELETEXT_BUTTONS )
        .arg( DEFULLSCREEN_BUTTON )
        .arg( WIDGET_SPACER_EXTEND )
        .arg( TIME_LABEL )
        .arg( VOLUME ); */

    QString line = getSettings()->value( "MainWindow/FSCline",
            "0-2;21;4;2;5;21;18;21;19;21;9;22;23-4" ).toString();
    parseAndCreateLine( line, 1 );

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

#ifdef WIN32TRICK
    setWindowOpacity( 0.0 );
    b_fscHidden = true;
    adjustSize();
    show();
#endif

    vlc_mutex_init_recursive( &lock );
}

FullscreenControllerWidget::~FullscreenControllerWidget()
{
    getSettings()->setValue( "FullScreen/pos", pos() );
    detachVout();
    vlc_mutex_destroy( &lock );
}

/**
 * Show fullscreen controller
 */
void FullscreenControllerWidget::showFSC()
{
    adjustSize();
    /* center down */
    int number = QApplication::desktop()->screenNumber( p_intf->p_sys->p_mi );
    if( number != i_screennumber )
    {
        msg_Dbg( p_intf, "Calculation fullscreen controllers center");
        /* screen has changed, calculate new position */
        QRect screenRes = QApplication::desktop()->screenGeometry(number);
        QPoint pos = QPoint( screenRes.x() + (screenRes.width() / 2) - (width() / 2),
                             screenRes.y() + screenRes.height() - height());
        move( pos );
        i_screennumber = number;
    }
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

#ifdef WIN32TRICK
            if( b_fs && b_fscHidden )
#else
            if( b_fs && !isVisible() )
#endif
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
static int FullscreenControllerWidgetFullscreenChanged( vlc_object_t *vlc_object,
                const char *variable, vlc_value_t old_val,
                vlc_value_t new_val,  void *data )
{
    vout_thread_t *p_vout = (vout_thread_t *) vlc_object;
    msg_Dbg( p_vout, "Qt4: Fullscreen state changed" );
    FullscreenControllerWidget *p_fs = (FullscreenControllerWidget *)data;

    p_fs->fullscreenChanged( p_vout, new_val.b_bool,
            var_GetInteger( p_vout, "mouse-hide-timeout" ) );

    return VLC_SUCCESS;
}
/* */
static int FullscreenControllerWidgetMouseMoved( vlc_object_t *vlc_object, const char *variable,
                                                 vlc_value_t old_val, vlc_value_t new_val,
                                                 void *data )
{
    FullscreenControllerWidget *p_fs = (FullscreenControllerWidget *)data;

    int i_mousex, i_mousey;
    bool b_toShow = false;

    /* Get the value from the Vout - Trust the vout more than Qt */
    i_mousex = var_GetInteger( p_fs->p_vout, "mouse-x" );
    i_mousey = var_GetInteger( p_fs->p_vout, "mouse-y" );

    /* First time */
    if( p_fs->i_mouse_last_move_x == -1 || p_fs->i_mouse_last_move_y == -1 )
    {
        p_fs->i_mouse_last_move_x = i_mousex;
        p_fs->i_mouse_last_move_y = i_mousey;
        b_toShow = true;
    }
    /* All other times */
    else
    {
        /* Trigger only if move > 3 px dans une direction */
        if( abs( p_fs->i_mouse_last_move_x - i_mousex ) > 2 ||
            abs( p_fs->i_mouse_last_move_y - i_mousey ) > 2 )
        {
            b_toShow = true;
            p_fs->i_mouse_last_move_x = i_mousex;
            p_fs->i_mouse_last_move_y = i_mousey;
        }
    }

    if( b_toShow )
    {
        /* Show event */
        IMEvent *eShow = new IMEvent( FullscreenControlShow_Type, 0 );
        QApplication::postEvent( p_fs, static_cast<QEvent *>(eShow) );

        /* Plan hide event */
        IMEvent *eHide = new IMEvent( FullscreenControlPlanHide_Type, 0 );
        QApplication::postEvent( p_fs, static_cast<QEvent *>(eHide) );
    }

    return VLC_SUCCESS;
}

/**
 * It is called when video start
 */
void FullscreenControllerWidget::attachVout( vout_thread_t *p_nvout )
{
    assert( p_nvout && !p_vout );

    p_vout = p_nvout;

    msg_Dbg( p_vout, "Qt FS: Attaching Vout" );
    vlc_mutex_lock( &lock );

    var_AddCallback( p_vout, "fullscreen",
            FullscreenControllerWidgetFullscreenChanged, this );
            /* I miss a add and fire */
    fullscreenChanged( p_vout, var_GetBool( p_vout, "fullscreen" ),
                       var_GetInteger( p_vout, "mouse-hide-timeout" ) );
    vlc_mutex_unlock( &lock );
}

/**
 * It is called after turn off video.
 */
void FullscreenControllerWidget::detachVout()
{
    if( p_vout )
    {
        msg_Dbg( p_vout, "Qt FS: Detaching Vout" );
        var_DelCallback( p_vout, "fullscreen",
                FullscreenControllerWidgetFullscreenChanged, this );
        vlc_mutex_lock( &lock );
        fullscreenChanged( p_vout, false, 0 );
        vlc_mutex_unlock( &lock );
        p_vout = NULL;
    }
}

/**
 * Register and unregister callback for mouse moving
 */
void FullscreenControllerWidget::fullscreenChanged( vout_thread_t *p_vout,
        bool b_fs, int i_timeout )
{
    msg_Dbg( p_vout, "Qt: Entering Fullscreen" );

    vlc_mutex_lock( &lock );
    /* Entering fullscreen, register callback */
    if( b_fs && !b_fullscreen )
    {
        b_fullscreen = true;
        i_hide_timeout = i_timeout;
        var_AddCallback( p_vout, "mouse-moved",
                FullscreenControllerWidgetMouseMoved, this );
    }
    /* Quitting fullscreen, unregistering callback */
    else if( !b_fs && b_fullscreen )
    {
        b_fullscreen = false;
        i_hide_timeout = i_timeout;
        var_DelCallback( p_vout, "mouse-moved",
                FullscreenControllerWidgetMouseMoved, this );

        /* Force fs hidding */
        IMEvent *eHide = new IMEvent( FullscreenControlHide_Type, 0 );
        QApplication::postEvent( this, static_cast<QEvent *>(eHide) );
    }
    vlc_mutex_unlock( &lock );
}
