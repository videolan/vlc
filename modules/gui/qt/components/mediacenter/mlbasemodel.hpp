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
#include "mcmedialib.hpp"
#include <memory>

class MCMediaLib;

class MLBaseModel : public QAbstractListModel
{
    Q_OBJECT

public:
    explicit MLBaseModel(QObject *parent = nullptr);
    virtual ~MLBaseModel();

    Q_INVOKABLE void sortByColumn(QByteArray name, Qt::SortOrder order);

    Q_INVOKABLE virtual QObject *get(unsigned int idx) const = 0;

    Q_PROPERTY( MLParentId parentId READ parentId WRITE setParentId NOTIFY parentIdChanged RESET unsetParentId )
    Q_PROPERTY( MCMediaLib* ml READ ml WRITE setMl )
    Q_PROPERTY( unsigned int maxItems MEMBER m_nb_max_items )
    Q_PROPERTY( QString searchPattern READ searchPattern WRITE setSearchPattern )

    Q_PROPERTY( Qt::SortOrder sortOrder READ getSortOrder WRITE setSortOder NOTIFY sortOrderChanged )
    Q_PROPERTY( QString sortCriteria READ getSortCriteria WRITE setSortCriteria NOTIFY sortCriteriaChanged RESET unsetSortCriteria )

signals:
    void parentIdChanged();
    void resetRequested();
    void sortOrderChanged();
    void sortCriteriaChanged();

protected slots:
    void onResetRequested();

private:
    static void onVlcMlEvent( void* data, const vlc_ml_event_t* event );

protected:
    virtual void clear() = 0;
    virtual vlc_ml_sorting_criteria_t roleToCriteria(int role) const = 0;
    virtual vlc_ml_sorting_criteria_t nameToCriteria(QByteArray) const {
        return VLC_ML_SORTING_DEFAULT;
    }
    virtual QByteArray criteriaToName(vlc_ml_sorting_criteria_t ) const
    {
        return "";
    }

    MLParentId parentId() const;
    void setParentId(MLParentId parentId);
    void unsetParentId();

    MCMediaLib* ml() const;
    void setMl(MCMediaLib* ml);

    const QString& searchPattern() const;
    void setSearchPattern( const QString& pattern );

    Qt::SortOrder getSortOrder() const;
    void setSortOder(Qt::SortOrder order);
    const QString getSortCriteria() const;
    void setSortCriteria(const QString& criteria);
    void unsetSortCriteria();

    virtual void onVlcMlEvent( const vlc_ml_event_t* event );

    MLParentId m_parent;

    vlc_medialibrary_t* m_ml;
    MCMediaLib* m_mcMediaLib;
    mutable vlc_ml_query_params_t m_query_param;
    std::unique_ptr<char, void(*)(void*)> m_search_pattern_cstr;
    QString m_search_pattern;

    unsigned int m_nb_max_items;

    mutable vlc_mutex_t m_item_lock;

    std::unique_ptr<vlc_ml_event_callback_t,
                    std::function<void(vlc_ml_event_callback_t*)>> m_ml_event_handle;
    std::atomic_bool m_need_reset;
    std::atomic_bool m_is_reloading;
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
    {
        m_query_param.i_nbResults = BatchSize;
    }

    int rowCount(const QModelIndex &parent) const override
    {
        if (parent.isValid())
            return 0;
        vlc_mutex_locker lock( &m_item_lock );
        if ( m_initialized == false )
        {
            m_total_count = countTotalElements();
            m_initialized = true;
            m_item_list = const_cast<MLSlidingWindowModel<T>*>(this)->fetch();
        }
        return m_total_count;
    }

    QObject* get(unsigned int idx) const override
    {
        vlc_mutex_locker lock( &m_item_lock );
        T* obj = item( idx );
        if (!obj)
            return nullptr;
        return obj->clone();
    }

    void clear() override
    {
        vlc_mutex_locker lock( &m_item_lock );
        m_query_param.i_offset = 0;
        m_initialized = false;
        m_total_count = 0;
        m_item_list.clear();
    }

protected:
    T* item(unsigned int idx) const
    {
        // Must be called in a locked context
        if ( m_initialized == false )
        {
            m_total_count = countTotalElements();
            if ( m_total_count > 0 )
                m_item_list = const_cast<MLSlidingWindowModel<T>*>(this)->fetch();
            m_initialized = true;
        }

        if ( m_total_count == 0 || idx >= m_total_count || idx < 0 )
            return nullptr;

        if ( idx < m_query_param.i_offset ||  idx >= m_query_param.i_offset + m_item_list.size() )
        {
            if (m_query_param.i_nbResults == 0)
                m_query_param.i_offset = 0;
            else
                m_query_param.i_offset = idx - idx % m_query_param.i_nbResults;
            m_item_list = const_cast<MLSlidingWindowModel<T>*>(this)->fetch();
        }

        //db has changed
        if ( idx - m_query_param.i_offset >= m_item_list.size() || idx - m_query_param.i_offset < 0 )
            return nullptr;
        return m_item_list[idx - m_query_param.i_offset].get();
    }

private:
    virtual size_t countTotalElements() const = 0;
    virtual std::vector<std::unique_ptr<T>> fetch() = 0;

protected:
    mutable std::vector<std::unique_ptr<T>> m_item_list;

private:
    mutable bool m_initialized;
    mutable size_t m_total_count;
};

#endif // MLBASEMODEL_HPP
