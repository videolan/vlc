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

#include "bookmarks.hpp"
#include "player/player_controller.hpp"
#include "medialibrary/mlbookmarkmodel.hpp"
#include "maininterface/mainctx.hpp"

#include <QHBoxLayout>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QTreeView>

BookmarksDialog::BookmarksDialog( qt_intf_t *_p_intf ):QVLCFrame( _p_intf )
{
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

    bookmarksList = new QTreeView( this );
    m_model = new MLBookmarkModel( bookmarksList );
    m_model->setPlayer(_p_intf->p_player);
    m_model->setMl(_p_intf->p_mi->getMediaLibrary());
    bookmarksList->setModel( m_model );
    bookmarksList->setRootIsDecorated( false );
    bookmarksList->setAlternatingRowColors( true );
    /* Sort by default model order, otherwise column 0 will be used */
    bookmarksList->sortByColumn( -1, Qt::AscendingOrder );
    bookmarksList->setSortingEnabled( true );
    bookmarksList->setSelectionMode( QAbstractItemView::ExtendedSelection );
    bookmarksList->setSelectionBehavior( QAbstractItemView::SelectRows );
    bookmarksList->setEditTriggers( QAbstractItemView::SelectedClicked |
                                    QAbstractItemView::DoubleClicked );

    bookmarksList->resize( sizeHint() );

    layout->addWidget( buttonsBox );
    layout->addWidget( bookmarksList );

    connect( bookmarksList, &QTreeView::activated, this, &BookmarksDialog::activateItem);
    connect( m_model, &QAbstractListModel::modelReset, this, &BookmarksDialog::updateButtons );
    connect( bookmarksList->selectionModel(), &QItemSelectionModel::selectionChanged, this, &BookmarksDialog::updateButtons );
    connect( addButton,   &QAbstractButton::clicked, this,  &BookmarksDialog::add );
    connect( delButton,   &QAbstractButton::clicked, this,  &BookmarksDialog::del );
    connect( clearButton, &QAbstractButton::clicked, this,  &BookmarksDialog::clear );

#if 0
    connect( extractButton, &QAbstractButton::clicked, this,  &BookmarksDialog::extract );
#endif
    connect( buttonsBox, &QDialogButtonBox::rejected, this, &BookmarksDialog::close);
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

void BookmarksDialog::add()
{
    m_model->add();
}

void BookmarksDialog::del()
{
    m_model->remove( bookmarksList->selectionModel()->selectedIndexes() );
}

void BookmarksDialog::clear()
{
    m_model->clear();
}

void BookmarksDialog::extract()
{
    // TODO
}

void BookmarksDialog::activateItem( const QModelIndex& index )
{
    m_model->select( index );
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
