/*****************************************************************************
 * plugins.cpp : Plug-ins and extensions listing
 ****************************************************************************
 * Copyright (C) 2008-2010 the VideoLAN team
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

#include "widgets/native/searchlineedit.hpp"
#include "dialogs/extensions/extensions_manager.hpp"
#include "network/addonsmodel.hpp"
#include "widgets/native/animators.hpp"
#include "util/imagehelper.hpp"
#include "util/colorizedsvgicon.hpp"

#include <cassert>

#include <vlc_modules.h>
#include <vlc_extensions.h>

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
#include <QSpacerItem>
#include <QListView>
#include <QPainter>
#include <QStyleOptionViewItem>
#include <QKeyEvent>
#include <QPushButton>
#include <QCheckBox>
#include <QPixmap>
#include <QStylePainter>
#include <QProgressBar>
#include <QTextEdit>
#include <QUrl>
#include <QMimeData>
#include <QSplitter>
#include <QToolButton>
#include <QStackedWidget>
#include <QPainterPath>
#include <QSignalMapper>
#include <QtQml/QQmlFile>

//match the image source (width/height)
#define SCORE_ICON_WIDTH_SCALE 4
#define SPINNER_SIZE 32

static QPixmap *loadPixmapFromData( char *, int size );


PluginDialog::PluginDialog( qt_intf_t *_p_intf ) : QVLCFrame( _p_intf )
{
    setWindowTitle( qtr( "Plugins and extensions" ) );
    setWindowRole( "vlc-plugins" );

    QVBoxLayout *layout = new QVBoxLayout( this );
    tabs = new QTabWidget( this );
    tabs->addTab( addonsTab = new AddonsTab( p_intf ),
                  qtr( "Addons Manager" ) );
    tabs->addTab( extensionTab = new ExtensionTab( p_intf ),
                  qtr( "Active Extensions" ) );
    tabs->addTab( pluginTab = new PluginTab( p_intf ),
                  qtr( "Plugins" ) );
    layout->addWidget( tabs );

    QDialogButtonBox *box = new QDialogButtonBox;
    QPushButton *okButton = new QPushButton( qtr( "&Close" ), this );
    box->addButton( okButton, QDialogButtonBox::RejectRole );
    layout->addWidget( box );
    BUTTONACT( okButton, &PluginDialog::close );
    restoreWidgetPosition( "PluginsDialog", QSize( 435, 280 ) );
}

PluginDialog::~PluginDialog()
{
    delete pluginTab;
    delete extensionTab;
    delete addonsTab;
    saveWidgetPosition( "PluginsDialog" );
}

/* Plugins tab */

