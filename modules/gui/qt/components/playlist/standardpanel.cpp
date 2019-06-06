/*****************************************************************************
 * standardpanel.cpp : The "standard" playlist panel : just a treeview
 ****************************************************************************
 * Copyright © 2000-2010 VideoLAN
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
#include "components/playlist/views.hpp"          /* 3 views */
#include "components/playlist/selector.hpp"       /* PLSelector */
#include "util/animators.hpp"                     /* PixmapAnimator */
#include "menus.hpp"                              /* Popup */
#include "input_manager.hpp"                      /* THEMIM */
#include "dialogs_provider.hpp"                   /* THEDP */
#include "recents.hpp"                            /* RecentMRL */
#include "dialogs/playlist.hpp"                   /* Playlist Dialog */
#include "dialogs/mediainfo.hpp"                  /* MediaInfoDialog */
#include "util/qt_dirs.hpp"
#include "util/imagehelper.hpp"

#include <vlc_services_discovery.h>               /* SD_CMD_SEARCH */
#include <vlc_intf_strings.h>                     /* POP_ */

#define SPINNER_SIZE 32
#define I_NEW_DIR \
    I_DIR_OR_FOLDER( N_("Create Directory"), N_( "Create Folder" ) )
#define I_NEW_DIR_NAME \
    I_DIR_OR_FOLDER( N_( "Enter name for new directory:" ), \
                     N_( "Enter name for new folder:" ) )

#define I_RENAME_DIR \
    I_DIR_OR_FOLDER( N_("Rename Directory"), N_( "Rename Folder" ) )
#define I_RENAME_DIR_NAME \
    I_DIR_OR_FOLDER( N_( "Enter a new name for the directory:" ), \
                     N_( "Enter a new name for the folder:" ) )

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

#define DROPZONE_SIZE 112

/* local helper */
inline QModelIndex popupIndex( QAbstractItemView *view );

StandardPLPanel::StandardPLPanel( PlaylistWidget *_parent,
                                  intf_thread_t *_p_intf,
                                  playlist_item_t *p_root,
                                  PLSelector *_p_selector,
                                  VLCModel *_p_model )
                : QWidget( _parent ),
                  model( _p_model ),
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

    currentRootIndexPLId  = -1;
    lastActivatedPLItemId     = -1;

    QList<QString> frames;
    frames << ":/util/wait1.svg";
    frames << ":/util/wait2.svg";
    frames << ":/util/wait3.svg";
    frames << ":/util/wait4.svg";
    spinnerAnimation = new PixmapAnimator( this, frames, SPINNER_SIZE, SPINNER_SIZE );
    CONNECT( spinnerAnimation, pixmapReady( const QPixmap & ), this, updateViewport() );

    /* Saved Settings */
    int i_savedViewMode = getSettings()->value( "Playlist/view-mode", TREE_VIEW ).toInt();

    QFont font = QApplication::font();
    font.setPointSize( font.pointSize() + getSettings()->value( "Playlist/zoom", 0 ).toInt() );
    model->setData( QModelIndex(), font, Qt::FontRole );

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
    getSettings()->setValue( "zoom",
                model->data( QModelIndex(), Qt::FontRole ).value<QFont>().pointSize()
                - QApplication::font().pointSize() );
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
    if( currentRootIndexPLId != -1 && currentRootIndexPLId != model->itemId( index.parent() ) )
        browseInto( index.parent() );
    currentView->scrollTo( index );
}

void StandardPLPanel::popupPlView( const QPoint &point )
{
    QPoint globalPoint = currentView->viewport()->mapToGlobal( point );
    QModelIndex index = currentView->indexAt( point );
    if ( !index.isValid() )
    {
        currentView->clearSelection();
    }
    else if ( ! currentView->selectionModel()->selectedIndexes().contains( index ) )
    {
        currentView->selectionModel()->select( index, QItemSelectionModel::Select );
    }

    if( !popup( globalPoint ) ) THEDP->setPopupMenu();
}

