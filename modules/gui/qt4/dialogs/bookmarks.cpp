/*****************************************************************************
 * bookmarks.cpp : Bookmarks
 ****************************************************************************
 * Copyright (C) 2007-2008 the VideoLAN team
 *
 * Authors: Antoine Lejeune <phytos@via.ecp.fr>
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

#include "dialogs/bookmarks.hpp"
#include "dialogs_provider.hpp"
#include "input_manager.hpp"

#include <QGridLayout>
#include <QSpacerItem>
#include <QPushButton>

BookmarksDialog *BookmarksDialog::instance = NULL;

BookmarksDialog::BookmarksDialog( intf_thread_t *_p_intf ):QVLCFrame( _p_intf )
{
    setWindowFlags( Qt::Tool );
    setWindowOpacity( config_GetFloat( p_intf, "qt-opacity" ) );
    setWindowTitle( qtr( "Edit Bookmarks" ) );

    QGridLayout *layout = new QGridLayout( this );

    QPushButton *addButton = new QPushButton( qtr( "Create" ) );
    addButton->setToolTip( qtr( "Create a new bookmark" ) );
    QPushButton *delButton = new QPushButton( qtr( "Delete" ) );
    delButton->setToolTip( qtr( "Delete the selected item" ) );
    QPushButton *clearButton = new QPushButton( qtr( "Clear" ) );
    clearButton->setToolTip( qtr( "Delete all the bookmarks" ) );
#if 0
    QPushButton *extractButton = new QPushButton( qtr( "Extract" ) );
    extractButton->setToolTip( qtr() );
#endif
    QPushButton *closeButton = new QPushButton( qtr( "&Close" ) );

    bookmarksList = new QTreeWidget( this );
    bookmarksList->setRootIsDecorated( false );
    bookmarksList->setAlternatingRowColors( true );
    bookmarksList->setSelectionMode( QAbstractItemView::ExtendedSelection );
    bookmarksList->setSelectionBehavior( QAbstractItemView::SelectRows );
    bookmarksList->setEditTriggers( QAbstractItemView::SelectedClicked );
    bookmarksList->setColumnCount( 3 );
    bookmarksList->resize( sizeHint() );

    QStringList headerLabels;
    headerLabels << qtr( "Description" );
    headerLabels << qtr( "Bytes" );
    headerLabels << qtr( "Time" );
    bookmarksList->setHeaderLabels( headerLabels );


    layout->addWidget( addButton, 0, 0 );
    layout->addWidget( delButton, 1, 0 );
    layout->addWidget( clearButton, 2, 0 );
    layout->addItem( new QSpacerItem( 20, 20, QSizePolicy::Expanding ), 4, 0 );
#if 0
    layout->addWidget( extractButton, 5, 0 );
#endif
    layout->addWidget( bookmarksList, 0, 1, 6, 2);
    layout->setColumnStretch( 1, 1 );
    layout->addWidget( closeButton, 7, 2 );

    CONNECT( bookmarksList, activated( QModelIndex ), this,
             activateItem( QModelIndex ) );
    CONNECT( bookmarksList, itemChanged( QTreeWidgetItem*, int ),
             this, edit( QTreeWidgetItem*, int ) );

    BUTTONACT( addButton, add() );
    BUTTONACT( delButton, del() );
    BUTTONACT( clearButton, clear() );
#if 0
    BUTTONACT( extractButton, extract() );
#endif
    BUTTONACT( closeButton, close() );

    readSettings( "Bookmarks", QSize( 435, 206 ) );
    updateGeometry();
}

BookmarksDialog::~BookmarksDialog()
{
    writeSettings( "Bookmarks" );
}

void BookmarksDialog::update()
{
    input_thread_t *p_input = THEMIM->getInput();
    if( !p_input ) return;

    seekpoint_t **pp_bookmarks;
    int i_bookmarks;

    if( bookmarksList->topLevelItemCount() > 0 )
    {
        bookmarksList->model()->removeRows( 0, bookmarksList->topLevelItemCount() );
    }

    if( input_Control( p_input, INPUT_GET_BOOKMARKS, &pp_bookmarks,
                       &i_bookmarks ) != VLC_SUCCESS )
        return;

    for( int i = 0; i < i_bookmarks; i++ )
    {
        // List with the differents elements of the row
        QStringList row;
        row << QString( pp_bookmarks[i]->psz_name );
        row << QString( "%1" ).arg( pp_bookmarks[i]->i_byte_offset );
        row << QString( "%1" ).arg( pp_bookmarks[i]->i_time_offset / 1000000 );
        QTreeWidgetItem *item = new QTreeWidgetItem( bookmarksList, row );
        item->setFlags( Qt::ItemIsSelectable | Qt::ItemIsEditable |
                        Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
        bookmarksList->insertTopLevelItem( i, item );
    }

}

void BookmarksDialog::add()
{
    input_thread_t *p_input = THEMIM->getInput();
    if( !p_input ) return;

    seekpoint_t bookmark;
    vlc_value_t pos;
    bookmark.psz_name = qtu( THEMIM->getIM()->getName() +
                    QString("_%1" ).arg( bookmarksList->topLevelItemCount() ) );
    bookmark.i_byte_offset = 0;
    bookmark.i_time_offset = 0;

    input_Control( p_input, INPUT_GET_BYTE_POSITION, &bookmark.i_byte_offset );
    var_Get( p_input, "time", &pos );
    bookmark.i_time_offset = pos.i_time;
    input_Control( p_input, INPUT_ADD_BOOKMARK, &bookmark );

    update();

}

void BookmarksDialog::del()
{
    input_thread_t *p_input = THEMIM->getInput();
    if( !p_input ) return;

    int i_focused = bookmarksList->currentIndex().row();

    if( i_focused >= 0 )
    {
        input_Control( p_input, INPUT_DEL_BOOKMARK, i_focused );
    }

    update();
}

void BookmarksDialog::clear()
{
    input_thread_t *p_input = THEMIM->getInput();
    if( !p_input ) return;

    input_Control( p_input, INPUT_CLEAR_BOOKMARKS );

    update();
}

void BookmarksDialog::edit( QTreeWidgetItem *item, int column )
{
    // We can only edit a item if it is the last item selected
    if( bookmarksList->selectedItems().isEmpty() ||
        bookmarksList->selectedItems().last() != item )
        return;

    input_thread_t *p_input = THEMIM->getInput();
    if( !p_input ) return;

    // We get the row number of the item
    int i_edit = bookmarksList->indexOfTopLevelItem( item );

    // We get the bookmarks list
    seekpoint_t **pp_bookmarks;
    int i_bookmarks;

    if( input_Control( p_input, INPUT_GET_BOOKMARKS, &pp_bookmarks,
                       &i_bookmarks ) != VLC_SUCCESS )
        return;

    if( i_edit >= i_bookmarks )
        return;

    // We modify the seekpoint
    seekpoint_t *p_seekpoint = pp_bookmarks[i_edit];
    if( column == 0 )
        p_seekpoint->psz_name = strdup( qtu( item->text( column ) ) );
    else if( column == 1 )
        p_seekpoint->i_byte_offset = atoi( qtu( item->text( column ) ) );
    else if( column == 2 )
        p_seekpoint->i_time_offset = 1000000 * atoll( qtu( item->text( column ) ) );

    if( input_Control( p_input, INPUT_CHANGE_BOOKMARK, p_seekpoint, i_edit ) !=
        VLC_SUCCESS )
        return;

    update();

}

void BookmarksDialog::extract()
{
    // TODO
}

void BookmarksDialog::activateItem( QModelIndex index )
{
    input_thread_t *p_input = THEMIM->getInput();
    if( !p_input ) return;

    input_Control( p_input, INPUT_SET_BOOKMARK, index.row() );
}
