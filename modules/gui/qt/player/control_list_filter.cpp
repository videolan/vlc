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

#include "control_list_filter.hpp"

// Player includes
#include "player_controller.hpp"
#include "control_list_model.hpp"
#include "maininterface/mainctx.hpp"

// Ctor / dtor

/* explicit */ ControlListFilter::ControlListFilter(QObject * parent)
    : QSortFilterProxyModel(parent) {}

// QAbstractProxyModel reimplementation

void ControlListFilter::setSourceModel(QAbstractItemModel * sourceModel) /* override */
{
    assert(sourceModel->inherits("ControlListModel"));

    QSortFilterProxyModel::setSourceModel(sourceModel);
}

// Protected QSortFilterProxyModel reimplementation

bool ControlListFilter::filterAcceptsRow(int source_row, const QModelIndex &) const /* override */
{
    QAbstractItemModel * model = sourceModel();

    if (model == nullptr || m_player == nullptr || m_ctx == nullptr)
        return true;

    QVariant variant = model->data(model->index(source_row, 0), ControlListModel::ID_ROLE);

    if (variant.isValid() == false)
        return true;

    ControlListModel::ControlType type
        = static_cast<ControlListModel::ControlType> (variant.toInt());

    // NOTE: These controls are completely hidden when the current media does not support them.
    if (type == ControlListModel::NAVIGATION_BUTTONS)
    {
        return (m_player->hasMenu() || m_player->hasPrograms() || m_player->isTeletextAvailable());
    }
    else if (type == ControlListModel::BOOKMARK_BUTTON)
    {
        return (m_ctx->hasMediaLibrary() || m_player->hasChapters() || m_player->hasTitles());
    }
    else if (type == ControlListModel::PROGRAM_BUTTON)
    {
        return m_player->hasPrograms();
    }

    return true;
}

// Properties

PlayerController * ControlListFilter::player()
{
    return m_player;
}

void ControlListFilter::setPlayer(PlayerController * player)
{
    if (m_player == player) return;

    if (m_player)
        disconnect(m_player, nullptr, this, nullptr);

    m_player = player;

    connect(player, &PlayerController::teletextAvailableChanged, this, &ControlListFilter::invalidateFilter);
    connect(player, &PlayerController::hasMenuChanged,           this, &ControlListFilter::invalidateFilter);
    connect(player, &PlayerController::hasChaptersChanged,       this, &ControlListFilter::invalidateFilter);
    connect(player, &PlayerController::hasTitlesChanged,         this, &ControlListFilter::invalidateFilter);

    invalidate();

    emit playerChanged();
}

MainCtx* ControlListFilter::ctx() const
{
    return m_ctx;
}

void ControlListFilter::setCtx(MainCtx* ctx)
{
    if (m_ctx == ctx)
        return;
    m_ctx = ctx;
    emit ctxChanged();
}