/*********** Popup *********/
bool StandardPLPanel::popup( const QPoint &point )
{
    QModelIndex index = popupIndex( currentView ); /* index for menu logic only. Do not store.*/
    VLCModel *model = qobject_cast<VLCModel *>(currentView->model());

#define ADD_MENU_ENTRY( icon, title, act ) \
    if ( model->isSupportedAction( act, index ) )\
    {\
    action = menu.addAction( icon, title ); \
    container.action = act; \
    action->setData( QVariant::fromValue( container ) );\
    }

    /* */
    QMenu menu;
    QAction *action;
    VLCModelSubInterface::actionsContainerType container;

    /* Play/Stream/Info static actions */

    ADD_MENU_ENTRY( QIcon( ":/toolbar/play_b.svg" ), qtr(I_POP_PLAY),
                    VLCModelSubInterface::ACTION_PLAY )

    ADD_MENU_ENTRY( QIcon( ":/toolbar/pause_b.svg" ), qtr("Pause"),
                    VLCModelSubInterface::ACTION_PAUSE )

    ADD_MENU_ENTRY( QIcon( ":/menu/stream.svg" ), qtr(I_POP_STREAM),
                    VLCModelSubInterface::ACTION_STREAM )

    ADD_MENU_ENTRY( QIcon(), qtr(I_POP_SAVE),
                    VLCModelSubInterface::ACTION_SAVE );

    ADD_MENU_ENTRY( QIcon( ":/menu/info.svg" ), qtr(I_POP_INFO),
                    VLCModelSubInterface::ACTION_INFO );

    menu.addSeparator();

    ADD_MENU_ENTRY( QIcon( ":/type/folder-grey.svg" ), qtr(I_POP_EXPLORE),
                    VLCModelSubInterface::ACTION_EXPLORE );

    QIcon addIcon( ":/buttons/playlist/playlist_add.svg" );

    ADD_MENU_ENTRY( addIcon, qtr(I_POP_NEWFOLDER),
                    VLCModelSubInterface::ACTION_CREATENODE )

    ADD_MENU_ENTRY( QIcon(), qtr(I_POP_RENAMEFOLDER),
                    VLCModelSubInterface::ACTION_RENAMENODE )

    menu.addSeparator();
    /* In PL or ML, allow to add a file/folder */
    ADD_MENU_ENTRY( addIcon, qtr(I_PL_ADDF),
                    VLCModelSubInterface::ACTION_ENQUEUEFILE )

    ADD_MENU_ENTRY( addIcon, qtr(I_PL_ADDDIR),
                    VLCModelSubInterface::ACTION_ENQUEUEDIR )

    ADD_MENU_ENTRY( addIcon, qtr(I_OP_ADVOP),
                    VLCModelSubInterface::ACTION_ENQUEUEGENERIC )

    ADD_MENU_ENTRY( QIcon(), qtr(I_PL_ADDPL),
                    VLCModelSubInterface::ACTION_ADDTOPLAYLIST );

    menu.addSeparator();
    ADD_MENU_ENTRY( QIcon(), qtr( I_PL_SAVE ),
                    VLCModelSubInterface::ACTION_SAVETOPLAYLIST );

    menu.addSeparator();

    /* Item removal */

    ADD_MENU_ENTRY( QIcon( ":/buttons/playlist/playlist_remove.svg" ), qtr(I_POP_DEL),
                    VLCModelSubInterface::ACTION_REMOVE );

    ADD_MENU_ENTRY( QIcon( ":/toolbar/clear.svg" ), qtr("Clear the playlist"),
                    VLCModelSubInterface::ACTION_CLEAR );

    menu.addSeparator();

    /* Playlist sorting */
    if ( model->isSupportedAction( VLCModelSubInterface::ACTION_SORT, index ) )
    {
        QMenu *sortingMenu = new QMenu( qtr( "Sort by" ), &menu );
        /* Choose what columns to show in sorting menu, not sure if this should be configurable*/
        QList<int> sortingColumns;
        sortingColumns << COLUMN_TITLE << COLUMN_ARTIST << COLUMN_ALBUM << COLUMN_TRACK_NUMBER << COLUMN_URI << COLUMN_DISC_NUMBER;
        container.action = VLCModelSubInterface::ACTION_SORT;
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
    }

    /* Zoom */
    QMenu *zoomMenu = new QMenu( qtr( "Display size" ), &menu );
    zoomMenu->addAction( qtr( "Increase" ), this, SLOT( increaseZoom() ) );
    zoomMenu->addAction( qtr( "Decrease" ), this, SLOT( decreaseZoom() ) );
    menu.addMenu( zoomMenu );

    CONNECT( &menu, triggered( QAction * ), this, popupAction( QAction * ) );

    menu.addMenu( StandardPLPanel::viewSelectionMenu( this ) );

    /* Display and forward the result */
    if( !menu.isEmpty() )
    {
        menu.exec( point ); return true;
    }
    else return false;

#undef ADD_MENU_ENTRY
}

