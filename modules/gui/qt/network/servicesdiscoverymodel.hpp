/*****************************************************************************
 * Copyright (C) 2020 VLC authors and VideoLAN
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

#ifndef MLServicesDiscoveryModel_HPP
#define MLServicesDiscoveryModel_HPP

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "util/base_model.hpp"
#include "maininterface/mainctx.hpp"

class ServicesDiscoveryModelPrivate;
class ServicesDiscoveryModel : public BaseModel
{
    Q_OBJECT

public:

    Q_PROPERTY(MainCtx* ctx READ getCtx WRITE setCtx NOTIFY ctxChanged FINAL)

    enum State // equivalent to addon_state_t
    {
        NOTINSTALLED = 0,
        INSTALLING,
        INSTALLED,
        UNINSTALLING
    };
    Q_ENUM(State)

    enum Role {
        SERVICE_NAME = Qt::UserRole + 1,
        SERVICE_AUTHOR,
        SERVICE_SUMMARY,
        SERVICE_DESCRIPTION,
        SERVICE_DOWNLOADS,
        SERVICE_SCORE,
        SERVICE_STATE,
        SERVICE_ARTWORK
    };
    Q_ENUM(Role)

    explicit ServicesDiscoveryModel(QObject* parent = nullptr);

public: //QAbstractListModel override
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

public: //invokable functions
    Q_INVOKABLE void installService(int idx);
    Q_INVOKABLE void removeService(int idx);

public: // properties
    void setCtx(MainCtx* ctx);
    inline MainCtx* getCtx() const { return m_ctx; }

signals:
    void ctxChanged();

private:
    MainCtx* m_ctx = nullptr;

    Q_DECLARE_PRIVATE(ServicesDiscoveryModel);
};

#endif // MLServicesDiscoveryModel_HPP
