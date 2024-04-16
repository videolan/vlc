/*****************************************************************************
 * Copyright (C) 2020 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * ( at your option ) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/
#include "qml_menu_wrapper.hpp"
#include "menus.hpp"
#include "medialibrary/medialib.hpp"
#include "medialibrary/mlplaylistlistmodel.hpp"
#include "medialibrary/mlplaylistmodel.hpp"
#include "medialibrary/mlbookmarkmodel.hpp"
#include "network/networkdevicemodel.hpp"
#include "network/networkmediamodel.hpp"
#include "playlist/playlist_controller.hpp"
#include "playlist/playlist_model.hpp"
#include "dialogs/dialogs_provider.hpp"

// Qt includes
#include <QPainter>
#include <QSignalMapper>
#include <QScreen>
#include <QActionGroup>

namespace
{
    QIcon sortIcon(QWidget *widget, int order)
    {
        assert(order == Qt::AscendingOrder || order == Qt::DescendingOrder);

        QStyleOptionHeader headerOption;
        headerOption.initFrom(widget);
        headerOption.sortIndicator = (order == Qt::AscendingOrder)
                ? QStyleOptionHeader::SortDown
                : QStyleOptionHeader::SortUp;

        QStyle *style = qApp->style();
        int arrowsize = style->pixelMetric(QStyle::PM_HeaderMarkSize, &headerOption, widget);
        if (arrowsize <= 0)
            arrowsize = 32;

        qreal dpr = widget ? widget->devicePixelRatioF() : 1.0;
        headerOption.rect = QRect(0, 0, arrowsize, arrowsize);

        QPixmap arrow(arrowsize * dpr, arrowsize * dpr);
        arrow.setDevicePixelRatio(dpr);
        arrow.fill(Qt::transparent);

        {
            QPainter arrowPainter(&arrow);
            style->drawPrimitive(QStyle::PE_IndicatorHeaderArrow, &headerOption, &arrowPainter, widget);
        }

        return QIcon(arrow);
    }
}

void StringListMenu::popup(const QPoint &point, const QVariantList &stringList)
{
    QMenu *m = new QMenu;
    m->setAttribute(Qt::WA_DeleteOnClose);

    for (int i = 0; i != stringList.size(); ++i)
    {
        const auto str = stringList[i].toString();
        m->addAction(str, this, [this, i, str]()
        {
            emit selected(i, str);
        });
    }

    m->popup(point);
}

// SortMenu

void SortMenu::popup(const QPoint &point, const bool popupAbovePoint, const QVariantList &model)
{
    m_menu = std::make_unique<QMenu>();

    connect( m_menu.get(), &QMenu::aboutToShow, this, [this]() {
        m_shown = true;
        shownChanged();
    } );
    connect( m_menu.get(), &QMenu::aboutToHide, this, [this]() {
        m_shown = false;
        shownChanged();
    } );

    // model => [{text: "", checked: <bool>, order: <sort order> if checked else <invalid>}...]
    for (int i = 0; i != model.size(); ++i)
    {
        const auto obj = model[i].toMap();

        auto action = m_menu->addAction(obj.value("text").toString());
        action->setCheckable(true);

        const bool checked = obj.value("checked").toBool();
        action->setChecked(checked);

        if (checked)
            action->setIcon(sortIcon(m_menu.get(), obj.value("order").toInt()));

        connect(action, &QAction::triggered, this, [this, i]()
        {
            emit selected(i);
        });
    }

    onPopup(m_menu.get());

    if (popupAbovePoint)
        m_menu->popup(QPoint(point.x(), point.y() - m_menu->sizeHint().height()));
    else
        m_menu->popup(point);
}

void SortMenu::close()
{
    if (m_menu)
        m_menu->close();
}

// Protected functions

/* virtual */ void SortMenu::onPopup(QMenu *) {}

// SortMenuVideo

// Protected SortMenu reimplementation

