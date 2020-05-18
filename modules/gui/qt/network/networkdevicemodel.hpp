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

#include <vlc_media_library.h>
#include <vlc_media_source.h>
#include <vlc_threads.h>
#include <vlc_cxx_helpers.hpp>

#include <util/qml_main_context.hpp>
#include "networksourcelistener.hpp"

#include <memory>

class NetworkDeviceModel : public QAbstractListModel, public NetworkSourceListener::SourceListenerCb
{
    Q_OBJECT
public:
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


    Q_PROPERTY(QmlMainContext* ctx READ getCtx WRITE setCtx NOTIFY ctxChanged)
    Q_PROPERTY(SDCatType sd_source READ getSdSource WRITE setSdSource NOTIFY sdSourceChanged)
    Q_PROPERTY(int count READ getCount NOTIFY countChanged)

public:
    NetworkDeviceModel( QObject* parent = nullptr );

    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    int rowCount(const QModelIndex& parent) const override;

    void setCtx(QmlMainContext* ctx);
    void setSdSource(SDCatType s);

    inline QmlMainContext* getCtx() { return m_ctx; }
    inline SDCatType getSdSource() { return m_sdSource; }

    int getCount() const;

    Q_INVOKABLE bool addToPlaylist( int index );
    Q_INVOKABLE bool addToPlaylist(const QVariantList& itemIdList);
    Q_INVOKABLE bool addToPlaylist(const QModelIndexList& itemIdList);
    Q_INVOKABLE bool addAndPlay( int index );
    Q_INVOKABLE bool addAndPlay(const QVariantList& itemIdList);
    Q_INVOKABLE bool addAndPlay(const QModelIndexList& itemIdList);

    Q_INVOKABLE QMap<QString, QVariant> getDataAt(int index);

signals:
    void ctxChanged();
    void sdSourceChanged();
    void countChanged();

private:
    using MediaSourcePtr = vlc_shared_data_ptr_type(vlc_media_source_t,
                                    vlc_media_source_Hold, vlc_media_source_Release);

    using InputItemPtr = vlc_shared_data_ptr_type(input_item_t,
                                                  input_item_Hold,
                                                  input_item_Release);

    struct Item
    {
        QString name;
        QUrl mainMrl;
        std::vector<QUrl> mrls;
        QString protocol;
        ItemType type;
        MediaSourcePtr mediaSource;
        InputItemPtr inputItem;
        QUrl artworkUrl;
    };

    bool initializeMediaSources();
    void onItemCleared( MediaSourcePtr mediaSource, input_item_node_t* node ) override;
    void onItemAdded( MediaSourcePtr mediaSource, input_item_node_t* parent, input_item_node_t *const children[], size_t count ) override;
    void onItemRemoved( MediaSourcePtr mediaSource, input_item_node_t * node, input_item_node_t *const children[], size_t count ) override;
    inline void onItemPreparseEnded( MediaSourcePtr, input_item_node_t *, enum input_item_preparse_status ) override {}

    void refreshDeviceList(MediaSourcePtr mediaSource, input_item_node_t* const children[], size_t count , bool clear);

private:
    std::vector<Item> m_items;
    QmlMainContext* m_ctx = nullptr;
    vlc_medialibrary_t* m_ml = nullptr;
    SDCatType m_sdSource = CAT_UNDEFINED;

    std::vector<std::unique_ptr<NetworkSourceListener>> m_listeners;
};

#endif // MLNETWORKDEVICEMODEL_HPP
