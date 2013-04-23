/*****************************************************************************
 * controller.cpp : Controller for the main interface
 ****************************************************************************
 * Copyright (C) 2006-2009 the VideoLAN team
 * $Id$
 *
 * Authors: Jean-Baptiste Kempf <jb@videolan.org>
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

#include <vlc_vout.h>                       /* vout_thread_t for FSC */

/* Widgets */
#include "components/controller.hpp"
#include "components/controller_widget.hpp"
#include "components/interface_widgets.hpp"
#include "util/buttons/DeckButtonsLayout.hpp"
#include "util/buttons/BrowseButton.hpp"
#include "util/buttons/RoundButton.hpp"

#include "dialogs_provider.hpp"                     /* Opening Dialogs */
#include "actions_manager.hpp"                             /* *_ACTION */

#include "util/input_slider.hpp"                         /* SeekSlider */
#include "util/customwidgets.hpp"                       /* qEventToKey */

#include "adapters/seekpoints.hpp"

#include <QToolButton>
#include <QHBoxLayout>
#include <QRegion>
#include <QSignalMapper>
#include <QTimer>

//#define DEBUG_LAYOUT 1

/**********************************************************************
 * TEH controls
 **********************************************************************/

/******
 * This is an abstract Toolbar/Controller
 * This has helper to create any toolbar, any buttons and to manage the actions
 *
 *****/
