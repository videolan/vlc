/*****************************************************************************
 * Copyright (C) 2019 VLC authors and VideoLAN
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

#ifndef MLNETWORKDEVICEMODEL_HPP
#define MLNETWORKDEVICEMODEL_HPP

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <QAbstractListModel>
#include <QUrl>

#include <vlc_media_source.h>
#include <vlc_cxx_helpers.hpp>

#include "mediatreelistener.hpp"
#include "util/cliplistmodel.hpp"

#include <memory>

class MainCtx;

struct NetworkDeviceItem;

class NetworkDeviceModel : public ClipListModel<NetworkDeviceItem>
{
    Q_OBJECT

    Q_PROPERTY(MainCtx* ctx READ getCtx WRITE setCtx NOTIFY ctxChanged FINAL)
    Q_PROPERTY(SDCatType sd_source READ getSdSource WRITE setSdSource NOTIFY sdSourceChanged FINAL)
    Q_PROPERTY(QString name READ getName NOTIFY nameChanged FINAL)
    Q_PROPERTY(QString source_name READ getSourceName WRITE setSourceName NOTIFY sourceNameChanged FINAL)

public: // Enums
    enum Role {
        NETWORK_NAME = Qt::UserRole + 1,
        NETWORK_MRL,
        NETWORK_TYPE,
        NETWORK_PROTOCOL,
        NETWORK_SOURCE,
        NETWORK_TREE,
        NETWORK_ARTWORK,
    };

    enum ItemType{
        // qt version of input_item_type_e
        TYPE_UNKNOWN = ITEM_TYPE_UNKNOWN,
        TYPE_FILE,
        TYPE_DIRECTORY,
        TYPE_DISC,
        TYPE_CARD,
        TYPE_STREAM,
        TYPE_PLAYLIST,
        TYPE_NODE,
    };
    Q_ENUM( ItemType )

    enum SDCatType{
        // qt version of input_item_type_e
        CAT_UNDEFINED = 0,
        CAT_DEVICES = SD_CAT_DEVICES,
        CAT_LAN,
        CAT_INTERNET,
        CAT_MYCOMPUTER,
    };
    Q_ENUM( SDCatType )

public: // Declarations
    using MediaSourcePtr = vlc_shared_data_ptr_type(vlc_media_source_t,
                                    vlc_media_source_Hold, vlc_media_source_Release);

    using MediaTreePtr = vlc_shared_data_ptr_type(vlc_media_tree_t,
                                                  vlc_media_tree_Hold,
                                                  vlc_media_tree_Release);

    using InputItemPtr = vlc_shared_data_ptr_type(input_item_t,
                                                  input_item_Hold,
                                                  input_item_Release);

public:
    NetworkDeviceModel( QObject* parent = nullptr );

    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setCtx(MainCtx* ctx);
    void setSdSource(SDCatType s);
    void setSourceName(const QString& sourceName);

    inline MainCtx* getCtx() { return m_ctx; }
    inline SDCatType getSdSource() { return m_sdSource; }
    inline QString getName() { return m_name; }
    inline QString getSourceName() { return m_sourceName; }

    Q_INVOKABLE bool insertIntoPlaylist( const QModelIndexList& itemIdList, ssize_t playlistIndex );
    Q_INVOKABLE bool addToPlaylist( int index );
    Q_INVOKABLE bool addToPlaylist(const QVariantList& itemIdList);
    Q_INVOKABLE bool addToPlaylist(const QModelIndexList& itemIdList);
    Q_INVOKABLE bool addAndPlay( int index );
    Q_INVOKABLE bool addAndPlay(const QVariantList& itemIdList);
    Q_INVOKABLE bool addAndPlay(const QModelIndexList& itemIdList);

    Q_INVOKABLE QMap<QString, QVariant> getDataAt(int index);

    Q_INVOKABLE QVariantList getItemsForIndexes(const QModelIndexList & indexes) const;

protected: // ClipListModel implementation
    void onUpdateSort(const QString & criteria, Qt::SortOrder order) override;

signals:
    void ctxChanged();
    void sdSourceChanged();
    void sourceNameChanged();
    void nameChanged();

private:
    bool initializeMediaSources();

    void refreshDeviceList(MediaSourcePtr mediaSource, input_item_node_t* const children[], size_t count , bool clear);

    void addItems(const std::vector<InputItemPtr> & inputList, const MediaSourcePtr & mediaSource);

    void eraseItem(std::vector<NetworkDeviceItem>::iterator & it, int index, int count);

private: // Static functions
    static bool matchItem(const NetworkDeviceItem & a, const NetworkDeviceItem & b);

    static bool ascendingName(const NetworkDeviceItem & a, const NetworkDeviceItem & b);
    static bool ascendingMrl (const NetworkDeviceItem & a, const NetworkDeviceItem & b);

    static bool descendingName(const NetworkDeviceItem & a, const NetworkDeviceItem & b);
    static bool descendingMrl (const NetworkDeviceItem & a, const NetworkDeviceItem & b);

private:
    struct ListenerCb : public MediaTreeListener::MediaTreeListenerCb {
        ListenerCb(NetworkDeviceModel *model, MediaSourcePtr mediaSource)
            : model(model)
            , mediaSource(std::move(mediaSource))
        {}

        void onItemCleared( MediaTreePtr tree, input_item_node_t* node ) override;
        void onItemAdded( MediaTreePtr tree, input_item_node_t* parent, input_item_node_t *const children[], size_t count ) override;
        void onItemRemoved( MediaTreePtr tree, input_item_node_t * node, input_item_node_t *const children[], size_t count ) override;
        inline void onItemPreparseEnded( MediaTreePtr, input_item_node_t *, enum input_item_preparse_status ) override {}

        NetworkDeviceModel *model;
        MediaSourcePtr mediaSource;
    };

    MainCtx* m_ctx = nullptr;
    SDCatType m_sdSource = CAT_UNDEFINED;
    QString m_sourceName; // '*' -> all sources
    QString m_name; // source long name

    std::vector<std::unique_ptr<MediaTreeListener>> m_listeners;
};

struct NetworkDeviceItem
{
    QString name;
    QUrl mainMrl;
    std::vector<QUrl> mrls;
    QString protocol;
    NetworkDeviceModel::ItemType type;
    NetworkDeviceModel::MediaSourcePtr mediaSource;
    NetworkDeviceModel::InputItemPtr inputItem;
    QUrl artworkUrl;
};

#endif // MLNETWORKDEVICEMODEL_HPP