void SortMenuVideo::onPopup(QMenu * menu) /* override */
{
    if (!m_ctx)
        return;

    menu->addSeparator();

    struct
    {
        const char * title;

        MainCtx::Grouping grouping;
    }
    entries[] =
    {
        { N_("Do not group videos"), MainCtx::GROUPING_NONE },
        { N_("Group by name"), MainCtx::GROUPING_NAME },
        { N_("Group by folder"), MainCtx::GROUPING_FOLDER },
    };

    QActionGroup * group = new QActionGroup(this);

    int index = m_ctx->grouping();

    for (size_t i = 0; i < ARRAY_SIZE(entries); i++)
    {
        QAction * action = menu->addAction(qtr(entries[i].title));

        action->setCheckable(true);

        MainCtx::Grouping grouping = entries[i].grouping;

        connect(action, &QAction::triggered, this, [this, grouping]()
        {
            emit this->grouping(grouping);
        });

        group->addAction(action);

        if (index == grouping)
            action->setChecked(true);
    }
}

QmlGlobalMenu::QmlGlobalMenu(QObject *parent)
    : VLCMenuBar(parent)
{
}

void QmlGlobalMenu::popup(QPoint pos)
{
    if (!m_ctx)
        return;

    qt_intf_t* p_intf = m_ctx->getIntf();
    if (!p_intf)
        return;

    m_menu = std::make_unique<QMenu>();
    QMenu* submenu;

    connect( m_menu.get(), &QMenu::aboutToShow, this, [this]() {
        m_shown = true;
        shownChanged();
        aboutToShow();
    });
    connect( m_menu.get(), &QMenu::aboutToHide, this, [this]() {
        m_shown = false;
        shownChanged();
        aboutToHide();
    });

    submenu = m_menu->addMenu(qtr( "&Media" ));
    FileMenu( p_intf, submenu );

    /* Dynamic menus, rebuilt before being showed */
    submenu = m_menu->addMenu(qtr( "P&layback" ));
    NavigMenu( p_intf, submenu );

    submenu = m_menu->addMenu(qtr( "&Audio" ));
    AudioMenu( p_intf, submenu );

    submenu = m_menu->addMenu(qtr( "&Video" ));
    VideoMenu( p_intf, submenu );

    submenu = m_menu->addMenu(qtr( "Subti&tle" ));
    SubtitleMenu( p_intf, submenu );

    submenu = m_menu->addMenu(qtr( "Tool&s" ));
    ToolsMenu( p_intf, submenu );

    /* View menu, a bit different */
    submenu = m_menu->addMenu(qtr( "V&iew" ));
    ViewMenu( p_intf, submenu );

    submenu = m_menu->addMenu(qtr( "&Help" ));
    HelpMenu(submenu);

    m_menu->popup(pos);
}

QmlMenuBarMenu::QmlMenuBarMenu(QmlMenuBar* menubar, QWidget* parent)
    : QMenu(parent)
    , m_menubar(menubar)
{}

QmlMenuBarMenu::~QmlMenuBarMenu()
{
}

void QmlMenuBarMenu::mouseMoveEvent(QMouseEvent* mouseEvent)
{
    QPoint globalPos =m_menubar-> m_menu->mapToGlobal(mouseEvent->pos());
    if (m_menubar->getmenubar()->contains(m_menubar->getmenubar()->mapFromGlobal(globalPos))
        && !m_menubar->m_button->contains(m_menubar->m_button->mapFromGlobal(globalPos)))
    {
        m_menubar->setopenMenuOnHover(true);
        close();
        return;
    }
    QMenu::mouseMoveEvent(mouseEvent);
}

void QmlMenuBarMenu::keyPressEvent(QKeyEvent * event)
{
    QMenu::keyPressEvent(event);
    if (!event->isAccepted()
        && (event->key() == Qt::Key_Left  || event->key() == Qt::Key_Right))
    {
        event->accept();
        emit m_menubar->navigateMenu(event->key() == Qt::Key_Left ? -1 : 1);
    }
}

void QmlMenuBarMenu::keyReleaseEvent(QKeyEvent * event)
{
    QMenu::keyReleaseEvent(event);
}

QmlMenuBar::QmlMenuBar(QObject *parent)
    : VLCMenuBar(parent)
{
}

void QmlMenuBar::popupMenuCommon( QQuickItem* button, std::function<void(QMenu*)> createMenuFunc)
{
    if (!m_ctx || !m_menubar || !button)
        return;

    qt_intf_t* p_intf = m_ctx->getIntf();
    if (!p_intf)
        return;

    m_menu = std::make_unique<QmlMenuBarMenu>(this);
    createMenuFunc(m_menu.get());
    m_button = button;
    m_openMenuOnHover = false;
    connect(m_menu.get(), &QMenu::aboutToHide, this, &QmlMenuBar::onMenuClosed);
    QPointF position = button->mapToGlobal(QPoint(0, button->height()));
    m_menu->popup(position.toPoint());
}

