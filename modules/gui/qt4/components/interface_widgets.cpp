/*****************************************************************************
 * interface_widgets.cpp : Custom widgets for the main interface
 ****************************************************************************
 * Copyright ( C ) 2006 the VideoLAN team
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Jean-Baptiste Kempf <jb@videolan.org>
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
#include "qt4.hpp"
#include "components/interface_widgets.hpp"
#include "main_interface.hpp"
#include "input_manager.hpp"

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

#define ICON_SIZE 300

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

    label = new QLabel( "" );
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
 * More controls
 **********************************************************************/
AdvControlsWidget::AdvControlsWidget( intf_thread_t *_p_i ) :
                                           QFrame( NULL ), p_intf( _p_i )
{
    QHBoxLayout *layout = new QHBoxLayout( this );
    layout->setMargin( 0 );

    normalButton = new QPushButton( "N" );
    BUTTON_SET_ACT( normalButton, "N", qtr( "Normal rate" ), normal() );
    layout->addWidget( normalButton );
    normalButton->setMaximumWidth( 35 );


    layout->addItem( new QSpacerItem( 100,20,
                              QSizePolicy::Expanding, QSizePolicy::Minimum ) );

    snapshotButton = new QPushButton( "S" );
    BUTTON_SET_ACT( snapshotButton, "S", qtr( "Take a snapshot" ), snapshot() );
    layout->addWidget( snapshotButton );
    snapshotButton->setMaximumWidth( 35 );
}

AdvControlsWidget::~AdvControlsWidget()
{
}

void AdvControlsWidget::enableInput( bool enable )
{
//    slowerButton->setEnabled( enable );
    normalButton->setEnabled( enable );
//    fasterButton->setEnabled( enable );
}
void AdvControlsWidget::enableVideo( bool enable )
{
    snapshotButton->setEnabled( enable );
    //fullscreenButton->setEnabled( enable );
}

void AdvControlsWidget::normal()
{
    THEMIM->getIM()->normalRate();
}

void AdvControlsWidget::snapshot()
{
}

void AdvControlsWidget::fullscreen()
{
}

ControlsWidget::ControlsWidget( intf_thread_t *_p_i ) :
                             QFrame( NULL ), p_intf( _p_i )
{
    //QSize size( 500, 200 );
    //resize( size );
    controlLayout = new QGridLayout( this );

    /** The main Slider **/
    slider = new InputSlider( Qt::Horizontal, NULL );
    controlLayout->addWidget( slider, 0, 1, 1, 15 );
    /* Update the position when the IM has changed */
    CONNECT( THEMIM->getIM(), positionUpdated( float, int, int ),
             slider, setPosition( float,int, int ) );
    /* And update the IM, when the position has changed */
    CONNECT( slider, sliderDragged( float ),
             THEMIM->getIM(), sliderUpdate( float ) );

    /** Slower and faster Buttons **/
    slowerButton = new QPushButton( "S" );
    BUTTON_SET_ACT( slowerButton, "S", qtr( "Slower" ), slower() );
    controlLayout->addWidget( slowerButton, 0, 0 );
    slowerButton->setMaximumSize( QSize( 26, 20 ) );

    fasterButton = new QPushButton( "F" );
    BUTTON_SET_ACT( fasterButton, "F", qtr( "Faster" ), faster() );
    controlLayout->addWidget( fasterButton, 0, 16 );
    fasterButton->setMaximumSize( QSize( 26, 20 ) );

    /** TODO: Insert here the AdvControls Widget 
     * and add - A->B button
     *         - frame by frame
     *         - record button
     * and put the snapshot in the same QFrame 
     * Then fix all the size issues in main_interface.cpp
     **/
    
    /** Disc and Menus handling */
    discFrame = new QFrame( this );
    QHBoxLayout *discLayout = new QHBoxLayout( discFrame );

    QPushButton *menuButton = new QPushButton( discFrame );
    discLayout->addWidget( menuButton );

    QPushButton *prevSectionButton = new QPushButton( discFrame );
    discLayout->addWidget( prevSectionButton );

    QPushButton *nextSectionButton = new QPushButton( discFrame );
    discLayout->addWidget( nextSectionButton );

    controlLayout->addWidget( discFrame, 1, 13, 1, 4 );

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
    QSizePolicy sizePolicy( QSizePolicy::Maximum, QSizePolicy::Fixed );
    sizePolicy.setHorizontalStretch( 0 );
    sizePolicy.setVerticalStretch( 0 );
//  sizePolicy.setHeightForWidth( playButton->sizePolicy().hasHeightForWidth() );

    /* Play */
    QPushButton *playButton = new QPushButton;
    playButton->setSizePolicy( sizePolicy );
    playButton->setMaximumSize( QSize( 45, 45 ) );
    playButton->setIconSize( QSize( 30, 30 ) );

    controlLayout->addWidget( playButton, 2, 0, 2, 2 );

    /** Prev + Stop + Next Block **/
    QHBoxLayout *controlButLayout = new QHBoxLayout;
    controlButLayout->setSpacing( 0 ); /* Don't remove that, will be useful */

    /* Prev */
    QPushButton *prevButton = new QPushButton;
    prevButton->setSizePolicy( sizePolicy );
    prevButton->setMaximumSize( QSize( 26, 26 ) );
    prevButton->setIconSize( QSize( 20, 20 ) );

    controlButLayout->addWidget( prevButton );

    /* Stop */
    QPushButton *stopButton = new QPushButton;
    stopButton->setSizePolicy( sizePolicy );
    stopButton->setMaximumSize( QSize( 26, 26 ) );
    stopButton->setIconSize( QSize( 20, 20 ) );

    controlButLayout->addWidget( stopButton );

    /* next */
    QPushButton *nextButton = new QPushButton;
    nextButton->setSizePolicy( sizePolicy );
    nextButton->setMaximumSize( QSize( 26, 26 ) );
    nextButton->setIconSize( QSize( 20, 20 ) );

    controlButLayout->addWidget( nextButton );

    /* Add this block to the main layout */
    controlLayout->addLayout( controlButLayout, 3, 3, 1, 3 );

    BUTTON_SET_ACT_I( playButton, "", play.png, qtr( "Play" ), play() );
    BUTTON_SET_ACT_I( prevButton, "" , previous.png,
                      qtr( "Previous" ), prev() );
    BUTTON_SET_ACT_I( nextButton, "", next.png, qtr( "Next" ), next() );
    BUTTON_SET_ACT_I( stopButton, "", stop.png, qtr( "Stop" ), stop() );

    /*
     * Other first Line buttons
     * Might need to be inside a frame to avoid a few resizing pb
     * FIXME
     */
    /** Fullscreen/Visualisation **/
    fullscreenButton = new QPushButton( "F" );
    BUTTON_SET_ACT( fullscreenButton, "F", qtr( "Fullscreen" ), fullscreen() );
    fullscreenButton->setMaximumSize( QSize( 26, 26 ) );
    controlLayout->addWidget( fullscreenButton, 3, 10 );

    /** Playlist Button **/
    playlistButton = new QPushButton;
    playlistButton->setMaximumSize( QSize( 26, 26 ) );
    playlistButton->setIconSize( QSize( 20, 20 ) );

    controlLayout->addWidget( playlistButton, 3, 11 );

    /** extended Settings **/
    QPushButton *extSettingsButton = new QPushButton( "F" );
    BUTTON_SET_ACT( extSettingsButton, "Ex", qtr( "Extended Settings" ),
            extSettings() );
    extSettingsButton->setMaximumSize( QSize( 26, 26 ) );
    controlLayout->addWidget( extSettingsButton, 3, 12 );

    /** Preferences **/
    QPushButton *prefsButton = new QPushButton( "P" );
    BUTTON_SET_ACT( prefsButton, "P", qtr( "Preferences / Settings" ), prefs() );
    prefsButton->setMaximumSize( QSize( 26, 26 ) );
    controlLayout->addWidget( prefsButton, 3, 13 );

    /* Volume */
    VolumeClickHandler *h = new VolumeClickHandler( p_intf, this );

    QLabel *volMuteLabel = new QLabel;
    volMuteLabel->setPixmap( QPixmap( ":/pixmaps/volume-low.png" ) );
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

    volumeSlider->setMaximum( 100 );
    volumeSlider->setFocusPolicy( Qt::NoFocus );
    controlLayout->addWidget( volMuteLabel, 3, 14 );
    controlLayout->addWidget( volumeSlider, 3, 15, 1, 2 );

    /* Volume control connection */
    CONNECT( volumeSlider, valueChanged( int ), this, updateVolume( int ) );

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
        prevSectionButton->show();
        prevSectionButton->setToolTip( qfu( HELP_PCH ) );
        nextSectionButton->show();
        nextSectionButton->setToolTip( qfu( HELP_NCH ) );
        menuButton->show();
        discFrame->show();
    } else {
        prevSectionButton->show();
        prevSectionButton->setToolTip( qfu( HELP_PCH ) );
        nextSectionButton->show();
        nextSectionButton->setToolTip( qfu( HELP_NCH ) );
        menuButton->hide();
        discFrame->show();
    }
}

