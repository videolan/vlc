/*****************************************************************************
 * standardpanel.cpp : The "standard" playlist panel : just a treeview
 ****************************************************************************
 * Copyright © 2000-2010 VideoLAN
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Jean-Baptiste Kempf <jb@videolan.org>
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

#include "components/playlist/standardpanel.hpp"

#include "components/playlist/vlc_model.hpp"      /* VLCModel */
#include "components/playlist/playlist_model.hpp" /* PLModel */
#include "components/playlist/ml_model.hpp"       /* MLModel */
#include "components/playlist/views.hpp"          /* 3 views */
#include "components/playlist/selector.hpp"       /* PLSelector */
#include "menus.hpp"                              /* Popup */
#include "input_manager.hpp"                      /* THEMIM */

#include "sorting.h"                              /* Columns order */

#include <vlc_services_discovery.h>               /* SD_CMD_SEARCH */

#include <QHeaderView>
#include <QModelIndexList>
#include <QMenu>
#include <QKeyEvent>
#include <QWheelEvent>
#include <QStackedLayout>
#include <QSignalMapper>
#include <QSettings>
#include <QScrollBar>

#include <assert.h>

StandardPLPanel::StandardPLPanel( PlaylistWidget *_parent,
                                  intf_thread_t *_p_intf,
                                  playlist_item_t *p_root,
                                  PLSelector *_p_selector,
                                  PLModel *_p_model,
                                  MLModel *_p_plmodel)
                : QWidget( _parent ),
                  model( _p_model ),
                  mlmodel( _p_plmodel),
                  p_intf( _p_intf ),
                  p_selector( _p_selector )
{
    viewStack = new QStackedLayout( this );
    viewStack->setSpacing( 0 ); viewStack->setMargin( 0 );
    setMinimumWidth( 300 );

    iconView    = NULL;
    treeView    = NULL;
    listView    = NULL;
    picFlowView = NULL;

    currentRootIndexId  = -1;
    lastActivatedId     = -1;

    /* Saved Settings */
    int i_savedViewMode = getSettings()->value( "Playlist/view-mode", TREE_VIEW ).toInt();
    showView( i_savedViewMode );

    DCONNECT( THEMIM, leafBecameParent( int ),
              this, browseInto( int ) );

    CONNECT( model, currentChanged( const QModelIndex& ),
             this, handleExpansion( const QModelIndex& ) );
    CONNECT( model, rootChanged(), this, browseInto() );

    setRoot( p_root, false );
}

StandardPLPanel::~StandardPLPanel()
{
    getSettings()->beginGroup("Playlist");
    if( treeView )
        getSettings()->setValue( "headerStateV2", treeView->header()->saveState() );
    getSettings()->setValue( "view-mode", currentViewIndex() );
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
    if( currentRootIndexId != -1 && currentRootIndexId != model->itemId( index.parent() ) )
        browseInto( index.parent() );
    currentView->scrollTo( index );
}

void StandardPLPanel::popupPlView( const QPoint &point )
{
    QModelIndex index = currentView->indexAt( point );
    QPoint globalPoint = currentView->viewport()->mapToGlobal( point );
    QItemSelectionModel *selection = currentView->selectionModel();
    QModelIndexList list = selection->selectedIndexes();

    if( !model->popup( index, globalPoint, list ) )
        QVLCMenu::PopupMenu( p_intf, true );
}

