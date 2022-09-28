/*****************************************************************************
 * Copyright (C) 2019 VLC authors and VideoLAN
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

#ifndef MLNETWORKMEDIAMODEL_HPP
#define MLNETWORKMEDIAMODEL_HPP

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <QAbstractListModel>

#include <vlc_media_library.h>
#include <vlc_media_source.h>
#include <vlc_threads.h>
#include <vlc_cxx_helpers.hpp>

#include "networksourcelistener.hpp"
#include <maininterface/mainctx.hpp>

#include <QSemaphore>
#include <QDateTime>

#include <memory>

using MediaSourcePtr = vlc_shared_data_ptr_type(vlc_media_source_t,
                                vlc_media_source_Hold, vlc_media_source_Release);

using InputItemPtr = vlc_shared_data_ptr_type(input_item_t,
                                              input_item_Hold,
                                              input_item_Release);

class NetworkTreeItem
{
    Q_GADGET
public:
    NetworkTreeItem() : source(nullptr), media(nullptr) {}
    NetworkTreeItem( MediaSourcePtr s, input_item_t* m )
        : source( std::move( s ) )
        , media( m )
    {
    }

    NetworkTreeItem( const NetworkTreeItem& ) = default;
    NetworkTreeItem& operator=( const NetworkTreeItem& ) = default;
    NetworkTreeItem( NetworkTreeItem&& ) = default;
    NetworkTreeItem& operator=( NetworkTreeItem&& ) = default;

    operator bool() const
    {
        return source.get() != nullptr;
    }

    bool isValid() {
        vlc_media_tree_Lock(source->tree);
        input_item_node_t* node;
        bool ret = vlc_media_tree_Find( source->tree, media.get(), &node, nullptr);
        vlc_media_tree_Unlock(source->tree);
        return ret;
    }

    MediaSourcePtr source;
    InputItemPtr media;
};

class PathNode
{
    Q_GADGET
    Q_PROPERTY( QVariant tree READ getTree CONSTANT  FINAL )
    Q_PROPERTY( QString display READ getDisplay CONSTANT  FINAL )

public:
    PathNode() = default;
    PathNode( const NetworkTreeItem &tree, const QString &display )
        : m_tree( QVariant::fromValue(tree) )
        , m_display( display )
    {
    }

    QVariant getTree() const { return m_tree; }
    QString getDisplay() const { return m_display; }

private:
    const QVariant m_tree;
    const QString m_display;
};

Q_DECLARE_METATYPE(PathNode)

class NetworkMediaModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role {
        NETWORK_NAME = Qt::UserRole + 1,
        NETWORK_MRL,
        NETWORK_INDEXED,
        NETWORK_CANINDEX,
        NETWORK_TYPE,
        NETWORK_PROTOCOL,
        NETWORK_TREE,
        NETWORK_SOURCE,
        NETWORK_ARTWORK,
        NETWORK_FILE_SIZE,
        NETWORK_FILE_MODIFIED,
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

    Q_PROPERTY(MainCtx* ctx READ getCtx WRITE setCtx NOTIFY ctxChanged)
    Q_PROPERTY(QVariant tree READ getTree WRITE setTree NOTIFY treeChanged)
    Q_PROPERTY(QVariantList path READ getPath NOTIFY pathChanged)

    Q_PROPERTY(QString name READ getName NOTIFY nameChanged)
    Q_PROPERTY(QUrl url READ getUrl NOTIFY urlChanged)
    Q_PROPERTY(ItemType type READ getType NOTIFY typeChanged)
    Q_PROPERTY(bool indexed READ isIndexed WRITE setIndexed NOTIFY isIndexedChanged)
    Q_PROPERTY(bool canBeIndexed READ canBeIndexed NOTIFY canBeIndexedChanged)
    Q_PROPERTY(bool parsingPending READ getParsingPending NOTIFY parsingPendingChanged)
    Q_PROPERTY(int count READ getCount NOTIFY countChanged)

    explicit NetworkMediaModel(QObject* parent = nullptr);
    ~NetworkMediaModel() override;

    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    int rowCount(const QModelIndex& parent = {}) const override;

    Qt::ItemFlags flags( const QModelIndex& idx ) const override;
    bool setData( const QModelIndex& idx,const QVariant& value, int role ) override;

    void setIndexed(bool indexed);
    void setCtx(MainCtx* ctx);
    void setTree(QVariant tree);

    inline MainCtx* getCtx() const { return m_ctx; }
    inline QVariant getTree() const { return QVariant::fromValue( m_treeItem); }
    inline QVariantList getPath() const { return m_path; }

    inline QString getName() const { return m_name; }
    inline QUrl getUrl() const { return m_url; }
    inline ItemType getType() const { return m_type; }
    inline bool isIndexed() const { return m_indexed; }
    inline bool canBeIndexed() const { return m_canBeIndexed; }
    inline bool getParsingPending() const { return m_parsingPending; }
    int getCount() const;

    Q_INVOKABLE QMap<QString, QVariant> getDataAt(int idx);

    Q_INVOKABLE bool insertIntoPlaylist( const QModelIndexList& itemIdList, ssize_t playlistIndex );
    Q_INVOKABLE bool addToPlaylist( int index );
    Q_INVOKABLE bool addToPlaylist(const QVariantList& itemIdList);
    Q_INVOKABLE bool addToPlaylist(const QModelIndexList& itemIdList);
    Q_INVOKABLE bool addAndPlay( int index );
    Q_INVOKABLE bool addAndPlay(const QVariantList& itemIdList);
    Q_INVOKABLE bool addAndPlay(const QModelIndexList& itemIdList);

    Q_INVOKABLE QVariantList getItemsForIndexes(const QModelIndexList & indexes) const;

signals:
    void nameChanged();
    void urlChanged();
    void typeChanged();
    void isIndexedChanged();
    void canBeIndexedChanged();
    void parsingPendingChanged(bool);
    void countChanged();

    void ctxChanged();
    void treeChanged();
    void isOnProviderListChanged();
    void sdSourceChanged();
    void pathChanged();

private:
    struct Item
    {
        QString name;
        QUrl mainMrl;
        std::vector<QUrl> mrls;
        QString protocol;
        bool indexed;
        ItemType type;
        bool canBeIndexed;
        NetworkTreeItem tree;
        MediaSourcePtr mediaSource;
        QUrl artworkUrl;
        qint64 fileSize;
        QDateTime fileModified;
    };

    bool initializeMediaSources();

    void refreshMediaList(MediaSourcePtr s, std::vector<InputItemPtr> children , bool clear);

    bool canBeIndexed(const QUrl& url , ItemType itemType );

private:
    struct ListenerCb : public NetworkSourceListener::SourceListenerCb {
        ListenerCb(NetworkMediaModel *model) : model(model) {}

        void onItemCleared( MediaSourcePtr mediaSource, input_item_node_t* node ) override;
        void onItemAdded( MediaSourcePtr mediaSource, input_item_node_t* parent, input_item_node_t *const children[], size_t count ) override;
        void onItemRemoved( MediaSourcePtr mediaSource, input_item_node_t * node, input_item_node_t *const children[], size_t count ) override;
        void onItemPreparseEnded( MediaSourcePtr mediaSource, input_item_node_t* node, enum input_item_preparse_status status ) override;

        NetworkMediaModel *model;
    };

    //properties of the current node
    QString m_name;
    QUrl m_url;
    ItemType m_type = ItemType::TYPE_UNKNOWN;
    bool m_indexed = false;
    bool m_canBeIndexed  = false;
    bool m_parsingPending = false;
    QSemaphore m_preparseSem;

    std::vector<Item> m_items;
    MainCtx* m_ctx = nullptr;
    MediaLib* m_mediaLib;
    bool m_hasTree = false;
    NetworkTreeItem m_treeItem;
    std::unique_ptr<NetworkSourceListener> m_listener;
    QVariantList m_path;
};

Q_DECLARE_METATYPE(NetworkTreeItem)

#endif // MLNETWORKMEDIAMODEL_HPP