static bool b_my_volume;
void ControlsWidget::updateVolume( int sliderVolume )
{
    if( !b_my_volume )
    {
        int i_res = sliderVolume * AOUT_VOLUME_MAX /
                            ( 2*volumeSlider->maximum() );
        aout_VolumeSet( p_intf, i_res );
    }
}

void ControlsWidget::updateOnTimer()
{
    audio_volume_t i_volume;
    aout_VolumeGet( p_intf, &i_volume );
    i_volume = ( i_volume *  200 )/ AOUT_VOLUME_MAX ;
    int i_gauge = volumeSlider->value();
    b_my_volume = false;
    if( i_volume - i_gauge > 1 || i_gauge - i_volume > 1 )
    {
        b_my_volume = true;
        volumeSlider->setValue( i_volume );
        b_my_volume = false;
    }
}

/* FIXME */
void ControlsWidget::setStatus( int status )
{
    if( status == 1 ) // Playing
    {
        msg_Dbg( p_intf, "I was here %i", status );
        // playButton->setIcon( QIcon( ":/pixmaps/pause.png" ) );
    }
    else
    {
        msg_Dbg( p_intf, "I was here %i", status );
        // playButton->setIcon( QIcon( ":/pixmaps/play.png" ) );
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
    msg_Dbg( p_intf, "Not implemented yet" );
}

void ControlsWidget::extSettings()
{
    THEDP->extendedDialog();
}
void ControlsWidget::prefs()
{
    THEDP->prefsDialog();
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
}

void ControlsWidget::enableVideo( bool enable )
{
    // TODO Later make the fullscreenButton toggle Visualisation and so on.
    fullscreenButton->setEnabled( enable );
}


/**********************************************************************
 * Playlist Widget. The embedded playlist
 **********************************************************************/
#include "components/playlist/panels.hpp"
#include "components/playlist/selector.hpp"

PlaylistWidget::PlaylistWidget( intf_thread_t *_p_intf ) :
                                p_intf ( _p_intf )
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
        art->setPixmap( QPixmap( url ) );
    prevArt = url;
    emit artSet( url );
}

PlaylistWidget::~PlaylistWidget()
{
}

QSize PlaylistWidget::sizeHint() const
{
    return widgetSize;
}