void QmlMenuBar::popupMediaMenu( QQuickItem* button )
{
    popupMenuCommon(button, [this](QMenu* menu) {
        qt_intf_t* p_intf = m_ctx->getIntf();
        FileMenu( p_intf, menu );
    });
}

void QmlMenuBar::popupPlaybackMenu( QQuickItem* button )
{
    popupMenuCommon(button, [this](QMenu* menu) {
        NavigMenu( m_ctx->getIntf(), menu );
    });
}

void QmlMenuBar::popupAudioMenu(QQuickItem* button )
{
    popupMenuCommon(button, [this](QMenu* menu) {
        AudioMenu( m_ctx->getIntf(), menu );
    });
}

void QmlMenuBar::popupVideoMenu( QQuickItem* button )
{
    popupMenuCommon(button, [this](QMenu* menu) {
        VideoMenu( m_ctx->getIntf(), menu );
    });
}

void QmlMenuBar::popupSubtitleMenu( QQuickItem* button )
{
    popupMenuCommon(button, [this](QMenu* menu) {
        SubtitleMenu( m_ctx->getIntf(), menu );
    });
}


void QmlMenuBar::popupToolsMenu( QQuickItem* button )
{
    popupMenuCommon(button, [this](QMenu* menu) {
        ToolsMenu( m_ctx->getIntf(), menu );
    });
}

void QmlMenuBar::popupViewMenu( QQuickItem* button )
{
    popupMenuCommon(button, [this](QMenu* menu) {
        qt_intf_t* p_intf = m_ctx->getIntf();
        ViewMenu( p_intf, menu );
    });
}

void QmlMenuBar::popupHelpMenu( QQuickItem* button )
{
    popupMenuCommon(button, [](QMenu* menu) {
        HelpMenu(menu);
    });
}

void QmlMenuBar::onMenuClosed()
{
    if (!m_openMenuOnHover)
        emit menuClosed();
}

// QmlMenuPositioner

/* explicit */ QmlMenuPositioner::QmlMenuPositioner(QObject * parent) : QObject(parent) {}

// Interface

void QmlMenuPositioner::popup(QMenu * menu, const QPoint & position, bool above)
{
    menu->removeEventFilter(this);

    if (above == false)
    {
        menu->popup(position);

        return;
    }

    m_position = position;

    menu->installEventFilter(this);

    // NOTE: QMenu::height() returns an invalid height until the initial popup call.
    menu->popup(position);
}

// Public events

bool QmlMenuPositioner::eventFilter(QObject * object, QEvent * event)
{
    if (event->type() == QEvent::Resize)
    {
        QScreen * screen = QGuiApplication::screenAt(m_position);

        if (screen == nullptr)
            return QObject::eventFilter(object, event);

        QMenu * menu = static_cast<QMenu *> (object);

        int width  = menu->width();
        int height = menu->height();

        QRect geometry = screen->availableGeometry();

        int x = geometry.x();
        int y = geometry.y();

        // NOTE: We want a position within the screen boundaries.

        x = qBound(x, m_position.x(), x + geometry.width() - width);

        y = qBound(y, m_position.y() - height, y + geometry.height() - height);

        menu->move(QPoint(x, y));
    }

    return QObject::eventFilter(object, event);
}

// QmlBookmarkMenu

/* explicit */ QmlBookmarkMenu::QmlBookmarkMenu(QObject * parent)
    : QObject(parent)
{}

// Interface

