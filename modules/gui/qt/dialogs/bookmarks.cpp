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
#include "input_manager.hpp"

#include <QHBoxLayout>
#include <QSpacerItem>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QModelIndexList>

BookmarksDialog::BookmarksDialog( intf_thread_t *_p_intf ):QVLCFrame( _p_intf )
{
    b_ignore_updates = false;
    setWindowFlags( Qt::Tool );
    setWindowOpacity( var_InheritFloat( p_intf, "qt-opacity" ) );
    setWindowTitle( qtr( "Edit Bookmarks" ) );
    setWindowRole( "vlc-bookmarks" );

    QHBoxLayout *layout = new QHBoxLayout( this );

    QDialogButtonBox *buttonsBox = new QDialogButtonBox( Qt::Vertical );
    QPushButton *addButton = new QPushButton( qtr( "Create" ) );
    addButton->setToolTip( qtr( "Create a new bookmark" ) );
    buttonsBox->addButton( addButton, QDialogButtonBox::ActionRole );
    delButton = new QPushButton( qtr( "Delete" ) );
    delButton->setToolTip( qtr( "Delete the selected item" ) );
    buttonsBox->addButton( delButton, QDialogButtonBox::ActionRole );
    clearButton = new QPushButton( qtr( "Clear" ) );
    clearButton->setToolTip( qtr( "Delete all the bookmarks" ) );
    buttonsBox->addButton( clearButton, QDialogButtonBox::ResetRole );
#if 0
    QPushButton *extractButton = new QPushButton( qtr( "Extract" ) );
    extractButton->setToolTip( qtr() );
    buttonsBox->addButton( extractButton, QDialogButtonBox::ActionRole );
#endif
    /* ?? Feels strange as Qt guidelines will put reject on top */
    buttonsBox->addButton( new QPushButton( qtr( "&Close" ) ),
                          QDialogButtonBox::RejectRole);

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

    layout->addWidget( buttonsBox );
    layout->addWidget( bookmarksList );

    CONNECT( THEMIM->getIM(), bookmarksChanged(),
             this, update() );

    CONNECT( bookmarksList, activated( QModelIndex ), this,
             activateItem( QModelIndex ) );
    CONNECT( bookmarksList, itemChanged( QTreeWidgetItem*, int ),
             this, edit( QTreeWidgetItem*, int ) );
    CONNECT( bookmarksList->model(), rowsInserted( const QModelIndex &, int, int ),
             this, updateButtons() );
    CONNECT( bookmarksList->model(), rowsRemoved( const QModelIndex &, int, int ),
             this, updateButtons() );
    CONNECT( bookmarksList->selectionModel(), selectionChanged( const QItemSelection &, const QItemSelection & ),
             this, updateButtons() );
    BUTTONACT( addButton, add() );
    BUTTONACT( delButton, del() );
    BUTTONACT( clearButton, clear() );

#if 0
    BUTTONACT( extractButton, extract() );
#endif
    CONNECT( buttonsBox, rejected(), this, close() );
    updateButtons();

    restoreWidgetPosition( "Bookmarks", QSize( 435, 280 ) );
    updateGeometry();
}

BookmarksDialog::~BookmarksDialog()
{
    saveWidgetPosition( "Bookmarks" );
}

void BookmarksDialog::updateButtons()
{
    clearButton->setEnabled( bookmarksList->model()->rowCount() > 0 );
    delButton->setEnabled( bookmarksList->selectionModel()->hasSelection() );
}