PluginTab::PluginTab( qt_intf_t *p_intf_ )
        : QVLCFrame( p_intf_ )
{
    QGridLayout *layout = new QGridLayout( this );

    /* Main Tree for modules */
    treePlugins = new QTreeWidget;
    layout->addWidget( treePlugins, 0, 0, 1, -1 );

    /* Users cannot move the columns around but we need to sort */
    treePlugins->header()->setSectionsMovable( false );
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
    connect( edit, &SearchLineEdit::textChanged, this, &PluginTab::search );

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
        qs_item << qfu( module_GetLongName( p_module ) )
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
ExtensionTab::ExtensionTab( qt_intf_t *p_intf_ )
        : QVLCFrame( p_intf_ )
{
    // Layout
    QVBoxLayout *layout = new QVBoxLayout( this );

    // ListView
    extList = new QListView( this );
    connect( extList, &QListView::activated, this, &ExtensionTab::moreInformation );
    layout->addWidget( extList );

    // List item delegate
    ExtensionItemDelegate *itemDelegate = new ExtensionItemDelegate( extList );
    extList->setItemDelegate( itemDelegate );

    // Extension list look & feeling
    extList->setAlternatingRowColors( true );
    extList->setSelectionMode( QAbstractItemView::SingleSelection );

    // Model
    ExtensionListModel *model =
      new ExtensionListModel( extList, ExtensionsManager::getInstance( p_intf ) );
    extList->setModel( model );

    // Buttons' layout
    QDialogButtonBox *buttonsBox = new QDialogButtonBox;

    // More information button
    butMoreInfo = new QPushButton( QIcon( ":/menu/info.svg" ),
                                   qtr( "More information..." ),
                                   this );
    connect( butMoreInfo, &QPushButton::clicked, this, &ExtensionTab::moreInformation );
    buttonsBox->addButton( butMoreInfo, QDialogButtonBox::ActionRole );

    // Reload button
    ExtensionsManager *EM = ExtensionsManager::getInstance( p_intf );
    QPushButton *reload = new QPushButton( QIcon( ":/menu/update.svg" ),
                                           qtr( "Reload extensions" ),
                                           this );
    connect( reload, &QPushButton::clicked, [this, EM](){
        extList->clearSelection();
        EM->reloadExtensions();
    });
    connect( reload, &QPushButton::clicked, this, &ExtensionTab::updateButtons );
    connect( extList->selectionModel(), &QItemSelectionModel::selectionChanged,
             this, &ExtensionTab::updateButtons );
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

    QWidget* windowWidget = window();
    QWindow* parentWindow = windowWidget ? windowWidget->windowHandle() : nullptr;
    ExtensionInfoDialog dlg( index, p_intf, parentWindow );
    dlg.exec();
}

/* Add-ons tab */
AddonsTab::AddonsTab( qt_intf_t *p_intf_ ) : QVLCFrame( p_intf_ )
{
    b_localdone = false;
    QSplitter *splitter = new QSplitter( this );
    setLayout( new QHBoxLayout() );
    layout()->addWidget( splitter );

    QWidget *leftPane = new QWidget();
    splitter->addWidget( leftPane );
    leftPane->setLayout( new QVBoxLayout() );

    QWidget *rightPane = new QWidget();
    splitter->addWidget( rightPane );

    splitter->setCollapsible( 0, false );
    splitter->setCollapsible( 1, false );
    splitter->setSizeIncrement( 32, 1 );

    // Layout
    QVBoxLayout *layout = new QVBoxLayout( rightPane );

    // Left Pane
    leftPane->layout()->setContentsMargins(0, 0, 0, 0);
    leftPane->layout()->setSpacing(0);

    SearchLineEdit *searchInput = new SearchLineEdit();
    leftPane->layout()->addWidget( searchInput );
    leftPane->layout()->addItem( new QSpacerItem( 0, 10 ) );

    signalMapper = new QSignalMapper();

    auto addCategory = [this, leftPane]( const QString& label, const QString& ltooltip,  AddonsModel::Type type) {
        auto button = new QToolButton( this );

        QString iconpath = QQmlFile::urlToLocalFileOrQrc(AddonsModel::getIconForType( type ));
        button->setIcon( QIcon{ iconpath } );
        button->setText( label );
        button->setToolTip( ltooltip );
        button->setToolButtonStyle( Qt::ToolButtonTextBesideIcon );
        button->setIconSize( QSize( 32, 32 ) );
        button->setSizePolicy(QSizePolicy::MinimumExpanding,QSizePolicy::Maximum) ;
        button->setMinimumSize( 32, 32 );
        button->setAutoRaise( true );
        button->setCheckable( true );
        if ( type == AddonsModel::Type::TYPE_NONE )
            button->setChecked( true );
        button->setAutoExclusive( true );
        connect( button, &QToolButton::clicked, signalMapper, QOverload<>::of(&QSignalMapper::map) );
        signalMapper->setMapping( button, static_cast<int>(type) );
        leftPane->layout()->addWidget( button );
    };

    addCategory( qtr("All"), qtr("Interface Settings"),
                  AddonsModel::Type::TYPE_NONE );
    addCategory( qtr("Skins"),
                  qtr( "Skins customize player's appearance."
                       " You can activate them through preferences." ),
                  AddonsModel::Type::TYPE_SKIN2 );
    addCategory( qtr("Playlist parsers"),
                  qtr( "Playlist parsers add new capabilities to read"
                       " internet streams or extract meta data." ),
                  AddonsModel::Type::TYPE_PLAYLIST_PARSER );
    addCategory( qtr("Service Discovery"),
                  qtr( "Service discoveries adds new sources to your playlist"
                       " such as web radios, video websites, ..." ),
                  AddonsModel::Type::TYPE_SERVICE_DISCOVERY );
    addCategory( qtr("Interfaces"),
                  "",
                  AddonsModel::Type::TYPE_INTERFACE );
    addCategory( qtr("Art and meta fetchers"),
                  qtr( "Retrieves extra info and art for playlist items" ),
                  AddonsModel::Type::TYPE_META );
    addCategory( qtr("Extensions"),
                  qtr( "Extensions brings various enhancements."
                       " Check descriptions for more details" ),
                  AddonsModel::Type::TYPE_EXTENSION );

    // Right Pane
    rightPane->layout()->setContentsMargins(0, 0, 0, 0);
    rightPane->layout()->setSpacing(0);

    // Splitter sizes init
    QList<int> sizes;
    int width = leftPane->sizeHint().width();
    sizes << width << size().width() - width;
    splitter->setSizes( sizes );

    // Filters
    leftPane->layout()->addItem( new QSpacerItem( 0, 30 ) );

    QStackedWidget *switchStack = new QStackedWidget();
    switchStack->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Maximum );
    leftPane->layout()->addWidget( switchStack );

    QCheckBox *installedOnlyBox = new QCheckBox( qtr("Only installed") );
    installedOnlyBox->setSizePolicy( QSizePolicy::Ignored, QSizePolicy::Preferred );
    switchStack->insertWidget( WITHONLINEADDONS, installedOnlyBox );
    connect( installedOnlyBox, &QtCheckboxChanged, this, &AddonsTab::installChecked );

    // Model
    m_model = std::make_unique<AddonsModel>( );
    //model expect QMLlike behavior
    m_model->classBegin();
    m_model->setCtx(p_intf->p_mi);
    connect( signalMapper, &QSignalMapper::mappedInt,
            m_model.get(), [model = m_model.get()](int mapped){
                model->setTypeFilter(static_cast<AddonsModel::Type>(mapped));
            });

    connect( searchInput, &SearchLineEdit::textChanged,
            m_model.get(), &AddonsModel::setSearchPattern );


    // Update button
    QPushButton *reposyncButton = new QPushButton( QIcon( ":/menu/update.svg" ),
                                              qtr("Find more addons online") );
    reposyncButton->setSizePolicy( QSizePolicy::Ignored, QSizePolicy::Preferred );
    switchStack->insertWidget( ONLYLOCALADDONS, reposyncButton );
    switchStack->setCurrentIndex( ONLYLOCALADDONS );
    connect( reposyncButton, &QPushButton::clicked, this, &AddonsTab::reposync );
    connect(
        m_model.get(), &AddonsModel::loadingChanged,
        this, [this]() {
            if ( !m_model->loading()) {
                spinnerAnimation->stop();
                addonsView->viewport()->update();
            }
        });

    leftPane->layout()->addItem( new QSpacerItem( 0, 0, QSizePolicy::Maximum, QSizePolicy::Expanding ) );

    // Main View

    // ListView
    addonsView = new QListView( this );
    addonsView->setModel(m_model.get());

    connect( addonsView, &QListView::activated, this, &AddonsTab::moreInformation );
    layout->addWidget( addonsView );

    // List item delegate
    AddonItemDelegate *addonsDelegate = new AddonItemDelegate( addonsView );
    addonsView->setItemDelegate( addonsDelegate );
    addonsDelegate->setAnimator( new DelegateAnimationHelper( addonsView ) );
    connect( addonsDelegate, &AddonItemDelegate::showInfo, this, &AddonsTab::moreInformation );

    // Extension list look & feeling
    addonsView->setAlternatingRowColors( true );
    addonsView->setSelectionMode( QAbstractItemView::SingleSelection );

    // Drop packages
    addonsView->setAcceptDrops( true );
    addonsView->setDefaultDropAction( Qt::CopyAction );
    addonsView->setDropIndicatorShown( true );
    addonsView->setDragDropMode( QAbstractItemView::DropOnly );

    connect( addonsView->selectionModel(), &QItemSelectionModel::currentChanged,
             addonsView, QOverload<const QModelIndex&>::of(&QListView::edit) );


    QList<QString> frames;
    frames << ":/misc/wait1.svg";
    frames << ":/misc/wait2.svg";
    frames << ":/misc/wait3.svg";
    frames << ":/misc/wait4.svg";
    spinnerAnimation = new PixmapAnimator( this, std::move(frames), SPINNER_SIZE, SPINNER_SIZE );
    connect( spinnerAnimation, &PixmapAnimator::pixmapReady,
             addonsView->viewport(), QOverload<>::of(&QWidget::update) );
    addonsView->viewport()->installEventFilter( this );

    //model expect QMLlike behavior
    m_model->componentComplete();
}

