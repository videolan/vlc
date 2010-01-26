/*****************************************************************************
 * standardpanel.cpp : The "standard" playlist panel : just a treeview
 ****************************************************************************
 * Copyright (C) 2000-2009 VideoLAN
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          JB Kempf <jb@videolan.org>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "dialogs_provider.hpp"

#include "components/playlist/playlist_model.hpp"
#include "components/playlist/standardpanel.hpp"
#include "components/playlist/icon_view.hpp"
#include "util/customwidgets.hpp"

#include <vlc_intf_strings.h>

#include <QPushButton>
#include <QHeaderView>
#include <QKeyEvent>
#include <QModelIndexList>
#include <QLabel>
#include <QMenu>
#include <QSignalMapper>
#include <QWheelEvent>

#include <assert.h>

#include "sorting.h"

StandardPLPanel::StandardPLPanel( PlaylistWidget *_parent,
                                  intf_thread_t *_p_intf,
                                  playlist_t *p_playlist,
                                  playlist_item_t *p_root ):
                                  QWidget( _parent ), p_intf( _p_intf )
{
    layout = new QGridLayout( this );
    layout->setSpacing( 0 ); layout->setMargin( 0 );
    setMinimumWidth( 300 );

    model = new PLModel( p_playlist, p_intf, p_root, this );

    iconView = NULL;
    treeView = NULL;

    /* Saved Settings */
    getSettings()->beginGroup("Playlist");

    int i_viewMode = getSettings()->value( "view-mode", TREE_VIEW ).toInt();
    if( i_viewMode == ICON_VIEW )
    {
        createIconView();
        currentView = iconView;
    }
    else
    {
        createTreeView();
        currentView = treeView;
    }

    getSettings()->endGroup();

    currentRootId = -1;

    /* Title label */
    title = new QLabel;
    QFont titleFont;
    titleFont.setPointSize( titleFont.pointSize() + 6 );
    titleFont.setFamily( "Verdana" );
    title->setFont( titleFont );
    layout->addWidget( title, 0, 0 );

    /* A Spacer and the search possibilities */
    layout->setColumnStretch( 1, 10 );

    SearchLineEdit *search = new SearchLineEdit( this );
    search->setMaximumWidth( 300 );
    layout->addWidget( search, 0, 4 );
    CONNECT( search, textChanged( const QString& ),
             this, search( const QString& ) );
    layout->setColumnStretch( 4, 2 );

    /* Add item to the playlist button */
    addButton = new QPushButton;
    addButton->setIcon( QIcon( ":/buttons/playlist/playlist_add" ) );
    addButton->setMaximumWidth( 30 );
    BUTTONACT( addButton, popupAdd() );
    layout->addWidget( addButton, 0, 3 );

    QPushButton *viewButton = new QPushButton( this );
    viewButton->setIcon( QIcon( ":/buttons/playlist/playlist_add" ) );
    layout->addWidget( viewButton, 0, 2 );
    BUTTONACT( viewButton, toggleView() );
}

StandardPLPanel::~StandardPLPanel()
{
    getSettings()->beginGroup("Playlist");
    if( treeView )
        getSettings()->setValue( "headerStateV2", treeView->header()->saveState() );
    getSettings()->setValue( "view-mode", ( currentView == iconView ) ? ICON_VIEW : TREE_VIEW );
    getSettings()->endGroup();
}

/* Unused anymore, but might be useful, like in right-click menu */
void StandardPLPanel::gotoPlayingItem()
{
    currentView->scrollTo( model->currentIndex() );
}

void StandardPLPanel::handleExpansion( const QModelIndex& index )
{
    assert( currentView );
    currentView->scrollTo( index );
}

/* PopupAdd Menu for the Add Menu */
void StandardPLPanel::popupAdd()
{
    QMenu popup;
    if( currentRootId == THEPL->p_local_category->i_id ||
        currentRootId == THEPL->p_local_onelevel->i_id )
    {
        popup.addAction( qtr(I_PL_ADDF), THEDP, SLOT( simplePLAppendDialog()) );
        popup.addAction( qtr(I_PL_ADDDIR), THEDP, SLOT( PLAppendDir()) );
        popup.addAction( qtr(I_OP_ADVOP), THEDP, SLOT( PLAppendDialog()) );
    }
    else if( ( THEPL->p_ml_category &&
                currentRootId == THEPL->p_ml_category->i_id ) ||
             ( THEPL->p_ml_onelevel &&
                currentRootId == THEPL->p_ml_onelevel->i_id ) )
    {
        popup.addAction( qtr(I_PL_ADDF), THEDP, SLOT( simpleMLAppendDialog()) );
        popup.addAction( qtr(I_PL_ADDDIR), THEDP, SLOT( MLAppendDir() ) );
        popup.addAction( qtr(I_OP_ADVOP), THEDP, SLOT( MLAppendDialog() ) );
    }

    popup.exec( QCursor::pos() - addButton->mapFromGlobal( QCursor::pos() )
                        + QPoint( 0, addButton->height() ) );
}

void StandardPLPanel::popupPlView( const QPoint &point )
{
    QModelIndex index = currentView->indexAt( point );
    QPoint globalPoint = currentView->viewport()->mapToGlobal( point );
    QItemSelectionModel *selection = currentView->selectionModel();
    QModelIndexList list = selection->selectedIndexes();
    model->popup( index, globalPoint, list );
}

void StandardPLPanel::popupSelectColumn( QPoint pos )
{
    QMenu menu;
    assert( treeView );

    /* We do not offer the option to hide index 0 column, or
    * QTreeView will behave weird */
    int i, j;
    for( i = 1 << 1, j = 1; i < COLUMN_END; i <<= 1, j++ )
    {
        QAction* option = menu.addAction(
            qfu( psz_column_title( i ) ) );
        option->setCheckable( true );
        option->setChecked( !treeView->isColumnHidden( j ) );
        selectColumnsSigMapper->setMapping( option, j );
        CONNECT( option, triggered(), selectColumnsSigMapper, map() );
    }
    menu.exec( QCursor::pos() );
}