/* Q_INVOKABLE */ void QmlBookmarkMenu::popup(const QPoint & position, bool above)
{
    if (m_ctx == nullptr || m_player == nullptr)
        return;

    m_menu = std::make_unique<QMenu>();

    connect(m_menu.get(), &QMenu::aboutToHide, this, &QmlBookmarkMenu::aboutToHide);
    connect(m_menu.get(), &QMenu::aboutToShow, this, &QmlBookmarkMenu::aboutToShow);

    QAction * sectionTitles    = m_menu->addSection(qtr("Titles"));
    QAction * sectionChapters  = m_menu->addSection(qtr("Chapters"));
    QAction * sectionBookmarks = nullptr;

    if (m_ctx->hasMediaLibrary())
        sectionBookmarks = m_menu->addSection(qtr("Bookmarks"));

    // Titles

    TitleListModel * titles = m_player->getTitles();

    sectionTitles->setVisible(titles->rowCount() != 0);

    ListMenuHelper * helper = new ListMenuHelper(m_menu.get(), titles, sectionChapters, m_menu.get());

    connect(helper, &ListMenuHelper::select, [titles](int index)
    {
        titles->setData(titles->index(index), true, Qt::CheckStateRole);
    });

    connect(helper, &ListMenuHelper::countChanged, [sectionTitles](int count)
    {
        // NOTE: The section should only be visible when the model has content.
        sectionTitles->setVisible(count != 0);
    });

    // Chapters

    ChapterListModel * chapters = m_player->getChapters();

    sectionChapters->setVisible(chapters->rowCount() != 0);

    helper = new ListMenuHelper(m_menu.get(), chapters, sectionBookmarks, m_menu.get());

    connect(helper, &ListMenuHelper::select, [chapters](int index)
    {
        chapters->setData(chapters->index(index), true, Qt::CheckStateRole);
    });

    connect(helper, &ListMenuHelper::countChanged, [sectionChapters](int count)
    {
        // NOTE: The section should only be visible when the model has content.
        sectionChapters->setVisible(count != 0);
    });

    // Bookmarks
    if (m_ctx->hasMediaLibrary())
    {
        // FIXME: Do we really need a translation call for the string shortcut ?
        m_menu->addAction(qtr("&Manage"), THEDP, &DialogsProvider::bookmarksDialog, qtr("Ctrl+B"));

        m_menu->addSeparator();

        MLBookmarkModel * bookmarks = new MLBookmarkModel(m_menu.get());
        bookmarks->setPlayer(m_player->getPlayer());
        bookmarks->setMl(m_ctx->getMediaLibrary());

        helper = new ListMenuHelper(m_menu.get(), bookmarks, nullptr, m_menu.get());

        connect(helper, &ListMenuHelper::select, [bookmarks](int index)
        {
            bookmarks->select(bookmarks->index(index, 0));
        });
    }

    m_positioner.popup(m_menu.get(), position, above);
}

// QmlProgramMenu

/* explicit */ QmlProgramMenu::QmlProgramMenu(QObject * parent)
    : QObject(parent)
{}

// Interface

/* Q_INVOKABLE */ void QmlProgramMenu::popup(const QPoint & position, bool above)
{
    if (m_player == nullptr)
        return;

    m_menu = std::make_unique<QMenu>();

    connect(m_menu.get(), &QMenu::aboutToHide, this, &QmlProgramMenu::aboutToHide);
    connect(m_menu.get(), &QMenu::aboutToShow, this, &QmlProgramMenu::aboutToShow);

    m_menu->addSection(qtr("Programs"));

    ProgramListModel * programs = m_player->getPrograms();

    ListMenuHelper * helper = new ListMenuHelper(m_menu.get(), programs, nullptr, m_menu.get());

    connect(helper, &ListMenuHelper::select, [programs](int index)
    {
        programs->setData(programs->index(index), true, Qt::CheckStateRole);
    });

    m_positioner.popup(m_menu.get(), position, above);
}

// QmlRendererMenu

/* explicit */ QmlRendererMenu::QmlRendererMenu(QObject * parent)
    : QObject(parent)
{}

// Interface

/* Q_INVOKABLE */ void QmlRendererMenu::popup(const QPoint & position, bool above)
{
    if (m_ctx == nullptr)
        return;

    m_menu = std::make_unique<RendererMenu>(nullptr, m_ctx->getIntf());

    connect(m_menu.get(), &QMenu::aboutToHide, this, &QmlRendererMenu::aboutToHide);
    connect(m_menu.get(), &QMenu::aboutToShow, this, &QmlRendererMenu::aboutToShow);

    m_positioner.popup(m_menu.get(), position, above);
}

// Tracks

// QmlTrackMenu

/* explicit */ QmlTrackMenu::QmlTrackMenu(QObject * parent) : QObject(parent) {}

// Interface

/* Q_INVOKABLE */ void QmlTrackMenu::popup(const QPoint & position)
{
    m_menu = std::make_unique<QMenu>();

    beforePopup(m_menu.get());

    m_menu->popup(position);
}

// QmlSubtitleMenu

/* explicit */ QmlSubtitleMenu::QmlSubtitleMenu(QObject * parent) : QmlTrackMenu(parent) {}

// Protected QmlTrackMenu implementation