AddonsTab::~AddonsTab()
{
    delete spinnerAnimation;
    delete signalMapper;
}

// Do not close on ESC or ENTER
void AddonsTab::keyPressEvent( QKeyEvent *keyEvent )
{
    if( keyEvent->key() == Qt::Key_Return ||
        keyEvent->key() == Qt::Key_Enter )
        keyEvent->accept();
    else
        keyEvent->ignore();
}

bool AddonsTab::eventFilter( QObject *obj, QEvent *event )
{
    if ( obj != addonsView->viewport() )
        return false;

    switch( event->type() )
    {
    case QEvent::Paint:
        if ( spinnerAnimation->state() == PixmapAnimator::Running )
        {
            QWidget *viewport = qobject_cast<QWidget *>( obj );
            if ( !viewport ) break;
            QStylePainter painter( viewport );
            const QPixmap& spinner = spinnerAnimation->getPixmap();
            QPoint point = viewport->geometry().center();
            point -= QPoint( spinner.width() / 2, spinner.height() / 2 );
            painter.drawPixmap( point, spinner );
            QString text = qtr("Retrieving addons...");
            QSize textsize = fontMetrics().size( 0, text );
            point = viewport->geometry().center();
            point -= QPoint( textsize.width() / 2, -spinner.height() );
            painter.drawText( point, text );
        }
        else if ( m_model->rowCount() == 0 )
        {
            QWidget *viewport = qobject_cast<QWidget *>( obj );
            if ( !viewport ) break;
            QStylePainter painter( viewport );
            QString text = qtr("No addons found");
            QSize size = fontMetrics().size( 0, text );
            QPoint point = viewport->geometry().center();
            point -= QPoint( size.width() / 2, size.height() / 2 );
            painter.drawText( point, text );
        }
        break;
    case QEvent::DragEnter:
    {
        QDragEnterEvent *dragEvent = static_cast<QDragEnterEvent *>(event);
        if ( !dragEvent ) break;
        QList<QUrl> urls = dragEvent->mimeData()->urls();
        if ( dragEvent->proposedAction() != Qt::CopyAction
          || urls.count() != 1
          || urls.first().scheme() != "file"
          || ! urls.first().path().endsWith(".vlp") )
            return false;
        dragEvent->acceptProposedAction();
        return true;
    }
    case QEvent::DragMove:
    {
        QDragMoveEvent *moveEvent = static_cast<QDragMoveEvent *>(event);
        if ( !moveEvent ) break;
        if ( moveEvent->proposedAction() != Qt::CopyAction )
            return false;
        moveEvent->acceptProposedAction();
        return true;
    }
    case QEvent::Drop:
    {
        QDropEvent *dropEvent = static_cast<QDropEvent *>(event);
        if ( !dropEvent ) break;
        if ( dropEvent->proposedAction() != Qt::CopyAction )
            return false;
        if ( dropEvent->mimeData()->urls().count() )
        {
            m_model->loadFromExternalRepository(dropEvent->mimeData()->urls().first());
            dropEvent->acceptProposedAction();
        }
        return true;
    }
    default:
        break;
    }
    return false;
}

