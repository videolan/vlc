/*****************************************************************************
 * Copyright (C) 2021 VLC authors and VideoLAN
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

#ifndef MLPLAYLISTLISTMODEL_HPP
#define MLPLAYLISTLISTMODEL_HPP

// MediaLibrary includes
#include "mlbasemodel.hpp"

// Forward declarations
class vlc_medialibrary_t;
class MLPlaylist;

class MLPlaylistListModel : public MLBaseModel
{
    Q_OBJECT

    Q_PROPERTY(QSize coverSize READ coverSize WRITE setCoverSize NOTIFY coverSizeChanged)

    Q_PROPERTY(QString coverDefault READ coverDefault WRITE setCoverDefault
               NOTIFY coverDefaultChanged)

    Q_PROPERTY(QString coverPrefix READ coverPrefix WRITE setCoverPrefix NOTIFY coverPrefixChanged)

public:
    enum Roles
    {
        PLAYLIST_ID = Qt::UserRole + 1,
        PLAYLIST_NAME,
        PLAYLIST_THUMBNAIL,
        PLAYLIST_DURATION,
        PLAYLIST_COUNT
    };

public:
    MLPlaylistListModel(vlc_medialibrary_t * ml, QObject * parent = nullptr);

    explicit MLPlaylistListModel(QObject * parent = nullptr);

public: // Interface
    Q_INVOKABLE MLItemId create(const QString & name);

    Q_INVOKABLE bool append(const MLItemId & playlistId, const QVariantList & ids);

    Q_INVOKABLE bool deletePlaylists(const QVariantList & ids);

    MLItemId getItemId(int index) const;

public: // QAbstractItemModel implementation
    QHash<int, QByteArray> roleNames() const override;

    QVariant data(const QModelIndex & index, int role = Qt::DisplayRole) const override;

public: // QAbstractItemModel reimplementation
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;

protected: // MLBaseModel implementation
    vlc_ml_sorting_criteria_t roleToCriteria(int role) const override;

    ListCacheLoader<std::unique_ptr<MLItem>> * createLoader() const override;

private: // Functions
    QString getCover(MLPlaylist * playlist, int index) const;

private: // MLBaseModel implementation
    void onVlcMlEvent(const MLEvent & event) override;

    void thumbnailUpdated(int idx) override;

private slots:
    void onCover();

signals:
    void coverSizeChanged   ();
    void coverDefaultChanged();
    void coverPrefixChanged ();

public: // Properties
    QSize coverSize() const;
    void  setCoverSize(const QSize & size);

    QString coverDefault() const;
    void    setCoverDefault(const QString & fileName);

    QString coverPrefix() const;
    void    setCoverPrefix(const QString & prefix);

private: // Variables
    QSize   m_coverSize;
    QString m_coverDefault;
    QString m_coverPrefix;

private:
    struct Loader : public MLBaseModel::BaseLoader
    {
        Loader(const MLPlaylistListModel & model);

        size_t count() const override;

        std::vector<std::unique_ptr<MLItem>> load(size_t index, size_t count) const override;
    };
};

#endif // MLPLAYLISTLISTMODEL_HPP
