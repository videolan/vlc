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
#include "util/qml_main_context.hpp"
#include "medialibrary/medialib.hpp"
#include "medialibrary/mlvideomodel.hpp"
#include "medialibrary/mlgrouplistmodel.hpp"
#include "medialibrary/mlplaylistlistmodel.hpp"
#include "medialibrary/mlplaylistmodel.hpp"
#include "medialibrary/mlalbummodel.hpp"
#include "medialibrary/mlartistmodel.hpp"
#include "medialibrary/mlgenremodel.hpp"
#include "medialibrary/mlalbumtrackmodel.hpp"
#include "medialibrary/mlurlmodel.hpp"
#include "network/networkdevicemodel.hpp"
#include "network/networkmediamodel.hpp"
#include "playlist/playlist_controller.hpp"
#include "playlist/playlist_model.hpp"
#include "dialogs/dialogs_provider.hpp"
#include "maininterface/main_interface.hpp"


#include <QSignalMapper>


static inline void addSubMenu( QMenu *func, QString title, QMenu *bar ) {
    func->setTitle( title );
    bar->addMenu( func);
}

QmlGlobalMenu::QmlGlobalMenu(QObject *parent)
    : VLCMenuBar(parent)
{
}

QmlGlobalMenu::~QmlGlobalMenu()
{
    if (m_menu)
        delete m_menu;
}

