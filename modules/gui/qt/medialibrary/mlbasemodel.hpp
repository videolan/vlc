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

class MediaLib;

class MLBaseModel : public QAbstractListModel
{
    Q_OBJECT

public:
    explicit MLBaseModel(QObject *parent = nullptr);
    virtual ~MLBaseModel();

    Q_INVOKABLE void sortByColumn(QByteArray name, Qt::SortOrder order);

    Q_PROPERTY( MLParentId parentId READ parentId WRITE setParentId NOTIFY parentIdChanged RESET unsetParentId )
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
    MLParentId parentId() const;
    void setParentId(MLParentId parentId);
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

    MLParentId m_parent;

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
template <typename T>
class MLSlidingWindowModel : public MLBaseModel
{
public:
    static constexpr size_t BatchSize = 100;

    MLSlidingWindowModel(QObject* parent = nullptr)
        : MLBaseModel(parent)
        , m_initialized(false)
        , m_total_count(0)
        , m_offset(0)
    {
    }

    int rowCount(const QModelIndex &parent = {}) const override
    {
        if (parent.isValid())
            return 0;

        if ( m_initialized == false )
        {
            MLQueryParams params{ m_search_pattern.toUtf8(), m_sort, m_sort_desc };
            m_total_count = countTotalElements(params);
            m_initialized = true;
            emit countChanged( static_cast<unsigned int>(m_total_count) );
        }

        return m_total_count;
    }

    void clear() override
    {
        m_offset = 0;
        m_initialized = false;
        m_total_count = 0;
        m_item_list.clear();
        emit countChanged( static_cast<unsigned int>(m_total_count) );
    }


    virtual QVariant getIdForIndex( QVariant index ) const override
    {
        T* obj = nullptr;
        if (index.canConvert<int>())
            obj = item( index.toInt() );
        else if ( index.canConvert<QModelIndex>() )
            obj = item( index.value<QModelIndex>().row() );

        if (!obj)
            return {};

        return QVariant::fromValue(obj->getId());
    }

    virtual QVariantList getIdsForIndexes( QModelIndexList indexes ) const override
    {
        QVariantList idList;
        idList.reserve(indexes.length());
        std::transform( indexes.begin(), indexes.end(),std::back_inserter(idList), [this](const QModelIndex& index) -> QVariant {
            T* obj = item( index.row() );
            if (!obj)
                return {};
            return QVariant::fromValue(obj->getId());
        });
        return idList;
    }

    virtual QVariantList getIdsForIndexes( QVariantList indexes ) const override
    {
        QVariantList idList;

        idList.reserve(indexes.length());
        std::transform( indexes.begin(), indexes.end(),std::back_inserter(idList), [this](const QVariant& index) -> QVariant {
            T* obj = nullptr;
            if (index.canConvert<int>())
                obj = item( index.toInt() );
            else if ( index.canConvert<QModelIndex>() )
                obj = item( index.value<QModelIndex>().row() );

            if (!obj)
                return {};

            return QVariant::fromValue(obj->getId());
        });
        return idList;
    }

    unsigned int getCount() const override {
        return static_cast<unsigned int>(m_total_count);
    }

protected:
    T* item(int signedidx) const
    {
        if (!m_initialized || signedidx < 0)
            return nullptr;

        unsigned int idx = static_cast<unsigned int>(signedidx);
        if ( idx >= m_total_count )
            return nullptr;

        if ( idx < m_offset ||  idx >= m_offset + m_item_list.size() )
        {
            m_offset = idx - idx % BatchSize;
            MLQueryParams params{ m_search_pattern.toUtf8(), m_sort, m_sort_desc, m_offset, BatchSize };
            m_item_list = fetch(params);
        }

        //db has changed
        if ( idx >= m_offset + m_item_list.size() || idx < m_offset )
            return nullptr;
        return m_item_list[idx - m_offset].get();
    }

    virtual void onVlcMlEvent(const MLEvent &event) override
    {
        switch (event.i_type)
        {
            case VLC_ML_EVENT_MEDIA_THUMBNAIL_GENERATED:
            {
                if (event.media_thumbnail_generated.b_success) {
                    int idx = static_cast<int>(m_offset);
                    for ( const auto& it : m_item_list ) {
                        if (it->getId().id == event.media_thumbnail_generated.i_media_id) {
                            thumbnailUpdated(idx);
                            break;
                        }
                        idx += 1;
                    }
                }
                break;
            }
            default:
                break;
        }
        MLBaseModel::onVlcMlEvent( event );
    }

private:
    virtual size_t countTotalElements(const MLQueryParams &params) const = 0;
    virtual std::vector<std::unique_ptr<T>> fetch(const MLQueryParams &params) const = 0;
    virtual void thumbnailUpdated( int ) {}

    mutable std::vector<std::unique_ptr<T>> m_item_list;
    mutable bool m_initialized;
    mutable size_t m_total_count;
    mutable size_t m_offset; /* offset of m_item_list in the global list */
};

#endif // MLBASEMODEL_HPP
