/*****************************************************************************
 * plugins.hpp : Plug-ins and extensions listing
 ****************************************************************************
 * Copyright (C) 2008 the VideoLAN team
 * $Id$
 *
 * Authors: Jean-Baptiste Kempf <jb (at) videolan.org>
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

#include "plugins.hpp"

#include "util/customwidgets.hpp"

//#include <vlc_modules.h>

#include <QTreeWidget>
#include <QStringList>
#include <QHeaderView>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QLabel>

PluginDialog *PluginDialog::instance = NULL;

PluginDialog::PluginDialog( intf_thread_t *_p_intf ) : QVLCFrame( _p_intf )
{
    setWindowTitle( qtr( "Plugins and extensions" ) );
    setWindowRole( "vlc-plugins" );
    QGridLayout *layout = new QGridLayout( this );

    /* Main Tree for modules */
    treePlugins = new QTreeWidget;
    layout->addWidget( treePlugins, 0, 0, 1, -1 );

    /* Users cannot move the columns around but we need to sort */
    treePlugins->header()->setMovable( false );
    treePlugins->header()->setSortIndicatorShown( true );
    //    treePlugins->header()->setResizeMode( QHeaderView::ResizeToContents );
    treePlugins->setAlternatingRowColors( true );
    treePlugins->setColumnWidth( 0, 200 );

    QStringList headerNames;
    headerNames << qtr("Name") << qtr("Capability" ) << qtr( "Score" );
    treePlugins->setHeaderLabels( headerNames );

    FillTree();

    /* Set capability column to the correct Size*/
    treePlugins->resizeColumnToContents( 1 );
    treePlugins->header()->restoreState(
            getSettings()->value( "Plugins/Header-State" ).toByteArray() );

    treePlugins->setSortingEnabled( true );
    treePlugins->sortByColumn( 1, Qt::AscendingOrder );

    QLabel *label = new QLabel( qtr("&Search:"), this );
    edit = new SearchLineEdit( this );
    label->setBuddy( edit );

    layout->addWidget( label, 1, 0 );
    layout->addWidget( edit, 1, 1, 1, -1 );
    CONNECT( edit, textChanged( const QString& ),
            this, search( const QString& ) );

    QDialogButtonBox *box = new QDialogButtonBox;
    QPushButton *okButton = new QPushButton( qtr( "&Close" ), this );
    box->addButton( okButton, QDialogButtonBox::AcceptRole );
    layout->addWidget( box, 2, 2 );

    BUTTONACT( okButton, close() );

    setMinimumSize( 500, 300 );
    readSettings( "Plugins", QSize( 540, 400 ) );
}

inline void PluginDialog::FillTree()
{
    module_t **p_list = module_list_get( NULL );
    module_t *p_module;

    for( unsigned int i = 0; (p_module = p_list[i] ) != NULL; i++ )
    {
        QStringList qs_item;
        qs_item << qfu( module_get_name( p_module, true ) )
                << qfu( module_get_capability( p_module ) )
                << QString::number( module_get_score( p_module ) );
#ifndef DEBUG
        if( qs_item.at(1).isEmpty() ) continue;
#endif

        QTreeWidgetItem *item = new QTreeWidgetItem( qs_item );
        treePlugins->addTopLevelItem( item );
    }
}

void PluginDialog::search( const QString& qs )
{
    QList<QTreeWidgetItem *> items = treePlugins->findItems( qs, Qt::MatchContains );
    items += treePlugins->findItems( qs, Qt::MatchContains, 1 );

    QTreeWidgetItem *item = NULL;
    for( int i = 0; i < treePlugins->topLevelItemCount(); i++ )
    {
        item = treePlugins->topLevelItem( i );
        item->setHidden( !items.contains( item ) );
    }
}

PluginDialog::~PluginDialog()
{
    writeSettings( "Plugins" );
    getSettings()->setValue( "Plugins/Header-State",
                             treePlugins->header()->saveState() );
}

