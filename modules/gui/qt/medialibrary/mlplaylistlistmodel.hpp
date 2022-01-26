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
struct vlc_medialibrary_t;
class MLPlaylist;

class MLPlaylistListModel : public MLBaseModel
{
    Q_OBJECT

    Q_PROPERTY(QSize coverSize READ coverSize WRITE setCoverSize NOTIFY coverSizeChanged FINAL)

    Q_PROPERTY(QString coverDefault READ coverDefault WRITE setCoverDefault
               NOTIFY coverDefaultChanged FINAL)

    Q_PROPERTY(QString coverPrefix READ coverPrefix WRITE setCoverPrefix NOTIFY coverPrefixChanged FINAL)

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
    explicit MLPlaylistListModel(QObject * parent = nullptr);

public: // Interface
    Q_INVOKABLE void create(const QString & name, const QVariantList& initialItems);

    Q_INVOKABLE bool append(const MLItemId & playlistId, const QVariantList & ids);

    Q_INVOKABLE bool deletePlaylists(const QVariantList & ids);

    MLItemId getItemId(int index) const;

public: // QAbstractItemModel implementation
    QHash<int, QByteArray> roleNames() const override;

public: // QAbstractItemModel reimplementation
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;

protected: // MLBaseModel implementation
    QVariant itemRoleData(MLItem* item, int role = Qt::DisplayRole) const override;

    vlc_ml_sorting_criteria_t roleToCriteria(int role) const override;

    std::unique_ptr<MLBaseModel::BaseLoader> createLoader() const override;

private: // Functions
    QString getCover(MLPlaylist * playlist) const;

    void endTransaction();

private: // MLBaseModel implementation
    void onVlcMlEvent(const MLEvent & event) override;

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

    bool m_transactionPending = false;
    bool m_resetAfterTransaction = false;

private:
    struct Loader : public MLBaseModel::BaseLoader
    {
        Loader(const MLPlaylistListModel & model);

        size_t count(vlc_medialibrary_t* ml) const override;

        std::vector<std::unique_ptr<MLItem>> load(vlc_medialibrary_t* ml, size_t index, size_t count) const override;

        std::unique_ptr<MLItem> loadItemById(vlc_medialibrary_t* ml, MLItemId itemId) const override;
    };
};

#endif // MLPLAYLISTLISTMODEL_HPP
