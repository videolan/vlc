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

#include <QAbstractListModel>
#include <QUrl>

#include "vlcmediasourcewrapper.hpp"
#include "util/shared_input_item.hpp"
#include "devicesourceprovider.hpp"

#include "networkbasemodel.hpp"

#include <memory>

Q_MOC_INCLUDE( "maininterface/mainctx.hpp" )

class MainCtx;

class NetworkTreeItem
{
    Q_GADGET
public:
    NetworkTreeItem() : source(nullptr), tree(nullptr), media(nullptr) {}

    NetworkTreeItem( SharedMediaSourceModel source, const SharedInputItem& item )
        : source(source)
        , tree(source->getTree())
        , media(item)
    {
    }

    NetworkTreeItem( MediaTreePtr tree, const SharedInputItem& item )
        : source(nullptr)
        , tree(tree)
        , media(item)
    {
    }

    //build a NetworkTreeItem with the same source/tree as parent
    NetworkTreeItem(NetworkTreeItem& parent, const SharedInputItem& item)
        : source(parent.source)
        , tree(parent.tree)
        , media(item)
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
        MediaTreeLocker lock{ tree };
        input_item_node_t* node;
        return vlc_media_tree_Find( tree.get(), media.get(), &node, nullptr);
    }

    SharedMediaSourceModel source;
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
class NetworkMediaModel : public NetworkBaseModel
{
    Q_OBJECT

public:
    enum Role {
        NETWORK_INDEXED = NETWORK_BASE_MAX,
        NETWORK_CANINDEX,
        NETWORK_TREE,
        NETWORK_FILE_SIZE,
        NETWORK_FILE_MODIFIED,
        NETWORK_MEDIA,
        NETWORK_MEDIA_PROGRESS,
        NETWORK_MEDIA_DURATION,
        URL
    };

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

    MainCtx* getCtx() const;
    QVariant getTree() const;
    QVariantList getPath() const;

    QString getName() const;
    QUrl getUrl() const;
    ItemType getType() const;
    bool isIndexed() const;
    bool canBeIndexed() const;

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
    Q_DECLARE_PRIVATE(NetworkMediaModel);
};

#endif // MLNETWORKMEDIAMODEL_HPP
