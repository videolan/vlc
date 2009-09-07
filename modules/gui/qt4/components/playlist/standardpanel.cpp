/*****************************************************************************
 * standardpanel.cpp : The "standard" playlist panel : just a treeview
 ****************************************************************************
 * Copyright (C) 2000-2005 the VideoLAN team
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "qt4.hpp"
#include "dialogs_provider.hpp"

#include "components/playlist/playlist_model.hpp"
#include "components/playlist/panels.hpp"
#include "util/customwidgets.hpp"

#include <vlc_intf_strings.h>

#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QModelIndexList>
#include <QLabel>
#include <QSpacerItem>
#include <QMenu>
#include <QSignalMapper>
#include <assert.h>

#include "sorting.h"

StandardPLPanel::StandardPLPanel( PlaylistWidget *_parent,
                                  intf_thread_t *_p_intf,
                                  playlist_t *p_playlist,
                                  playlist_item_t *p_root ):
                                  PLPanel( _parent, _p_intf )
{
    model = new PLModel( p_playlist, p_intf, p_root, -1, this );

    QVBoxLayout *layout = new QVBoxLayout();
    layout->setSpacing( 0 ); layout->setMargin( 0 );

    /* Create and configure the QTreeView */
    view = new QVLCTreeView;
    view->setModel( model );
    view->setIconSize( QSize( 20, 20 ) );
    view->setAlternatingRowColors( true );
    view->setAnimated( true );
    view->setSelectionBehavior( QAbstractItemView::SelectRows );
    view->setSelectionMode( QAbstractItemView::ExtendedSelection );
    view->setDragEnabled( true );
    view->setAcceptDrops( true );
    view->setDropIndicatorShown( true );
    view->header()->setSortIndicator( -1 , Qt::AscendingOrder );
    view->setUniformRowHeights( true );
    view->setSortingEnabled( true );


    getSettings()->beginGroup("Playlist");
    if( getSettings()->contains( "headerState" ) )
    {
        view->header()->restoreState(
                getSettings()->value( "headerState" ).toByteArray() );
    }
    else
    {
        int m, c;
        for( m = 1, c = 0; m != COLUMN_END; m <<= 1, c++ )
        {
            view->setColumnHidden( c, !( m & COLUMN_DEFAULT ) );
            if( m == COLUMN_TITLE ) view->header()->resizeSection( c, 200 );
            else if( m == COLUMN_DURATION ) view->header()->resizeSection( c, 80 );
        }
    }
    view->header()->setSortIndicatorShown( true );
    view->header()->setClickable( true );
    view->header()->setContextMenuPolicy( Qt::CustomContextMenu );
    getSettings()->endGroup();

    /* Connections for the TreeView */
    CONNECT( view, activated( const QModelIndex& ) ,
             model,activateItem( const QModelIndex& ) );
    CONNECT( view, rightClicked( QModelIndex , QPoint ),
             this, doPopup( QModelIndex, QPoint ) );
    CONNECT( view->header(), customContextMenuRequested( const QPoint & ),
             this, popupSelectColumn( QPoint ) );
    CONNECT( model, currentChanged( const QModelIndex& ),
             this, handleExpansion( const QModelIndex& ) );

    currentRootId = -1;
    CONNECT( parent, rootChanged( playlist_item_t * ),
             this, setCurrentRootId( playlist_item_t * ) );

    /* Buttons configuration */
    QHBoxLayout *buttons = new QHBoxLayout;

    /* Add item to the playlist button */
    addButton = new QPushButton;
    addButton->setIcon( QIcon( ":/buttons/playlist/playlist_add" ) );
    addButton->setMaximumWidth( 30 );
    BUTTONACT( addButton, popupAdd() );
    buttons->addWidget( addButton );

    /* Random 2-state button */
    randomButton = new QPushButton( this );
    randomButton->setIcon( QIcon( ":/buttons/playlist/shuffle_on" ));
    randomButton->setToolTip( qtr( I_PL_RANDOM ));
    randomButton->setCheckable( true );
    randomButton->setChecked( model->hasRandom() );
    BUTTONACT( randomButton, toggleRandom() );
    buttons->addWidget( randomButton );

    /* Repeat 3-state button */
    repeatButton = new QPushButton( this );
    repeatButton->setToolTip( qtr( "Click to toggle between loop one, loop all" ) );
    repeatButton->setCheckable( true );

    if( model->hasRepeat() )
    {
        repeatButton->setIcon( QIcon( ":/buttons/playlist/repeat_one" ) );
        repeatButton->setChecked( true );
    }
    else if( model->hasLoop() )
    {
        repeatButton->setIcon( QIcon( ":/buttons/playlist/repeat_all" ) );
        repeatButton->setChecked( true );
    }
    else
    {
        repeatButton->setIcon( QIcon( ":/buttons/playlist/repeat_one" ) );
        repeatButton->setChecked( false );
    }
    BUTTONACT( repeatButton, toggleRepeat() );
    buttons->addWidget( repeatButton );

    /* Goto */
    gotoPlayingButton = new QPushButton;
    BUTTON_SET_ACT_I( gotoPlayingButton, "", buttons/playlist/jump_to,
            qtr( "Show the current item" ), gotoPlayingItem() );
    buttons->addWidget( gotoPlayingButton );

    /* A Spacer and the search possibilities */
    QSpacerItem *spacer = new QSpacerItem( 10, 20 );
    buttons->addItem( spacer );

    QLabel *filter = new QLabel( qtr(I_PL_SEARCH) + " " );
    buttons->addWidget( filter );

    SearchLineEdit *search = new SearchLineEdit( this );
    buttons->addWidget( search );
    filter->setBuddy( search );
    CONNECT( search, textChanged( const QString& ), this, search( const QString& ) );

    /* Title label */
    title = new QLabel;
    QFont titleFont;
    titleFont.setPointSize( titleFont.pointSize() + 6 );
    titleFont.setFamily( "Verdana" );
    title->setFont( titleFont );

    /* Finish the layout */
    layout->addWidget( title );
    layout->addWidget( view );
    layout->addLayout( buttons );
