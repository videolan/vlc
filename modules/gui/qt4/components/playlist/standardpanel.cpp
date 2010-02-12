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
#include "menus.hpp"

#include <vlc_intf_strings.h>

#include <QPushButton>
#include <QHeaderView>
#include <QKeyEvent>
#include <QModelIndexList>
#include <QLabel>
#include <QMenu>
#include <QSignalMapper>
#include <QWheelEvent>
#include <QToolButton>
#include <QFontMetrics>
#include <QPainter>

#include <assert.h>

#include "sorting.h"

static const QString viewNames[] = { qtr( "Detailed View" ),
                                     qtr( "Icon View" ),
                                     qtr( "List View" ) };

StandardPLPanel::StandardPLPanel( PlaylistWidget *_parent,
                                  intf_thread_t *_p_intf,
                                  playlist_t *p_playlist,
                                  playlist_item_t *p_root ):
                                  QWidget( _parent ), p_intf( _p_intf )
{
    layout = new QGridLayout( this );
    layout->setSpacing( 0 ); layout->setMargin( 0 );
    setMinimumWidth( 300 );

    iconView = NULL;
    treeView = NULL;
    listView = NULL;

    model = new PLModel( p_playlist, p_intf, p_root, this );
    currentRootId = -1;
    currentRootIndexId = -1;
    lastActivatedId = -1;

    locationBar = new LocationBar( model );
    locationBar->setSizePolicy( QSizePolicy::Maximum, QSizePolicy::Preferred );
    layout->addWidget( locationBar, 0, 1 );
    layout->setColumnStretch( 1, 100 );
    CONNECT( locationBar, invoked( const QModelIndex & ),
             this, browseInto( const QModelIndex & ) );

    layout->setColumnStretch( 2, 1 );

    searchEdit = new SearchLineEdit( this );
    searchEdit->setMaximumWidth( 250 );
    searchEdit->setMinimumWidth( 80 );
    layout->addWidget( searchEdit, 0, 3 );
    CONNECT( searchEdit, textChanged( const QString& ),
             this, search( const QString& ) );
    layout->setColumnStretch( 3, 50 );

    /* Add item to the playlist button */
    addButton = new QToolButton;
    addButton->setIcon( QIcon( ":/buttons/playlist/playlist_add" ) );
    addButton->setMaximumWidth( 30 );
    BUTTONACT( addButton, popupAdd() );
    layout->addWidget( addButton, 0, 0 );

    /* Button to switch views */
    QToolButton *viewButton = new QToolButton( this );
    viewButton->setIcon( style()->standardIcon( QStyle::SP_FileDialogDetailedView ) );
    layout->addWidget( viewButton, 0, 4 );

    /* View selection menu */
    viewSelectionMapper = new QSignalMapper( this );
    CONNECT( viewSelectionMapper, mapped( int ), this, showView( int ) );

    QActionGroup *actionGroup = new QActionGroup( this );

    for( int i = 0; i < VIEW_COUNT; i++ )
    {
        viewActions[i] = actionGroup->addAction( viewNames[i] );
        viewActions[i]->setCheckable( true );
        viewSelectionMapper->setMapping( viewActions[i], i );
        CONNECT( viewActions[i], triggered(), viewSelectionMapper, map() );
    }

    BUTTONACT( viewButton, cycleViews() );
    QMenu *viewMenu = new QMenu( this );
    viewMenu->addActions( actionGroup->actions() );

    viewButton->setMenu( viewMenu );

    /* Saved Settings */
    getSettings()->beginGroup("Playlist");

    int i_viewMode = getSettings()->value( "view-mode", TREE_VIEW ).toInt();
    showView( i_viewMode );

    getSettings()->endGroup();

    CONNECT( THEMIM, leafBecameParent( input_item_t *),
             this, browseInto( input_item_t * ) );

    CONNECT( model, currentChanged( const QModelIndex& ),
             this, handleExpansion( const QModelIndex& ) );
    CONNECT( model, rootChanged(), this, handleRootChange() );
}