void BookmarksDialog::update()
{
    if ( b_ignore_updates ) return;
    input_thread_t *p_input = THEMIM->getInput();
    if( !p_input ) return;

    seekpoint_t **pp_bookmarks;
    int i_bookmarks = 0;

    if( bookmarksList->topLevelItemCount() > 0 )
    {
        bookmarksList->model()->removeRows( 0, bookmarksList->topLevelItemCount() );
    }

    if( input_Control( p_input, INPUT_GET_BOOKMARKS, &pp_bookmarks,
                       &i_bookmarks ) != VLC_SUCCESS )
        return;

    for( int i = 0; i < i_bookmarks; i++ )
    {
        mtime_t total = pp_bookmarks[i]->i_time_offset;
        unsigned hours   = ( total / ( CLOCK_FREQ * 3600 ) );
        unsigned minutes = ( total % ( CLOCK_FREQ * 3600 ) ) / ( CLOCK_FREQ * 60 );
        float    seconds = ( total % ( CLOCK_FREQ * 60 ) ) / ( CLOCK_FREQ * 1. );

        QStringList row;
        row << QString( qfu( pp_bookmarks[i]->psz_name ) );
        row << qfu("-");
        row << QString().sprintf( "%02u:%02u:%06.3f", hours, minutes, seconds );

        QTreeWidgetItem *item = new QTreeWidgetItem( bookmarksList, row );
        item->setFlags( Qt::ItemIsSelectable | Qt::ItemIsEditable |
                        Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
        bookmarksList->insertTopLevelItem( i, item );
        vlc_seekpoint_Delete( pp_bookmarks[i] );
    }
    free( pp_bookmarks );
}

void BookmarksDialog::add()
{
    input_thread_t *p_input = THEMIM->getInput();
    if( !p_input ) return;

    seekpoint_t bookmark;

    if( !input_Control( p_input, INPUT_GET_BOOKMARK, &bookmark ) )
    {
        QString name = THEMIM->getIM()->getName() + " #"
                     + QString::number( bookmarksList->topLevelItemCount() );
        QByteArray raw = name.toUtf8();
        bookmark.psz_name = raw.data();

        input_Control( p_input, INPUT_ADD_BOOKMARK, &bookmark );
    }
}

void BookmarksDialog::del()
{
    input_thread_t *p_input = THEMIM->getInput();
    if( !p_input ) return;

    QModelIndexList selected = bookmarksList->selectionModel()->selectedRows();
    if ( !selected.empty() )
    {
        b_ignore_updates = true;
        /* Sort needed to make sure that selected elements are deleted in descending
           order, otherwise the indexes might change and wrong bookmarks are deleted. */
        qSort( selected.begin(), selected.end() );
        QModelIndexList::Iterator it = selected.end();
        for( --it; it != selected.begin(); it-- )
        {
            input_Control( p_input, INPUT_DEL_BOOKMARK, (*it).row() );
        }
        input_Control( p_input, INPUT_DEL_BOOKMARK, (*it).row() );
        b_ignore_updates = false;
        update();
    }
}

void BookmarksDialog::clear()
{
    input_thread_t *p_input = THEMIM->getInput();
    if( !p_input ) return;

    input_Control( p_input, INPUT_CLEAR_BOOKMARKS );
}

void BookmarksDialog::edit( QTreeWidgetItem *item, int column )
{
    QStringList fields;
    // We can only edit a item if it is the last item selected
    if( bookmarksList->selectedItems().isEmpty() ||
        bookmarksList->selectedItems().last() != item )
        return;

    input_thread_t *p_input = THEMIM->getInput();
    if( !p_input )
        return;

    // We get the row number of the item
    int i_edit = bookmarksList->indexOfTopLevelItem( item );

    // We get the bookmarks list
    seekpoint_t** pp_bookmarks;
    seekpoint_t*  p_seekpoint = NULL;
    int i_bookmarks;

    if( input_Control( p_input, INPUT_GET_BOOKMARKS, &pp_bookmarks,
                       &i_bookmarks ) != VLC_SUCCESS )
        return;

    if( i_edit >= i_bookmarks )
        goto clear;

    // We modify the seekpoint
    p_seekpoint = pp_bookmarks[i_edit];
    if( column == 0 )
    {
        free( p_seekpoint->psz_name );
        p_seekpoint->psz_name = strdup( qtu( item->text( column ) ) );
    }
    else if( column == 2 )
    {
        fields = item->text( column ).split( ":", QString::SkipEmptyParts );
        if( fields.count() == 1 )
            p_seekpoint->i_time_offset = 1000000 * ( fields[0].toFloat() );
        else if( fields.count() == 2 )
            p_seekpoint->i_time_offset = 1000000 * ( fields[0].toInt() * 60 + fields[1].toInt() );
        else if( fields.count() == 3 )
            p_seekpoint->i_time_offset = 1000000 * ( fields[0].toInt() * 3600 + fields[1].toInt() * 60 + fields[2].toFloat() );
        else
        {
            msg_Err( p_intf, "Invalid string format for time" );
            goto clear;
        }
    }

    // Send the modification
    input_Control( p_input, INPUT_CHANGE_BOOKMARK, p_seekpoint, i_edit );

clear:
    // Clear the bookmark list
    for( int i = 0; i < i_bookmarks; i++)
        vlc_seekpoint_Delete( pp_bookmarks[i] );
    free( pp_bookmarks );
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

void BookmarksDialog::toggleVisible()
{
    /* Update, to show existing bookmarks in case a new playlist
       was opened */
    if( !isVisible() )
    {
        update();
    }
    QVLCFrame::toggleVisible();
}