void QmlSubtitleMenu::beforePopup(QMenu * menu) /* override */
{
    menu->addAction(qtr("Open file"), this, [this]()
    {
        emit triggered(Open);
    });

    menu->addAction(QIcon(":/menu/sync.svg"), qtr("Synchronize"), this, [this]()
    {
        emit triggered(Synchronize);
    });

    menu->addAction(QIcon(":/menu/download.svg"), qtr("Search online"), this, [this]()
    {
        emit triggered(Download);
    });

    menu->addSeparator();

    QAction * action = menu->addAction(qtr("Select multiple"), this, [this]()
    {
        TrackListModel * tracks = this->m_player->getSubtitleTracks();

        tracks->setMultiSelect(!(tracks->getMultiSelect()));
    });

    action->setCheckable(true);

    action->setChecked(m_player->getSubtitleTracks()->getMultiSelect());
}

// QmlAudioMenu

/* explicit */ QmlAudioMenu::QmlAudioMenu(QObject * parent) : QmlTrackMenu(parent) {}

// Protected QmlTrackMenu implementation

void QmlAudioMenu::beforePopup(QMenu * menu) /* override */
{
    menu->addAction(qtr("Open file"), this, [this]()
    {
        emit triggered(Open);
    });

    menu->addAction(QIcon(":/menu/sync.svg"), qtr("Synchronize"), this, [this]()
    {
        emit triggered(Synchronize);
    });
}

//=================================================================================================
// PlaylistListContextMenu
//=================================================================================================

PlaylistListContextMenu::PlaylistListContextMenu(QObject * parent)
    : QObject(parent)
{}


void PlaylistListContextMenu::popup(const QModelIndexList & selected, QPoint pos, QVariantMap)
{
    if (!m_model)
        return;

    QVariantList ids;

    for (const QModelIndex & modelIndex : selected)
        ids.push_back(m_model->data(modelIndex, MLPlaylistListModel::PLAYLIST_ID));

    m_menu = std::make_unique<QMenu>();

    MediaLib * ml = m_model->ml();

    QAction * action = m_menu->addAction(qtr("Add and play"));

    connect(action, &QAction::triggered, [ml, ids]() {
        ml->addAndPlay(ids);
    });

    action = m_menu->addAction(qtr("Enqueue"));

    connect(action, &QAction::triggered, [ml, ids]() {
        ml->addToPlaylist(ids);
    });

    if (ids.count() == 1)
    {
        action = m_menu->addAction(qtr("Rename"));

        QModelIndex index = selected.first();

        connect(action, &QAction::triggered, [this, index]() {
            m_model->showDialogRename(index);
        });
    }

    action = m_menu->addAction(qtr("Delete"));

    connect(action, &QAction::triggered, [this, ids]() {
        m_model->deletePlaylists(ids);
    });

    m_menu->popup(pos);
}

//=================================================================================================
// PlaylistMediaContextMenu
//=================================================================================================

PlaylistMediaContextMenu::PlaylistMediaContextMenu(QObject * parent) : QObject(parent) {}

void PlaylistMediaContextMenu::popup(const QModelIndexList & selected, QPoint pos,
                                     QVariantMap options)
{
    if (!m_model)
        return;

    QVariantList ids;

    for (const QModelIndex& modelIndex : selected)
        ids.push_back(m_model->data(modelIndex, MLPlaylistModel::MEDIA_ID));

    m_menu = std::make_unique<QMenu>();

    MediaLib * ml = m_model->ml();

    QAction * action = m_menu->addAction(qtr("Add and play"));

    connect(action, &QAction::triggered, [ml, ids]() {
        ml->addAndPlay(ids);
    });

    action = m_menu->addAction(qtr("Enqueue"));

    connect(action, &QAction::triggered, [ml, ids]() {
        ml->addToPlaylist(ids);
    });

    action = m_menu->addAction(qtr("Add to playlist"));

    connect(action, &QAction::triggered, [ids]() {
        DialogsProvider::getInstance()->playlistsDialog(ids);
    });

    action = m_menu->addAction(qtr("Play as audio"));

    connect(action, &QAction::triggered, [ml, ids]() {
        ml->addAndPlay(ids, {":no-video"});
    });

    if (options.contains("information") && options["information"].typeId() == QMetaType::Int) {
        action = m_menu->addAction(qtr("Information"));

        QSignalMapper * mapper = new QSignalMapper(m_menu.get());

        connect(action, &QAction::triggered, mapper, QOverload<>::of(&QSignalMapper::map));

        mapper->setMapping(action, options["information"].toInt());
        connect(mapper, QSIGNALMAPPER_MAPPEDINT_SIGNAL,
                this, &PlaylistMediaContextMenu::showMediaInformation);
    }

    m_menu->addSeparator();

    action = m_menu->addAction(qtr("Remove Selected"));

    action->setIcon(QIcon(":/menu/remove.svg"));

    connect(action, &QAction::triggered, [this, selected]() {
        m_model->remove(selected);
    });

    m_menu->popup(pos);
}

