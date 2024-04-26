/*****************************************************************************
 * Copyright (C) 2024 VLC authors and VideoLAN
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
#ifndef LOCALLISTBASEMODEL_HPP
#define LOCALLISTBASEMODEL_HPP

#include "locallistcacheloader.hpp"
#include "base_model_p.hpp"

/**
 * @brief The LocalListBaseModelPrivate
 * this ensure that cache is not created before data.
 * If models have static data  or are synchronous, they still
 * need to increase the mode revision to a non-zero value
 */
template<typename T>
class  LocalListBaseModelPrivate
    : public BaseModelPrivateT<T>
    , public LocalListCacheLoader<T>::ModelSource
{
public:
    using BaseModelPrivateT<T>::BaseModelPrivateT;
    virtual ~LocalListBaseModelPrivate() = default;

    virtual typename LocalListCacheLoader<T>::ItemCompare getSortFunction() const = 0;

public:
    //BaseModelPrivateT reimplementation
    virtual bool loading() const override
    {
        return (m_loading) || BaseModelPrivateT<T>::loading();
    }

    virtual bool cachable() const override
    {
        return (m_revision > 0) && BaseModelPrivateT<T>::cachable();
    }

    virtual std::unique_ptr<ListCacheLoader<T>> createLoader() const override
    {
        return std::make_unique<LocalListCacheLoader<T>>(
            this, this->m_searchPattern,
            getSortFunction()
            );
    }

public:
    //LocalListCacheLoader<T>::ModelSource reimplementation
    size_t getModelRevision() const override
    {
        return m_revision;
    }

protected:
    size_t m_revision = 0;
    bool m_loading = true;
};

#endif // LOCALLISTBASEMODEL_HPP