void QmlGlobalMenu::popup(QPoint pos)
{
    if (!m_ctx)
        return;

    intf_thread_t* p_intf = m_ctx->getIntf();
    if (!p_intf)
        return;

    if (m_menu)
        delete m_menu;

    m_menu = new QMenu();
    QMenu* submenu;

    submenu = m_menu->addMenu(qtr( "&Media" ));
    FileMenu( p_intf, submenu, p_intf->p_sys->p_mi );

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
    ViewMenu( p_intf, submenu, p_intf->p_sys->p_mi );

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

QmlMenuBar::~QmlMenuBar()
{
    if (m_menu)
        delete m_menu;
}

void QmlMenuBar::popupMenuCommon( QQuickItem* button, std::function<void(QMenu*)> createMenuFunc)
{
    if (!m_ctx || !m_menubar || !button)
        return;

    intf_thread_t* p_intf = m_ctx->getIntf();
    if (!p_intf)
        return;

    if (m_menu)
        delete m_menu;

    m_menu = new QmlMenuBarMenu(this, nullptr);
    createMenuFunc(m_menu);
    m_button = button;
    m_openMenuOnHover = false;
    connect(m_menu, &QMenu::aboutToHide, this, &QmlMenuBar::onMenuClosed);
    QPointF position = button->mapToGlobal(QPoint(0, button->height()));
    m_menu->popup(position.toPoint());
}

void QmlMenuBar::popupMediaMenu( QQuickItem* button )
{
    popupMenuCommon(button, [this](QMenu* menu) {
        intf_thread_t* p_intf = m_ctx->getIntf();
        FileMenu( p_intf, menu , p_intf->p_sys->p_mi );
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
        intf_thread_t* p_intf = m_ctx->getIntf();
        ViewMenu( p_intf, menu, p_intf->p_sys->p_mi );
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

BaseMedialibMenu::BaseMedialibMenu(QObject* parent)
    : QObject(parent)
{}

BaseMedialibMenu::~BaseMedialibMenu()
{
    if (m_menu)
        delete m_menu;
}

void BaseMedialibMenu::medialibAudioContextMenu(MediaLib* ml, const QVariantList& mlId, const QPoint& pos, const QVariantMap& options)
{
    if (m_menu)
        delete m_menu;

    m_menu = new QMenu();
    QAction* action;

    action = m_menu->addAction( qtr("Add and play") );
    connect(action, &QAction::triggered, [ml, mlId]( ) {
        ml->addAndPlay(mlId);
    });

    action = m_menu->addAction( qtr("Enqueue") );
    connect(action, &QAction::triggered, [ml, mlId]( ) {
        ml->addToPlaylist(mlId);
    });

    action = m_menu->addAction( qtr("Add to playlist") );
    connect(action, &QAction::triggered, [mlId]( ) {
        DialogsProvider::getInstance()->playlistsDialog(mlId);
    });

    if (options.contains("information") && options["information"].type() == QVariant::Int) {

        action = m_menu->addAction( qtr("Information") );
        QSignalMapper* sigmapper = new QSignalMapper(m_menu);
        connect(action, &QAction::triggered, sigmapper, QOverload<>::of(&QSignalMapper::map));
        sigmapper->setMapping(action, options["information"].toInt());
#if QT_VERSION >= QT_VERSION_CHECK(5,15,0)
        connect(sigmapper, &QSignalMapper::mappedInt, this, &BaseMedialibMenu   ::showMediaInformation);
#else
        connect(sigmapper, QOverload<int>::of(&QSignalMapper::mapped), this, &BaseMedialibMenu::showMediaInformation);
#endif
    }
    m_menu->popup(pos);
}

AlbumContextMenu::AlbumContextMenu(QObject* parent)
    : BaseMedialibMenu(parent)
{}

void AlbumContextMenu::popup(const QModelIndexList& selected, QPoint pos, QVariantMap options)
{
    BaseMedialibMenu::popup(m_model, MLAlbumModel::ALBUM_ID, selected, pos, options);
}


ArtistContextMenu::ArtistContextMenu(QObject* parent)
    : BaseMedialibMenu(parent)
{}

void ArtistContextMenu::popup(const QModelIndexList &selected, QPoint pos, QVariantMap options)
{
    BaseMedialibMenu::popup(m_model, MLArtistModel::ARTIST_ID, selected, pos, options);
}

GenreContextMenu::GenreContextMenu(QObject* parent)
    : BaseMedialibMenu(parent)
{}

void GenreContextMenu::popup(const QModelIndexList& selected, QPoint pos, QVariantMap options)
{
    BaseMedialibMenu::popup(m_model, MLGenreModel::GENRE_ID, selected, pos, options);
}

AlbumTrackContextMenu::AlbumTrackContextMenu(QObject* parent)
    : BaseMedialibMenu(parent)
{}

void AlbumTrackContextMenu::popup(const QModelIndexList &selected, QPoint pos, QVariantMap options)
{
    BaseMedialibMenu::popup(m_model, MLAlbumTrackModel::TRACK_ID, selected, pos, options);
}

URLContextMenu::URLContextMenu(QObject* parent)
    : BaseMedialibMenu(parent)
{}

void URLContextMenu::popup(const QModelIndexList &selected, QPoint pos, QVariantMap options)
{
    BaseMedialibMenu::popup(m_model, MLUrlModel::URL_ID, selected, pos, options);
}


VideoContextMenu::VideoContextMenu(QObject* parent)
    : QObject(parent)
{}

VideoContextMenu::~VideoContextMenu()
{
    if (m_menu)
        delete m_menu;
}

void VideoContextMenu::popup(const QModelIndexList& selected, QPoint pos, QVariantMap options)
{
    if (!m_model)
        return;

    if (m_menu)
        delete m_menu;

    m_menu = new QMenu();
    QAction* action;

    MediaLib* ml= m_model->ml();

    QVariantList itemIdList;
    for (const QModelIndex& modelIndex : selected)
        itemIdList.push_back(m_model->data(modelIndex, MLVideoModel::VIDEO_ID));

    action = m_menu->addAction( qtr("Add and play") );

    connect(action, &QAction::triggered, [ml, itemIdList]( ) {
        ml->addAndPlay(itemIdList);
    });

    action = m_menu->addAction( qtr("Enqueue") );
    connect(action, &QAction::triggered, [ml, itemIdList]( ) {
        ml->addToPlaylist(itemIdList);
    });

    action = m_menu->addAction( qtr("Add to playlist") );
    connect(action, &QAction::triggered, [itemIdList]( ) {
        DialogsProvider::getInstance()->playlistsDialog(itemIdList);
    });

    action = m_menu->addAction( qtr("Play as audio") );
    connect(action, &QAction::triggered, [ml, itemIdList]( ) {
        QStringList options({":no-video"});
        ml->addAndPlay(itemIdList, &options);
    });

    if (options.contains("information") && options["information"].type() == QVariant::Int) {
        action = m_menu->addAction( qtr("Information") );
        QSignalMapper* sigmapper = new QSignalMapper(m_menu);
        connect(action, &QAction::triggered, sigmapper, QOverload<>::of(&QSignalMapper::map));
        sigmapper->setMapping(action, options["information"].toInt());
#if QT_VERSION >= QT_VERSION_CHECK(5,15,0)
        connect(sigmapper, &QSignalMapper::mappedInt, this, &VideoContextMenu::showMediaInformation);
#else
        connect(sigmapper, QOverload<int>::of(&QSignalMapper::mapped), this, &VideoContextMenu::showMediaInformation);
#endif
    }

    m_menu->popup(pos);
}

//=================================================================================================
// GroupListContextMenu
//=================================================================================================

GroupListContextMenu::GroupListContextMenu(QObject * parent) : QObject(parent) {}

GroupListContextMenu::~GroupListContextMenu() /* override */
{
    if (m_menu)
        delete m_menu;
}

void GroupListContextMenu::popup(const QModelIndexList & selected, QPoint pos, QVariantMap)
{
    if (m_model == nullptr)
        return;

    if (m_menu)
        delete m_menu;

    QVariantList ids;

    for (const QModelIndex & modelIndex : selected)
        ids.push_back(m_model->data(modelIndex, MLGroupListModel::GROUP_ID));

    m_menu = new QMenu();

    MediaLib * ml = m_model->ml();

    QAction * action = m_menu->addAction(qtr("Add and play"));

    connect(action, &QAction::triggered, [ml, ids]() {
        ml->addAndPlay(ids);
    });

    action = m_menu->addAction(qtr("Enqueue"));

    connect(action, &QAction::triggered, [ml, ids]() {
        ml->addToPlaylist(ids);
    });

    m_menu->popup(pos);
}

//=================================================================================================
// PlaylistListContextMenu
//=================================================================================================

PlaylistListContextMenu::PlaylistListContextMenu(QObject * parent)
    : QObject(parent)
{}

PlaylistListContextMenu::~PlaylistListContextMenu() /* override */
{
    if (m_menu)
        delete m_menu;
}

void PlaylistListContextMenu::popup(const QModelIndexList & selected, QPoint pos, QVariantMap)
{
    if (!m_model)
        return;

    if (m_menu)
        delete m_menu;

    QVariantList ids;

    for (const QModelIndex & modelIndex : selected)
        ids.push_back(m_model->data(modelIndex, MLPlaylistListModel::PLAYLIST_ID));

    m_menu = new QMenu();

    MediaLib * ml = m_model->ml();

    QAction * action = m_menu->addAction(qtr("Add and play"));

    connect(action, &QAction::triggered, [ml, ids]() {
        ml->addAndPlay(ids);
    });

    action = m_menu->addAction(qtr("Enqueue"));

    connect(action, &QAction::triggered, [ml, ids]() {
        ml->addToPlaylist(ids);
    });

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

PlaylistMediaContextMenu::~PlaylistMediaContextMenu() /* override */
{
    if (m_menu)
        delete m_menu;
}

void PlaylistMediaContextMenu::popup(const QModelIndexList & selected, QPoint pos,
                                     QVariantMap options)
{
    if (!m_model)
        return;

    if (m_menu)
        delete m_menu;

    QVariantList ids;

    for (const QModelIndex& modelIndex : selected)
        ids.push_back(m_model->data(modelIndex, MLPlaylistModel::MEDIA_ID));

    m_menu = new QMenu();

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

    connect(action, &QAction::triggered, [ml, ids]() {
        DialogsProvider::getInstance()->playlistsDialog(ids);
    });

    action = m_menu->addAction(qtr("Play as audio"));

    connect(action, &QAction::triggered, [ml, ids]() {
        QStringList options({":no-video"});
        ml->addAndPlay(ids, &options);
    });

    if (options.contains("information") && options["information"].type() == QVariant::Int) {
        action = m_menu->addAction(qtr("Information"));

        QSignalMapper * mapper = new QSignalMapper(m_menu);

        connect(action, &QAction::triggered, mapper, QOverload<>::of(&QSignalMapper::map));

        mapper->setMapping(action, options["information"].toInt());
#if QT_VERSION >= QT_VERSION_CHECK(5,15,0)
        connect(mapper, &QSignalMapper::mappedInt, this,
                &PlaylistMediaContextMenu::showMediaInformation);
#else
        connect(mapper, QOverload<int>::of(&QSignalMapper::mapped), this,
                &PlaylistMediaContextMenu::showMediaInformation);
#endif
    }

    m_menu->addSeparator();

    action = m_menu->addAction(qtr("Remove Selected"));

    action->setIcon(QIcon(":/buttons/playlist/playlist_remove.svg"));

    connect(action, &QAction::triggered, [this, selected]() {
        m_model->remove(selected);
    });

    m_menu->popup(pos);
}

//=================================================================================================

NetworkMediaContextMenu::NetworkMediaContextMenu(QObject* parent)
    : QObject(parent)
{}

NetworkMediaContextMenu::~NetworkMediaContextMenu()
{
    if (m_menu)
        delete m_menu;
}

void NetworkMediaContextMenu::popup(const QModelIndexList& selected, QPoint pos)
{
    if (!m_model)
        return;

    if (m_menu)
        delete m_menu;

    m_menu = new QMenu();
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

NetworkDeviceContextMenu::~NetworkDeviceContextMenu()
{
    if (m_menu)
        delete m_menu;
}

void NetworkDeviceContextMenu::popup(const QModelIndexList& selected, QPoint pos)
{
    if (!m_model)
        return;

    if (m_menu)
        delete m_menu;

    QMenu* menu = new QMenu();
    QAction* action;

    menu->setAttribute(Qt::WA_DeleteOnClose);

    action = menu->addAction( qtr("Add and play") );
    connect(action, &QAction::triggered, [this, selected]( ) {
        m_model->addAndPlay(selected);
    });

    action = menu->addAction( qtr("Enqueue") );
    connect(action, &QAction::triggered, [this, selected]( ) {
        m_model->addToPlaylist(selected);
    });

    menu->popup(pos);
}

PlaylistContextMenu::PlaylistContextMenu(QObject* parent)
    : QObject(parent)
{}

PlaylistContextMenu::~PlaylistContextMenu()
{
    if (m_menu)
        delete m_menu;
}
void PlaylistContextMenu::popup(int currentIndex, QPoint pos )
{
    if (!m_controler || !m_model)
        return;

    if (m_menu)
        delete m_menu;

    m_menu = new QMenu();
    QAction* action;

    QList<QUrl> selectedUrlList;
    for (const int modelIndex : m_model->getSelection())
        selectedUrlList.push_back(m_model->itemAt(modelIndex).getUrl());

    PlaylistItem currentItem;
    if (currentIndex >= 0)
        currentItem = m_model->itemAt(currentIndex);

    if (currentItem)
    {
        action = m_menu->addAction( qtr("Play") );
        connect(action, &QAction::triggered, [this, currentIndex]( ) {
            m_controler->goTo(currentIndex, true);
        });

        m_menu->addSeparator();
    }

    if (m_model->getSelectedCount() > 0) {
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

    if (currentItem) {
        action = m_menu->addAction( qtr("Information") );
        action->setIcon(QIcon(":/menu/info.svg"));
        connect(action, &QAction::triggered, [currentItem]( ) {
            DialogsProvider::getInstance()->mediaInfoDialog(currentItem);
        });

        m_menu->addSeparator();

        action = m_menu->addAction( qtr("Show Containing Directory...") );
        action->setIcon(QIcon(":/type/folder-grey.svg"));
        connect(action, &QAction::triggered, [currentItem]( ) {
            DialogsProvider::getInstance()->mediaInfoDialog(currentItem);
        });

        m_menu->addSeparator();
    }

    action = m_menu->addAction( qtr("Add File...") );
    action->setIcon(QIcon(":/buttons/playlist/playlist_add.svg"));
    connect(action, &QAction::triggered, []( ) {
        DialogsProvider::getInstance()->simpleOpenDialog(false);
    });

    action = m_menu->addAction( qtr("Add Directory...") );
    action->setIcon(QIcon(":/buttons/playlist/playlist_add.svg"));
    connect(action, &QAction::triggered, []( ) {
        DialogsProvider::getInstance()->PLAppendDir();
    });

    action = m_menu->addAction( qtr("Advanced Open...") );
    action->setIcon(QIcon(":/buttons/playlist/playlist_add.svg"));
    connect(action, &QAction::triggered, []( ) {
        DialogsProvider::getInstance()->PLAppendDialog();
    });

    m_menu->addSeparator();

    if (m_model->getSelectedCount() > 0)
    {
        action = m_menu->addAction( qtr("Save Playlist to File...") );
        connect(action, &QAction::triggered, []( ) {
            DialogsProvider::getInstance()->savePlayingToPlaylist();
        });

        m_menu->addSeparator();

        action = m_menu->addAction( qtr("Remove Selected") );
        action->setIcon(QIcon(":/buttons/playlist/playlist_remove.svg"));
        connect(action, &QAction::triggered, [this]( ) {
            m_model->removeItems(m_model->getSelection());
        });
    }

    action = m_menu->addAction( qtr("Clear the playlist") );
    action->setIcon(QIcon(":/toolbar/clear.svg"));
    connect(action, &QAction::triggered, [this]( ) {
        m_controler->clear();
    });

    m_menu->popup(pos);
}
