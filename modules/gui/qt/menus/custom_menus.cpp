/*****************************************************************************
 * custom_menus.cpp : Qt custom menus classes
 *****************************************************************************
 * Copyright © 2006-2018 VideoLAN authors
 *                  2018 VideoLabs
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * ( at your option ) any later version.
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

#include <vlc_common.h>
#include <vlc_renderer_discovery.h>

#include "custom_menus.hpp"
#include "util/renderer_manager.hpp"

// MediaLibrary includes
#include "medialibrary/mlbookmarkmodel.hpp"

// Dialogs includes
#include "dialogs/dialogs_provider.hpp"

// Menus includes
#include "menus/menus.hpp"

// Qt includes
#include <QMenu>
#include <QAction>
#include <QActionGroup>
#include <QWidgetAction>
#include <QVBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QMetaObject>
#include <QMetaProperty>
#include <QMetaMethod>

RendererAction::RendererAction( vlc_renderer_item_t *p_item_ )
    : QAction()
{
    p_item = p_item_;
    vlc_renderer_item_hold( p_item );
    if( vlc_renderer_item_flags( p_item ) & VLC_RENDERER_CAN_VIDEO )
        setIcon( QIcon( ":/menu/movie.svg" ) );
    else
        setIcon( QIcon( ":/menu/music.svg" ) );
    setText( vlc_renderer_item_name( p_item ) );
    setCheckable(true);
}

RendererAction::~RendererAction()
{
    vlc_renderer_item_release( p_item );
}

vlc_renderer_item_t * RendererAction::getItem()
{
    return p_item;
}

RendererMenu::RendererMenu( QMenu *parent, qt_intf_t *p_intf_ )
    : QMenu( parent ), p_intf( p_intf_ )
{
    setTitle( qtr("&Renderer") );

    group = new QActionGroup( this );

    QAction *action = new QAction( qtr("<Local>"), this );
    action->setCheckable(true);
    addAction( action );
    group->addAction(action);

    vlc_player_Lock( p_intf_->p_player );
    if ( vlc_player_GetRenderer( p_intf->p_player ) == nullptr )
        action->setChecked( true );
    vlc_player_Unlock( p_intf_->p_player );

    addSeparator();

    QWidget *statusWidget = new QWidget();
    statusWidget->setLayout( new QVBoxLayout );
    QLabel *label = new QLabel();
    label->setObjectName( "statuslabel" );
    statusWidget->layout()->addWidget( label );
    QProgressBar *pb = new QProgressBar();
    pb->setObjectName( "statusprogressbar" );
    pb->setMaximumHeight( 10 );
    pb->setStyleSheet( QString("\
        QProgressBar:horizontal {\
            border: none;\
            background: transparent;\
            padding: 1px;\
        }\
        QProgressBar::chunk:horizontal {\
            background: qlineargradient(x1: 0, y1: 0.5, x2: 1, y2: 0.5, \
                        stop: 0 white, stop: 0.4 orange, stop: 0.6 orange, stop: 1 white);\
        }") );
    pb->setRange( 0, 0 );
    pb->setSizePolicy( QSizePolicy::MinimumExpanding, QSizePolicy::Maximum );
    statusWidget->layout()->addWidget( pb );
    QWidgetAction *qwa = new QWidgetAction( this );
    qwa->setDefaultWidget( statusWidget );
    qwa->setDisabled( true );
    addAction( qwa );
    status = qwa;

    RendererManager *manager = RendererManager::getInstance( p_intf );
    connect( this, &RendererMenu::aboutToShow, manager, &RendererManager::StartScan );
    connect( group, &QActionGroup::triggered, this, &RendererMenu::RendererSelected );
    connect( manager, SIGNAL(rendererItemAdded( vlc_renderer_item_t * )),
             this, SLOT(addRendererItem( vlc_renderer_item_t * )), Qt::DirectConnection );
    connect( manager, SIGNAL(rendererItemRemoved( vlc_renderer_item_t * )),
             this, SLOT(removeRendererItem( vlc_renderer_item_t * )), Qt::DirectConnection );
    connect( manager, &RendererManager::statusUpdated, this, &RendererMenu::updateStatus );
}

RendererMenu::~RendererMenu()
{
    reset();
}

void RendererMenu::updateStatus( int val )
{
    QProgressBar *pb = findChild<QProgressBar *>("statusprogressbar");
    QLabel *label = findChild<QLabel *>("statuslabel");
    if( val >= RendererManager::RendererStatus::RUNNING )
    {
        label->setText( qtr("Scanning...").
               append( QString(" (%1s)").arg( val ) ) );
        pb->setVisible( true );
        status->setVisible( true );
    }
    else if( val == RendererManager::RendererStatus::FAILED )
    {
        label->setText( "Failed (no discovery module available)" );
        pb->setVisible( false );
        status->setVisible( true );
    }
    else status->setVisible( false );
}

void RendererMenu::addRendererItem( vlc_renderer_item_t *p_item )
{
    QAction *action = new RendererAction( p_item );
    insertAction( status, action );
    group->addAction( action );
}

void RendererMenu::removeRendererItem( vlc_renderer_item_t *p_item )
{
    foreach (QAction* action, group->actions())
    {
        RendererAction *ra = qobject_cast<RendererAction *>( action );
        if( !ra || ra->getItem() != p_item )
            continue;
        removeRendererAction( ra );
        delete ra;
        break;
    }
}

void RendererMenu::addRendererAction(QAction *action)
{
    insertAction( status, action );
    group->addAction( action );
}

void RendererMenu::removeRendererAction(QAction *action)
{
    removeAction( action );
    group->removeAction( action );
}

void RendererMenu::reset()
{
    /* reset the list of renderers */
    foreach (QAction* action, group->actions())
    {
        RendererAction *ra = qobject_cast<RendererAction *>( action );
        if( ra )
        {
            removeRendererAction( ra );
            delete ra;
        }
    }
}

