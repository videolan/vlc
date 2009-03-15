/*****************************************************************************
 * playlist.cpp : Custom widgets for the playlist
 ****************************************************************************
 * Copyright © 2007-2008 the VideoLAN team
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "components/playlist/panels.hpp"
#include "components/playlist/selector.hpp"
#include "components/playlist/playlist.hpp"

#include "input_manager.hpp" /* art signal */
#include "main_interface.hpp" /* DropEvent TODO remove this*/

/**********************************************************************
 * Playlist Widget. The embedded playlist
 **********************************************************************/

PlaylistWidget::PlaylistWidget( intf_thread_t *_p_i ) : p_intf ( _p_i )
{
    setContentsMargins( 3, 3, 3, 3 );

    /* Left Part and design */
    QSplitter *leftW = new QSplitter( Qt::Vertical, this );

    /* Source Selector */
    selector = new PLSelector( this, p_intf );
    leftW->addWidget( selector );

    /* Create a Container for the Art Label
       in order to have a beautiful resizing for the selector above it */
    QWidget *artContainer = new QWidget;
    QHBoxLayout *artContLay = new QHBoxLayout( artContainer );
    artContLay->setMargin( 0 );
    artContLay->setSpacing( 0 );
    artContainer->setMaximumHeight( 128 );

    /* Art label */
    art = new ArtLabel( artContainer, p_intf );
    art->setToolTip( qtr( "Double click to get media information" ) );

    artContLay->addWidget( art, 1 );

    leftW->addWidget( artContainer );

    /* Initialisation of the playlist */
    playlist_t * p_playlist = THEPL;
    PL_LOCK;
    playlist_item_t *p_root =
                  playlist_GetPreferredNode( THEPL, THEPL->p_local_category );
    PL_UNLOCK;

    rightPanel = new StandardPLPanel( this, p_intf, THEPL, p_root );

    /* Connect the activation of the selector to a redefining of the PL */
    CONNECT( selector, activated( int ), rightPanel, setRoot( int ) );

    /* Connect the activated() to the rootChanged() signal
       This will be used by StandardPLPanel to setCurrentRootId, that will
       change the label of the addButton  */
    connect( selector, SIGNAL( activated( int ) ),
             this, SIGNAL( rootChanged( int ) ) );

    /* Forward removal requests from the selector to the main panel */
    CONNECT( qobject_cast<PLSelector *>( selector )->model,
             shouldRemove( int ),
             qobject_cast<StandardPLPanel *>( rightPanel ), removeItem( int ) );

    emit rootChanged( p_root->i_id );

    /* Add the two sides of the QSplitter */
    addWidget( leftW );
    addWidget( rightPanel );

    QList<int> sizeList;
    sizeList << 180 << 420 ;
    setSizes( sizeList );
    //setSizePolicy( QSizePolicy::Preferred, QSizePolicy::Expanding );
    setStretchFactor( 0, 0 );
    setStretchFactor( 1, 3 );
    leftW->setMaximumWidth( 250 );
    setCollapsible( 1, false );

    /* In case we want to keep the splitter informations */
    // components shall never write there setting to a fixed location, may infer
    // with other uses of the same component...
    // getSettings()->beginGroup( "playlist" );
    getSettings()->beginGroup("Playlist");
    restoreState( getSettings()->value("splitterSizes").toByteArray());
    getSettings()->endGroup();

    setAcceptDrops( true );
    setWindowTitle( qtr( "Playlist" ) );
    setWindowIcon( QApplication::windowIcon() );
}

PlaylistWidget::~PlaylistWidget()
{
    getSettings()->beginGroup("Playlist");
    getSettings()->setValue( "splitterSizes", saveState() );
    getSettings()->endGroup();
    msg_Dbg( p_intf, "Playlist Destroyed" );
}

void PlaylistWidget::dropEvent( QDropEvent *event )
{
    if( p_intf->p_sys->p_mi )
        p_intf->p_sys->p_mi->dropEventPlay( event, false );
}
void PlaylistWidget::dragEnterEvent( QDragEnterEvent *event )
{
    event->acceptProposedAction();
}

void PlaylistWidget::closeEvent( QCloseEvent *event )
{
    if( THEDP->isDying() )
    {
        /* FIXME is it needed ? */
        close();
    }
    else
    {
        if( p_intf->p_sys->p_mi )
            p_intf->p_sys->p_mi->togglePlaylist();
    }
    event->accept();
}
