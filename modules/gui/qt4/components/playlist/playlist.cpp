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

#include "components/playlist/panels.hpp"
#include "components/playlist/selector.hpp"
#include "components/playlist/playlist.hpp"

#include <QSettings>
#include <QLabel>
#include <QSpacerItem>
#include <QCursor>
#include <QPushButton>
#include <QVBoxLayout>

/**********************************************************************
 * Playlist Widget. The embedded playlist
 **********************************************************************/

PlaylistWidget::PlaylistWidget( intf_thread_t *_p_i, QSettings *settings ) :
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
    sizeList << 180 << 420 ;
    setSizes( sizeList );
    setSizePolicy( QSizePolicy::Preferred, QSizePolicy::Expanding );
    setStretchFactor( 0, 0 );
    setStretchFactor( 1, 3 );

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
    settings->setValue( "pos", pos() );
    settings->setValue( "size", size() );
    settings->setValue("splitterSizes", saveState() );
    settings->endGroup();
}