void AddonsTab::moreInformation()
{
    QModelIndex index = addonsView->selectionModel()->selectedIndexes().first();
    if( !index.isValid() ) return;
    QWidget* windowWidget = window();
    QWindow* parentWindow = windowWidget ? windowWidget->windowHandle() : nullptr;
    AddonInfoDialog dlg( index, p_intf, parentWindow );
    dlg.exec();
}

void AddonsTab::installChecked( int i )
{
    if ( i == Qt::Checked )
        m_model->setStateFilter( AddonsModel::State::STATE_INSTALLED );
    else
        m_model->setStateFilter( AddonsModel::State::STATE_NONE );
}

void AddonsTab::reposync()
{
    QStackedWidget *tab = qobject_cast<QStackedWidget *>(sender()->parent());
    if ( tab )
    {
        tab->setCurrentIndex( WITHONLINEADDONS );

        spinnerAnimation->start();

        m_model->loadFromDefaultRepository();
    }
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
    case SummaryRole:
        return shortdesc;
    case VersionRole:
        return version;
    case AuthorRole:
        return author;
    case LinkRole:
        return url;
    case FilenameRole:
        return name;
    default:
        return QVariant();
    }
}

/* Extensions list model for the QListView */
ExtensionListModel::ExtensionListModel( QObject *parent )
    : QAbstractListModel( parent ), EM( NULL )
{

}

ExtensionListModel::ExtensionListModel( QObject *parent, ExtensionsManager* EM_ )
        : QAbstractListModel( parent ), EM( EM_ )
{
    // Connect to ExtensionsManager::extensionsUpdated()
    connect( EM, &ExtensionsManager::extensionsUpdated, this, &ExtensionListModel::updateList );

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
    extensions_manager_t *p_mgr = EM->getManager();
    if( !p_mgr )
        return;

    vlc_mutex_lock( &p_mgr->lock );
    extension_t *p_ext;
    ARRAY_FOREACH( p_ext, p_mgr->extensions )
    {
        ext = new ExtensionCopy( p_ext );
        extensions.append( ext );
    }
    vlc_mutex_unlock( &p_mgr->lock );

    emit dataChanged( index( 0 ), index( rowCount() - 1 ) );
}

int ExtensionListModel::rowCount( const QModelIndex& ) const
{
    int count = 0;
    extensions_manager_t *p_mgr = EM->getManager();
    if( !p_mgr )
        return 0;

    vlc_mutex_lock( &p_mgr->lock );
    count = p_mgr->extensions.i_size;
    vlc_mutex_unlock( &p_mgr->lock );

    return count;
}