void StandardPLPanel::popupAction( QAction *action )
{
    VLCModel *model = qobject_cast<VLCModel *>(currentView->model());
    VLCModelSubInterface::actionsContainerType a =
            action->data().value<VLCModelSubInterface::actionsContainerType>();
    QModelIndexList list = currentView->selectionModel()->selectedRows();
    QModelIndex index = popupIndex( currentView );
    char *path = NULL;
    OpenDialog *dialog;
    QString temp;
    QStringList uris;
    bool ok;

    /* first try to complete actions requiring missing parameters thru UI dialogs */
    switch( a.action )
    {
    case VLCModelSubInterface::ACTION_INFO:
        /* locally handled only */
        if( index.isValid() )
        {
            input_item_t* p_input = model->getInputItem( index );
            MediaInfoDialog *mid = new MediaInfoDialog( p_intf, p_input );
            mid->setParent( PlaylistDialog::getInstance( p_intf ),
                            Qt::Dialog );
            mid->show();
        }
        break;

    case VLCModelSubInterface::ACTION_EXPLORE:
        /* locally handled only */
        temp = model->getURI( index );
        if( ! temp.isEmpty() ) path = vlc_uri2path( temp.toLatin1().constData() );
        if( path == NULL ) return;
        QDesktopServices::openUrl(
                    QUrl::fromLocalFile( QFileInfo( qfu( path ) ).absolutePath() ) );
        free( path );
        break;

    case VLCModelSubInterface::ACTION_STREAM:
        /* locally handled only */
        temp = model->getURI( index );
        if ( ! temp.isEmpty() )
        {
            QStringList tempList;
            tempList.append(temp);
            THEDP->streamingDialog( NULL, tempList, false );
        }
        break;

    case VLCModelSubInterface::ACTION_SAVE:
        /* locally handled only */
        temp = model->getURI( index );
        if ( ! temp.isEmpty() )
        {
            QStringList tempList;
            tempList.append(temp);
            THEDP->streamingDialog( NULL, tempList );
        }
        break;

    case VLCModelSubInterface::ACTION_CREATENODE:
        temp = QInputDialog::getText( PlaylistDialog::getInstance( p_intf ),
            qtr( I_NEW_DIR ), qtr( I_NEW_DIR_NAME ),
            QLineEdit::Normal, QString(), &ok);
        if ( !ok ) return;
        model->createNode( index, temp );
        break;

    case VLCModelSubInterface::ACTION_RENAMENODE:
        temp = QInputDialog::getText( PlaylistDialog::getInstance( p_intf ),
            qtr( I_RENAME_DIR ), qtr( I_RENAME_DIR_NAME ),
            QLineEdit::Normal, model->getTitle( index ), &ok);
        if ( !ok ) return;
        model->renameNode( index, temp );
        break;

    case VLCModelSubInterface::ACTION_ENQUEUEFILE:
        uris = THEDP->showSimpleOpen();
        if ( uris.isEmpty() ) return;
        uris.sort();
        a.uris = uris;
        action->setData( QVariant::fromValue( a ) );
        model->action( action, list );
        break;

    case VLCModelSubInterface::ACTION_ENQUEUEDIR:
        temp = DialogsProvider::getDirectoryDialog( p_intf );
        if ( temp.isEmpty() ) return;
        a.uris << temp;
        action->setData( QVariant::fromValue( a ) );
        model->action( action, list );
        break;

    case VLCModelSubInterface::ACTION_ENQUEUEGENERIC:
        dialog = OpenDialog::getInstance( this, p_intf, false, SELECT, true );
        dialog->showTab( OPEN_FILE_TAB );
        dialog->exec(); /* make it modal */
        a.uris = dialog->getMRLs();
        a.options = dialog->getOptions();
        if ( a.uris.isEmpty() ) return;
        action->setData( QVariant::fromValue( a ) );
        model->action( action, list );
        break;

    case VLCModelSubInterface::ACTION_SAVETOPLAYLIST:
        THEDP->savePlayingToPlaylist();
        break;
    default:
        model->action( action, list );
    }
}

