/*****************************************************************************
 * interface_widgets.cpp : Custom widgets for the main interface
 ****************************************************************************
 * Copyright (C) 2006 the VideoLAN team
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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
    setMinimumSize( 0, 0 );
    return (void*)winId();
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

    backgroundLayout = new QHBoxLayout;
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
    backgroundLayout->takeAt(0);
    delete backgroundLayout;
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
    QPushButton *nextButton = new QPushButton( "Next");
    layout->addWidget( prevButton );
    layout->addWidget( nextButton );

    layout->addItem( new QSpacerItem( 40,20,
                              QSizePolicy::Expanding, QSizePolicy::Minimum) );
    layout->addWidget( new QLabel( qtr("Current visualization:") ) );

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
ControlsWidget::ControlsWidget( intf_thread_t *_p_i ) :
                                           QFrame( NULL ), p_intf( _p_i )
{
    QHBoxLayout *layout = new QHBoxLayout( this );
    layout->setMargin( 0 );

    slowerButton = new QPushButton( "S" );
    BUTTON_SET_ACT( slowerButton, "S", qtr("Slower" ), slower() );
    layout->addWidget( slowerButton );
    slowerButton->setMaximumWidth( 35 );

    normalButton = new QPushButton( "N" );
    BUTTON_SET_ACT( normalButton, "N", qtr("Normal rate"), normal() );
    layout->addWidget( normalButton );
    normalButton->setMaximumWidth( 35 );

    fasterButton = new QPushButton( "F" );
    BUTTON_SET_ACT( fasterButton, "F", qtr("Faster" ), faster() );
    layout->addWidget( fasterButton );
    fasterButton->setMaximumWidth( 35 );

    layout->addItem( new QSpacerItem( 100,20,
                              QSizePolicy::Expanding, QSizePolicy::Minimum) );

    snapshotButton = new QPushButton( "S" );
    BUTTON_SET_ACT( snapshotButton, "S", qtr("Take a snapshot"), snapshot() );
    layout->addWidget( snapshotButton );
    snapshotButton->setMaximumWidth( 35 );

    fullscreenButton = new QPushButton( "F" );
    BUTTON_SET_ACT( fullscreenButton, "F", qtr("Fullscreen"), fullscreen() );
    layout->addWidget( fullscreenButton );
    fullscreenButton->setMaximumWidth( 35 );
}

ControlsWidget::~ControlsWidget()
{
}

void ControlsWidget::enableInput( bool enable )
{
    slowerButton->setEnabled( enable );
    normalButton->setEnabled( enable );
    fasterButton->setEnabled( enable );
}
void ControlsWidget::enableVideo( bool enable )
{
    snapshotButton->setEnabled( enable );
    fullscreenButton->setEnabled( enable );
}

void ControlsWidget::slower()
{
    THEMIM->getIM()->slower();
}

void ControlsWidget::faster()
{
    THEMIM->getIM()->faster();
}

void ControlsWidget::normal()
{
    THEMIM->getIM()->normalRate();
}

void ControlsWidget::snapshot()
{
}

void ControlsWidget::fullscreen()
{
}

/**********************************************************************
 * Playlist Widget. The embedded playlist
 **********************************************************************/
#include "components/playlist/panels.hpp"
#include "components/playlist/selector.hpp"

PlaylistWidget::PlaylistWidget( intf_thread_t *_p_intf ) :
                                BasePlaylistWidget ( _p_intf)
{
    QVBoxLayout *left = new QVBoxLayout( );

    selector = new PLSelector( this, p_intf, THEPL );
    selector->setMaximumWidth( 130 );
    left->addWidget( selector );

    art = new QLabel( "" );
    art->setMinimumHeight( 128 );
    art->setMinimumWidth( 128 );
    art->setMaximumHeight( 128 );
    art->setMaximumWidth( 128 );
    art->setScaledContents( true );

    art->setPixmap( QPixmap( ":/noart.png" ) );
    left->addWidget( art );

    playlist_item_t *p_root = playlist_GetPreferredNode( THEPL,
                                                THEPL->p_local_category );

    rightPanel = qobject_cast<PLPanel *>(new StandardPLPanel( this,
                              p_intf, THEPL, p_root ) );

    CONNECT( selector, activated( int ), rightPanel, setRoot( int ) );

    CONNECT( qobject_cast<StandardPLPanel *>(rightPanel)->model,
             artSet( QString ) , this, setArt( QString ) );
    /* Forward removal requests from the selector to the main panel */
    CONNECT( qobject_cast<PLSelector *>(selector)->model,
             shouldRemove( int ),
             qobject_cast<StandardPLPanel *>(rightPanel), removeItem(int) );

    connect( selector, SIGNAL(activated( int )),
             this, SIGNAL( rootChanged( int ) ) );
    emit rootChanged( p_root->i_id );

    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->addLayout( left, 0 );
    layout->addWidget( rightPanel, 10 );
    setLayout( layout );
}

void PlaylistWidget::setArt( QString url )
{
    if( url.isNull() )
        art->setPixmap( QPixmap( ":/noart.png" ) );
    else if( prevArt != url )
        art->setPixmap( QPixmap( url ) );
    prevArt = url;
}

PlaylistWidget::~PlaylistWidget()
{
}

QSize PlaylistWidget::sizeHint() const
{
    return widgetSize;
}

