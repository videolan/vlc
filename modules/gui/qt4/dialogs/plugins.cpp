/*****************************************************************************
 * plugins.cpp : Plug-ins and extensions listing
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

#include "util/searchlineedit.hpp"
#include "extensions_manager.hpp"

#include <assert.h>

#include <vlc_modules.h>

#include <QTreeWidget>
#include <QStringList>
#include <QTabWidget>
#include <QHeaderView>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QLabel>
#include <QVBoxLayout>
#include <QComboBox>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSpacerItem>
#include <QListView>
#include <QPainter>
#include <QStyleOptionViewItem>
#include <QKeyEvent>
#include <QPushButton>
#include <QPixmap>

static QPixmap *loadPixmapFromData( char *, int size );


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
    box->addButton( okButton, QDialogButtonBox::RejectRole );
    layout->addWidget( box );
    BUTTONACT( okButton, close() );
    restoreWidgetPosition( "PluginsDialog", QSize( 435, 280 ) );
}

PluginDialog::~PluginDialog()
{
    saveWidgetPosition( "PluginsDialog" );
}

/* Plugins tab */

PluginTab::PluginTab( intf_thread_t *p_intf_ )
        : QVLCFrame( p_intf_ )
{
    QGridLayout *layout = new QGridLayout( this );

    /* Main Tree for modules */
    treePlugins = new QTreeWidget;
    layout->addWidget( treePlugins, 0, 0, 1, -1 );

    /* Users cannot move the columns around but we need to sort */
#if QT_VERSION >= 0x050000
    treePlugins->header()->setSectionsMovable( false );
#else
    treePlugins->header()->setMovable( false );
#endif
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
    restoreWidgetPosition( "Plugins", QSize( 540, 400 ) );
}

