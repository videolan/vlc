/*****************************************************************************
 * interface_widgets.cpp : Custom widgets for the main interface
 ****************************************************************************
 * Copyright ( C ) 2006 the VideoLAN team
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Jean-Baptiste Kempf <jb@videolan.org>
 *          Rafaël Carré <funman@videolanorg>
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

#include "dialogs_provider.hpp"
#include "components/interface_widgets.hpp"
#include "main_interface.hpp"
#include "input_manager.hpp"
#include "menus.hpp"
#include "util/input_slider.hpp"
#include <vlc_vout.h>

#include <QLabel>
#include <QSpacerItem>
#include <QCursor>
#include <QPushButton>
#include <QHBoxLayout>
#include <QMenu>
#include <QPalette>
#include <QResizeEvent>

#define ICON_SIZE 128
#define MAX_BG_SIZE 300

/**********************************************************************
 * Video Widget. A simple frame on which video is drawn
 * This class handles resize issues
 **********************************************************************/
static void *DoRequest( intf_thread_t *, vout_thread_t *, int*,int*,
                        unsigned int *, unsigned int * );
static void DoRelease( intf_thread_t *, void * );
static int DoControl( intf_thread_t *, void *, int, va_list );

VideoWidget::VideoWidget( intf_thread_t *_p_i ) : QFrame( NULL ), p_intf( _p_i )
{
    vlc_mutex_init( p_intf, &lock );
    p_vout = NULL;
    CONNECT( this, askResize(), this, SetMinSize() );
    setSizePolicy( QSizePolicy::Preferred, QSizePolicy::Preferred );
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

QSize VideoWidget::sizeHint() const
{
    return widgetSize;
}

void *VideoWidget::request( vout_thread_t *p_nvout, int *pi_x, int *pi_y,
                           unsigned int *pi_width, unsigned int *pi_height )
{
    show();
    if( p_vout )
    {
        msg_Dbg( p_intf, "embedded video already in use" );
        return NULL;
    }
    p_vout = p_nvout;
    emit askResize();
    return ( void* )winId();
}

void VideoWidget::SetMinSize()
{
    setMinimumSize( 16, 16 );
}

void VideoWidget::release( void *p_win )
{
    p_vout = NULL;
}
/**********************************************************************
 * Background Widget. Show a simple image background. Currently,
 * it's a static cone.
 **********************************************************************/
BackgroundWidget::BackgroundWidget( intf_thread_t *_p_i ) :
                                        QFrame( NULL ), p_intf( _p_i )
{

    setAutoFillBackground( true );
    plt =  palette();
    plt.setColor( QPalette::Active, QPalette::Window , Qt::black );
    plt.setColor( QPalette::Inactive, QPalette::Window , Qt::black );
    setPalette( plt );

    label = new QLabel;
    label->setMaximumHeight( ICON_SIZE );
    label->setMaximumWidth( ICON_SIZE );
    label->setScaledContents( true );
    label->setPixmap( QPixmap( ":/vlc128.png" ) );
    backgroundLayout = new QHBoxLayout;
    backgroundLayout->addWidget( label );
    setLayout( backgroundLayout );
}

BackgroundWidget::~BackgroundWidget()
{
    backgroundLayout->takeAt( 0 );
    delete backgroundLayout;
}

void BackgroundWidget::setArt( QString url )
{
    if( url.isNull() )
        label->setPixmap( QPixmap( ":/vlc128.png" ) );
    else
        label->setPixmap( QPixmap( url ) );
}

QSize BackgroundWidget::sizeHint() const
{
    return widgetSize;
}

void BackgroundWidget::resizeEvent( QResizeEvent *e )
{
    if( e->size().height() < ICON_SIZE -1 )
        label->setMaximumWidth( e->size().height() );
    else
        label->setMaximumWidth( ICON_SIZE );
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

/* FIXME A to B function */
    ABButton = new QPushButton( "AB" );
    ABButton->setMaximumSize( QSize( 26, 26 ) );
    ABButton->setIconSize( QSize( 20, 20 ) );
    advLayout->addWidget( ABButton );
    BUTTON_SET_ACT( ABButton, "AB", qtr( "A to B" ), fromAtoB() );

    snapshotButton = new QPushButton( "S" );
    snapshotButton->setMaximumSize( QSize( 26, 26 ) );
    snapshotButton->setIconSize( QSize( 20, 20 ) );
    advLayout->addWidget( snapshotButton );
    BUTTON_SET_ACT( snapshotButton, "S", qtr( "Take a snapshot" ), snapshot() );

//FIXME Frame by frame function
    frameButton = new QPushButton( "Fr" );
    frameButton->setMaximumSize( QSize( 26, 26 ) );
    frameButton->setIconSize( QSize( 20, 20 ) );
    advLayout->addWidget( frameButton );
    BUTTON_SET_ACT( frameButton, "Fr", qtr( "Frame by Frame" ), frame() );

/* FIXME Record function */
    recordButton = new QPushButton( "R" );
    recordButton->setMaximumSize( QSize( 26, 26 ) );
    recordButton->setIconSize( QSize( 20, 20 ) );
    advLayout->addWidget( recordButton );
    BUTTON_SET_ACT_I( recordButton, "", record_16px.png,
            qtr( "Record" ), record() );

    normalButton = new QPushButton( "N" );
    normalButton->setMaximumSize( QSize( 26, 26 ) );
    normalButton->setIconSize( QSize( 20, 20 ) );
    advLayout->addWidget( normalButton );
    BUTTON_SET_ACT( normalButton, "N", qtr( "Normal rate" ), normal() );

}

AdvControlsWidget::~AdvControlsWidget()
{
}

void AdvControlsWidget::enableInput( bool enable )
{
    ABButton->setEnabled( enable );
    recordButton->setEnabled( enable );
    normalButton->setEnabled( enable );
}
void AdvControlsWidget::enableVideo( bool enable )
{
    snapshotButton->setEnabled( enable );
    frameButton->setEnabled( enable );
}

void AdvControlsWidget::normal()
{
    THEMIM->getIM()->normalRate();
}

void AdvControlsWidget::snapshot()
{
    vout_thread_t *p_vout = (vout_thread_t *)vlc_object_find( p_intf, VLC_OBJECT_VOUT, FIND_ANYWHERE );
    if( p_vout ) vout_Control( p_vout, VOUT_SNAPSHOT );
}

void AdvControlsWidget::frame(){}
void AdvControlsWidget::fromAtoB(){}
void AdvControlsWidget::record(){}

/*****************************
 * DA Control Widget !
 *****************************/
ControlsWidget::ControlsWidget( intf_thread_t *_p_i, bool b_advControls ) :
                             QFrame( NULL ), p_intf( _p_i )
{
    //QSize size( 500, 200 );
    //resize( size );
    controlLayout = new QGridLayout( this );
    controlLayout->setSpacing( 0 );
    setSizePolicy( QSizePolicy::Preferred, QSizePolicy::Minimum );

#if DEBUG_COLOR
    QPalette palette2;
    palette2.setColor(this->backgroundRole(), Qt::magenta);
    setPalette(palette2);
#endif

    /** The main Slider **/
    slider = new InputSlider( Qt::Horizontal, NULL );
    controlLayout->addWidget( slider, 0, 1, 1, 16 );
    /* Update the position when the IM has changed */
    CONNECT( THEMIM->getIM(), positionUpdated( float, int, int ),
             slider, setPosition( float,int, int ) );
    /* And update the IM, when the position has changed */
    CONNECT( slider, sliderDragged( float ),
             THEMIM->getIM(), sliderUpdate( float ) );

    /** Slower and faster Buttons **/
    slowerButton = new QPushButton( "S" );
    slowerButton->setFlat( true );

    BUTTON_SET_ACT( slowerButton, "S", qtr( "Slower" ), slower() );
    controlLayout->addWidget( slowerButton, 0, 0 );
    slowerButton->setMaximumSize( QSize( 26, 20 ) );

    fasterButton = new QPushButton( "F" );
    fasterButton->setFlat( true );

    BUTTON_SET_ACT( fasterButton, "F", qtr( "Faster" ), faster() );
    controlLayout->addWidget( fasterButton, 0, 17 );
    fasterButton->setMaximumSize( QSize( 26, 20 ) );

    /* advanced Controls handling */
    b_advancedVisible = b_advControls;

    advControls = new AdvControlsWidget( p_intf );
    controlLayout->addWidget( advControls, 1, 3, 2, 5, Qt::AlignBottom );
    if( !b_advancedVisible ) advControls->hide();
    // FIXME THIS should be removed.    need_components_update = true;

    /** Disc and Menus handling */
    discFrame = new QFrame( this );

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

    /** TODO
     * Telextext QFrame
     **/

    /** Play Buttons **/
    QSizePolicy sizePolicy( QSizePolicy::Fixed, QSizePolicy::Fixed );
    sizePolicy.setHorizontalStretch( 0 );
    sizePolicy.setVerticalStretch( 0 );

    /* Play */
    playButton = new QPushButton;
    playButton->setSizePolicy( sizePolicy );
    playButton->setMaximumSize( QSize( 38, 38 ) );
    playButton->setMinimumSize( QSize( 45, 45 ) );
    playButton->setIconSize( QSize( 30, 30 ) );

    controlLayout->addWidget( playButton, 2, 0, 2, 2 );

    controlLayout->setColumnMinimumWidth( 2, 20 );
    controlLayout->setColumnStretch( 2, 0 );

    /** Prev + Stop + Next Block **/
    QHBoxLayout *controlButLayout = new QHBoxLayout;
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
    controlLayout->addLayout( controlButLayout, 3, 3, 1, 3 );

    BUTTON_SET_ACT_I( playButton, "", play.png, qtr( "Play" ), play() );
    BUTTON_SET_ACT_I( prevButton, "" , previous.png,
                      qtr( "Previous" ), prev() );
    BUTTON_SET_ACT_I( nextButton, "", next.png, qtr( "Next" ), next() );
    BUTTON_SET_ACT_I( stopButton, "", stop.png, qtr( "Stop" ), stop() );

    controlLayout->setColumnStretch( 8 , 10 );
    controlLayout->setColumnStretch( 9, 0 );

    /*
     * Other first Line buttons
     * Might need to be inside a frame to avoid a few resizing pb
     * FIXME
     */
    /** Fullscreen/Visualisation **/
    fullscreenButton = new QPushButton( "F" );
    BUTTON_SET_ACT( fullscreenButton, "F", qtr( "Fullscreen" ), fullscreen() );
    setupSmallButton( fullscreenButton );
    controlLayout->addWidget( fullscreenButton, 3, 10 );

    /** Playlist Button **/
    playlistButton = new QPushButton;
    setupSmallButton( playlistButton );
    controlLayout->addWidget( playlistButton, 3, 11 );
    BUTTON_SET_IMG( playlistButton, "" , playlist.png, qtr( "Show playlist" ) );

    /** extended Settings **/
    QPushButton *extSettingsButton = new QPushButton( "F" );
    BUTTON_SET_ACT( extSettingsButton, "Ex", qtr( "Extended Settings" ),
            extSettings() );
    setupSmallButton( extSettingsButton );
    controlLayout->addWidget( extSettingsButton, 3, 12 );

    controlLayout->setColumnStretch( 14, 5 );

    /* Volume */
    VolumeClickHandler *h = new VolumeClickHandler( p_intf, this );

    volMuteLabel = new QLabel;
    volMuteLabel->setPixmap( QPixmap( ":/pixmaps/volume-high.png" ) );
    volMuteLabel->setToolTip( qtr( "Mute" ) );
    volMuteLabel->installEventFilter( h );

    /** TODO:
     * Change this slider to use a nice Amarok-like one
     * Add a Context menu to change to the most useful %
     * **/
    /** FIXME
     *  THis percerntage thing has to be handled correctly
     *  This has to match to the OSD
     **/
    volumeSlider = new QSlider;
    volumeSlider->setSizePolicy( sizePolicy );
    volumeSlider->setMaximumSize( QSize( 80, 200 ) );
    volumeSlider->setOrientation( Qt::Horizontal );

    volumeSlider->setMaximum( VOLUME_MAX );
    volumeSlider->setFocusPolicy( Qt::NoFocus );
    controlLayout->addWidget( volMuteLabel, 3, 15 );
    controlLayout->addWidget( volumeSlider, 3, 16, 1, 2 );

    /* Volume control connection */
    CONNECT( volumeSlider, valueChanged( int ), this, updateVolume( int ) );
    msg_Dbg( p_intf, "size: %i - %i", size().height(), size().width() );
}
ControlsWidget::~ControlsWidget()
{
}
void ControlsWidget::stop()
{
    THEMIM->stop();
}

void ControlsWidget::play()
{
    if( THEPL )
        msg_Dbg( p_intf, "There is %i playlist items", THEPL->items.i_size ); /* FIXME: remove me */
    if( playlist_IsEmpty( THEPL ) )
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
    else if( i_sliderVolume < VOLUME_MAX / 2 )
        volMuteLabel->setPixmap( QPixmap( ":/pixmaps/volume-low.png" ) );
    else volMuteLabel->setPixmap( QPixmap( ":/pixmaps/volume-high.png" ) );
}

void ControlsWidget::updateOnTimer()
{
    /* Audio part */
    audio_volume_t i_volume;
    aout_VolumeGet( p_intf, &i_volume );
    i_volume = ( i_volume *  VOLUME_MAX )/ (AOUT_VOLUME_MAX/2) ;
    int i_gauge = volumeSlider->value();
    b_my_volume = false;
    if( i_volume - i_gauge > 1 || i_gauge - i_volume > 1 )
    {
        b_my_volume = true;
        volumeSlider->setValue( i_volume );
        b_my_volume = false;
    }
 
    /* Activate the interface buttons according to the presence of the input */
    enableInput( THEMIM->getIM()->hasInput() );
    //enableVideo( THEMIM->getIM()->hasVideo() );
    enableVideo( true );
}

void ControlsWidget::setStatus( int status )
{
    if( status == PLAYING_S ) // Playing
        playButton->setIcon( QIcon( ":/pixmaps/pause.png" ) );
    else
        playButton->setIcon( QIcon( ":/pixmaps/play.png" ) );
}

/**
 * TODO
 * This functions toggle the fullscreen mode
 * If there is no video, it should first activate Visualisations...
 *  This has also to be fixed in enableVideo()
 */
void ControlsWidget::fullscreen()
{
    vout_thread_t *p_vout = (vout_thread_t *)vlc_object_find( p_intf, VLC_OBJECT_VOUT, FIND_ANYWHERE );
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
    //FIXME connect this one :D
    emit advancedControlsToggled( b_advancedVisible );  //  doComponentsUpdate();
}

/**********************************************************************
 * Playlist Widget. The embedded playlist
 **********************************************************************/
#include "components/playlist/panels.hpp"
#include "components/playlist/selector.hpp"

PlaylistWidget::PlaylistWidget( intf_thread_t *_p_i ) :
                                p_intf ( _p_i )
{
    /* Left Part and design */
    QWidget *leftW = new QWidget( this );
    QVBoxLayout *left = new QVBoxLayout( leftW );

    /* Source Selector */
    selector = new PLSelector( this, p_intf, THEPL );
    left->addWidget( selector );

    /* Art label */
    art = new QLabel( "" );
    art->setMinimumHeight( 128 );
    art->setMinimumWidth( 128 );
    art->setMaximumHeight( 128 );
    art->setMaximumWidth( 128 );
    art->setScaledContents( true );
    art->setPixmap( QPixmap( ":/noart.png" ) );
    left->addWidget( art );

    /* Initialisation of the playlist */
    playlist_item_t *p_root = playlist_GetPreferredNode( THEPL,
                                                THEPL->p_local_category );

    rightPanel = qobject_cast<PLPanel *>( new StandardPLPanel( this,
                              p_intf, THEPL, p_root ) );

    /* Connects */
    CONNECT( selector, activated( int ), rightPanel, setRoot( int ) );

    CONNECT( qobject_cast<StandardPLPanel *>( rightPanel )->model,
             artSet( QString ) , this, setArt( QString ) );
    /* Forward removal requests from the selector to the main panel */
    CONNECT( qobject_cast<PLSelector *>( selector )->model,
             shouldRemove( int ),
             qobject_cast<StandardPLPanel *>( rightPanel ), removeItem( int ) );

    connect( selector, SIGNAL( activated( int ) ),
             this, SIGNAL( rootChanged( int ) ) );
    emit rootChanged( p_root->i_id );

    /* Add the two sides of the QSplitter */
    addWidget( leftW );
    addWidget( rightPanel );

    leftW->setMaximumWidth( 250 );
    setCollapsible( 1, false );

    QList<int> sizeList;
    sizeList << 180 << 520 ;
    setSizes( sizeList );
}

void PlaylistWidget::setArt( QString url )
{
    if( url.isNull() )
        art->setPixmap( QPixmap( ":/noart.png" ) );
    else if( prevArt != url )
    {
        art->setPixmap( QPixmap( url ) );
        prevArt = url;
        emit artSet( url );
    }
}

PlaylistWidget::~PlaylistWidget()
{
}

QSize PlaylistWidget::sizeHint() const
{
    return widgetSize;
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
    
    normalSpeedButton = new QPushButton( "N" );
    normalSpeedButton->setMaximumSize( QSize( 26, 20 ) );
    normalSpeedButton->setFlat( true );
    normalSpeedButton->setToolTip( qtr( "Revert to normal play speed" ) );
 
    CONNECT( normalSpeedButton, clicked(), this, resetRate() );
 
    QVBoxLayout *speedControlLayout = new QVBoxLayout;
    speedControlLayout->addWidget(speedSlider);
    speedControlLayout->addWidget(normalSpeedButton);
    setLayout(speedControlLayout);
}

SpeedControlWidget::~SpeedControlWidget()
{
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
        rate = INPUT_RATE_DEFAULT* RATE_SLIDER_LENGTH /
                ( sliderValue * ( 1.0 - RATE_SLIDER_MINIMUM ) + RATE_SLIDER_LENGTH ) ;
    }
    else
    {
        rate = INPUT_RATE_DEFAULT* RATE_SLIDER_LENGTH /
                ( sliderValue * ( RATE_SLIDER_MAXIMUM - 1.0 ) + RATE_SLIDER_LENGTH );
    }

    THEMIM->getIM()->setRate(rate);    
}

void SpeedControlWidget::resetRate()
{
    THEMIM->getIM()->setRate(INPUT_RATE_DEFAULT);    
}