void RendererMenu::RendererSelected(QAction *action)
{
    RendererAction *ra = qobject_cast<RendererAction *>( action );
    if( ra )
        RendererManager::getInstance( p_intf )->SelectRenderer( ra->getItem() );
    else
        RendererManager::getInstance( p_intf )->SelectRenderer( NULL );
}

/*   CheckableListMenu   */

CheckableListMenu::CheckableListMenu(QString title, QAbstractListModel* model , GroupingMode grouping,  QWidget *parent)
    : QMenu(parent)
    , m_model(model)
    , m_grouping(grouping)
{
    this->setTitle(title);
    if (m_grouping != UNGROUPED)
    {
        m_actionGroup = new QActionGroup(this);
        if (m_grouping == GROUPED_OPTIONAL)
        {
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
            m_actionGroup->setExclusionPolicy(QActionGroup::ExclusionPolicy::ExclusiveOptional);
#else
            m_actionGroup->setExclusive(false);
#endif
        }
    }

    connect(m_model, &QAbstractListModel::rowsAboutToBeRemoved, this, &CheckableListMenu::onRowsAboutToBeRemoved);
    connect(m_model, &QAbstractListModel::rowsInserted, this, &CheckableListMenu::onRowInserted);
    connect(m_model, &QAbstractListModel::dataChanged, this, &CheckableListMenu::onDataChanged);
    connect(m_model, &QAbstractListModel::modelAboutToBeReset, this, &CheckableListMenu::onModelAboutToBeReset);
    connect(m_model, &QAbstractListModel::modelReset, this, &CheckableListMenu::onModelReset);
    onModelReset();
}

void CheckableListMenu::onRowsAboutToBeRemoved(const QModelIndex &, int first, int last)
{
    for (int i = last; i >= first; i--)
    {
        QAction* action = actions()[i];
        if (m_actionGroup)
            m_actionGroup->removeAction(action);
        delete action;
    }
    if (actions().count() == 0)
        setEnabled(false);
}

void CheckableListMenu::onRowInserted(const QModelIndex &, int first, int last)
{
    for (int i = first; i <= last; i++)
    {
        QModelIndex index = m_model->index(i);
        QString title = m_model->data(index, Qt::DisplayRole).toString();
        bool checked = m_model->data(index, Qt::CheckStateRole).toBool();

        QAction *choiceAction = new QAction(title, this);
        addAction(choiceAction);
        if (m_actionGroup)
            m_actionGroup->addAction(choiceAction);
        connect(choiceAction, &QAction::triggered, [this, i](bool checked){
            QModelIndex dataIndex = m_model->index(i);
            m_model->setData(dataIndex, QVariant::fromValue<bool>(checked), Qt::CheckStateRole);
        });
        choiceAction->setCheckable(true);
        choiceAction->setChecked(checked);
        setEnabled(true);
    }
}

