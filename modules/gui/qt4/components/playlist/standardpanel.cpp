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
#include "util/customwidgets.hpp"                 /* PixmapAnimator */
#include "menus.hpp"                              /* Popup */
#include "input_manager.hpp"                      /* THEMIM */
#include "dialogs_provider.hpp"                   /* THEDP */
#include "dialogs/playlist.hpp"                   /* Playlist Dialog */
#include "dialogs/mediainfo.hpp"                  /* MediaInfoDialog */

#include <vlc_services_discovery.h>               /* SD_CMD_SEARCH */
#include <vlc_intf_strings.h>                     /* POP_ */

#define I_NEW_DIR \
    I_DIR_OR_FOLDER( N_("Create Directory"), N_( "Create Folder" ) )
#define I_NEW_DIR_NAME \
    I_DIR_OR_FOLDER( N_( "Enter name for new directory:" ), \
                     N_( "Enter name for new folder:" ) )

#include <QHeaderView>
#include <QMenu>
#include <QKeyEvent>
#include <QWheelEvent>
#include <QStackedLayout>
#include <QSignalMapper>
#include <QSettings>
#include <QStylePainter>
#include <QInputDialog>
#include <QDesktopServices>
#include <QUrl>
#include <QFont>

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

    QList<QString> frames;
    frames << ":/util/wait1";
    frames << ":/util/wait2";
    frames << ":/util/wait3";
    frames << ":/util/wait4";
    spinnerAnimation = new PixmapAnimator( this, frames );
    CONNECT( spinnerAnimation, pixmapReady( const QPixmap & ), this, updateViewport() );

    /* Saved Settings */
    int i_savedViewMode = getSettings()->value( "Playlist/view-mode", TREE_VIEW ).toInt();
    i_zoom = getSettings()->value( "Playlist/zoom", 0 ).toInt();

    showView( i_savedViewMode );

    DCONNECT( THEMIM, leafBecameParent( int ),
              this, browseInto( int ) );

    CONNECT( model, currentIndexChanged( const QModelIndex& ),
             this, handleExpansion( const QModelIndex& ) );
    CONNECT( model, rootIndexChanged(), this, browseInto() );

    setRootItem( p_root, false );
}

StandardPLPanel::~StandardPLPanel()
{
    getSettings()->beginGroup("Playlist");
    if( treeView )
        getSettings()->setValue( "headerStateV2", treeView->header()->saveState() );
    getSettings()->setValue( "view-mode", currentViewIndex() );
    getSettings()->setValue( "zoom", i_zoom );
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
    QModelIndexList list = selection->selectedRows();

    if( !popup( index, globalPoint, list ) )
        VLCMenuBar::PopupMenu( p_intf, true );
}