inline void PluginTab::FillTree()
{
    size_t count;
    module_t **p_list = module_list_get( &count );

    for( unsigned int i = 0; i < count; i++ )
    {
        module_t *p_module = p_list[i];

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
    module_list_free( p_list );
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
    saveWidgetPosition( "Plugins" );
    getSettings()->setValue( "Plugins/Header-State",
                             treePlugins->header()->saveState() );
}

void PluginTab::keyPressEvent( QKeyEvent *keyEvent )
{
    if( keyEvent->key() == Qt::Key_Return ||
        keyEvent->key() == Qt::Key_Enter )
        keyEvent->accept();
    else
        keyEvent->ignore();
}

bool PluginTreeItem::operator< ( const QTreeWidgetItem & other ) const
{
    int col = treeWidget()->sortColumn();
    if( col == PluginTab::SCORE )
        return text( col ).toInt() < other.text( col ).toInt();
    else if ( col == PluginTab::CAPABILITY )
    {
        if ( text( PluginTab::CAPABILITY ) == other.text( PluginTab::CAPABILITY ) )
            return text( PluginTab::NAME ) < other.text( PluginTab::NAME );
        else
            return text( PluginTab::CAPABILITY ) < other.text( PluginTab::CAPABILITY );
    }
    return text( col ) < other.text( col );
}

/* Extensions tab */
ExtensionTab::ExtensionTab( intf_thread_t *p_intf_ )
        : QVLCFrame( p_intf_ )
{
    // Layout
    QVBoxLayout *layout = new QVBoxLayout( this );

    QLabel *notice = new QLabel( qtr("Get more extensions from")
            + QString( " <a href=\"http://addons.videolan.org/\">"
                       "addons.videolan.org</a>." ) );
    notice->setOpenExternalLinks( true );
    layout->addWidget( notice );

    // ListView
    extList = new QListView( this );
    CONNECT( extList, activated( const QModelIndex& ),
             this, moreInformation() );
    layout->addWidget( extList );

    // List item delegate
    ExtensionItemDelegate *itemDelegate = new ExtensionItemDelegate( p_intf,
                                                                     extList );
    extList->setItemDelegate( itemDelegate );

    // Extension list look & feeling
    extList->setAlternatingRowColors( true );
    extList->setSelectionMode( QAbstractItemView::SingleSelection );

    // Model
    ExtensionListModel *model = new ExtensionListModel( extList, p_intf );
    extList->setModel( model );

    // Buttons' layout
    QDialogButtonBox *buttonsBox = new QDialogButtonBox;

    // More information button
    butMoreInfo = new QPushButton( QIcon( ":/menu/info" ),
                                   qtr( "More information..." ),
                                   this );
    CONNECT( butMoreInfo, clicked(), this, moreInformation() );
    buttonsBox->addButton( butMoreInfo, QDialogButtonBox::ActionRole );

    // Reload button
    ExtensionsManager *EM = ExtensionsManager::getInstance( p_intf );
    QPushButton *reload = new QPushButton( QIcon( ":/update" ),
                                           qtr( "Reload extensions" ),
                                           this );
    CONNECT( reload, clicked(), EM, reloadExtensions() );
    CONNECT( reload, clicked(), this, updateButtons() );
    CONNECT( extList->selectionModel(),
             selectionChanged( const QItemSelection &, const QItemSelection & ),
             this,
             updateButtons() );
    buttonsBox->addButton( reload, QDialogButtonBox::ResetRole );

    layout->addWidget( buttonsBox );
    updateButtons();
}

ExtensionTab::~ExtensionTab()
{
}

void ExtensionTab::updateButtons()
{
    butMoreInfo->setEnabled( extList->selectionModel()->hasSelection() );
}

// Do not close on ESC or ENTER
void ExtensionTab::keyPressEvent( QKeyEvent *keyEvent )
{
    if( keyEvent->key() == Qt::Key_Return ||
        keyEvent->key() == Qt::Key_Enter )
        keyEvent->accept();
    else
        keyEvent->ignore();
}

// Show more information
void ExtensionTab::moreInformation()
{
    QModelIndex index = extList->selectionModel()->selectedIndexes().first();

    if( !index.isValid() )
        return;

    ExtensionInfoDialog dlg( index, p_intf, this );
    dlg.exec();
}

/* Safe copy of the extension_t struct */
ExtensionListModel::ExtensionCopy::ExtensionCopy( extension_t *p_ext )
{
    name = qfu( p_ext->psz_name );
    description = qfu( p_ext->psz_description );
    shortdesc = qfu( p_ext->psz_shortdescription );
    if( description.isEmpty() )
        description = shortdesc;
    if( shortdesc.isEmpty() && !description.isEmpty() )
        shortdesc = description;
    title = qfu( p_ext->psz_title );
    author = qfu( p_ext->psz_author );
    version = qfu( p_ext->psz_version );
    url = qfu( p_ext->psz_url );
    icon = loadPixmapFromData( p_ext->p_icondata, p_ext->i_icondata_size );
}

ExtensionListModel::ExtensionCopy::~ExtensionCopy()
{
    delete icon;
}

QVariant ExtensionListModel::ExtensionCopy::data( int role ) const
{
    switch( role )
    {
    case Qt::DisplayRole:
        return title;
    case Qt::DecorationRole:
        if ( !icon ) return QPixmap( ":/logo/vlc48.png" );
        return *icon;
    case DescriptionRole:
        return shortdesc;
    case VersionRole:
        return version;
    case AuthorRole:
        return author;
    case LinkRole:
        return url;
    case NameRole:
        return name;
    default:
        return QVariant();
    }
}

/* Extensions list model for the QListView */

ExtensionListModel::ExtensionListModel( QListView *view, intf_thread_t *intf )
        : QAbstractListModel( view ), p_intf( intf )
{
    // Connect to ExtensionsManager::extensionsUpdated()
    ExtensionsManager* EM = ExtensionsManager::getInstance( p_intf );
    CONNECT( EM, extensionsUpdated(), this, updateList() );

    // Load extensions now if not already loaded
    EM->loadExtensions();
}

ExtensionListModel::~ExtensionListModel()
{
    // Clear extensions list
    while( !extensions.isEmpty() )
        delete extensions.takeLast();
}

void ExtensionListModel::updateList()
{
    ExtensionCopy *ext;

    // Clear extensions list
    while( !extensions.isEmpty() )
    {
        ext = extensions.takeLast();
        delete ext;
    }

    // Find new extensions
    ExtensionsManager *EM = ExtensionsManager::getInstance( p_intf );
    extensions_manager_t *p_mgr = EM->getManager();
    if( !p_mgr )
        return;

    vlc_mutex_lock( &p_mgr->lock );
    extension_t *p_ext;
    FOREACH_ARRAY( p_ext, p_mgr->extensions )
    {
        ext = new ExtensionCopy( p_ext );
        extensions.append( ext );
    }
    FOREACH_END()
    vlc_mutex_unlock( &p_mgr->lock );
    vlc_object_release( p_mgr );

    emit dataChanged( index( 0 ), index( rowCount() - 1 ) );
}

int ExtensionListModel::rowCount( const QModelIndex& ) const
{
    int count = 0;
    ExtensionsManager *EM = ExtensionsManager::getInstance( p_intf );
    extensions_manager_t *p_mgr = EM->getManager();
    if( !p_mgr )
        return 0;

    vlc_mutex_lock( &p_mgr->lock );
    count = p_mgr->extensions.i_size;
    vlc_mutex_unlock( &p_mgr->lock );
    vlc_object_release( p_mgr );

    return count;
}

QVariant ExtensionListModel::data( const QModelIndex& index, int role ) const
{
    if( !index.isValid() )
        return QVariant();

    ExtensionCopy * extension =
            static_cast<ExtensionCopy *>(index.internalPointer());

    return extension->data( role );
}

QModelIndex ExtensionListModel::index( int row, int column,
                                       const QModelIndex& ) const
{
    if( column != 0 )
        return QModelIndex();
    if( row < 0 || row >= extensions.count() )
        return QModelIndex();

    return createIndex( row, 0, extensions.at( row ) );
}


/* Extension List Widget Item */
ExtensionItemDelegate::ExtensionItemDelegate( intf_thread_t *p_intf,
                                              QListView *view )
        : QStyledItemDelegate( view ), view( view ), p_intf( p_intf )
{
}

ExtensionItemDelegate::~ExtensionItemDelegate()
{
}

void ExtensionItemDelegate::paint( QPainter *painter,
                                   const QStyleOptionViewItem &option,
                                   const QModelIndex &index ) const
{
    int width = option.rect.width();

    // Pixmap: buffer where to draw
    QPixmap pix(option.rect.size());

    // Draw background
    pix.fill( Qt::transparent ); // FIXME

    // ItemView primitive style
    QApplication::style()->drawPrimitive( QStyle::PE_PanelItemViewItem,
                                          &option,
                                          painter );

    // Painter on the pixmap
    QPainter *pixpaint = new QPainter(&pix);

    // Text font & pen
    QFont font = painter->font();
    QPen pen = painter->pen();
    if( view->selectionModel()->selectedIndexes().contains( index ) )
    {
        pen.setBrush( option.palette.highlightedText() );
    }
    else
    {
        pen.setBrush( option.palette.text() );
    }
    pixpaint->setPen( pen );
    QFontMetrics metrics = option.fontMetrics;

    // Icon
    QPixmap icon = index.data( Qt::DecorationRole ).value<QPixmap>();
    if( !icon.isNull() )
    {
        pixpaint->drawPixmap( 7, 7, 2*metrics.height(), 2*metrics.height(),
                              icon );
    }

    // Title: bold
    pixpaint->setRenderHint( QPainter::TextAntialiasing );
    font.setBold( true );
    pixpaint->setFont( font );
    pixpaint->drawText( QRect( 17 + 2 * metrics.height(), 7,
                               width - 40 - 2 * metrics.height(),
                               metrics.height() ),
                        Qt::AlignLeft, index.data( Qt::DisplayRole ).toString() );

    // Short description: normal
    font.setBold( false );
    pixpaint->setFont( font );
    pixpaint->drawText( QRect( 17 + 2 * metrics.height(),
                               7 + metrics.height(), width - 40,
                               metrics.height() ),
                        Qt::AlignLeft, index.data( ExtensionListModel::DescriptionRole ).toString() );

    // Flush paint operations
    delete pixpaint;

    // Draw it on the screen!
    painter->drawPixmap( option.rect, pix );
}

QSize ExtensionItemDelegate::sizeHint( const QStyleOptionViewItem &option,
                                       const QModelIndex &index ) const
{
    if (index.isValid() && index.column() == 0)
    {
        QFontMetrics metrics = option.fontMetrics;
        return QSize( 200, 14 + 2 * metrics.height() );
    }
    else
        return QSize();
}

/* "More information" dialog */

ExtensionInfoDialog::ExtensionInfoDialog( const QModelIndex &index,
                                          intf_thread_t *p_intf,
                                          QWidget *parent )
       : QVLCDialog( parent, p_intf )
{
    // Let's be a modal dialog
    setWindowModality( Qt::WindowModal );

    // Window title
    setWindowTitle( qtr( "About" ) + " " + index.data(Qt::DisplayRole).toString() );

    // Layout
    QGridLayout *layout = new QGridLayout( this );

    // Icon
    QLabel *icon = new QLabel( this );
    QPixmap pix = index.data(Qt::DecorationRole).value<QPixmap>();
    Q_ASSERT( !pix.isNull() );
    icon->setPixmap( pix );
    icon->setAlignment( Qt::AlignCenter );
    icon->setFixedSize( 48, 48 );
    layout->addWidget( icon, 1, 0, 2, 1 );

    // Title
    QLabel *label = new QLabel( index.data(Qt::DisplayRole).toString(), this );
    QFont font = label->font();
    font.setBold( true );
    font.setPointSizeF( font.pointSizeF() * 1.3f );
    label->setFont( font );
    layout->addWidget( label, 0, 0, 1, -1 );

    // Version
    label = new QLabel( "<b>" + qtr( "Version" ) + ":</b>", this );
    layout->addWidget( label, 1, 1, 1, 1, Qt::AlignBottom );
    label = new QLabel( index.data(ExtensionListModel::VersionRole).toString(), this );
    layout->addWidget( label, 1, 2, 1, 2, Qt::AlignBottom );

    // Author
    label = new QLabel( "<b>" + qtr( "Author" ) + ":</b>", this );
    layout->addWidget( label, 2, 1, 1, 1, Qt::AlignTop );
    label = new QLabel( index.data(ExtensionListModel::AuthorRole).toString(), this );
    layout->addWidget( label, 2, 2, 1, 2, Qt::AlignTop );


    // Description
    label = new QLabel( this );
    label->setText( index.data(ExtensionListModel::DescriptionRole).toString() );
    label->setWordWrap( true );
    label->setOpenExternalLinks( true );
    layout->addWidget( label, 4, 0, 1, -1 );

    // URL
    label = new QLabel( "<b>" + qtr( "Website" ) + ":</b>", this );
    layout->addWidget( label, 5, 0, 1, 2 );
    label = new QLabel( QString("<a href=\"%1\">%2</a>")
                        .arg( index.data(ExtensionListModel::LinkRole).toString() )
                        .arg( index.data(ExtensionListModel::LinkRole).toString() )
                        , this );
    label->setOpenExternalLinks( true );
    layout->addWidget( label, 5, 2, 1, -1 );

    // Script file
    label = new QLabel( "<b>" + qtr( "File" ) + ":</b>", this );
    layout->addWidget( label, 6, 0, 1, 2 );
    QLineEdit *line =
            new QLineEdit( index.data(ExtensionListModel::NameRole).toString(), this );
    line->setReadOnly( true );
    layout->addWidget( line, 6, 2, 1, -1 );

    // Close button
    QDialogButtonBox *group = new QDialogButtonBox( this );
    QPushButton *closeButton = new QPushButton( qtr( "&Close" ) );
    group->addButton( closeButton, QDialogButtonBox::RejectRole );
    BUTTONACT( closeButton, close() );

    layout->addWidget( group, 7, 0, 1, -1 );

    // Fix layout
    layout->setColumnStretch( 2, 1 );
    layout->setRowStretch( 4, 1 );
    setMinimumSize( 450, 350 );
}

static QPixmap *loadPixmapFromData( char *data, int size )
{
    if( !data || size <= 0 )
        return NULL;
    QPixmap *pixmap = new QPixmap();
    if( !pixmap->loadFromData( (const uchar*) data, (uint) size ) )
    {
        delete pixmap;
        return NULL;
    }
    return pixmap;
}
