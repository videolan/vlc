/*****************************************************************************
 * plugins.hpp : Plug-ins and extensions listing
 ****************************************************************************
 * Copyright (C) 2008-2010 the VideoLAN team
 * $Id$
 *
 * Authors: Jean-Baptiste Kempf <jb (at) videolan.org>
 *          Jean-Philippe André <jpeg (at) videolan.org>
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
#include "extensions_manager.hpp"

//#include <vlc_modules.h>

#include <QTreeWidget>
#include <QStringList>
#include <QTabWidget>
#include <QHeaderView>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QLabel>
#include <QVBoxLayout>
#include <QComboBox>
#include <QTextBrowser>
#include <QHBoxLayout>
#include <QSpacerItem>

PluginDialog::PluginDialog( intf_thread_t *_p_intf ) : QVLCFrame( _p_intf )
{
    setWindowTitle( qtr( "Plugins and extensions" ) );
    setWindowRole( "vlc-plugins" );

    QVBoxLayout *layout = new QVBoxLayout( this );
    tabs = new QTabWidget( this );
    tabs->addTab( extensionTab = new ExtensionTab( p_intf ),
                  qtr( "Extensions" ) );
    tabs->addTab( pluginTab = new PluginTab( p_intf ),
                  qtr( "Plugins" ) );
    layout->addWidget( tabs );

    QDialogButtonBox *box = new QDialogButtonBox;
    QPushButton *okButton = new QPushButton( qtr( "&Close" ), this );
    box->addButton( okButton, QDialogButtonBox::AcceptRole );
    layout->addWidget( box );
    BUTTONACT( okButton, close() );
}

PluginDialog::~PluginDialog()
{
}

/* Plugins tab */

PluginTab::PluginTab( intf_thread_t *p_intf )
        : QVLCFrame( p_intf )
{
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
    layout->addWidget( edit, 1, 1, 1, 1 );
    CONNECT( edit, textChanged( const QString& ),
            this, search( const QString& ) );

    setMinimumSize( 500, 300 );
    readSettings( "Plugins", QSize( 540, 400 ) );
}

inline void PluginTab::FillTree()
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

        QTreeWidgetItem *item = new PluginTreeItem( qs_item );
        treePlugins->addTopLevelItem( item );
    }
}

void PluginTab::search( const QString& qs )
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

PluginTab::~PluginTab()
{
    writeSettings( "Plugins" );
    getSettings()->setValue( "Plugins/Header-State",
                             treePlugins->header()->saveState() );
}

bool PluginTreeItem::operator< ( const QTreeWidgetItem & other ) const
{
    int col = treeWidget()->sortColumn();
    if( col == 2 )
        return text( col ).toInt() < other.text( col ).toInt();
    return text( col ) < other.text( col );
}

