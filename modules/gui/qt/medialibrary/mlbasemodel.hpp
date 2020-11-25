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

#ifndef MLBASEMODEL_HPP
#define MLBASEMODEL_HPP

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "vlc_common.h"

#include <memory>
#include <QObject>
#include <QAbstractListModel>
#include "vlc_media_library.h"
#include "mlqmltypes.hpp"
#include "medialib.hpp"
#include <memory>
#include "mlevent.hpp"
#include "mlqueryparams.hpp"
#include "util/listcacheloader.hpp"

template <typename T>
class ListCache;

class MediaLib;

class MLBaseModel : public QAbstractListModel
{
    Q_OBJECT

public:
    explicit MLBaseModel(QObject *parent = nullptr);
    virtual ~MLBaseModel();

    Q_INVOKABLE void sortByColumn(QByteArray name, Qt::SortOrder order);

    Q_PROPERTY( MLItemId parentId READ parentId WRITE setParentId NOTIFY parentIdChanged RESET unsetParentId )
    Q_PROPERTY( MediaLib* ml READ ml WRITE setMl )
    Q_PROPERTY( QString searchPattern READ searchPattern WRITE setSearchPattern )

    Q_PROPERTY( Qt::SortOrder sortOrder READ getSortOrder WRITE setSortOder NOTIFY sortOrderChanged )
    Q_PROPERTY( QString sortCriteria READ getSortCriteria WRITE setSortCriteria NOTIFY sortCriteriaChanged RESET unsetSortCriteria )
    Q_PROPERTY( unsigned int count READ getCount NOTIFY countChanged )

    Q_INVOKABLE virtual QVariant getIdForIndex( QVariant index) const = 0;
    Q_INVOKABLE virtual QVariantList getIdsForIndexes( QVariantList indexes ) const = 0;
    Q_INVOKABLE virtual QVariantList getIdsForIndexes( QModelIndexList indexes ) const = 0;

    Q_INVOKABLE QMap<QString, QVariant> getDataAt(int index);

signals:
    void parentIdChanged();
    void resetRequested();
    void sortOrderChanged();
    void sortCriteriaChanged();
    void countChanged(unsigned int) const;

protected slots:
    void onResetRequested();
    void onLocalSizeAboutToBeChanged(size_t size);
    void onLocalSizeChanged(size_t size);
    void onLocalDataChanged(size_t index, size_t count);

private:
    static void onVlcMlEvent( void* data, const vlc_ml_event_t* event );

protected:
    virtual void clear() = 0;
    virtual vlc_ml_sorting_criteria_t roleToCriteria(int role) const = 0;
    static QString getFirstSymbol(QString str);
    virtual vlc_ml_sorting_criteria_t nameToCriteria(QByteArray) const {
        return VLC_ML_SORTING_DEFAULT;
    }
    virtual QByteArray criteriaToName(vlc_ml_sorting_criteria_t ) const
    {
        return "";
    }

public:
    MLItemId parentId() const;
    void setParentId(MLItemId parentId);
    void unsetParentId();

    MediaLib* ml() const;
    void setMl(MediaLib* ml);

    const QString& searchPattern() const;
    void setSearchPattern( const QString& pattern );

    Qt::SortOrder getSortOrder() const;
    void setSortOder(Qt::SortOrder order);
    const QString getSortCriteria() const;
    void setSortCriteria(const QString& criteria);
    void unsetSortCriteria();

    virtual unsigned int getCount() const = 0;

protected:
    virtual void onVlcMlEvent( const MLEvent &event );

    MLItemId m_parent;

    vlc_medialibrary_t* m_ml;
    MediaLib* m_mediaLib;
    QString m_search_pattern;
    vlc_ml_sorting_criteria_t m_sort;
    bool m_sort_desc;

    std::unique_ptr<vlc_ml_event_callback_t,
                    std::function<void(vlc_ml_event_callback_t*)>> m_ml_event_handle;
    bool m_need_reset;
};

/**
 * Implements a basic sliding window.
 * const_cast & immutable are unavoidable, since all access member functions
 * are marked as const. fetchMore & canFetchMore don't allow for the full size
 * to be known (so the scrollbar would grow as we scroll, until we displayed all
 * elements), and implies having all elements loaded in RAM at all time.
 */
class MLSlidingWindowModel : public MLBaseModel
{
public:
    MLSlidingWindowModel(QObject* parent = nullptr);
    ~MLSlidingWindowModel();

    int rowCount(const QModelIndex &parent = {}) const override;

    void clear() override;

    QVariant getIdForIndex( QVariant index ) const override;
    QVariantList getIdsForIndexes( QModelIndexList indexes ) const override;
    QVariantList getIdsForIndexes( QVariantList indexes ) const override;

    unsigned getCount() const override;

protected:
    virtual ListCacheLoader<std::unique_ptr<MLItem>> *createLoader() const = 0;

    void validateCache() const;
    void invalidateCache();
    MLItem* item(int signedidx) const;
    void onVlcMlEvent(const MLEvent &event) override;

    /* Data loader for the cache */
    struct BaseLoader : public ListCacheLoader<std::unique_ptr<MLItem>>
    {
        BaseLoader(vlc_medialibrary_t *ml, MLItemId parent, QString searchPattern,
                   vlc_ml_sorting_criteria_t sort, bool sort_desc);
        BaseLoader(const MLSlidingWindowModel &model);

        MLQueryParams getParams(size_t index = 0, size_t count = 0) const;

    protected:
        vlc_medialibrary_t *m_ml;
        MLItemId m_parent;
        QString m_searchPattern;
        vlc_ml_sorting_criteria_t m_sort;
        bool m_sort_desc;
    };

private:
    virtual void thumbnailUpdated( int ) {}

    mutable std::unique_ptr<ListCache<std::unique_ptr<MLItem>>> m_cache;
};

#endif // MLBASEMODEL_HPP