StandardPLPanel::~StandardPLPanel()
{
    getSettings()->beginGroup("Playlist");
    if( treeView )
        getSettings()->setValue( "headerStateV2", treeView->header()->saveState() );
    if( currentView == treeView )
        getSettings()->setValue( "view-mode", TREE_VIEW );
    else if( currentView == listView )
        getSettings()->setValue( "view-mode", LIST_VIEW );
    else if( currentView == iconView )
        getSettings()->setValue( "view-mode", ICON_VIEW );
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

void StandardPLPanel::handleRootChange()
{
    /* needed for popupAdd() */
    PLItem *root = model->getItem( QModelIndex() );
    currentRootId = root->id();

    browseInto();

    /* enable/disable adding */
    if( currentRootId == THEPL->p_playing->i_id )
    {
        addButton->setEnabled( true );
        addButton->setToolTip( qtr(I_PL_ADDPL) );
    }
    else if( THEPL->p_media_library &&
             currentRootId == THEPL->p_media_library->i_id )
    {
        addButton->setEnabled( true );
        addButton->setToolTip( qtr(I_PL_ADDML) );
    }
    else
        addButton->setEnabled( false );
}

/* PopupAdd Menu for the Add Menu */
void StandardPLPanel::popupAdd()
{
    QMenu popup;
    if( currentRootId == THEPL->p_playing->i_id )
    {
        popup.addAction( qtr(I_PL_ADDF), THEDP, SLOT( simplePLAppendDialog()) );
        popup.addAction( qtr(I_PL_ADDDIR), THEDP, SLOT( PLAppendDir()) );
        popup.addAction( qtr(I_OP_ADVOP), THEDP, SLOT( PLAppendDialog()) );
    }
    else if( THEPL->p_media_library &&
                currentRootId == THEPL->p_media_library->i_id )
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
    if( !index.isValid() ){
        QVLCMenu::PopupMenu( p_intf, true );
    }
    else
    {
        QItemSelectionModel *selection = currentView->selectionModel();
        QModelIndexList list = selection->selectedIndexes();
        model->popup( index, globalPoint, list );
    }
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
    bool flat = currentView == iconView || currentView == listView;
    model->search( searchText,
                   flat ? currentView->rootIndex() : QModelIndex(),
                   !flat );
}

/* Set the root of the new Playlist */
/* This activated by the selector selection */
void StandardPLPanel::setRoot( playlist_item_t *p_item )
{
    model->rebuild( p_item );
}

void StandardPLPanel::browseInto( const QModelIndex &index )
{
    if( currentView == iconView || currentView == listView )
    {
        currentRootIndexId = model->itemId( index );;
        currentView->setRootIndex( index );
    }

    locationBar->setIndex( index );
    model->search( QString(), index, false );
    searchEdit->clear();
}

void StandardPLPanel::browseInto( )
{
    browseInto( currentRootIndexId != -1 && currentView != treeView ?
                model->index( currentRootIndexId, 0 ) :
                QModelIndex() );
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
    CONNECT( iconView, activated( const QModelIndex & ),
             this, activate( const QModelIndex & ) );

    layout->addWidget( iconView, 1, 0, 1, -1 );
}

void StandardPLPanel::createListView()
{
    listView = new PlListView( model, this );
    listView->setContextMenuPolicy( Qt::CustomContextMenu );
    CONNECT( listView, customContextMenuRequested( const QPoint & ),
             this, popupPlView( const QPoint & ) );
    CONNECT( listView, activated( const QModelIndex & ),
             this, activate( const QModelIndex & ) );

    layout->addWidget( listView, 1, 0, 1, -1 );
}


void StandardPLPanel::createTreeView()
{
    /* Create and configure the QTreeView */
    treeView = new QTreeView;

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

    /* setModel after setSortingEnabled(true), or the model will sort immediately! */
    treeView->setModel( model );

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
             this, activate( const QModelIndex& ) );
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

void StandardPLPanel::showView( int i_view )
{
    switch( i_view )
    {
    case TREE_VIEW:
    {
        if( treeView == NULL )
            createTreeView();
        if( iconView ) iconView->hide();
        if( listView ) listView->hide();
        treeView->show();
        currentView = treeView;
        viewActions[i_view]->setChecked( true );
        break;
    }
    case ICON_VIEW:
    {
        if( iconView == NULL )
            createIconView();

        if( treeView ) treeView->hide();
        if( listView ) listView->hide();
        iconView->show();
        currentView = iconView;
        viewActions[i_view]->setChecked( true );
        break;
    }
    case LIST_VIEW:
    {
        if( listView == NULL )
            createListView();

        if( treeView ) treeView->hide();
        if( iconView ) iconView->hide();
        listView->show();
        currentView = listView;
        viewActions[i_view]->setChecked( true );
        break;
    }
    default: return;
    }

    browseInto();
}

void StandardPLPanel::cycleViews()
{
    if( currentView == iconView )
        showView( TREE_VIEW );
    else if( currentView == treeView )
        showView( LIST_VIEW );
    else if( currentView == listView )
        showView( ICON_VIEW );
    else
        assert( 0 );
}

void StandardPLPanel::wheelEvent( QWheelEvent *e )
{
    // Accept this event in order to prevent unwanted volume up/down changes
    e->accept();
}

void StandardPLPanel::activate( const QModelIndex &index )
{
    if( model->hasChildren( index ) )
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

void StandardPLPanel::browseInto( input_item_t *p_input )
{

    if( p_input->i_id != lastActivatedId ) return;

    playlist_Lock( THEPL );

    playlist_item_t *p_item = playlist_ItemGetByInput( THEPL, p_input );
    if( !p_item )
    {
        playlist_Unlock( THEPL );
        return;
    }

    QModelIndex index = model->index( p_item->i_id, 0 );

    playlist_Unlock( THEPL );

    if( currentView == treeView )
        treeView->setExpanded( index, true );
    else
        browseInto( index );

    lastActivatedId = -1;


}

LocationBar::LocationBar( PLModel *m )
{
    model = m;
    mapper = new QSignalMapper( this );
    CONNECT( mapper, mapped( int ), this, invoke( int ) );

    box = new QHBoxLayout;
    box->setSpacing( 0 );
    box->setContentsMargins( 0, 0, 0, 0 );
    setLayout( box );
}

void LocationBar::setIndex( const QModelIndex &index )
{
    qDeleteAll( buttons );
    buttons.clear();
    QModelIndex i = index;
    bool bold = true;
    while( true )
    {
        PLItem *item = model->getItem( i );

        char *fb_name = input_item_GetTitleFbName( item->inputItem() );
        QString text = qfu(fb_name);
        free(fb_name);
        QAbstractButton *btn = new LocationButton( text, bold, i.isValid() );
        btn->setSizePolicy( QSizePolicy::Maximum, QSizePolicy::Fixed );
        box->insertWidget( 0, btn, bold ? 1 : 0 );
        buttons.append( btn );

        mapper->setMapping( btn, item->id() );
        CONNECT( btn, clicked( ), mapper, map( ) );

        bold = false;

        if( i.isValid() ) i = i.parent();
        else break;
    }
}

void LocationBar::setRootIndex()
{
    setIndex( QModelIndex() );
}

void LocationBar::invoke( int i_id )
{
    QModelIndex index = model->index( i_id, 0 );
    emit invoked ( index );
}

LocationButton::LocationButton( const QString &text, bool bold, bool arrow )
  : b_arrow( arrow )
{
    QFont font;
    font.setBold( bold );
    setFont( font );
    setText( text );
}

#define PADDING 4

void LocationButton::paintEvent ( QPaintEvent * event )
{
    QStyleOptionButton option;
    option.initFrom( this );
    option.state |= QStyle::State_Enabled;
    QPainter p( this );

    if( underMouse() )
        style()->drawControl( QStyle::CE_PushButtonBevel, &option, &p );

    int margin = style()->pixelMetric(QStyle::PM_DefaultFrameWidth,0,this) + PADDING;

    QRect rect = option.rect.adjusted( b_arrow ? 15 + margin : margin, 0, margin * -1, 0 );
    p.drawText( rect, Qt::AlignVCenter,
                fontMetrics().elidedText( text(), Qt::ElideRight, rect.width() ) );

    if( b_arrow )
    {
        option.rect.setX( margin );
        option.rect.setWidth( 8 );
        style()->drawPrimitive( QStyle::PE_IndicatorArrowRight, &option, &p );
    }
}

QSize LocationButton::sizeHint() const
{
    int frameWidth = style()->pixelMetric(QStyle::PM_DefaultFrameWidth,0,this);
    QSize s( fontMetrics().boundingRect( text() ).size() );
    s.setWidth( s.width() + ( 2 * frameWidth ) + ( 2 * PADDING ) + ( b_arrow ? 15 : 0 ) );
    s.setHeight( QPushButton::sizeHint().height() );
    return s;
}

#undef PADDING
