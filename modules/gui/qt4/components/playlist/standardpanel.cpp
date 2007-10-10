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

#include "qt4.hpp"
#include "dialogs_provider.hpp"
#include "playlist_model.hpp"
#include "components/playlist/panels.hpp"
#include "util/customwidgets.hpp"

#include <vlc_intf_strings.h>

#include <QTreeView>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QModelIndexList>
#include <QToolBar>
#include <QLabel>
#include <QSpacerItem>
#include <QMenu>

#include <assert.h>

StandardPLPanel::StandardPLPanel( PlaylistWidget *_parent,
                                  intf_thread_t *_p_intf,
                                  playlist_t *p_playlist,
                                  playlist_item_t *p_root ):
                                  PLPanel( _parent, _p_intf )
{
    model = new PLModel( p_playlist, p_intf, p_root, -1, this );
 
    /* Create and configure the QTreeView */
    view = new QVLCTreeView( 0 );
    view->setModel(model);
    view->setIconSize( QSize(20,20) );
    view->setAlternatingRowColors( true );
    view->setAnimated( true );
    view->setSortingEnabled( true );    
    view->setSelectionMode( QAbstractItemView::ExtendedSelection );
    view->setDragEnabled( true );
    view->setAcceptDrops( true );
    view->setDropIndicatorShown( true );
    view->setAutoScroll( true );
 
    view->header()->resizeSection( 0, 230 );
    view->header()->resizeSection( 1, 170 );
    view->header()->setSortIndicatorShown( true );
    view->header()->setClickable( true );
    view->header()->setContextMenuPolicy( Qt::CustomContextMenu );
 
    CONNECT( view, activated( const QModelIndex& ) ,
             model,activateItem( const QModelIndex& ) );
    CONNECT( view, rightClicked( QModelIndex , QPoint ),
             this, doPopup( QModelIndex, QPoint ) );
    CONNECT( model, dataChanged( const QModelIndex&, const QModelIndex& ),
             this, handleExpansion( const QModelIndex& ) );
    CONNECT( view->header(), customContextMenuRequested( const QPoint & ),
             this, popupSelectColumn( QPoint ) );

    currentRootId = -1;
    CONNECT( parent, rootChanged(int), this, setCurrentRootId( int ) );

    QVBoxLayout *layout = new QVBoxLayout();
    layout->setSpacing( 0 ); layout->setMargin( 0 );

    /* Buttons configuration */
    QHBoxLayout *buttons = new QHBoxLayout();
 
    addButton = new QPushButton( QIcon( ":/pixmaps/playlist_add.png" ), "", this );
    addButton->setMaximumWidth( 25 );
    BUTTONACT( addButton, popupAdd() );
    buttons->addWidget( addButton );

    randomButton = new QPushButton( this );
    randomButton->setIcon( model->hasRandom() ?
                            QIcon( ":/pixmaps/playlist_shuffle_on.png" ) :
                            QIcon( ":/pixmaps/playlist_shuffle_off.png" ) );
    BUTTONACT( randomButton, toggleRandom() );
    buttons->addWidget( randomButton );

    QSpacerItem *spacer = new QSpacerItem( 10, 20 );
    buttons->addItem( spacer );

    repeatButton = new QPushButton( this );
    if( model->hasRepeat() ) repeatButton->setIcon(
                        QIcon( ":/pixmaps/playlist_repeat_one.png" ) );
    else if( model->hasLoop() ) repeatButton->setIcon(
                        QIcon( ":/pixmaps/playlist_repeat_all.png" ) );
    else repeatButton->setIcon(
                        QIcon( ":/pixmaps/playlist_repeat_off.png" ) );
    BUTTONACT( repeatButton, toggleRepeat() );
    buttons->addWidget( repeatButton );


    QLabel *filter = new QLabel( qtr(I_PL_SEARCH) + " " );
    buttons->addWidget( filter );
    searchLine = new  ClickLineEdit( qtr(I_PL_FILTER), 0 );
    CONNECT( searchLine, textChanged(QString), this, search(QString));
    buttons->addWidget( searchLine ); filter->setBuddy( searchLine );

    QPushButton *clear = new QPushButton( qfu( "CL") );
    clear->setMaximumWidth( 30 );
    BUTTONACT( clear, clearFilter() );
    buttons->addWidget( clear );

    layout->addWidget( view );
    layout->addLayout( buttons );
//    layout->addWidget( bar );
    setLayout( layout );
}

void StandardPLPanel::toggleRepeat()
{
    if( model->hasRepeat() )
    {
        model->setRepeat( false ); model->setLoop( true );
        repeatButton->setIcon( QIcon( ":/pixmaps/playlist_repeat_all.png" ) );
    }
    else if( model->hasLoop() )
    {
        model->setRepeat( false ) ; model->setLoop( false );
        repeatButton->setIcon( QIcon( ":/pixmaps/playlist_repeat_off.png" ) );
    }
    else
    {
        model->setRepeat( true );
        repeatButton->setIcon( QIcon( ":/pixmaps/playlist_repeat_one.png" ) );
    }
}