/*********** Popup *********/
bool StandardPLPanel::popup( const QModelIndex & index, const QPoint &point, const QModelIndexList &selectionlist )
{
    VLCModel *model = qobject_cast<VLCModel*>(currentView->model());
    QModelIndexList callerAsList;
    callerAsList << ( index.isValid() ? index : QModelIndex() );
    popupIndex = index; /* suitable for modal only */

#define ADD_MENU_ENTRY( icon, title, act, data ) \
    action = menu.addAction( icon, title ); \
    container.action = act; \
    container.indexes = data; \
    action->setData( QVariant::fromValue( container ) )

    /* */
    QMenu menu;
    QAction *action;
    PLModel::actionsContainerType container;

    /* Play/Stream/Info static actions */
    if( index.isValid() )
    {
        ADD_MENU_ENTRY( QIcon( ":/menu/play" ), qtr(I_POP_PLAY),
                        container.ACTION_PLAY, callerAsList );

        menu.addAction( QIcon( ":/menu/stream" ), qtr(I_POP_STREAM),
                        this, SLOT( popupStream() ) );

        menu.addAction( QIcon(), qtr(I_POP_SAVE),
                        this, SLOT( popupSave() ) );

        menu.addAction( QIcon( ":/menu/info" ), qtr(I_POP_INFO),
                        this, SLOT( popupInfoDialog() ) );

        menu.addSeparator();

        if( model->getURI( index ).startsWith( "file://" ) )
            menu.addAction( QIcon( ":/type/folder-grey" ), qtr(I_POP_EXPLORE),
                            this, SLOT( popupExplore() ) );
    }

    /* In PL or ML, allow to add a file/folder */
    if( model->canEdit() )
    {
        QIcon addIcon( ":/buttons/playlist/playlist_add" );

        if( model->isTree() )
            menu.addAction( addIcon, qtr(I_POP_NEWFOLDER),
                            this, SLOT( popupPromptAndCreateNode() ) );

        menu.addSeparator();
        if( model->isCurrentItem( model->rootIndex(), PLModel::IN_PLAYLIST ) )
        {
            menu.addAction( addIcon, qtr(I_PL_ADDF), THEDP, SLOT( simplePLAppendDialog()) );
            menu.addAction( addIcon, qtr(I_PL_ADDDIR), THEDP, SLOT( PLAppendDir()) );
            menu.addAction( addIcon, qtr(I_OP_ADVOP), THEDP, SLOT( PLAppendDialog()) );
        }
        else if( model->isCurrentItem( model->rootIndex(), PLModel::IN_MEDIALIBRARY ) )
        {
            menu.addAction( addIcon, qtr(I_PL_ADDF), THEDP, SLOT( simpleMLAppendDialog()) );
            menu.addAction( addIcon, qtr(I_PL_ADDDIR), THEDP, SLOT( MLAppendDir() ) );
            menu.addAction( addIcon, qtr(I_OP_ADVOP), THEDP, SLOT( MLAppendDialog() ) );
        }
    }

    if( index.isValid() )
    {
        if( !model->isCurrentItem( model->rootIndex(), PLModel::IN_PLAYLIST ) )
        {
            ADD_MENU_ENTRY( QIcon(), qtr(I_PL_ADDPL),
                            container.ACTION_ADDTOPLAYLIST, selectionlist );
        }
    }

    menu.addSeparator();

    /* Item removal */
    if( index.isValid() )
    {
        ADD_MENU_ENTRY( QIcon( ":/buttons/playlist/playlist_remove" ), qtr(I_POP_DEL),
                        container.ACTION_REMOVE, selectionlist );
    }

    if( model->canEdit() ) {
        menu.addAction( QIcon( ":/toolbar/clear" ), qtr("Clear the playlist"),
                        model, SLOT( clearPlaylist() ) );
    }

    menu.addSeparator();

    /* Playlist sorting */
    QMenu *sortingMenu = new QMenu( qtr( "Sort by" ) );
    /* Choose what columns to show in sorting menu, not sure if this should be configurable*/
    QList<int> sortingColumns;
    sortingColumns << COLUMN_TITLE << COLUMN_ARTIST << COLUMN_ALBUM << COLUMN_TRACK_NUMBER << COLUMN_URI;
    container.action = container.ACTION_SORT;
    container.indexes = callerAsList;
    foreach( int Column, sortingColumns )
    {
        action = sortingMenu->addAction( qfu( psz_column_title( Column ) ) + " " + qtr("Ascending") );
        container.column = model->columnFromMeta(Column) + 1;
        action->setData( QVariant::fromValue( container ) );

        action = sortingMenu->addAction( qfu( psz_column_title( Column ) ) + " " + qtr("Descending") );
        container.column = -1 * (model->columnFromMeta(Column)+1);
        action->setData( QVariant::fromValue( container ) );
    }
    menu.addMenu( sortingMenu );

    /* Zoom */
    QMenu *zoomMenu = new QMenu( qtr( "Display size" ) );
    zoomMenu->addAction( qtr( "Increase" ), this, SLOT( increaseZoom() ) );
    zoomMenu->addAction( qtr( "Decrease" ), this, SLOT( decreaseZoom() ) );
    menu.addMenu( zoomMenu );

    CONNECT( &menu, triggered( QAction * ), model, actionSlot( QAction * ) );

    menu.addMenu( StandardPLPanel::viewSelectionMenu( this ) );

    /* Display and forward the result */
    if( !menu.isEmpty() )
    {
        menu.exec( point ); return true;
    }
    else return false;

#undef ADD_MENU_ENTRY
}