//=================================================================================================

NetworkMediaContextMenu::NetworkMediaContextMenu(QObject* parent)
    : QObject(parent)
{}

void NetworkMediaContextMenu::popup(const QModelIndexList& selected, QPoint pos)
{
    if (!m_model)
        return;

    m_menu = std::make_unique<QMenu>();
    QAction* action;

    action = m_menu->addAction( qtr("Add and play") );
    connect(action, &QAction::triggered, [this, selected]( ) {
        m_model->addAndPlay(selected);
    });

    action = m_menu->addAction( qtr("Enqueue") );
    connect(action, &QAction::triggered, [this, selected]( ) {
        m_model->addToPlaylist(selected);
    });

    bool canBeIndexed = false;
    unsigned countIndexed = 0;
    for (const QModelIndex& idx : selected)
    {
        QVariant canIndex = m_model->data(m_model->index(idx.row()), NetworkMediaModel::NETWORK_CANINDEX );
        if (canIndex.isValid() && canIndex.toBool())
            canBeIndexed = true;
        else
            continue;
        QVariant isIndexed = m_model->data(m_model->index(idx.row()), NetworkMediaModel::NETWORK_INDEXED );
        if (!isIndexed.isValid())
            continue;
        if (isIndexed.toBool())
            ++countIndexed;
    }

    if (canBeIndexed)
    {
        bool removeFromML = countIndexed > 0;
        action = m_menu->addAction(removeFromML
            ? qtr("Remove from Media Library")
            : qtr("Add to Media Library"));

        connect(action, &QAction::triggered, [this, selected, removeFromML]( ) {
            for (const QModelIndex& idx : selected) {
                m_model->setData(m_model->index(idx.row()), !removeFromML, NetworkMediaModel::NETWORK_INDEXED);
            }
        });
    }

    m_menu->popup(pos);
}

NetworkDeviceContextMenu::NetworkDeviceContextMenu(QObject* parent)
    : QObject(parent)
{}

void NetworkDeviceContextMenu::popup(const QModelIndexList& selected, QPoint pos)
{
    if (!m_model)
        return;

    m_menu = std::make_unique<QMenu>();
    QAction* action;

    action = m_menu->addAction( qtr("Add and play") );
    connect(action, &QAction::triggered, [this, selected]( ) {
        m_model->addAndPlay(selected);
    });

    action = m_menu->addAction( qtr("Enqueue") );
    connect(action, &QAction::triggered, [this, selected]( ) {
        m_model->addToPlaylist(selected);
    });

    m_menu->popup(pos);
}

PlaylistContextMenu::PlaylistContextMenu(QObject* parent)
    : QObject(parent)
{}