/* Extensions tab */
ExtensionTab::ExtensionTab( intf_thread_t *p_intf )
        : QVLCFrame( p_intf )
{
    // Layout
    QGridLayout *layout = new QGridLayout( this );

    // Top: combo
    extList = new QComboBox( this );
    layout->addWidget( extList, 0, 0, 1, -1 );

    // Center: Description
    layout->addWidget( new QLabel( "<b>" + qtr( "Version" ) + "</b>" ),
                       1, 0, 1, 1 );
    layout->addWidget( new QLabel( "<b>" + qtr( "Author" ) + "</b>" ),
                       2, 0, 1, 1 );
    layout->addWidget( new QLabel( "<b>" + qtr( "Description" ) + "</b>" ),
                       3, 0, 1, 1 );
    layout->addWidget( new QLabel( "<b>" + qtr( "Website" ) + "</b>" ),
                       6, 0, 1, 1 );
    layout->addWidget( new QLabel( "<b>" + qtr( "File" ) + "</b>" ),
                       7, 0, 1, 1 );

    version = new QLabel( this );
    layout->addWidget( version, 1, 1, 1, 1 );
    author = new QLabel( this );
    layout->addWidget( author, 2, 1, 1, 1 );
    description = new QTextBrowser( this );
    description->setOpenExternalLinks( true );
    layout->addWidget( description, 4, 0, 1, -1 );
    url = new QLabel( this );
    url->setOpenExternalLinks( true );
    url->setTextFormat( Qt::RichText );
    layout->addWidget( url, 6, 1, 1, 1 );
    name = new QLineEdit( this );
    name->setReadOnly( true );
    layout->addWidget( name, 7, 1, 1, 1 );

    // Bottom: Configuration tools
    QHBoxLayout *hbox = new QHBoxLayout;
    QPushButton *reload = new QPushButton( QIcon( ":/update" ),
                                           qtr( "Reload extensions" ),
                                           this );
    QSpacerItem *spacer = new QSpacerItem( 1, 1, QSizePolicy::Expanding,
                                           QSizePolicy::Expanding );
    hbox->addItem( spacer );
    hbox->addWidget( reload );
    BUTTONACT( reload, reloadExtensions() );
    layout->addItem( hbox, 8, 0, 1, -1 );

    // Layout: compact display
    layout->setHorizontalSpacing( 15 );

    fillList();

    CONNECT( extList, currentIndexChanged( int ),
             this, selectionChanged( int ) );
    extList->setCurrentIndex( 0 );
    selectionChanged( 0 );

    // Connect to ExtensionsManager::extensionsUpdated()
    ExtensionsManager* EM = ExtensionsManager::getInstance( p_intf );
    CONNECT( EM, extensionsUpdated(), this, fillList() );
}

ExtensionTab::~ExtensionTab()
{
}

void ExtensionTab::fillList()
{
    ExtensionsManager* EM = ExtensionsManager::getInstance( p_intf );
    if( !EM->isLoaded() )
        EM->loadExtensions();
    extensions_manager_t* p_mgr = EM->getManager();
    if( !p_mgr )
        return;

    // Disconnect signal: we don't want to call selectionChanged now
    disconnect( extList, SIGNAL( currentIndexChanged( int ) ),
                this, SLOT( selectionChanged( int ) ) );

    extList->clear();

    vlc_mutex_lock( &p_mgr->lock );

    extension_t *p_ext;
    FOREACH_ARRAY( p_ext, p_mgr->extensions )
    {
        extList->addItem( p_ext->psz_title, QString( p_ext->psz_name ) );
    }
    FOREACH_END()

    vlc_mutex_unlock( &p_mgr->lock );
    vlc_object_release( p_mgr );

    // Reconnect signal and update screen
    connect( extList, SIGNAL( currentIndexChanged( int ) ),
             this, SLOT( selectionChanged( int ) ) );
    extList->setCurrentIndex( 0 );
    selectionChanged( 0 );
}

void ExtensionTab::selectionChanged( int index )
{
    QString extName = extList->itemData( index ).toString();
    if( extName.isEmpty() )
        return;

    ExtensionsManager* EM = ExtensionsManager::getInstance( p_intf );
    extensions_manager_t* p_mgr = EM->getManager();
    if( !p_mgr )
        return;

    vlc_mutex_lock( &p_mgr->lock );

    const char *psz_name = qtu( extName );

    extension_t *p_ext;
    FOREACH_ARRAY( p_ext, p_mgr->extensions )
    {
        if( !strcmp( p_ext->psz_name, psz_name ) )
        {
            char *psz_url;
            if( p_ext->psz_url != NULL
                && asprintf( &psz_url, "<a href=\"%s\">%s</a>", p_ext->psz_url,
                             p_ext->psz_url ) != -1 )
            {
                url->setText( psz_url );
                free( psz_url );
            }
            else
            {
                url->clear();
            }
            version->setText( qfu( p_ext->psz_version ) );
            description->setHtml( qfu( p_ext->psz_description ) );
            author->setText( qfu( p_ext->psz_author ) );
            name->setText( qfu( p_ext->psz_name ) );
            break;
        }
    }
    FOREACH_END()

    vlc_mutex_unlock( &p_mgr->lock );
    vlc_object_release( p_mgr );
}

void ExtensionTab::reloadExtensions()
{
    ExtensionsManager* EM = ExtensionsManager::getInstance( p_intf );
    EM->reloadExtensions();
    fillList();
}