void StandardPLPanel::toggleColumnShown( int i )
{
    treeView->setColumnHidden( i, !treeView->isColumnHidden( i ) );
}

/* Search in the playlist */
void StandardPLPanel::search( const QString& searchText )
{
    model->search( searchText );
}

/* Set the root of the new Playlist */
/* This activated by the selector selection */
void StandardPLPanel::setRoot( playlist_item_t *p_item )
{
    QPL_LOCK;
    assert( p_item );

    playlist_item_t *p_pref_item = playlist_GetPreferredNode( THEPL, p_item );
    if( p_pref_item ) p_item = p_pref_item;

    /* needed for popupAdd() */
    currentRootId = p_item->i_id;

    /* cosmetics, ..still need playlist locking.. */
    char *psz_title = input_item_GetName( p_item->p_input );
    title->setText( qfu(psz_title) );
    free( psz_title );

    QPL_UNLOCK;

    /* do THE job */
    model->rebuild( p_item );

    /* enable/disable adding */
    if( p_item == THEPL->p_local_category ||
        p_item == THEPL->p_local_onelevel )
    {
        addButton->setEnabled( true );
        addButton->setToolTip( qtr(I_PL_ADDPL) );
    }
    else if( ( THEPL->p_ml_category && p_item == THEPL->p_ml_category) ||
              ( THEPL->p_ml_onelevel && p_item == THEPL->p_ml_onelevel ) )
    {
        addButton->setEnabled( true );
        addButton->setToolTip( qtr(I_PL_ADDML) );
    }
    else
        addButton->setEnabled( false );
}

void StandardPLPanel::removeItem( int i_id )
{
    model->removeItem( i_id );
}

/* Delete and Suppr key remove the selection
   FilterKey function and code function */
void StandardPLPanel::keyPressEvent( QKeyEvent *e )
{
    switch( e->key() )
    {
    case Qt::Key_Back:
    case Qt::Key_Delete:
        deleteSelection();
        break;
    }
}

void StandardPLPanel::deleteSelection()
{
    QItemSelectionModel *selection = currentView->selectionModel();
    QModelIndexList list = selection->selectedIndexes();
    model->doDelete( list );
}

void StandardPLPanel::createIconView()
{
    iconView = new PlIconView( model, this );
    iconView->setContextMenuPolicy( Qt::CustomContextMenu );
    CONNECT( iconView, customContextMenuRequested( const QPoint & ),
             this, popupPlView( const QPoint & ) );

    layout->addWidget( iconView, 1, 0, 1, -1 );
}

void StandardPLPanel::createTreeView()
{
    /* Create and configure the QTreeView */
    treeView = new QTreeView;
    treeView->setModel( model );

    treeView->setIconSize( QSize( 20, 20 ) );
    treeView->setAlternatingRowColors( true );
    treeView->setAnimated( true );
    treeView->setUniformRowHeights( true );
    treeView->setSortingEnabled( true );
    treeView->header()->setSortIndicator( -1 , Qt::AscendingOrder );
    treeView->header()->setSortIndicatorShown( true );
    treeView->header()->setClickable( true );
    treeView->header()->setContextMenuPolicy( Qt::CustomContextMenu );

    treeView->setSelectionBehavior( QAbstractItemView::SelectRows );
    treeView->setSelectionMode( QAbstractItemView::ExtendedSelection );
    treeView->setDragEnabled( true );
    treeView->setAcceptDrops( true );
    treeView->setDropIndicatorShown( true );
    treeView->setContextMenuPolicy( Qt::CustomContextMenu );

    if( getSettings()->contains( "headerStateV2" ) )
    {
        treeView->header()->restoreState(
                getSettings()->value( "headerStateV2" ).toByteArray() );
    }
    else
    {
        for( int m = 1, c = 0; m != COLUMN_END; m <<= 1, c++ )
        {
            treeView->setColumnHidden( c, !( m & COLUMN_DEFAULT ) );
            if( m == COLUMN_TITLE ) treeView->header()->resizeSection( c, 200 );
            else if( m == COLUMN_DURATION ) treeView->header()->resizeSection( c, 80 );
        }
    }

    /* Connections for the TreeView */
    CONNECT( treeView, activated( const QModelIndex& ),
             model,activateItem( const QModelIndex& ) );
    CONNECT( treeView->header(), customContextMenuRequested( const QPoint & ),
             this, popupSelectColumn( QPoint ) );
    CONNECT( treeView, customContextMenuRequested( const QPoint & ),
             this, popupPlView( const QPoint & ) );

    /* SignalMapper for columns */
    selectColumnsSigMapper = new QSignalMapper( this );
    CONNECT( selectColumnsSigMapper, mapped( int ),
             this, toggleColumnShown( int ) );

    /* Finish the layout */
    layout->addWidget( treeView, 1, 0, 1, -1 );
}

void StandardPLPanel::toggleView()
{
    if( treeView && treeView->isVisible() )
    {
        if( iconView == NULL )
            createIconView();

        treeView->hide();
        iconView->show();
        currentView = iconView;
    }
    else
    {
        if( treeView == NULL )
            createTreeView();

        iconView->hide();
        treeView->show();
        currentView = treeView;
    }
    CONNECT( model, currentChanged( const QModelIndex& ),
             this, handleExpansion( const QModelIndex& ) );
}

void StandardPLPanel::wheelEvent( QWheelEvent *e )
{
    // Accept this event in order to prevent unwanted volume up/down changes
    e->accept();
}