void StandardPLPanel::popupSelectColumn( QPoint )
{
    QMenu menu;
    assert( treeView );

    /* We do not offer the option to hide index 0 column, or
     * QTreeView will behave weird */
    for( int i = 1 << 1, j = 1; i < COLUMN_END; i <<= 1, j++ )
    {
        QAction* option = menu.addAction( qfu( psz_column_title( i ) ) );
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
    int type;
    QString name;
    p_selector->getCurrentSelectedItem( &type, &name );
    if( type != SD_TYPE )
    {
        bool flat = ( currentView == iconView ||
                      currentView == listView ||
                      currentView == picFlowView );
        model->search( searchText,
                       flat ? currentView->rootIndex() : QModelIndex(),
                       !flat );
    }
}

void StandardPLPanel::searchDelayed( const QString& searchText )
{
    int type;
    QString name;
    p_selector->getCurrentSelectedItem( &type, &name );

    if( type == SD_TYPE )
    {
        if( !name.isEmpty() && !searchText.isEmpty() )
            playlist_ServicesDiscoveryControl( THEPL, qtu( name ), SD_CMD_SEARCH,
                                              qtu( searchText ) );
    }
}

/* Set the root of the new Playlist */
/* This activated by the selector selection */
void StandardPLPanel::setRoot( playlist_item_t *p_item, bool b )
{
#ifdef MEDIA_LIBRARY
    if( b )
    {
        msg_Dbg( p_intf, "Setting the SQL ML" );
        currentView->setModel( mlmodel );
    }
    else
#else
    Q_UNUSED( b );
#endif
    {
        msg_Dbg( p_intf, "Normal PL/ML or SD" );
        if( currentView->model() != model )
            currentView->setModel( model );
        model->rebuild( p_item );
    }
}

void StandardPLPanel::browseInto( const QModelIndex &index )
{
    if( currentView == iconView || currentView == listView || currentView == picFlowView )
    {
        currentRootIndexId = model->itemId( index );
        currentView->setRootIndex( index );
    }

    emit viewChanged( index );
}

void StandardPLPanel::browseInto()
{
    browseInto( (currentRootIndexId != -1 && currentView != treeView) ?
                 model->index( currentRootIndexId, 0 ) :
                 QModelIndex() );
}

void StandardPLPanel::wheelEvent( QWheelEvent *e )
{
    // Accept this event in order to prevent unwanted volume up/down changes
    e->accept();
}

bool StandardPLPanel::eventFilter ( QObject *, QEvent * event )
{
    if (event->type() == QEvent::KeyPress)
    {
        QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
        if( keyEvent->key() == Qt::Key_Delete ||
            keyEvent->key() == Qt::Key_Backspace )
        {
            deleteSelection();
            return true;
        }
    }
    return false;
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
    CONNECT( iconView, activated( const QModelIndex & ),
             this, activate( const QModelIndex & ) );
    iconView->installEventFilter( this );
    viewStack->addWidget( iconView );
}

void StandardPLPanel::createListView()
{
    listView = new PlListView( model, this );
    listView->setContextMenuPolicy( Qt::CustomContextMenu );
    CONNECT( listView, customContextMenuRequested( const QPoint & ),
             this, popupPlView( const QPoint & ) );
    CONNECT( listView, activated( const QModelIndex & ),
             this, activate( const QModelIndex & ) );
    listView->installEventFilter( this );
    viewStack->addWidget( listView );
}

void StandardPLPanel::createCoverView()
{
    picFlowView = new PicFlowView( model, this );
    picFlowView->setContextMenuPolicy( Qt::CustomContextMenu );
    CONNECT( picFlowView, customContextMenuRequested( const QPoint & ),
             this, popupPlView( const QPoint & ) );
    CONNECT( picFlowView, activated( const QModelIndex & ),
             this, activate( const QModelIndex & ) );
    viewStack->addWidget( picFlowView );
    picFlowView->installEventFilter( this );
}

void StandardPLPanel::createTreeView()
{
    /* Create and configure the QTreeView */
    treeView = new PlTreeView;

    treeView->setIconSize( QSize( 20, 20 ) );
    treeView->setAlternatingRowColors( true );
    treeView->setAnimated( true );
    treeView->setUniformRowHeights( true );
    treeView->setSortingEnabled( true );
    treeView->setAttribute( Qt::WA_MacShowFocusRect, false );
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

    /* setModel after setSortingEnabled(true), or the model will sort immediately! */

    getSettings()->beginGroup("Playlist");

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

    getSettings()->endGroup();

    /* Connections for the TreeView */
    CONNECT( treeView, activated( const QModelIndex& ),
             this, activate( const QModelIndex& ) );
    CONNECT( treeView->header(), customContextMenuRequested( const QPoint & ),
             this, popupSelectColumn( QPoint ) );
    CONNECT( treeView, customContextMenuRequested( const QPoint & ),
             this, popupPlView( const QPoint & ) );
    treeView->installEventFilter( this );

    /* SignalMapper for columns */
    selectColumnsSigMapper = new QSignalMapper( this );
    CONNECT( selectColumnsSigMapper, mapped( int ),
             this, toggleColumnShown( int ) );

    viewStack->addWidget( treeView );
}

void StandardPLPanel::changeModel( bool b_ml )
{
#ifdef MEDIA_LIBRARY
    VLCModel *mod;
    if( b_ml )
        mod = mlmodel;
    else
        mod = model;
    if( currentView->model() != mod )
        currentView->setModel( mod );
#else
    Q_UNUSED( b_ml );
    if( currentView->model() != model )
        currentView->setModel( model );
#endif
}

void StandardPLPanel::showView( int i_view )
{

    switch( i_view )
    {
    case ICON_VIEW:
    {
        if( iconView == NULL )
            createIconView();
        currentView = iconView;
        break;
    }
    case LIST_VIEW:
    {
        if( listView == NULL )
            createListView();
        currentView = listView;
        break;
    }
    case PICTUREFLOW_VIEW:
    {
        if( picFlowView == NULL )
            createCoverView();
        currentView = picFlowView;
        break;
    }
    default:
    case TREE_VIEW:
    {
        if( treeView == NULL )
            createTreeView();
        currentView = treeView;
        break;
    }
    }

    changeModel( false );

    viewStack->setCurrentWidget( currentView );
    browseInto();
    gotoPlayingItem();
}

int StandardPLPanel::currentViewIndex() const
{
    if( currentView == treeView )
        return TREE_VIEW;
    else if( currentView == iconView )
        return ICON_VIEW;
    else if( currentView == listView )
        return LIST_VIEW;
    else
        return PICTUREFLOW_VIEW;
}

int StandardPLPanel::getScrollBarsSize() const
{
    /* FIXME: should return a set in case of different widths */
    return currentView->verticalScrollBar()->sizeHint().width();
}

void StandardPLPanel::cycleViews()
{
    if( currentView == iconView )
        showView( TREE_VIEW );
    else if( currentView == treeView )
        showView( LIST_VIEW );
    else if( currentView == listView )
        showView( PICTUREFLOW_VIEW  );
    else if( currentView == picFlowView )
        showView( ICON_VIEW );
    else
        assert( 0 );
}

void StandardPLPanel::activate( const QModelIndex &index )
{
    if( currentView->model() == model )
    {
        /* If we are not a leaf node */
        if( !index.data( PLModel::IsLeafNodeRole ).toBool() )
        {
            if( currentView != treeView )
                browseInto( index );
        }
        else
        {
            playlist_Lock( THEPL );
            playlist_item_t *p_item = playlist_ItemGetById( THEPL, model->itemId( index ) );
            p_item->i_flags |= PLAYLIST_SUBITEM_STOP_FLAG;
            lastActivatedId = p_item->p_input->i_id;
            playlist_Unlock( THEPL );
            model->activateItem( index );
        }
    }
}

void StandardPLPanel::browseInto( int i_id )
{
    if( i_id != lastActivatedId ) return;

    QModelIndex index = model->index( i_id, 0 );
    playlist_Unlock( THEPL );

    if( currentView == treeView )
        treeView->setExpanded( index, true );
    else
        browseInto( index );

    lastActivatedId = -1;
}