void CheckableListMenu::onDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight, const QVector<int> & )
{
    for (int i = topLeft.row(); i <= bottomRight.row(); i++)
    {
        if (i >= actions().size())
            break;
        QAction *choiceAction = actions()[i];

        QModelIndex index = m_model->index(i);
        QString title = m_model->data(index, Qt::DisplayRole).toString();
        bool checked = m_model->data(index, Qt::CheckStateRole).toBool();

        choiceAction->setText(title);
        choiceAction->setChecked(checked);
    }
}

void CheckableListMenu::onModelAboutToBeReset()
{
    for (QAction* action  :actions())
    {
        if (m_actionGroup)
            m_actionGroup->removeAction(action);
        delete action;
    }
    setEnabled(false);
}

void CheckableListMenu::onModelReset()
{
    int nb_rows = m_model->rowCount();
    if (nb_rows == 0)
        setEnabled(false);
    else
        onRowInserted({}, 0, nb_rows - 1);
}

// ListMenuHelper

ListMenuHelper::ListMenuHelper(QMenu * menu, QAbstractListModel * model, QAction * before,
                               QObject * parent)
    : QObject(parent), m_menu(menu), m_model(model), m_before(before)
{
    m_group = new QActionGroup(this);

    onModelReset();

    connect(m_model, &QAbstractListModel::rowsInserted, this, &ListMenuHelper::onRowsInserted);
    connect(m_model, &QAbstractListModel::rowsRemoved,  this, &ListMenuHelper::onRowsRemoved);

    connect(m_model, &QAbstractListModel::dataChanged, this, &ListMenuHelper::onDataChanged);

    connect(m_model, &QAbstractListModel::modelReset, this, &ListMenuHelper::onModelReset);
}

// Interface

int ListMenuHelper::count() const
{
    return m_actions.count();
}

// Private slots

void ListMenuHelper::onRowsInserted(const QModelIndex &, int first, int last)
{
    QAction * before;

    if (first < m_actions.count())
        before = m_actions.at(first);
    else
        before = m_before;

    for (int i = first; i <= last; i++)
    {
        QModelIndex index = m_model->index(i, 0);

        QString name = m_model->data(index, Qt::DisplayRole).toString();

        QAction * action = new QAction(name, this);

        action->setCheckable(true);

        bool checked = m_model->data(index, Qt::CheckStateRole).toBool();

        action->setChecked(checked);

        // NOTE: We are adding sequentially *before* the next action in the list.
        m_menu->insertAction(before, action);

        m_group->addAction(action);

        m_actions.insert(i, action);

        connect(action, &QAction::triggered, this, &ListMenuHelper::onTriggered);
    }

    emit countChanged(m_actions.count());
}

void ListMenuHelper::onRowsRemoved(const QModelIndex &, int first, int last)
{
    for (int i = first; i <= last; i++)
    {
        QAction * action = m_actions.at(i);

        m_group->removeAction(action);

        delete action;
    }

    QList<QAction *>::iterator begin = m_actions.begin();

    m_actions.erase(begin + first, begin + last + 1);

    emit countChanged(m_actions.count());
}

void ListMenuHelper::onDataChanged(const QModelIndex & topLeft,
                                   const QModelIndex & bottomRight, const QVector<int> &)
{
    for (int i = topLeft.row(); i <= bottomRight.row(); i++)
    {
        QAction * action = m_actions.at(i);

        QModelIndex index = m_model->index(i, 0);

        QString name = m_model->data(index, Qt::DisplayRole).toString();

        action->setText(name);

        bool checked = m_model->data(index, Qt::CheckStateRole).toBool();

        action->setChecked(checked);
    }
}

void ListMenuHelper::onModelReset()
{
    for (QAction * action : m_actions)
    {
        m_group->removeAction(action);

        delete action;
    }

    m_actions.clear();

    int count = m_model->rowCount();

    if (count)
        onRowsInserted(QModelIndex(), 0, count - 1);
}

void ListMenuHelper::onTriggered(bool)
{
    QAction * action = static_cast<QAction *> (sender());

    emit select(m_actions.indexOf(action));
}

/*     BooleanPropertyAction    */

