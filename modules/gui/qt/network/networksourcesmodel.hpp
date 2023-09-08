/*****************************************************************************
 * Copyright (C) 2020 VLC authors and VideoLAN
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

#ifndef MLNetworkSourcesModel_HPP
#define MLNetworkSourcesModel_HPP

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif


#include "util/base_model.hpp"
#include "maininterface/mainctx.hpp"

class NetworkSourcesModelPrivate;
class NetworkSourcesModel : public BaseModel
{
    Q_OBJECT

    Q_PROPERTY(MainCtx* ctx READ getCtx WRITE setCtx NOTIFY ctxChanged FINAL)

public:
    enum Role {
        SOURCE_NAME = Qt::UserRole + 1,
        SOURCE_LONGNAME,
        SOURCE_TYPE,
        SOURCE_ARTWORK
    };
    Q_ENUM(Role)

    enum ItemType {
        TYPE_DUMMY = -1, // provided for UI for entry "Add a service"
        TYPE_SOURCE = 0
    };
    Q_ENUM(ItemType)

    NetworkSourcesModel( QObject* parent = nullptr );

    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

public: // properties
    void setCtx(MainCtx* ctx);
    inline MainCtx* getCtx() { return m_ctx; }

signals:
    void ctxChanged();

private:
    MainCtx* m_ctx = nullptr;

    Q_DECLARE_PRIVATE(NetworkSourcesModel);
};

#endif // MLNetworkSourcesModel_HPP