QVariant ExtensionListModel::data( const QModelIndex& index, int role ) const
{
    if( !index.isValid() )
        return QVariant();

    ExtensionCopy * extension =
            static_cast<ExtensionCopy *>(index.internalPointer());
    if ( !extension )
        return QVariant();
    else
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

static void commonPaint(
    QPainter *painter,
    const QStyleOptionViewItem &option,
    const QMargins& margins,
    const QPixmap& icon,
    const QString& name,
    const QString& description
    )
{
    QStyleOptionViewItem opt = option;

    // Draw background
    if ( opt.state & QStyle::State_Selected )
        painter->fillRect( opt.rect, opt.palette.highlight() );

    // Icon
    if( !icon.isNull() )
    {
        painter->drawPixmap( opt.rect.left() + margins.left(),
                             opt.rect.top() + margins.top(),
                             icon.scaled( opt.decorationSize,
                                          Qt::KeepAspectRatio,
                                          Qt::SmoothTransformation )
        );
    }

    painter->save();
    painter->setRenderHint( QPainter::TextAntialiasing );

    if ( opt.state & QStyle::State_Selected )
        painter->setPen( opt.palette.highlightedText().color() );

    QFont font( option.font );
    font.setBold( true );
    painter->setFont( font );
    QRect textrect( opt.rect );
    textrect.adjust( 2 * margins.left() + margins.right() + opt.decorationSize.width(),
                     margins.top(),
                     - margins.right(),
                     - margins.bottom() - opt.fontMetrics.height() );

    painter->drawText( textrect, Qt::AlignLeft, name);

    font.setBold( false );
    painter->setFont( font );
    painter->drawText( textrect.translated( 0, option.fontMetrics.height() ),
                       Qt::AlignLeft,
                       description );

    painter->restore();
}

/* Extension List Widget Item */
ExtensionItemDelegate::ExtensionItemDelegate( QObject *parent )
        : QStyledItemDelegate( parent )
{
    margins = QMargins( 4, 4, 4, 4 );
}

ExtensionItemDelegate::~ExtensionItemDelegate()
{
}

void ExtensionItemDelegate::paint( QPainter *painter,
                                   const QStyleOptionViewItem &option,
                                   const QModelIndex &index ) const
{
    QStyleOptionViewItem opt = option;
    initStyleOption( &opt, index );

    /* Draw common base  */
    QString name = index.data( Qt::DisplayRole ).toString();
    QPixmap icon = index.data( Qt::DecorationRole ).value<QPixmap>();
    QString description = index.data( ExtensionListModel::SummaryRole ).toString();

    commonPaint(
        painter,
        option,
        margins,
        icon,
        name,
        description
        );
}

QSize ExtensionItemDelegate::sizeHint( const QStyleOptionViewItem &option,
                                       const QModelIndex &index ) const
{
    if ( index.isValid() )
    {
        return QSize( 200, 2 * option.fontMetrics.height()
                      + margins.top() + margins.bottom() );
    }
    else
        return QSize();
}

void ExtensionItemDelegate::initStyleOption( QStyleOptionViewItem *option,
                                             const QModelIndex &index ) const
{
    QStyledItemDelegate::initStyleOption( option, index );
    option->decorationSize = QSize( option->rect.height(), option->rect.height() );
    option->decorationSize -= QSize( margins.left() + margins.right(),
                                     margins.top() + margins.bottom() );
}

AddonItemDelegate::AddonItemDelegate( QObject *parent )
    : QStyledItemDelegate( parent )
    , margins( 4,4,4,4 )
{ }

AddonItemDelegate::~AddonItemDelegate()
{
    delete progressbar;
}

void AddonItemDelegate::paint( QPainter *painter,
                               const QStyleOptionViewItem &option,
                               const QModelIndex &index ) const
{
    QStyleOptionViewItem newopt = option;
    auto i_state = index.data( AddonsModel::Role::STATE ).value<AddonsModel::State>();
    auto i_type = index.data( AddonsModel::Role::TYPE ).value<AddonsModel::Type>();

    /* Draw Background gradient by addon type */
    QColor backgroundColor = AddonsModel::getColorForType( i_type );

    if ( backgroundColor.isValid() )
    {
        painter->save();
        int i_corner = qMin( (int)(option.rect.width() * .05), 30 );
        QLinearGradient gradient(
                    QPoint( option.rect.right() - i_corner, option.rect.bottom() - i_corner ),
                    option.rect.bottomRight() );
        gradient.setColorAt( 0, Qt::transparent );
        gradient.setColorAt( 1.0, backgroundColor );
        painter->fillRect( option.rect, gradient );
        painter->restore();
    }

    /* Draw base info from parent */
    initStyleOption( &newopt, index );

    /* Draw common base  */
    QString name = index.data( Qt::DisplayRole ).toString();
    QPixmap icon = index.data( Qt::DecorationRole ).value<QPixmap>();
    QString description = index.data( AddonsModel::Role::SUMMARY ).toString();

    commonPaint(
        painter,
        newopt,
        margins,
        icon,
        name,
        description
        );

    painter->save();
    painter->setRenderHint( QPainter::TextAntialiasing );

    /* Addon status */
    if ( i_state == AddonsModel::State::STATE_INSTALLED )
    {
        painter->save();
        painter->setRenderHint( QPainter::Antialiasing );
        QMargins statusMargins( 5, 2, 5, 2 );
        QFont font( newopt.font );
        font.setBold( true );
        QFontMetrics metrics( font );
        painter->setFont( font );
        QRect statusRect = metrics.boundingRect( qtr("Installed") );
        statusRect.translate( newopt.rect.width() - statusRect.width(),
                              newopt.rect.top() + statusRect.height() );
        statusRect.adjust( - statusMargins.left() - statusMargins.right(),
                           0, 0,
                           statusMargins.top() + statusMargins.bottom() );
        QPainterPath path;
        path.addRoundedRect( statusRect, 2.0, 2.0 );
        painter->fillPath( path, QColor( Qt::green ).darker( 125 ) );
        painter->setPen( Qt::white );
        painter->drawText(
            statusRect.adjusted( statusMargins.left(), statusMargins.top(),
                                 -statusMargins.right(), -statusMargins.bottom() ),
            qtr("Installed") );
        painter->restore();
    }

    if ( newopt.state & QStyle::State_Selected )
        painter->setPen( newopt.palette.highlightedText().color() );

    /* Start below text */
    QRect textrect( newopt.rect );
    textrect.adjust( 2 * margins.left() + margins.right() + newopt.decorationSize.width(),
                     margins.top(),
                     - margins.right(),
                     - margins.bottom() - newopt.fontMetrics.height() );
    textrect.translate( 0, newopt.fontMetrics.height() * 2 );

    /* Version */
    QString version = index.data( AddonsModel::Role::ADDON_VERSION).toString();
    if ( !version.isEmpty() )
        painter->drawText( textrect, Qt::AlignLeft, qtr("Version %1").arg( version ) );

    textrect.translate( 0, newopt.fontMetrics.height() );

    /* Score */
    int i_score = index.data( AddonsModel::Role::SCORE).toInt();
    QPixmap scoreicon;
    if ( i_score )
    {
        int i_scoreicon_height = newopt.fontMetrics.height();
        int i_scoreicon_width = i_scoreicon_height * SCORE_ICON_WIDTH_SCALE;
        scoreicon = ImageHelper::loadSvgToPixmap( ":/addons/addon_score.svg",
                    i_scoreicon_width, i_scoreicon_height );
        int i_width = ( (float) i_score / AddonsModel::getMaxScore() ) * i_scoreicon_width;
        /* Erase the end (value) of our pixmap with a shadow */
        QPainter erasepainter( &scoreicon );
        erasepainter.setCompositionMode( QPainter::CompositionMode_SourceIn );
        erasepainter.fillRect( QRect( i_width, 0,
                                      scoreicon.width() - i_width, scoreicon.height() ),
                               newopt.palette.color( QPalette::Dark ) );
        erasepainter.end();
        painter->drawPixmap( textrect.topLeft(), scoreicon );
    }

    /* Downloads # */
    int i_downloads = index.data( AddonsModel::DOWNLOAD_COUNT).toInt();
    if ( i_downloads )
        painter->drawText( textrect.translated( scoreicon.width() + margins.left(), 0 ),
                           Qt::AlignLeft, qtr("%1 downloads").arg( i_downloads ) );

    painter->restore();

    if ( animator )
    {
        if ( animator->isRunning() && animator->getIndex() == index )
        {
            if ( i_state != AddonsModel::State::STATE_INSTALLING
                && i_state != AddonsModel::State::STATE_UNINSTALLING )
                animator->run( false );
        }
        /* Create our installation progress overlay */

        if ( i_state == AddonsModel::State::STATE_INSTALLING
            || i_state == AddonsModel::State::STATE_UNINSTALLING )
        {
            painter->save();
            painter->setCompositionMode( QPainter::CompositionMode_SourceOver );
            painter->fillRect( newopt.rect, QColor( 255, 255, 255, 128 ) );
            if ( animator && index.isValid() )
            {
                animator->setIndex( index );
                animator->run( true );
                QSize adjustment = newopt.rect.size() / 4;
                progressbar->setGeometry(
                    newopt.rect.adjusted( adjustment.width(), adjustment.height(),
                                          -adjustment.width(), -adjustment.height() ) );
                painter->drawPixmap( newopt.rect.left() + adjustment.width(),
                                     newopt.rect.top() + adjustment.height(),
                                     progressbar->grab() );
            }
            painter->restore();
        }
    }
}

QSize AddonItemDelegate::sizeHint( const QStyleOptionViewItem &option,
                                   const QModelIndex &index ) const
{
    if ( index.isValid() )
    {
        return QSize( 200, 4 * option.fontMetrics.height()
                      + margins.top() + margins.bottom() );
    }
    else
        return QSize();
}

void AddonItemDelegate::initStyleOption( QStyleOptionViewItem *option,
                                            const QModelIndex &index ) const
{
    QStyledItemDelegate::initStyleOption( option, index );
    option->decorationSize = QSize( option->rect.height(), option->rect.height() );
    option->decorationSize -= QSize( margins.left() + margins.right(),
                                    margins.top() + margins.bottom() );
}

QWidget *AddonItemDelegate::createEditor( QWidget *parent,
                                          const QStyleOptionViewItem &option,
                                          const QModelIndex &index) const
{
    Q_UNUSED( option );
    QWidget *editorWidget = new QWidget( parent );
    QPushButton *installButton;
    QPushButton *infoButton;

    editorWidget->setLayout( new QHBoxLayout() );
    editorWidget->layout()->setContentsMargins(0, 0, 0, 0);

    infoButton = new QPushButton( QIcon( ":/menu/info.svg" ),
                                  qtr( "More information..." ) );
    connect( infoButton, &QPushButton::clicked, this, &AddonItemDelegate::showInfo );
    editorWidget->layout()->addWidget( infoButton );

    if ( index.data( AddonsModel::Role::MANAGEABLE).toBool() )
    {
        if ( index.data( AddonsModel::Role::STATE ).value<AddonsModel::State>()
            == AddonsModel::State::STATE_INSTALLED )
            installButton = new QPushButton( QIcon( ":/menu/remove.svg" ),
                                             qtr("&Uninstall"), parent );
        else
        {
            installButton = new QPushButton( qtr("&Install"), parent );
            installButton->setIcon( ColorizedSvgIcon::colorizedIconForWidget( ":/menu/add.svg", installButton ) );
        }
        connect( installButton, &QPushButton::clicked, this, &AddonItemDelegate::editButtonClicked );
        editorWidget->layout()->addWidget( installButton );
    }

    editorWidget->setSizePolicy( QSizePolicy::Maximum, QSizePolicy::Preferred );

    return editorWidget;
}

void AddonItemDelegate::updateEditorGeometry( QWidget *editor,
                                              const QStyleOptionViewItem &option,
                                              const QModelIndex &index) const
{
    Q_UNUSED( index );
    QSize size = editor->sizeHint();
    editor->setGeometry( option.rect.right() - size.width(),
                         option.rect.top() + ( option.rect.height() - size.height()),
                         size.width(),
                         size.height() );
}

void AddonItemDelegate::setModelData( QWidget *editor, QAbstractItemModel *model,
                                      const QModelIndex &index ) const
{
    model->setData( index, editor->property("Addon::state"), AddonsModel::Role::STATE);
}

void AddonItemDelegate::setEditorData( QWidget *editor, const QModelIndex &index ) const
{
    editor->setProperty("Addon::state", index.data( AddonsModel::Role::STATE ) );
}

void AddonItemDelegate::setAnimator( DelegateAnimationHelper *animator_ )
{
    if ( !progressbar )
    {
        QProgressBar *progress = new QProgressBar(  );
        progress->setMinimum( 0 );
        progress->setMaximum( 0 );
        progress->setTextVisible( false );
        progressbar = progress;
    }
    animator = animator_;
}

void AddonItemDelegate::editButtonClicked()
{
    QWidget *editor = qobject_cast<QWidget *>(sender()->parent());
    if ( !editor ) return;
    auto value = editor->property("Addon::state").value<AddonsModel::State>();
    if ( value == AddonsModel::State::STATE_INSTALLED )
        /* uninstall */
        editor->setProperty("Addon::state", QVariant::fromValue(AddonsModel::State::STATE_UNINSTALLING) );
    else
        /* install */
        editor->setProperty("Addon::state", QVariant::fromValue(AddonsModel::State::STATE_INSTALLING) );
    emit commitData( editor );
    emit closeEditor( editor );
}

/* "More information" dialog */

ExtensionInfoDialog::ExtensionInfoDialog( const QModelIndex &index,
                                          qt_intf_t *p_intf,
                                          QWindow *parent )
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
    label->setText( index.data(ExtensionListModel::SummaryRole).toString() );
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
            new QLineEdit( index.data(ExtensionListModel::FilenameRole).toString(), this );
    line->setReadOnly( true );
    layout->addWidget( line, 6, 2, 1, -1 );

    // Close button
    QDialogButtonBox *group = new QDialogButtonBox( this );
    QPushButton *closeButton = new QPushButton( qtr( "&Close" ) );
    group->addButton( closeButton, QDialogButtonBox::RejectRole );
    BUTTONACT( closeButton, &ExtensionInfoDialog::close );

    layout->addWidget( group, 7, 0, 1, -1 );

    // Fix layout
    layout->setColumnStretch( 2, 1 );
    layout->setRowStretch( 4, 1 );
    setMinimumSize( 450, 350 );
}