QMenu* StandardPLPanel::viewSelectionMenu( StandardPLPanel *panel )
{
    QMenu *viewMenu = new QMenu( qtr( "Playlist View Mode" ), panel );
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

inline QModelIndex popupIndex( QAbstractItemView *view )
{
    QModelIndexList list = view->selectionModel()->selectedIndexes();
    if ( list.isEmpty() )
        return QModelIndex();
    else
        return list.first();
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
    bool can_search;
    p_selector->getCurrentItemInfos( &type, &can_search, &name );

    if( type != SD_TYPE || !can_search )
    {
        bool flat = ( currentView == iconView ||
                      currentView == listView ||
                      currentView == picFlowView );
        model->filter( searchText,
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
    Q_UNUSED( b );
    model->rebuild( p_item );
}

void StandardPLPanel::browseInto( const QModelIndex &index )
{
    if( currentView == iconView || currentView == listView || currentView == picFlowView )
    {

        currentView->setRootIndex( index );

        /* When going toward root in LocationBar, scroll to the item
           that was previously as root */
        QModelIndex newIndex = model->indexByPLID(currentRootIndexPLId,0);
        while( newIndex.isValid() && (newIndex.parent() != index) )
            newIndex = newIndex.parent();
        if( newIndex.isValid() )
            currentView->scrollTo( newIndex );

        /* Store new rootindexid*/
        currentRootIndexPLId = model->itemId( index );

        model->ensureArtRequested( index );
    }

    emit viewChanged( index );
}

void StandardPLPanel::browseInto()
{
    browseInto( (currentRootIndexPLId != -1 && currentView != treeView) ?
                 model->indexByPLID( currentRootIndexPLId, 0 ) :
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

            QPixmap dropzone = ImageHelper::loadSvgToPixmap(":/dropzone.svg", DROPZONE_SIZE, DROPZONE_SIZE);
            QRect rect = viewport->geometry();
#if HAS_QT56
            qreal scale = dropzone.devicePixelRatio();
            QSize size = rect.size()  / 2 - dropzone.size() / (2 * scale);
#else
            QSize size = rect.size()  / 2 - dropzone.size() / 2;
#endif
            rect.adjust( 0, size.height(), 0 , 0 );
            painter.drawItemPixmap( rect, Qt::AlignHCenter, dropzone );
            /* now select the zone just below the drop zone and let Qt center
               the text by itself */
#if HAS_QT56
            rect.adjust( 0, dropzone.height() / scale + 10, 0, 0 );
#else
            rect.adjust( 0, dropzone.height() + 10, 0, 0 );
#endif
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
                const QPixmap& spinner = spinnerAnimation->getPixmap();
                QPoint point = viewport->geometry().center();
                point -= QPoint( spinner.width() / 2, spinner.height() / 2 );
                painter.drawPixmap( point, spinner );
            }
        }
    }
    return false;
}

void StandardPLPanel::deleteSelection()
{
    QModelIndexList list = currentView->selectionModel()->selectedIndexes();
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
    QVariant fontdata = model->data( QModelIndex(), Qt::FontRole );
    QFont font = fontdata.value<QFont>();
    font.setPointSize( font.pointSize() + i );
    if ( font.pointSize() < 5 - QApplication::font().pointSize() ) return;
    if ( font.pointSize() > 3 + QApplication::font().pointSize() ) return;
    model->setData( QModelIndex(), font, Qt::FontRole );
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

    currentView->setModel( model );

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
        vlc_assert_unreachable();
}

void StandardPLPanel::activate( const QModelIndex &index )
{
    if( currentView->model() == model )
    {
        /* If we are not a leaf node */
        if( !index.data( VLCModelSubInterface::LEAF_NODE_ROLE ).toBool() )
        {
            if( currentView != treeView )
                browseInto( index );
        }
        else
        {
            playlist_Lock( THEPL );
            playlist_item_t *p_item = playlist_ItemGetById( THEPL, model->itemId( index ) );
            if ( p_item )
            {
                p_item->i_flags |= PLAYLIST_SUBITEM_STOP_FLAG;
                lastActivatedPLItemId = p_item->i_id;
            }
            playlist_Unlock( THEPL );
            if ( p_item && index.isValid() )
                model->activateItem( index );
        }
    }
}

void StandardPLPanel::browseInto( int i_pl_item_id )
{
    if( i_pl_item_id != lastActivatedPLItemId ) return;

    QModelIndex index = model->indexByPLID( i_pl_item_id, 0 );

    if( currentView == treeView )
        treeView->setExpanded( index, true );
    else
        browseInto( index );

    lastActivatedPLItemId = -1;
}
