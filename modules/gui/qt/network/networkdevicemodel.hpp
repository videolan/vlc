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

#include "util/base_model.hpp"
#include "util/shared_input_item.hpp"

#include <memory>

Q_MOC_INCLUDE("maininterface/mainctx.hpp")

class MainCtx;

class NetworkDeviceModelPrivate;
class NetworkDeviceModel : public BaseModel
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
public:
    NetworkDeviceModel( QObject* parent = nullptr );
    virtual ~NetworkDeviceModel() = default;
protected:
    NetworkDeviceModel( NetworkDeviceModelPrivate* priv, QObject* parent = nullptr);

public:
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setCtx(MainCtx* ctx);
    void setSdSource(SDCatType s);
    void setSourceName(const QString& sourceName);

    inline MainCtx* getCtx() const { return m_ctx; }
    inline SDCatType getSdSource() const { return m_sdSource; }
    inline QString getName() const { return m_name; }
    inline QString getSourceName() const { return m_sourceName; }

    Q_INVOKABLE bool insertIntoPlaylist( const QModelIndexList& itemIdList, ssize_t playlistIndex );
    Q_INVOKABLE bool addToPlaylist(int row );
    Q_INVOKABLE bool addToPlaylist(const QVariantList& itemIdList);
    Q_INVOKABLE bool addToPlaylist(const QModelIndexList& itemIdList);
    Q_INVOKABLE bool addAndPlay(int row );
    Q_INVOKABLE bool addAndPlay(const QVariantList& itemIdList);
    Q_INVOKABLE bool addAndPlay(const QModelIndexList& itemIdList);

    Q_INVOKABLE QVariantList getItemsForIndexes(const QModelIndexList & indexes) const;

signals:
    void ctxChanged();
    void sdSourceChanged();
    void sourceNameChanged();
    void nameChanged();
    void itemsUpdated();

private:
    void refreshDeviceList(MediaSourcePtr mediaSource, input_item_node_t* const children[], size_t count , bool clear);
    bool initializeMediaSources();

private:
    MainCtx* m_ctx = nullptr;
    SDCatType m_sdSource = CAT_UNDEFINED;
    QString m_sourceName; // '*' -> all sources
    QString m_name; // source long name

    struct ListenerCb;
    Q_DECLARE_PRIVATE(NetworkDeviceModel)
};

#endif // MLNETWORKDEVICEMODEL_HPP
