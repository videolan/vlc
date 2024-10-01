/*****************************************************************************
 * Copyright (C) 2024 VLC authors and VideoLAN
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
#ifndef RENDERER_MANAGER_HPP
#define RENDERER_MANAGER_HPP

#include "qt.hpp"
#include <QAbstractListModel>

class RendererManagerPrivate;
class RendererManager : public QAbstractListModel
{
    Q_OBJECT

public:
    enum RendererStatus
    {
        FAILED = -2,
        IDLE = -1,
        RUNNING,
    };
    Q_ENUM(RendererStatus)

    enum Roles {
        NAME = Qt::UserRole,
        TYPE,
        DEMUX_FILTER,
        SOUT,
        ICON_URI,
        FLAGS,
        SELECTED
    };

    ///remaining time before scan timeout
    Q_PROPERTY(int scanRemain READ getScanRemain NOTIFY scanRemainChanged FINAL)
    Q_PROPERTY(RendererStatus status READ getStatus NOTIFY statusChanged FINAL)
    Q_PROPERTY(bool useRenderer READ useRenderer NOTIFY useRendererChanged FINAL)

public:
    RendererManager( qt_intf_t* intf, vlc_player_t* player );
    virtual ~RendererManager();

    void disableRenderer();

public slots:
    void StartScan();
    void StopScan();

    //QAbstractListModel override
public:
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    QHash<int,QByteArray> roleNames() const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    //properties accessor
public:
    int getScanRemain() const;
    RendererStatus getStatus() const;
    bool useRenderer() const;
signals:
    void statusChanged();
    void scanRemainChanged();
    void useRendererChanged();

private:
    Q_DECLARE_PRIVATE(RendererManager);
    QScopedPointer<RendererManagerPrivate> d_ptr;
};

#endif // RENDERER_MANAGER_HPP
