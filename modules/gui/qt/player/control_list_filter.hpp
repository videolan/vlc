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

Q_MOC_INCLUDE("maininterface/mainctx.hpp")
Q_MOC_INCLUDE("player/player_controller.hpp")
Q_MOC_INCLUDE("player/control_list_model.hpp")

// Forward declarations
class PlayerController;
class MainCtx;
class ControlListModel;

class ControlListFilter : public QSortFilterProxyModel
{
    using QSortFilterProxyModel::setSourceModel;

    Q_OBJECT

    Q_PROPERTY(ControlListModel* sourceModel READ sourceModel WRITE setSourceModel NOTIFY sourceModelChanged1 FINAL)
    Q_PROPERTY(PlayerController * player READ player WRITE setPlayer NOTIFY playerChanged FINAL)
    Q_PROPERTY(MainCtx* ctx READ ctx WRITE setCtx NOTIFY ctxChanged FINAL)

public:
    explicit ControlListFilter(QObject * parent = nullptr);

    void setSourceModel(ControlListModel* sourceModel);

    ControlListModel* sourceModel() const;

protected: // QSortFilterProxyModel reimplementation
    bool filterAcceptsRow(int source_row, const QModelIndex & source_parent) const override;

signals:
    void playerChanged();
    void ctxChanged();
    void sourceModelChanged1();

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
