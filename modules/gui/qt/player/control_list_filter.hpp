/*****************************************************************************
 * Copyright (C) 2022 VLC authors and VideoLAN
 *
 * Authors: Benjamin Arnaud <bunjee@omega.gg>
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

#ifndef CONTROLLISTFILTER_HPP
#define CONTROLLISTFILTER_HPP

// Qt includes
#include <QSortFilterProxyModel>

// Forward declarations
class PlayerController;
class MainCtx;

class ControlListFilter : public QSortFilterProxyModel
{
    Q_OBJECT

    Q_PROPERTY(PlayerController * player READ player WRITE setPlayer NOTIFY playerChanged)
    Q_PROPERTY(MainCtx* ctx READ ctx WRITE setCtx NOTIFY ctxChanged)

public:
    explicit ControlListFilter(QObject * parent = nullptr);

public: // QAbstractProxyModel reimplementation
    void setSourceModel(QAbstractItemModel * sourceModel) override;

protected: // QSortFilterProxyModel reimplementation
    bool filterAcceptsRow(int source_row, const QModelIndex & source_parent) const override;

signals:
    void playerChanged();
    void ctxChanged();

public: // Properties
    PlayerController * player();
    void setPlayer(PlayerController * player);

    MainCtx* ctx() const;
    void setCtx(MainCtx* ctx);

private: // Variables
    PlayerController * m_player = nullptr;
    MainCtx* m_ctx = nullptr;
};

#endif // CONTROLLISTFILTER_HPP