QMenu* StandardPLPanel::viewSelectionMenu( StandardPLPanel *panel )
{
    QMenu *viewMenu = new QMenu( qtr( "Playlist View Mode" ) );
    QSignalMapper *viewSelectionMapper = new QSignalMapper( viewMenu );
    CONNECT( viewSelectionMapper, mapped( int ), panel, showView( int ) );

    QActionGroup *viewGroup = new QActionGroup( viewMenu );
# define MAX_VIEW StandardPLPanel::VIEW_COUNT
    for( int i = 0; i < MAX_VIEW; i++ )
    {
        QAction *action = viewMenu->addAction( viewNames[i] );
        action->setCheckable( true );
        viewGroup->addAction( action );
        viewSelectionMapper->setMapping( action, i );
        CONNECT( action, triggered(), viewSelectionMapper, map() );
        if( panel->currentViewIndex() == i )
            action->setChecked( true );
    }
    return viewMenu;
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

void StandardPLPanel::popupPromptAndCreateNode()
{
    bool ok;
    QString name = QInputDialog::getText( PlaylistDialog::getInstance( p_intf ),
        qtr( I_NEW_DIR ), qtr( I_NEW_DIR_NAME ),
        QLineEdit::Normal, QString(), &ok);
    if ( !ok ) return;
    qobject_cast<VLCModel *>(currentView->model())->createNode( popupIndex, name );
}

void StandardPLPanel::popupInfoDialog()
{
    if( popupIndex.isValid() )
    {
        VLCModel *model = qobject_cast<VLCModel *>(currentView->model());
        input_item_t* p_input = model->getInputItem( popupIndex );
        MediaInfoDialog *mid = new MediaInfoDialog( p_intf, p_input );
        mid->setParent( PlaylistDialog::getInstance( p_intf ),
                        Qt::Dialog );
        mid->show();
    }
}

void StandardPLPanel::popupExplore()
{
    VLCModel *model = qobject_cast<VLCModel *>(currentView->model());
    QString uri = model->getURI( popupIndex );
    char *path = NULL;

    if( ! uri.isEmpty() )
        path = make_path( uri.toLatin1().constData() );

    if( path == NULL )
        return;

    QDesktopServices::openUrl(
                QUrl::fromLocalFile( QFileInfo( qfu( path ) ).absolutePath() ) );

    free( path );
}

void StandardPLPanel::popupStream()
{
    VLCModel *model = qobject_cast<VLCModel *>(currentView->model());
    QString uri = model->getURI( popupIndex );
    if ( ! uri.isEmpty() )
        THEDP->streamingDialog( NULL, uri, false );
}

void StandardPLPanel::popupSave()
{
    VLCModel *model = qobject_cast<VLCModel *>(currentView->model());
    QString uri = model->getURI( popupIndex );
    if ( ! uri.isEmpty() )
        THEDP->streamingDialog( NULL, uri );
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
    bool can_search;
    p_selector->getCurrentItemInfos( &type, &can_search, &name );

    if( type != SD_TYPE || !can_search )
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
    bool can_search;
    p_selector->getCurrentItemInfos( &type, &can_search, &name );

    if( type == SD_TYPE && can_search )
    {
        if( !name.isEmpty() && !searchText.isEmpty() )
            playlist_ServicesDiscoveryControl( THEPL, qtu( name ), SD_CMD_SEARCH,
                                              qtu( searchText ) );
    }
}

/* Set the root of the new Playlist */
/* This activated by the selector selection */
void StandardPLPanel::setRootItem( playlist_item_t *p_item, bool b )
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
        if( currentView->model() != model )
            currentView->setModel( model );
        model->rebuild( p_item );
    }
}