//    layout->addWidget( bar );
    setLayout( layout );

    selectColumnsSigMapper = new QSignalMapper( this );
    CONNECT( selectColumnsSigMapper, mapped( int ), this, toggleColumnShown( int ) );
}

/* Function to toggle between the Repeat states */
void StandardPLPanel::toggleRepeat()
{
    if( model->hasRepeat() )
    {
        model->setRepeat( false ); model->setLoop( true );
        repeatButton->setIcon( QIcon( ":/buttons/playlist/repeat_all" ) );
        repeatButton->setChecked( true );
    }
    else if( model->hasLoop() )
    {
        model->setRepeat( false ) ; model->setLoop( false );
        repeatButton->setChecked( false );
        repeatButton->setIcon( QIcon( ":/buttons/playlist/repeat_one" ) );
    }
    else
    {
        model->setRepeat( true ); model->setLoop( false );
        repeatButton->setChecked( true );
        repeatButton->setIcon( QIcon( ":/buttons/playlist/repeat_one" ) );
    }
}

/* Function to toggle between the Random states */
void StandardPLPanel::toggleRandom()
{
    bool prev = model->hasRandom();
    model->setRandom( !prev );
}

void StandardPLPanel::gotoPlayingItem()
{
    view->scrollTo( model->currentIndex() );
}

void StandardPLPanel::handleExpansion( const QModelIndex& index )
{
    view->scrollTo( index );
}

void StandardPLPanel::setCurrentRootId( playlist_item_t *p_item )
{
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

    /* <jleben> do we need to lock here? */
    playlist_Lock( THEPL );
    char *psz_title = input_item_GetName( p_item->p_input );
    title->setText( psz_title );
    free( psz_title );
    playlist_Unlock( THEPL );
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

void StandardPLPanel::popupSelectColumn( QPoint pos )
{
    QMenu menu;

    /* We do not offer the option to hide index 0 column, or
    * QTreeView will behave weird */
    int i, j;
    for( i = 1 << 1, j = 1; i < COLUMN_END; i <<= 1, j++ )
    {
        QAction* option = menu.addAction(
            qfu( psz_column_title( i ) ) );
        option->setCheckable( true );
        option->setChecked( !view->isColumnHidden( j ) );
        selectColumnsSigMapper->setMapping( option, j );
        CONNECT( option, triggered(), selectColumnsSigMapper, map() );
    }
    menu.exec( QCursor::pos() );
}

void StandardPLPanel::toggleColumnShown( int i )
{
    view->setColumnHidden( i, !view->isColumnHidden( i ) );
}

/* Search in the playlist */
void StandardPLPanel::search( const QString& searchText )
{
    model->search( searchText );
}

void StandardPLPanel::doPopup( QModelIndex index, QPoint point )
{
    QItemSelectionModel *selection = view->selectionModel();
    QModelIndexList list = selection->selectedIndexes();
    model->popup( index, point, list );
}

/* Set the root of the new Playlist */
/* This activated by the selector selection */
void StandardPLPanel::setRoot( playlist_item_t *p_item )
{
    QPL_LOCK;
    p_item = playlist_GetPreferredNode( THEPL, p_item );
    assert( p_item );
    QPL_UNLOCK;

    model->rebuild( p_item );
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
    QItemSelectionModel *selection = view->selectionModel();
    QModelIndexList list = selection->selectedIndexes();
    model->doDelete( list );
}

StandardPLPanel::~StandardPLPanel()
{
    getSettings()->beginGroup("Playlist");
    getSettings()->setValue( "headerState", view->header()->saveState() );
    getSettings()->endGroup();
}