AbstractController::AbstractController( intf_thread_t * _p_i, QWidget *_parent )
                   : QFrame( _parent )
{
    p_intf = _p_i;
    advControls = NULL;
    buttonGroupLayout = NULL;

    /* Main action provider */
    toolbarActionsMapper = new QSignalMapper( this );
    CONNECT( toolbarActionsMapper, mapped( int ),
             ActionsManager::getInstance( p_intf  ), doAction( int ) );
    CONNECT( THEMIM->getIM(), playingStatusChanged( int ), this, setStatus( int ) );

    setSizePolicy( QSizePolicy::MinimumExpanding, QSizePolicy::Fixed );
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

    emit inputIsTrickPlayable( b_hasInput &&
                            var_GetBool( THEMIM->getInput(), "can-rewind" ) );
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
void AbstractController::parseAndCreate( const QString& config,
                                         QBoxLayout *newControlLayout )
{
    QStringList list = config.split( ";", QString::SkipEmptyParts ) ;
    for( int i = 0; i < list.count(); i++ )
    {
        QStringList list2 = list.at( i ).split( "-" );
        if( list2.count() < 1 )
        {
            msg_Warn( p_intf, "Parsing error 1. Please, report this." );
            continue;
        }

        bool ok;
        int i_option = WIDGET_NORMAL;
        buttonType_e i_type = (buttonType_e)list2.at( 0 ).toInt( &ok );
        if( !ok )
        {
            msg_Warn( p_intf, "Parsing error 2. Please, report this." );
            continue;
        }

        if( list2.count() > 1 )
        {
            i_option = list2.at( 1 ).toInt( &ok );
            if( !ok )
            {
                msg_Warn( p_intf, "Parsing error 3. Please, report this." );
                continue;
            }
        }

        createAndAddWidget( newControlLayout, -1, i_type, i_option );
    }

    if( buttonGroupLayout )
    {
        newControlLayout->addLayout( buttonGroupLayout );
        buttonGroupLayout = NULL;
    }
}

void AbstractController::createAndAddWidget( QBoxLayout *controlLayout_,
                                             int i_index,
                                             buttonType_e i_type,
                                             int i_option )
{
    VLC_UNUSED( i_index ); // i_index should only be required for edition

    /* Close the current buttonGroup if we have a special widget or a spacer */
    if( buttonGroupLayout && i_type > BUTTON_MAX )
    {
        controlLayout_->addLayout( buttonGroupLayout );
        buttonGroupLayout = NULL;
    }

    /* Special case for SPACERS, who aren't QWidgets */
    if( i_type == WIDGET_SPACER )
    {
        controlLayout_->addSpacing( 12 );
    }
    else if(  i_type == WIDGET_SPACER_EXTEND )
    {
        controlLayout_->addStretch( 12 );
    }
    else
    {
        /* Create the widget */
        QWidget *widg = createWidget( i_type, i_option );
        if( !widg ) return;

        /* Buttons */
        if( i_type < BUTTON_MAX )
        {
            if( !buttonGroupLayout )
            {
                buttonGroupLayout = new QHBoxLayout;

            }
            buttonGroupLayout->addWidget( widg );
        }
        else /* Special widgets */
        {
            controlLayout_->addWidget( widg );
        }
    }
}


#define CONNECT_MAP( a ) CONNECT( a, clicked(),  toolbarActionsMapper, map() )
#define SET_MAPPING( a, b ) toolbarActionsMapper->setMapping( a , b )
#define CONNECT_MAP_SET( a, b ) \
    CONNECT_MAP( a ); \
    SET_MAPPING( a, b );
#define BUTTON_SET_BAR( a_button ) \
    a_button->setToolTip( qtr( tooltipL[button] ) ); \
    a_button->setIcon( QIcon( iconL[button] ) );
#define BUTTON_SET_BAR2( button, image, tooltip ) \
    button->setToolTip( tooltip );          \
    button->setIcon( QIcon( ":/"#image ) );

#define ENABLE_ON_VIDEO( a ) \
    CONNECT( THEMIM->getIM(), voutChanged( bool ), a, setEnabled( bool ) ); \
    a->setEnabled( THEMIM->getIM()->hasVideo() ); /* TODO: is this necessary? when input is started before the interface? */

#define ENABLE_ON_INPUT( a ) \
    CONNECT( this, inputExists( bool ), a, setEnabled( bool ) ); \
    a->setEnabled( THEMIM->getIM()->hasInput() ); /* TODO: is this necessary? when input is started before the interface? */

#define NORMAL_BUTTON( name )                           \
    QToolButton * name ## Button = new QToolButton;     \
    setupButton( name ## Button );                      \
    CONNECT_MAP_SET( name ## Button, name ## _ACTION ); \
    BUTTON_SET_BAR( name ## Button );                   \
    widget = name ## Button;

QWidget *AbstractController::createWidget( buttonType_e button, int options )
{
    bool b_flat  = options & WIDGET_FLAT;
    bool b_big   = options & WIDGET_BIG;
    bool b_shiny = options & WIDGET_SHINY;
    bool b_special = false;

    QWidget *widget = NULL;
    switch( button )
    {
    case PLAY_BUTTON: {
        PlayButton *playButton = new PlayButton;
        setupButton( playButton );
        BUTTON_SET_BAR(  playButton );
        CONNECT_MAP_SET( playButton, PLAY_ACTION );
        CONNECT( this, inputPlaying( bool ),
                 playButton, updateButtonIcons( bool ));
        widget = playButton;
        }
        break;
    case STOP_BUTTON:{
        NORMAL_BUTTON( STOP );
        }
        break;
    case OPEN_BUTTON:{
        NORMAL_BUTTON( OPEN );
        }
        break;
    case OPEN_SUB_BUTTON:{
        NORMAL_BUTTON( OPEN_SUB );
        }
        break;
    case PREVIOUS_BUTTON:{
        NORMAL_BUTTON( PREVIOUS );
        }
        break;
    case NEXT_BUTTON: {
        NORMAL_BUTTON( NEXT );
        }
        break;
    case SLOWER_BUTTON:{
        NORMAL_BUTTON( SLOWER );
        ENABLE_ON_INPUT( SLOWERButton );
        }
        break;
    case FASTER_BUTTON:{
        NORMAL_BUTTON( FASTER );
        ENABLE_ON_INPUT( FASTERButton );
        }
        break;
    case PREV_SLOW_BUTTON:{
        QToolButtonExt *but = new QToolButtonExt;
        setupButton( but );
        BUTTON_SET_BAR( but );
        CONNECT( but, shortClicked(), THEMIM, prev() );
        CONNECT( but, longClicked(), THEAM, skipBackward() );
        widget = but;
        }
        break;
    case NEXT_FAST_BUTTON:{
        QToolButtonExt *but = new QToolButtonExt;
        setupButton( but );
        BUTTON_SET_BAR( but );
        CONNECT( but, shortClicked(), THEMIM, next() );
        CONNECT( but, longClicked(), THEAM, skipForward() );
        widget = but;
        }
        break;
    case FRAME_BUTTON: {
        NORMAL_BUTTON( FRAME );
        ENABLE_ON_VIDEO( FRAMEButton );
        }
        break;
    case FULLSCREEN_BUTTON:
    case DEFULLSCREEN_BUTTON:
        {
        NORMAL_BUTTON( FULLSCREEN );
        ENABLE_ON_VIDEO( FULLSCREENButton );
        }
        break;
    case FULLWIDTH_BUTTON: {
            NORMAL_BUTTON( FULLWIDTH );
        }
        break;
    case EXTENDED_BUTTON:{
        NORMAL_BUTTON( EXTENDED );
        }
        break;
    case PLAYLIST_BUTTON:{
        NORMAL_BUTTON( PLAYLIST );
        }
        break;
    case SNAPSHOT_BUTTON:{
        NORMAL_BUTTON( SNAPSHOT );
        ENABLE_ON_VIDEO( SNAPSHOTButton );
        }
        break;
    case RECORD_BUTTON:{
        QToolButton *recordButton = new QToolButton;
        setupButton( recordButton );
        CONNECT_MAP_SET( recordButton, RECORD_ACTION );
        BUTTON_SET_BAR(  recordButton );
        ENABLE_ON_INPUT( recordButton );
        recordButton->setCheckable( true );
        CONNECT( THEMIM->getIM(), recordingStateChanged( bool ),
                 recordButton, setChecked( bool ) );
        widget = recordButton;
        }
        break;
    case ATOB_BUTTON: {
        AtoB_Button *ABButton = new AtoB_Button;
        setupButton( ABButton );
        ABButton->setShortcut( qtr("Shift+L") );
        BUTTON_SET_BAR( ABButton );
        ENABLE_ON_INPUT( ABButton );
        CONNECT_MAP_SET( ABButton, ATOB_ACTION );
        CONNECT( THEMIM->getIM(), AtoBchanged( bool, bool),
                 ABButton, updateButtonIcons( bool, bool ) );
        widget = ABButton;
        }
        break;
    case INPUT_SLIDER: {
        SeekSlider *slider = new SeekSlider( Qt::Horizontal, NULL, !b_shiny );
        SeekPoints *chapters = new SeekPoints( this, p_intf );
        CONNECT( THEMIM->getIM(), chapterChanged( bool ), chapters, update() );
        slider->setChapters( chapters );

        /* Update the position when the IM has changed */
        CONNECT( THEMIM->getIM(), positionUpdated( float, int64_t, int ),
                slider, setPosition( float, int64_t, int ) );
        /* And update the IM, when the position has changed */
        CONNECT( slider, sliderDragged( float ),
                 THEMIM->getIM(), sliderUpdate( float ) );
        CONNECT( THEMIM->getIM(), cachingChanged( float ),
                 slider, updateBuffering( float ) );
        /* Give hint to disable slider's interactivity when useless */
        CONNECT( THEMIM->getIM(), inputCanSeek( bool ),
                 slider, setSeekable( bool ) );
        widget = slider;
        }
        break;
    case MENU_BUTTONS:
        widget = discFrame();
        widget->hide();
        break;
    case TELETEXT_BUTTONS:
        widget = telexFrame();
        widget->hide();
        break;
    case VOLUME_SPECIAL:
        b_special = true;
    case VOLUME:
        {
            SoundWidget *snd = new SoundWidget( this, p_intf, b_shiny, b_special );
            widget = snd;
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
            advControls = new AdvControlsWidget( p_intf, this );
            widget = advControls;
        }
        break;
    case REVERSE_BUTTON:{
        QToolButton *reverseButton = new QToolButton;
        setupButton( reverseButton );
        CONNECT_MAP_SET( reverseButton, REVERSE_ACTION );
        BUTTON_SET_BAR(  reverseButton );
        reverseButton->setCheckable( true );
        /* You should, of COURSE change this to the correct event,
           when/if we have one, that tells us if trickplay is possible . */
        CONNECT( this, inputIsTrickPlayable( bool ), reverseButton, setVisible( bool ) );
        reverseButton->setVisible( false );
        widget = reverseButton;
        }
        break;
    case SKIP_BACK_BUTTON: {
        NORMAL_BUTTON( SKIP_BACK );
        ENABLE_ON_INPUT( SKIP_BACKButton );
        }
        break;
    case SKIP_FW_BUTTON: {
        NORMAL_BUTTON( SKIP_FW );
        ENABLE_ON_INPUT( SKIP_FWButton );
        }
        break;
    case QUIT_BUTTON: {
        NORMAL_BUTTON( QUIT );
        }
        break;
    case RANDOM_BUTTON: {
        NORMAL_BUTTON( RANDOM );
        RANDOMButton->setCheckable( true );
        RANDOMButton->setChecked( var_GetBool( THEPL, "random" ) );
        CONNECT( THEMIM, randomChanged( bool ),
                 RANDOMButton, setChecked( bool ) );
        }
        break;
    case LOOP_BUTTON:{
        LoopButton *loopButton = new LoopButton;
        setupButton( loopButton );
        loopButton->setToolTip( qtr( "Click to toggle between loop all, loop one and no loop") );
        loopButton->setCheckable( true );
        int i_state = 2 * var_GetBool( THEPL, "loop" ) + var_GetBool( THEPL, "repeat" );
        loopButton->updateButtonIcons( i_state );
        CONNECT( THEMIM, repeatLoopChanged( int ), loopButton, updateButtonIcons( int ) );
        CONNECT( loopButton, clicked(), THEMIM, loopRepeatLoopStatus() );
        widget = loopButton;
        }
        break;
    case INFO_BUTTON: {
        NORMAL_BUTTON( INFO );
        }
        break;
    case PLAYBACK_BUTTONS:{
        widget = new QWidget;
        DeckButtonsLayout *layout = new DeckButtonsLayout( widget );
        BrowseButton *prev = new BrowseButton( widget, BrowseButton::Backward );
        BrowseButton *next = new BrowseButton( widget );
        RoundButton *play = new RoundButton( widget );
        layout->setBackwardButton( prev );
        layout->setForwardButton( next );
        layout->setRoundButton( play );
        CONNECT_MAP_SET( prev, PREVIOUS_ACTION );
        CONNECT_MAP_SET( next, NEXT_ACTION );
        CONNECT_MAP_SET( play, PLAY_ACTION );
        }
        break;
    case ASPECT_RATIO_COMBOBOX:
        widget = new AspectRatioComboBox( p_intf );
        widget->setMinimumHeight( 26 );
        break;
    case SPEED_LABEL:
        widget = new SpeedLabel( p_intf, this );
        break;
    case TIME_LABEL_ELAPSED:
        widget = new TimeLabel( p_intf, TimeLabel::Elapsed );
        break;
    case TIME_LABEL_REMAINING:
        widget = new TimeLabel( p_intf, TimeLabel::Remaining );
        break;
    default:
        msg_Warn( p_intf, "This should not happen %i", button );
        break;
    }

    /* Customize Buttons */
    if( b_flat || b_big )
    {
        QFrame *frame = qobject_cast<QFrame *>(widget);
        if( frame )
        {
            QList<QToolButton *> allTButtons = frame->findChildren<QToolButton *>();
            for( int i = 0; i < allTButtons.count(); i++ )
                applyAttributes( allTButtons[i], b_flat, b_big );
        }
        else
        {
            QToolButton *tmpButton = qobject_cast<QToolButton *>(widget);
            if( tmpButton )
                applyAttributes( tmpButton, b_flat, b_big );
        }
    }
    return widget;
}
#undef NORMAL_BUTTON

void AbstractController::applyAttributes( QToolButton *tmpButton, bool b_flat, bool b_big )
{
    if( tmpButton )
    {
        if( b_flat )
            tmpButton->setAutoRaise( b_flat );
        if( b_big )
        {
            tmpButton->setFixedSize( QSize( 32, 32 ) );
            tmpButton->setIconSize( QSize( 26, 26 ) );
        }
    }
}

QFrame *AbstractController::discFrame()
{
    /** Disc and Menus handling */
    QFrame *discFrame = new QFrame( this );

    QHBoxLayout *discLayout = new QHBoxLayout( discFrame );
    discLayout->setSpacing( 0 ); discLayout->setMargin( 0 );

    QToolButton *prevSectionButton = new QToolButton( discFrame );
    setupButton( prevSectionButton );
    BUTTON_SET_BAR2( prevSectionButton, toolbar/dvd_prev,
            qtr("Previous Chapter/Title" ) );
    discLayout->addWidget( prevSectionButton );

    QToolButton *menuButton = new QToolButton( discFrame );
    setupButton( menuButton );
    discLayout->addWidget( menuButton );
    BUTTON_SET_BAR2( menuButton, toolbar/dvd_menu, qtr( "Menu" ) );

    QToolButton *nextSectionButton = new QToolButton( discFrame );
    setupButton( nextSectionButton );
    discLayout->addWidget( nextSectionButton );
    BUTTON_SET_BAR2( nextSectionButton, toolbar/dvd_next,
            qtr("Next Chapter/Title" ) );

    /* Change the navigation button display when the IM
       navigation changes */
    CONNECT( THEMIM->getIM(), chapterChanged( bool ),
            discFrame, setVisible( bool ) );
    CONNECT( THEMIM->getIM(), titleChanged( bool ),
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

QFrame *AbstractController::telexFrame()
{
    /**
     * Telextext QFrame
     **/
    QFrame *telexFrame = new QFrame( this );
    QHBoxLayout *telexLayout = new QHBoxLayout( telexFrame );
    telexLayout->setSpacing( 0 ); telexLayout->setMargin( 0 );
    CONNECT( THEMIM->getIM(), teletextPossible( bool ),
             telexFrame, setVisible( bool ) );

    /* On/Off button */
    QToolButton *telexOn = new QToolButton;
    setupButton( telexOn );
    BUTTON_SET_BAR2( telexOn, toolbar/tv, qtr( "Teletext Activation" ) );
    telexOn->setEnabled( false );
    telexOn->setCheckable( true );

    telexLayout->addWidget( telexOn );

    /* Teletext Activation and set */
    CONNECT( telexOn, clicked( bool ),
             THEMIM->getIM(), activateTeletext( bool ) );
    CONNECT( THEMIM->getIM(), teletextPossible( bool ),
             telexOn, setEnabled( bool ) );

    /* Transparency button */
    QToolButton *telexTransparent = new QToolButton;
    setupButton( telexTransparent );
    BUTTON_SET_BAR2( telexTransparent, toolbar/tvtelx,
                     qtr( "Toggle Transparency " ) );
    telexTransparent->setEnabled( false );
    telexTransparent->setCheckable( true );
    telexLayout->addWidget( telexTransparent );

    /* Transparency change and set */
    CONNECT( telexTransparent, clicked( bool ),
            THEMIM->getIM(), telexSetTransparency( bool ) );
    CONNECT( THEMIM->getIM(), teletextTransparencyActivated( bool ),
             telexTransparent, setChecked( bool ) );


    /* Page setting */
    QSpinBox *telexPage = new QSpinBox( telexFrame );
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

    CONNECT( THEMIM->getIM(), teletextActivated( bool ), telexPage, setEnabled( bool ) );
    CONNECT( THEMIM->getIM(), teletextActivated( bool ), telexTransparent, setEnabled( bool ) );
    CONNECT( THEMIM->getIM(), teletextActivated( bool ), telexOn, setChecked( bool ) );
    return telexFrame;
}
#undef CONNECT_MAP
#undef SET_MAPPING
#undef CONNECT_MAP_SET
#undef BUTTON_SET_BAR
#undef BUTTON_SET_BAR2
#undef ENABLE_ON_VIDEO
#undef ENABLE_ON_INPUT

#include <QHBoxLayout>
/*****************************
 * DA Control Widget !
 *****************************/
ControlsWidget::ControlsWidget( intf_thread_t *_p_i,
                                bool b_advControls,
                                QWidget *_parent ) :
                                AbstractController( _p_i, _parent )
{
    RTL_UNAFFECTED_WIDGET
    /* advanced Controls handling */
    b_advancedVisible = b_advControls;
#ifdef DEBUG_LAYOUT
    setStyleSheet( "background: red ");
#endif
    setAttribute( Qt::WA_MacBrushedMetal);
    controlLayout = new QVBoxLayout( this );
    controlLayout->setContentsMargins( 3, 1, 0, 1 );
    controlLayout->setSpacing( 0 );
    QHBoxLayout *controlLayout1 = new QHBoxLayout;
    controlLayout1->setSpacing( 0 ); controlLayout1->setMargin( 0 );

    QString line1 = getSettings()->value( "MainWindow/MainToolbar1", MAIN_TB1_DEFAULT )
                                        .toString();
    parseAndCreate( line1, controlLayout1 );

    QHBoxLayout *controlLayout2 = new QHBoxLayout;
    controlLayout2->setSpacing( 0 ); controlLayout2->setMargin( 0 );
    QString line2 = getSettings()->value( "MainWindow/MainToolbar2", MAIN_TB2_DEFAULT )
                                        .toString();
    parseAndCreate( line2, controlLayout2 );

    grip = new QSizeGrip( this );
    controlLayout2->addWidget( grip, 0, Qt::AlignBottom|Qt::AlignRight );

    if( !b_advancedVisible && advControls ) advControls->hide();

    controlLayout->addLayout( controlLayout1 );
    controlLayout->addLayout( controlLayout2 );
}

void ControlsWidget::toggleAdvanced()
{
    if( !advControls ) return;

    advControls->setVisible( !b_advancedVisible );
    b_advancedVisible = !b_advancedVisible;
    emit advancedControlsToggled( b_advancedVisible );
}

AdvControlsWidget::AdvControlsWidget( intf_thread_t *_p_i, QWidget *_parent ) :
                                     AbstractController( _p_i, _parent )
{
    RTL_UNAFFECTED_WIDGET
    controlLayout = new QHBoxLayout( this );
    controlLayout->setMargin( 0 );
    controlLayout->setSpacing( 0 );
#ifdef DEBUG_LAYOUT
    setStyleSheet( "background: orange ");
#endif


    QString line = getSettings()->value( "MainWindow/AdvToolbar", ADV_TB_DEFAULT )
        .toString();
    parseAndCreate( line, controlLayout );
}

InputControlsWidget::InputControlsWidget( intf_thread_t *_p_i, QWidget *_parent ) :
                                     AbstractController( _p_i, _parent )
{
    RTL_UNAFFECTED_WIDGET
    controlLayout = new QHBoxLayout( this );
    controlLayout->setMargin( 0 );
    controlLayout->setSpacing( 0 );
#ifdef DEBUG_LAYOUT
    setStyleSheet( "background: green ");
#endif

    QString line = getSettings()->value( "MainWindow/InputToolbar", INPT_TB_DEFAULT ).toString();
    parseAndCreate( line, controlLayout );
}
/**********************************************************************
 * Fullscrenn control widget
 **********************************************************************/
FullscreenControllerWidget::FullscreenControllerWidget( intf_thread_t *_p_i, QWidget *_parent )
                           : AbstractController( _p_i, _parent )
{
    RTL_UNAFFECTED_WIDGET
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
    i_screennumber      = -1;

    vout.clear();

    setWindowFlags( Qt::ToolTip );
    setMinimumWidth( FSC_WIDTH );
    isWideFSC = false;

    setFrameShape( QFrame::StyledPanel );
    setFrameStyle( QFrame::Sunken );
    setSizePolicy( QSizePolicy::Minimum, QSizePolicy::Minimum );

    QVBoxLayout *controlLayout2 = new QVBoxLayout( this );
    controlLayout2->setContentsMargins( 4, 6, 4, 2 );

    /* First line */
    InputControlsWidget *inputC = new InputControlsWidget( p_intf, this );
    controlLayout2->addWidget( inputC );

    controlLayout = new QHBoxLayout;
    QString line = getSettings()->value( "MainWindow/FSCtoolbar", FSC_TB_DEFAULT ).toString();
    parseAndCreate( line, controlLayout );
    controlLayout2->addLayout( controlLayout );

    /* hiding timer */
    p_hideTimer = new QTimer( this );
    p_hideTimer->setSingleShot( true );
    CONNECT( p_hideTimer, timeout(), this, hideFSC() );

    /* slow hiding timer */
#if HAVE_TRANSPARENCY
    p_slowHideTimer = new QTimer( this );
    CONNECT( p_slowHideTimer, timeout(), this, slowHideFSC() );
    f_opacity = var_InheritFloat( p_intf, "qt-fs-opacity" );
#endif

    vlc_mutex_init_recursive( &lock );

    DCONNECT( THEMIM->getIM(), voutListChanged( vout_thread_t **, int ),
              this, setVoutList( vout_thread_t **, int ) );

    /* First Move */
    previousPosition = getSettings()->value( "FullScreen/pos" ).toPoint();
    screenRes = getSettings()->value( "FullScreen/screen" ).toRect();
    isWideFSC = getSettings()->value( "FullScreen/wide" ).toBool();
    i_screennumber = var_InheritInteger( p_intf, "qt-fullscreen-screennumber" );
}

FullscreenControllerWidget::~FullscreenControllerWidget()
{
    getSettings()->setValue( "FullScreen/pos", previousPosition );
    getSettings()->setValue( "FullScreen/screen", screenRes );
    getSettings()->setValue( "FullScreen/wide", isWideFSC );

    setVoutList( NULL, 0 );
    vlc_mutex_destroy( &lock );
}

void FullscreenControllerWidget::restoreFSC()
{
    if( !isWideFSC )
    {
        /* Restore half-bar and re-centre if needed */
        setMinimumWidth( FSC_WIDTH );
        adjustSize();

        QRect currentRes = QApplication::desktop()->screenGeometry( targetScreen() );

        if( currentRes == screenRes &&
            QApplication::desktop()->screen()->geometry().contains( previousPosition, true ) )
        {
            /* Restore to the last known position */
            move( previousPosition );
        }
        else
        {
            /* FSC is out of screen or screen resolution changed */
            msg_Dbg( p_intf, "Recentering the Fullscreen Controller" );
            centerFSC( targetScreen() );
            screenRes = currentRes;
            previousPosition = pos();
        }
    }
    else
    {
        /* Dock at the bottom of the screen */
        updateFullwidthGeometry( targetScreen() );
    }

#ifdef Q_WS_X11
    // Tell kwin that we do not want a shadow around the fscontroller
    setMask( QRegion( 0, 0, width(), height() ) );
#endif
}

void FullscreenControllerWidget::centerFSC( int number )
{
    QRect currentRes = QApplication::desktop()->screenGeometry( number );

    /* screen has changed, calculate new position */
    QPoint pos = QPoint( currentRes.x() + (currentRes.width() / 2) - (width() / 2),
            currentRes.y() + currentRes.height() - height());
    move( pos );
}

/**
 * Show fullscreen controller
 */
void FullscreenControllerWidget::showFSC()
{
    restoreFSC();

#if HAVE_TRANSPARENCY
    setWindowOpacity( f_opacity );
#endif

    show();
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
         if ( windowOpacity() > 0.0 )
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

void FullscreenControllerWidget::updateFullwidthGeometry( int number )
{
    QRect screenGeometry = QApplication::desktop()->screenGeometry( number );
    setMinimumWidth( screenGeometry.width() );
    setGeometry( screenGeometry.x(), screenGeometry.y() + screenGeometry.height() - height(), screenGeometry.width(), height() );
    adjustSize();
}

void FullscreenControllerWidget::toggleFullwidth()
{
    /* Toggle isWideFSC switch */
    isWideFSC = !isWideFSC;

    restoreFSC();
}

int FullscreenControllerWidget::targetScreen()
{
    if( i_screennumber == -1 || i_screennumber > QApplication::desktop()->numScreens() )
        return QApplication::desktop()->screenNumber( p_intf->p_sys->p_mi );
    return i_screennumber;
}

/**
 * event handling
 * events: show, hide, start timer for hiding
 */
void FullscreenControllerWidget::customEvent( QEvent *event )
{
    bool b_fs;

    switch( (int)event->type() )
    {
        /* This is used when the 'i' hotkey is used, to force quick toggle */
        case IMEvent::FullscreenControlToggle:
            vlc_mutex_lock( &lock );
            b_fs = b_fullscreen;
            vlc_mutex_unlock( &lock );

            if( b_fs )
            {
                if( isHidden() )
                {
                    p_hideTimer->stop();
                    showFSC();
                }
                else
                    hideFSC();
            }
            break;
        /* Event called to Show the FSC on mouseChanged() */
        case IMEvent::FullscreenControlShow:
            vlc_mutex_lock( &lock );
            b_fs = b_fullscreen;
            vlc_mutex_unlock( &lock );

            if( b_fs )
                showFSC();

            break;
        /* Start the timer to hide later, called usually with above case */
        case IMEvent::FullscreenControlPlanHide:
            if( !b_mouse_over ) // Only if the mouse is not over FSC
                planHideFSC();
            break;
        /* Hide */
        case IMEvent::FullscreenControlHide:
            hideFSC();
            break;
        default:
            break;
    }
}

/**
 * On mouse move
 * moving with FSC
 */
void FullscreenControllerWidget::mouseMoveEvent( QMouseEvent *event )
{
    if( event->buttons() == Qt::LeftButton )
    {
        if( i_mouse_last_x == -1 || i_mouse_last_y == -1 )
            return;

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
    if( isWideFSC ) return;
    i_mouse_last_x = event->globalX();
    i_mouse_last_y = event->globalY();
    event->accept();
}

void FullscreenControllerWidget::mouseReleaseEvent( QMouseEvent *event )
{
    if( isWideFSC ) return;
    i_mouse_last_x = -1;
    i_mouse_last_y = -1;
    event->accept();

    // Save the new FSC position
    previousPosition = pos();
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
    setWindowOpacity( f_opacity );
#endif
    event->accept();
}

/**
 * On mouse go out from FSC
 */
void FullscreenControllerWidget::leaveEvent( QEvent *event )
{
    planHideFSC();

    b_mouse_over = false;
    event->accept();
}

/**
 * When you get pressed key, send it to video output
 */
void FullscreenControllerWidget::keyPressEvent( QKeyEvent *event )
{
    emit keyPressed( event );
}

/* */
static int FullscreenControllerWidgetFullscreenChanged( vlc_object_t *vlc_object,
                const char *variable, vlc_value_t old_val,
                vlc_value_t new_val,  void *data )
{
    VLC_UNUSED( variable ); VLC_UNUSED( old_val );

    vout_thread_t *p_vout = (vout_thread_t *) vlc_object;

    msg_Dbg( p_vout, "Qt: Fullscreen state changed" );
    FullscreenControllerWidget *p_fs = (FullscreenControllerWidget *)data;

    p_fs->fullscreenChanged( p_vout, new_val.b_bool, var_GetInteger( p_vout, "mouse-hide-timeout" ) );

    return VLC_SUCCESS;
}
/* */
static int FullscreenControllerWidgetMouseMoved( vlc_object_t *vlc_object, const char *variable,
                                                 vlc_value_t old_val, vlc_value_t new_val,
                                                 void *data )
{
    VLC_UNUSED( variable ); VLC_UNUSED( old_val );

    vout_thread_t *p_vout = (vout_thread_t *)vlc_object;
    FullscreenControllerWidget *p_fs = (FullscreenControllerWidget *)data;

    /* Get the value from the Vout - Trust the vout more than Qt */
    p_fs->mouseChanged( p_vout, new_val.coords.x, new_val.coords.y );

    return VLC_SUCCESS;
}

/**
 * It is call to update the list of vout handled by the fullscreen controller
 */
void FullscreenControllerWidget::setVoutList( vout_thread_t **pp_vout, int i_vout )
{
    QList<vout_thread_t*> del;
    QList<vout_thread_t*> add;

    QList<vout_thread_t*> set;

    /* */
    for( int i = 0; i < i_vout; i++ )
        set += pp_vout[i];

    /* Vout to remove */
    vlc_mutex_lock( &lock );
    foreach( vout_thread_t *p_vout, vout )
    {
        if( !set.contains( p_vout ) )
            del += p_vout;
    }
    vlc_mutex_unlock( &lock );

    foreach( vout_thread_t *p_vout, del )
    {
        var_DelCallback( p_vout, "fullscreen",
                         FullscreenControllerWidgetFullscreenChanged, this );
        vlc_mutex_lock( &lock );
        fullscreenChanged( p_vout, false, 0 );
        vout.removeAll( p_vout );
        vlc_mutex_unlock( &lock );

        vlc_object_release( VLC_OBJECT(p_vout) );
    }

    /* Vout to track */
    vlc_mutex_lock( &lock );
    foreach( vout_thread_t *p_vout, set )
    {
        if( !vout.contains( p_vout ) )
            add += p_vout;
    }
    vlc_mutex_unlock( &lock );

    foreach( vout_thread_t *p_vout, add )
    {
        vlc_object_hold( VLC_OBJECT(p_vout) );

        vlc_mutex_lock( &lock );
        vout.append( p_vout );
        var_AddCallback( p_vout, "fullscreen",
                         FullscreenControllerWidgetFullscreenChanged, this );
        /* I miss a add and fire */
        fullscreenChanged( p_vout, var_GetBool( p_vout, "fullscreen" ),
                           var_GetInteger( p_vout, "mouse-hide-timeout" ) );
        vlc_mutex_unlock( &lock );
    }
}
/**
 * Register and unregister callback for mouse moving
 */
void FullscreenControllerWidget::fullscreenChanged( vout_thread_t *p_vout,
        bool b_fs, int i_timeout )
{
    /* FIXME - multiple vout (ie multiple mouse position ?) and thread safety if multiple vout ? */

    vlc_mutex_lock( &lock );
    /* Entering fullscreen, register callback */
    if( b_fs && !b_fullscreen )
    {
        msg_Dbg( p_vout, "Qt: Entering Fullscreen" );
        b_fullscreen = true;
        i_hide_timeout = i_timeout;
        var_AddCallback( p_vout, "mouse-moved",
                FullscreenControllerWidgetMouseMoved, this );
    }
    /* Quitting fullscreen, unregistering callback */
    else if( !b_fs && b_fullscreen )
    {
        msg_Dbg( p_vout, "Qt: Quitting Fullscreen" );
        b_fullscreen = false;
        i_hide_timeout = i_timeout;
        var_DelCallback( p_vout, "mouse-moved",
                FullscreenControllerWidgetMouseMoved, this );

        /* Force fs hiding */
        IMEvent *eHide = new IMEvent( IMEvent::FullscreenControlHide, 0 );
        QApplication::postEvent( this, eHide );
    }
    vlc_mutex_unlock( &lock );
}

/**
 * Mouse change callback (show/hide the controller on mouse movement)
 */
void FullscreenControllerWidget::mouseChanged( vout_thread_t *, int i_mousex, int i_mousey )
{
    bool b_toShow;

    /* FIXME - multiple vout (ie multiple mouse position ?) and thread safety if multiple vout ? */

    b_toShow = false;
    if( ( i_mouse_last_move_x == -1 || i_mouse_last_move_y == -1 ) ||
        ( abs( i_mouse_last_move_x - i_mousex ) > 2 ||
          abs( i_mouse_last_move_y - i_mousey ) > 2 ) )
    {
        i_mouse_last_move_x = i_mousex;
        i_mouse_last_move_y = i_mousey;
        b_toShow = true;
    }

    if( b_toShow )
    {
        /* Show event */
        IMEvent *eShow = new IMEvent( IMEvent::FullscreenControlShow, 0 );
        QApplication::postEvent( this, eShow );

        /* Plan hide event */
        IMEvent *eHide = new IMEvent( IMEvent::FullscreenControlPlanHide, 0 );
        QApplication::postEvent( this, eHide );
    }
}

