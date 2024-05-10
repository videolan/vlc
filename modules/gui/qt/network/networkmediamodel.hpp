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
#include <QUrl>

#include <vlc_media_source.h>
#include <vlc_cxx_helpers.hpp>

#include "util/shared_input_item.hpp"
#include "util/base_model.hpp"

#include <memory>

Q_MOC_INCLUDE( "maininterface/mainctx.hpp" )

class MainCtx;

using MediaSourcePtr = vlc_shared_data_ptr_type(vlc_media_source_t,
                                vlc_media_source_Hold, vlc_media_source_Release);

using MediaTreePtr = vlc_shared_data_ptr_type(vlc_media_tree_t,
                                              vlc_media_tree_Hold,
                                              vlc_media_tree_Release);

class NetworkTreeItem
{
    Q_GADGET
public:
    NetworkTreeItem() : tree(nullptr), media(nullptr) {}
    NetworkTreeItem( MediaTreePtr tree, input_item_t* m )
        : tree( std::move( tree ) )
        , media( m )
    {
    }

    NetworkTreeItem( const NetworkTreeItem& ) = default;
    NetworkTreeItem& operator=( const NetworkTreeItem& ) = default;
    NetworkTreeItem( NetworkTreeItem&& ) = default;
    NetworkTreeItem& operator=( NetworkTreeItem&& ) = default;

    operator bool() const
    {
        return tree.get() != nullptr;
    }

    bool isValid() {
        vlc_media_tree_Lock(tree.get());
        input_item_node_t* node;
        bool ret = vlc_media_tree_Find( tree.get(), media.get(), &node, nullptr);
        vlc_media_tree_Unlock(tree.get());
        return ret;
    }

    MediaTreePtr tree;
    SharedInputItem media;
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

class NetworkMediaModelPrivate;
class NetworkMediaModel : public BaseModel
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

    explicit NetworkMediaModel(QObject* parent = nullptr);
    ~NetworkMediaModel() override;

    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

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

    void ctxChanged();
    void treeChanged();
    void isOnProviderListChanged();
    void sdSourceChanged();
    void pathChanged();

private:
    //properties of the current node
    QString m_name;
    QUrl m_url;
    ItemType m_type = ItemType::TYPE_UNKNOWN;
    bool m_indexed = false;
    bool m_canBeIndexed  = false;

    MainCtx* m_ctx = nullptr;
    NetworkTreeItem m_treeItem;
    QVariantList m_path;

    struct ListenerCb;
    Q_DECLARE_PRIVATE(NetworkMediaModel);
};

#endif // MLNETWORKMEDIAMODEL_HPP