BooleanPropertyAction::BooleanPropertyAction(QString title, QObject *model, QString propertyName, QWidget *parent)
    : QAction(parent)
    , m_model(model)
    , m_propertyName(propertyName)
{
    setText(title);
    assert(model);
    const QMetaObject* meta = model->metaObject();
    int propertyId = meta->indexOfProperty(qtu(propertyName));
    assert(propertyId != -1);
    QMetaProperty property = meta->property(propertyId);
    assert(property.typeId() ==  QMetaType::Bool);
    const QMetaObject* selfMeta = this->metaObject();

    assert(property.hasNotifySignal());
    QMetaMethod checkedSlot = selfMeta->method(selfMeta->indexOfSlot( "setChecked(bool)" ));
    connect( model, property.notifySignal(), this, checkedSlot );
    connect( this, &BooleanPropertyAction::triggered, this, &BooleanPropertyAction::setModelChecked );

    setCheckable(true);
    setChecked(property.read(model).toBool());
}

void BooleanPropertyAction::setModelChecked(bool checked)
{
    m_model->setProperty(qtu(m_propertyName), QVariant::fromValue<bool>(checked) );
}


RecentMenu::RecentMenu(MLRecentsModel* model, MediaLib* ml,  QWidget* parent)
    : QMenu(parent)
    , m_model(model)
    , m_ml(ml)
{
    connect(m_model, &MLRecentsModel::rowsRemoved, this, &RecentMenu::onRowsRemoved);
    connect(m_model, &MLRecentsModel::rowsInserted, this, &RecentMenu::onRowInserted);
    connect(m_model, &MLRecentsModel::dataChanged, this, &RecentMenu::onDataChanged);
    connect(m_model, &MLRecentsModel::modelReset, this, &RecentMenu::onModelReset);
    m_separator = addSeparator();
    addAction( qtr("&Clear"), m_model, &MLRecentsModel::clearHistory );
    onModelReset();
}

void RecentMenu::onRowsRemoved(const QModelIndex&, int first, int last)
{
    for (int i = first; i <= last; i++)
    {
        delete m_actions.at(i);
    }

    QList<QAction *>::iterator begin = m_actions.begin();

    m_actions.erase(begin + first, begin + last + 1);

    if (m_actions.isEmpty())
        setEnabled(false);
}

void RecentMenu::onRowInserted(const QModelIndex&, int first, int last)
{
    QAction * before;

    if (first < m_actions.count())
        before = m_actions.at(first);
    else
        // NOTE: In that case we insert *before* the 'Clear' separator.
        before = m_separator;

    for (int i = first; i <= last; i++)
    {
        QModelIndex index = m_model->index(i);
        QString url = m_model->data(index, MLRecentsModel::RECENT_MEDIA_URL).toString();

        QAction *choiceAction = new QAction(url, this);

        // NOTE: We are adding sequentially *before* the next action in the list.
        insertAction(before, choiceAction);

        m_actions.insert(i, choiceAction);

        connect(choiceAction, &QAction::triggered, [this, choiceAction](){
            QModelIndex index = m_model->index(m_actions.indexOf(choiceAction));

            MLItemId id = m_model->data(index, MLRecentsModel::RECENT_MEDIA_ID).value<MLItemId>();
            m_ml->addAndPlay(id);
        });
        setEnabled(true);
    }
}

void RecentMenu::onDataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight, const QVector<int>& )
{
    for (int i = topLeft.row(); i <= bottomRight.row(); i++)
    {
        QModelIndex index = m_model->index(i);
        QString title = m_model->data(index, MLRecentsModel::RECENT_MEDIA_URL).toString();

        m_actions.at(i)->setText(title);
    }
}

void RecentMenu::onModelReset()
{
    qDeleteAll(m_actions);
    m_actions.clear();

    int nb_rows = m_model->rowCount();
    if (nb_rows == 0 || nb_rows == -1)
        setEnabled(false);
    else
        onRowInserted({}, 0, nb_rows - 1);
}

// BookmarkMenu

BookmarkMenu::BookmarkMenu(MediaLib * mediaLib, vlc_player_t * player, QWidget * parent)
    : QMenu(parent)
{
    // FIXME: Do we really need a translation call for the string shortcut ?
    addAction(qtr("&Manage"), THEDP, &DialogsProvider::bookmarksDialog, qtr("Ctrl+B"));

    addSeparator();

    MLBookmarkModel * model = new MLBookmarkModel(this);
    model->setPlayer(player);
    model->setMl(mediaLib);

    ListMenuHelper * helper = new ListMenuHelper(this, model, nullptr, this);

    connect(helper, &ListMenuHelper::select, [model](int index)
    {
        model->select(model->index(index, 0));
    });

    setTearOffEnabled(true);
}