void StandardPLPanel::toggleRandom()
{
    bool prev = model->hasRandom();
    model->setRandom( !prev );
    randomButton->setIcon( prev ?
                QIcon( ":/pixmaps/playlist_shuffle_off.png" ) :
                QIcon( ":/pixmaps/playlist_shuffle_on.png" ) );
}

void StandardPLPanel::handleExpansion( const QModelIndex &index )
{
    QModelIndex parent;
    if( model->isCurrent( index ) )
    {
        view->scrollTo( index, QAbstractItemView::EnsureVisible );
        parent = index;
        while( parent.isValid() )
        {
            view->setExpanded( parent, true );
            parent = model->parent( parent );
        }
    }
}

void StandardPLPanel::setCurrentRootId( int _new )
{
    currentRootId = _new;
    if( currentRootId == THEPL->p_local_category->i_id ||
        currentRootId == THEPL->p_local_onelevel->i_id  )
    {
        addButton->setEnabled( true );
        addButton->setToolTip( qtr(I_PL_ADDPL) );
    }
    else if( currentRootId == THEPL->p_ml_category->i_id ||
             currentRootId == THEPL->p_ml_onelevel->i_id )
    {
        addButton->setEnabled( true );
        addButton->setToolTip( qtr(I_PL_ADDML) );
    }
    else
        addButton->setEnabled( false );
}

void StandardPLPanel::popupAdd()
{
    QMenu popup;
    if( currentRootId == THEPL->p_local_category->i_id ||
        currentRootId == THEPL->p_local_onelevel->i_id )
    {
        popup.addAction( qtr(I_PL_ADDF), THEDP, SLOT(simplePLAppendDialog()));
        popup.addAction( qtr(I_PL_ADVADD), THEDP, SLOT(PLAppendDialog()) );
        popup.addAction( qtr(I_PL_ADDDIR), THEDP, SLOT( PLAppendDir()) );
    }
    else if( currentRootId == THEPL->p_ml_category->i_id ||
             currentRootId == THEPL->p_ml_onelevel->i_id )
    {
        popup.addAction( qtr(I_PL_ADDF), THEDP, SLOT(simpleMLAppendDialog()));
        popup.addAction( qtr(I_PL_ADVADD), THEDP, SLOT( MLAppendDialog() ) );
        popup.addAction( qtr(I_PL_ADDDIR), THEDP, SLOT( MLAppendDir() ) );
    }
    popup.exec( QCursor::pos() );
}

void StandardPLPanel::popupSelectColumn( QPoint )
{
    ContextUpdateMapper = new QSignalMapper(this);

    QMenu selectColMenu;

#define ADD_META_ACTION( meta ) { \
   QAction* option = selectColMenu.addAction( qfu(VLC_META_##meta) );     \
   option->setCheckable( true );                                           \
   option->setChecked( model->shownFlags() & VLC_META_ENGINE_##meta );   \
   ContextUpdateMapper->setMapping( option, VLC_META_ENGINE_##meta );      \
   CONNECT( option, triggered(), ContextUpdateMapper, map() );             \
   }
    CONNECT( ContextUpdateMapper, mapped( int ),  model, viewchanged( int ) );

    ADD_META_ACTION( TITLE );
    ADD_META_ACTION( ARTIST );
    ADD_META_ACTION( DURATION );
    ADD_META_ACTION( COLLECTION );
    ADD_META_ACTION( GENRE );
    ADD_META_ACTION( SEQ_NUM );
    ADD_META_ACTION( RATING );
    ADD_META_ACTION( DESCRIPTION );

#undef ADD_META_ACTION
 
    selectColMenu.exec( QCursor::pos() );
 }

void StandardPLPanel::clearFilter()
{
    searchLine->setText( "" );
}

void StandardPLPanel::search( QString searchText )
{
    model->search( searchText );
}

void StandardPLPanel::doPopup( QModelIndex index, QPoint point )
{
    if( !index.isValid() ) return;
    QItemSelectionModel *selection = view->selectionModel();
    QModelIndexList list = selection->selectedIndexes();
    model->popup( index, point, list );
}

void StandardPLPanel::setRoot( int i_root_id )
{
    playlist_item_t *p_item = playlist_ItemGetById( THEPL, i_root_id,
                                                    VLC_TRUE );
    assert( p_item );
    p_item = playlist_GetPreferredNode( THEPL, p_item );
    assert( p_item );
    model->rebuild( p_item );
}

void StandardPLPanel::removeItem( int i_id )
{
    model->removeItem( i_id );
}

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
{}
