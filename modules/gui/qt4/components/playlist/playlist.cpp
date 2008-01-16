/*****************************************************************************
 * playlist.cpp : Custom widgets for the playlist
 ****************************************************************************
 * Copyright © 2006 the VideoLAN team
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

#include "components/playlist/panels.hpp"
#include "components/playlist/selector.hpp"
#include "components/playlist/playlist.hpp"
#include "input_manager.hpp" /* art */

#include <QSettings>
#include <QLabel>
#include <QSpacerItem>
#include <QCursor>
#include <QPushButton>
#include <QVBoxLayout>

/**********************************************************************
 * Playlist Widget. The embedded playlist
 **********************************************************************/

PlaylistWidget::PlaylistWidget( intf_thread_t *_p_i, QSettings *settings, QWidget *_parent ) :
                                p_intf ( _p_i ), parent( _parent )
{
    /* Left Part and design */
    QSplitter *leftW = new QSplitter( Qt::Vertical, this );

    /* Source Selector */
    selector = new PLSelector( this, p_intf, THEPL );
    leftW->addWidget( selector );

    /* Art label */
    art = new QLabel( "" );
    art->setMinimumHeight( 128 );
    art->setMinimumWidth( 128 );
    art->setMaximumHeight( 128 );
    art->setMaximumWidth( 128 );
    art->setScaledContents( true );
    art->setPixmap( QPixmap( ":/noart.png" ) );
    leftW->addWidget( art );

    /* Initialisation of the playlist */
    playlist_item_t *p_root =
                  playlist_GetPreferredNode( THEPL, THEPL->p_local_category );

    rightPanel = new StandardPLPanel( this, p_intf, THEPL, p_root );

    /* Connect the activation of the selector to a redefining of the PL */
    CONNECT( selector, activated( int ), rightPanel, setRoot( int ) );

    /* Connect the activated() to the rootChanged() signal
       This will be used by StandardPLPanel to setCurrentRootId, that will 
       change the label of the addButton  */
    connect( selector, SIGNAL( activated( int ) ),
             this, SIGNAL( rootChanged( int ) ) );

    CONNECT( THEMIM->getIM(), artChanged( QString ) , this, setArt( QString ) );
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
    setSizePolicy( QSizePolicy::Preferred, QSizePolicy::Expanding );
    setStretchFactor( 0, 0 );
    setStretchFactor( 1, 3 );
    leftW->setMaximumWidth( 250 );
    setCollapsible( 1, false );

    /* In case we want to keep the splitter informations */
    settings->beginGroup( "playlist" );
    restoreState( settings->value("splitterSizes").toByteArray());
    resize( settings->value("size", QSize(600, 300)).toSize());
    move( settings->value("pos", QPoint( 0, 400)).toPoint());
    settings->endGroup();
}

void PlaylistWidget::setArt( QString url )
{
    if( url.isNull() )
    {
        art->setPixmap( QPixmap( ":/noart.png" ) );
        emit artSet( url );
    }
    else if( prevArt != url )
    {
        art->setPixmap( QPixmap( url ) );
        prevArt = url;
        emit artSet( url );
    }
}

QSize PlaylistWidget::sizeHint() const
{
   return QSize( 600 , 300 );
}

PlaylistWidget::~PlaylistWidget()
{}

void PlaylistWidget::savingSettings( QSettings *settings )
{
    settings->beginGroup( "playlist" );
    settings->setValue( "pos", parent->pos() );
    settings->setValue( "size", parent->size() );
    settings->setValue( "splitterSizes", saveState() );
    settings->endGroup();
}