void StandardPLPanel::browseInto( const QModelIndex &index )
{
    if( currentView == iconView || currentView == listView || currentView == picFlowView )
    {

        currentView->setRootIndex( index );

        /* When going toward root in LocationBar, scroll to the item
           that was previously as root */
        QModelIndex newIndex = model->index(currentRootIndexId,0);
        while( newIndex.isValid() && (newIndex.parent() != index) )
            newIndex = newIndex.parent();
        if( newIndex.isValid() )
            currentView->scrollTo( newIndex );

        /* Store new rootindexid*/
        currentRootIndexId = model->itemId( index );
        model->ensureArtRequested( index );
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
    if( e->modifiers() & Qt::ControlModifier ) {
        int numSteps = e->delta() / 8 / 15;
        if( numSteps > 0)
            increaseZoom();
        else if( numSteps < 0)
            decreaseZoom();
    }
    // Accept this event in order to prevent unwanted volume up/down changes
    e->accept();
}

bool StandardPLPanel::eventFilter ( QObject *obj, QEvent * event )
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
    else if ( event->type() == QEvent::Paint )
    {/* Warn! Don't filter events from anything else than views ! */
        if ( model->rowCount() == 0 && p_selector->getCurrentItemCategory() == PL_ITEM_TYPE )
        {
            QWidget *viewport = qobject_cast<QWidget *>( obj );
            QStylePainter painter( viewport );
            QPixmap dropzone(":/dropzone");
            QRect rect = viewport->geometry();
            QSize size = rect.size() / 2 - dropzone.size() / 2;
            rect.adjust( 0, size.height(), 0 , 0 );
            painter.drawItemPixmap( rect, Qt::AlignHCenter, dropzone );
            /* now select the zone just below the drop zone and let Qt center
               the text by itself */
            rect.adjust( 0, dropzone.size().height() + 10, 0, 0 );
            rect.setRight( viewport->geometry().width() );
            rect.setLeft( 0 );
            painter.drawItemText( rect,
                                  Qt::AlignHCenter,
                                  palette(),
                                  true,
                                  qtr("Playlist is currently empty.\n"
                                      "Drop a file here or select a "
                                      "media source from the left."),
                                  QPalette::Text );
        }
        else if ( spinnerAnimation->state() == PixmapAnimator::Running )
        {
            if ( currentView->model()->rowCount() )
                spinnerAnimation->stop(); /* Trick until SD emits events */
            else
            {
                QWidget *viewport = qobject_cast<QWidget *>( obj );
                QStylePainter painter( viewport );
                QPixmap *spinner = spinnerAnimation->getPixmap();
                QPoint point = viewport->geometry().center();
                point -= QPoint( spinner->size().width() / 2, spinner->size().height() / 2 );
                painter.drawPixmap( point, *spinner );
            }
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
    iconView->viewport()->installEventFilter( this );
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
    listView->viewport()->installEventFilter( this );
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
    treeView = new PlTreeView( model, this );

    /* setModel after setSortingEnabled(true), or the model will sort immediately! */

    /* Connections for the TreeView */
    CONNECT( treeView, activated( const QModelIndex& ),
             this, activate( const QModelIndex& ) );
    CONNECT( treeView->header(), customContextMenuRequested( const QPoint & ),
             this, popupSelectColumn( QPoint ) );
    CONNECT( treeView, customContextMenuRequested( const QPoint & ),
             this, popupPlView( const QPoint & ) );
    treeView->installEventFilter( this );
    treeView->viewport()->installEventFilter( this );

    /* SignalMapper for columns */
    selectColumnsSigMapper = new QSignalMapper( this );
    CONNECT( selectColumnsSigMapper, mapped( int ),
             this, toggleColumnShown( int ) );

    viewStack->addWidget( treeView );
}

void StandardPLPanel::updateZoom( int i )
{
    if ( i < 5 - QApplication::font().pointSize() ) return;
    if ( i > 3 + QApplication::font().pointSize() ) return;
    i_zoom = i;
#define A_ZOOM( view ) \
    if ( view ) \
    qobject_cast<AbstractPlViewItemDelegate*>( view->itemDelegate() )->setZoom( i_zoom )
    /* Can't iterate as picflow & tree aren't using custom delegate */
    A_ZOOM( iconView );
    A_ZOOM( listView );
#undef A_ZOOM
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
    bool b_treeViewCreated = false;

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
        {
            createTreeView();
            b_treeViewCreated = true;
        }
        currentView = treeView;
        break;
    }
    }

    changeModel( false );

    /* Restoring the header Columns must come after changeModel */
    if( b_treeViewCreated )
    {
        assert( treeView );
        if( getSettings()->contains( "Playlist/headerStateV2" ) )
        {
            treeView->header()->restoreState(getSettings()
                    ->value( "Playlist/headerStateV2" ).toByteArray() );
            /* if there is allready stuff in playlist, we don't sort it and we reset
               sorting */
            if( model->rowCount() )
            {
                treeView->header()->setSortIndicator( -1 , Qt::AscendingOrder );
            }
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
    }

    updateZoom( i_zoom );
    viewStack->setCurrentWidget( currentView );
    browseInto();
    gotoPlayingItem();
}

void StandardPLPanel::setWaiting( bool b )
{
    if ( b )
    {
        spinnerAnimation->setLoopCount( 20 ); /* Trick until SD emits an event */
        spinnerAnimation->start();
    }
    else
        spinnerAnimation->stop();
}

void StandardPLPanel::updateViewport()
{
    /* A single update on parent widget won't work */
    currentView->viewport()->repaint();
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

void StandardPLPanel::cycleViews()
{
    if( currentView == iconView )
        showView( TREE_VIEW );
    else if( currentView == treeView )
        showView( LIST_VIEW );
    else if( currentView == listView )
#ifndef NDEBUG
        showView( PICTUREFLOW_VIEW  );
    else if( currentView == picFlowView )
#endif
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
            lastActivatedId = p_item->i_id;
            playlist_Unlock( THEPL );
            model->activateItem( index );
        }
    }
}

void StandardPLPanel::browseInto( int i_id )
{
    if( i_id != lastActivatedId ) return;

    QModelIndex index = model->index( i_id, 0 );

    if( currentView == treeView )
        treeView->setExpanded( index, true );
    else
        browseInto( index );

    lastActivatedId = -1;
}