AddonInfoDialog::AddonInfoDialog( const QModelIndex &index,
                                  qt_intf_t *p_intf, QWindow *parent )
       : QVLCDialog( parent, p_intf )
{
    // Let's be a modal dialog
    setWindowModality( Qt::WindowModal );

    // Window title
    setWindowTitle( qtr( "About" ) + " " + index.data(AddonsModel::Role::NAME).toString() );

    // Layout
    QGridLayout *layout = new QGridLayout( this );
    QLabel *label;

    // Icon
    QLabel *iconLabel = new QLabel( this );
    iconLabel->setFixedSize( 100, 100 );
    QPixmap icon = index.data( Qt::DecorationRole ).value<QPixmap>();
    icon.scaled( iconLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation );
    iconLabel->setPixmap( icon );
    iconLabel->setAlignment( Qt::AlignCenter | Qt::AlignTop );
    layout->addWidget( iconLabel, 1, 0, 2, 1 );

    // Title
    label = new QLabel( index.data(AddonsModel::Role::NAME).toString(), this );
    QFont font = label->font();
    font.setBold( true );
    font.setPointSizeF( font.pointSizeF() * 1.3f );
    label->setFont( font );
    layout->addWidget( label, 0, 0, 1, -1 );

    // HTML Content on right side
    QTextEdit *textContent = new QTextEdit();
    textContent->viewport()->setAutoFillBackground( false );
    textContent->setAcceptRichText( true );
    textContent->setBackgroundRole( QPalette::Window );
    textContent->setFrameStyle( QFrame::NoFrame );
    textContent->setAutoFillBackground( false );
    textContent->setReadOnly( true );
    layout->addWidget( textContent, 1, 1, 4, -1 );

    // Type
    QString type = AddonsModel::getLabelForType( index.data(AddonsModel::Role::TYPE).value<AddonsModel::Type>() );
    textContent->append( QString("<b>%1:</b> %2<br/>")
                         .arg( qtr("Type") ).arg( type ) );

    // Version
    QString version = index.data(AddonsModel::Role::ADDON_VERSION).toString();
    if ( !version.isEmpty() )
    {
        textContent->append( QString("<b>%1:</b> %2<br/>")
                             .arg( qtr("Version") ).arg( version ) );
    }

    // Author
    QString author = index.data(AddonsModel::Role::AUTHOR).toString();
    if ( !author.isEmpty() )
    {
        textContent->append( QString("<b>%1:</b> %2<br/>")
                             .arg( qtr("Author") ).arg( author ) );
    }

    // Summary
    textContent->append( QString("%1<br/>\n")
                .arg( index.data(AddonsModel::Role::SUMMARY).toString() ) );

    // Description
    QString description = index.data(AddonsModel::Role::DESCRIPTION).toString();
    if ( !description.isEmpty() )
    {
        textContent->append( QString("<hr/>\n%1")
                             .arg( description.replace("\n", "<br/>") ) );
    }

    // URL
    QString sourceUrl = index.data(AddonsModel::Role::LINK).toString();
    if ( !sourceUrl.isEmpty() )
    {
        label = new QLabel( "<b>" + qtr( "Website" ) + ":</b>", this );
        layout->addWidget( label, 5, 0, 1, 2 );
        label = new QLabel( QString("<a href=\"%1\">%2</a>")
                            .arg( sourceUrl ).arg( sourceUrl ), this );
        label->setOpenExternalLinks( true );
        layout->addWidget( label, 5, 2, 1, -1 );
    }

    // Script files
    QList<QVariant> list = index.data(AddonsModel::Role::FILENAME).toList();
    if ( ! list.empty() )
    {
        label = new QLabel( "<b>" + qtr( "Files" ) + ":</b>", this );
        layout->addWidget( label, 6, 0, 1, 2 );
        QComboBox *filesCombo = new QComboBox();
        Q_FOREACH( const QVariant & file, list )
            filesCombo->addItem( file.toString() );
        layout->addWidget( filesCombo, 6, 2, 1, -1 );
    }

    // Close button
    QDialogButtonBox *group = new QDialogButtonBox( this );
    QPushButton *closeButton = new QPushButton( qtr( "&Close" ) );
    group->addButton( closeButton, QDialogButtonBox::RejectRole );
    BUTTONACT( closeButton, &AddonInfoDialog::close );

    layout->addWidget( group, 7, 0, 1, -1 );

    // Fix layout
    layout->setColumnStretch( 2, 1 );
    layout->setRowStretch( 4, 1 );
    setMinimumSize( 640, 480 );
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