void PlaylistContextMenu::popup(int selectedIndex, QPoint pos )
{
    if (!m_controler || !m_model || !m_selectionModel)
        return;

    m_menu = std::make_unique<QMenu>();
    QAction* action;

    QList<QUrl> selectedUrlList;
    for (const int modelIndex : m_selectionModel->selectedIndexesFlat())
        selectedUrlList.push_back(m_model->itemAt(modelIndex).getUrl());

    PlaylistItem selectedItem;
    if (selectedIndex >= 0)
        selectedItem = m_model->itemAt(selectedIndex);

    if (selectedItem)
    {
        action = m_menu->addAction( qtr("Play") );
        connect(action, &QAction::triggered, [this, selectedIndex]( ) {
            m_controler->goTo(selectedIndex, true);
        });

        m_menu->addSeparator();
    }

    if (m_controler->currentIndex() != -1)
    {
        action = m_menu->addAction( qtr("Jump to current playing"));
        connect(action, &QAction::triggered, this, &PlaylistContextMenu::jumpToCurrentPlaying);
    }

    if (m_selectionModel->hasSelection()) {
        action = m_menu->addAction( qtr("Stream") );
        connect(action, &QAction::triggered, [selectedUrlList]( ) {
            DialogsProvider::getInstance()->streamingDialog(selectedUrlList, false);
        });

        action = m_menu->addAction( qtr("Save") );
        connect(action, &QAction::triggered, [selectedUrlList]( ) {
            DialogsProvider::getInstance()->streamingDialog(selectedUrlList, true);
        });

        m_menu->addSeparator();
    }

    if (selectedItem) {
        action = m_menu->addAction( qtr("Information") );
        action->setIcon(QIcon(":/menu/info.svg"));
        connect(action, &QAction::triggered, [selectedItem]( ) {
            DialogsProvider::getInstance()->mediaInfoDialog(selectedItem);
        });

        m_menu->addSeparator();

        action = m_menu->addAction( qtr("Show Containing Directory...") );
        action->setIcon(QIcon(":/menu/folder.svg"));
        connect(action, &QAction::triggered, [this, selectedItem]( ) {
            m_controler->explore(selectedItem);
        });

        m_menu->addSeparator();
    }

    action = m_menu->addAction( qtr("Add File...") );
    action->setIcon(QIcon(":/menu/add.svg"));
    connect(action, &QAction::triggered, []( ) {
        DialogsProvider::getInstance()->simpleOpenDialog(false);
    });

    action = m_menu->addAction( qtr("Add Directory...") );
    action->setIcon(QIcon(":/menu/add.svg"));
    connect(action, &QAction::triggered, []( ) {
        DialogsProvider::getInstance()->PLAppendDir();
    });

    action = m_menu->addAction( qtr("Advanced Open...") );
    action->setIcon(QIcon(":/menu/add.svg"));
    connect(action, &QAction::triggered, []( ) {
        DialogsProvider::getInstance()->PLAppendDialog();
    });

    m_menu->addSeparator();

    if (m_selectionModel->hasSelection())
    {
        action = m_menu->addAction( qtr("Save Playlist to File...") );
        connect(action, &QAction::triggered, []( ) {
            DialogsProvider::getInstance()->savePlayingToPlaylist();
        });

        m_menu->addSeparator();

        action = m_menu->addAction( qtr("Remove Selected") );
        action->setIcon(QIcon(":/menu/remove.svg"));
        connect(action, &QAction::triggered, [this]( ) {
            m_model->removeItems(m_selectionModel->selectedIndexesFlat());
        });
    }


    if (m_model->rowCount() > 0)
    {
        action = m_menu->addAction( qtr("Clear the playlist") );
        action->setIcon(QIcon(":/menu/clear.svg"));
        connect(action, &QAction::triggered, [this]( ) {
            m_controler->clear();
        });

        m_menu->addSeparator();

        using namespace vlc::playlist;
        PlaylistController::SortKey currentKey = m_controler->getSortKey();
        PlaylistController::SortOrder currentOrder = m_controler->getSortOrder();

        QMenu* sortMenu = m_menu->addMenu(qtr("Sort by"));
        QActionGroup * group = new QActionGroup(sortMenu);

        auto addSortAction = [&](const QString& label, PlaylistController::SortKey key, PlaylistController::SortOrder order) {
            QAction* action = sortMenu->addAction(label);
            connect(action, &QAction::triggered, this, [this, key, order]( ) {
                m_controler->sort(key, order);
            });
            action->setCheckable(true);
            action->setActionGroup(group);
            if (key == currentKey && currentOrder == order)
                action->setChecked(true);
        };

        for (const QVariant& it: m_controler->getSortKeyTitleList())
        {
            const QVariantMap varmap = it.toMap();

            auto key = static_cast<PlaylistController::SortKey>(varmap.value("key").toInt());
            QString label = varmap.value("text").toString();

            addSortAction(qtr("%1 Ascending").arg(label), key, PlaylistController::SORT_ORDER_ASC);
            addSortAction(qtr("%1 Descending").arg(label), key, PlaylistController::SORT_ORDER_DESC);
        }

        action = m_menu->addAction( qtr("Shuffle the playlist") );
        action->setIcon(QIcon(":/menu/ic_fluent_arrow_shuffle_on.svg"));
        connect(action, &QAction::triggered, this, [this]( ) {
            m_controler->shuffle();
        });

    }

    m_menu->popup(pos);
}
