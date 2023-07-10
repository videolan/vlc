/*****************************************************************************
 * Copyright (C) 2023 VLC authors and VideoLAN
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

#include "list_selection_model.hpp"


ListSelectionModel::ListSelectionModel(QAbstractItemModel *model, QObject *parent)
    : QItemSelectionModel{model, parent}
{
    connect(this, &QItemSelectionModel::modelChanged, this, [this](const QAbstractItemModel * const model) {
        if (m_oldModel)
        {
            m_oldModel->disconnect(this);
        }

        m_oldModel = model;

        if (model)
        {
            // For some reason, even if QItemSelectionModel changes its selection based on the changes
            // in the target model it does not emit changed signal itself.
            connect(model, &QAbstractItemModel::rowsAboutToBeRemoved, this, &ListSelectionModel::invalidateCache);
            connect(model, &QAbstractItemModel::rowsAboutToBeInserted, this, &ListSelectionModel::invalidateCache);
            connect(model, &QAbstractItemModel::rowsAboutToBeMoved, this, &ListSelectionModel::invalidateCache);
            connect(model, &QAbstractItemModel::layoutChanged, this, &ListSelectionModel::invalidateCache);
            connect(model, &QAbstractItemModel::modelReset, this, &ListSelectionModel::invalidateCache);
        }
    });

    connect(this, &QItemSelectionModel::selectionChanged, this, &ListSelectionModel::invalidateCache);

    connect(this, &QItemSelectionModel::selectionChanged, this, &ListSelectionModel::_selectionChanged);
    connect(this, &QItemSelectionModel::currentChanged, this, &ListSelectionModel::_currentChanged);
}

int ListSelectionModel::currentIndexInt() const
{
    return QItemSelectionModel::currentIndex().row();
}

QVector<int> ListSelectionModel::selectionFlat() const
{
    const QItemSelection& baseSelection = QItemSelectionModel::selection();
    QVector<int> selection(baseSelection.size() * 2);
    assert(selection.size() == baseSelection.size() * 2);

    for (int i = 0, j = 0; i < baseSelection.size(); ++i, j = j + 2)
    {
        selection[j] = baseSelection[i].top();
        selection[j + 1] = baseSelection[i].bottom();
    }

    return selection;
}

QVector<int> ListSelectionModel::selectedIndexesFlat() const
{
    if (!m_caching || (m_cache.status == Cache::Invalid))
        updateSelectedIndexes();
    return m_cache.selectedIndexes;
}

QVector<int> ListSelectionModel::sortedSelectedIndexesFlat() const
{
    if (!m_caching || (m_cache.status != Cache::Sorted))
        updateSortedSelectedIndexes();
    return m_cache.selectedIndexes;
}

bool ListSelectionModel::isSelected(int index) const
{
    assert(model());
    return QItemSelectionModel::isSelected(model()->index(index, 0));
}

void ListSelectionModel::setCurrentIndex(int index, SelectionFlags command)
{
    assert(model());
    QItemSelectionModel::setCurrentIndex(model()->index(index, 0), command);
}

void ListSelectionModel::select(int index, SelectionFlags command)
{
    assert(model());
    QItemSelectionModel::select(model()->index(index, 0), command);
}

void ListSelectionModel::select(const QVector<int> &selection, SelectionFlags command)
{
    assert(selection.count() % 2 == 0);

    QItemSelection _selection;

    if (selection.count() == 2)
    {
        _selection = QItemSelection{model()->index(selection[0], 0),
                                    model()->index(selection[1], 0)};
    }
    else if (selection.count() > 2)
    {
        for (int i = 0; i < selection.count() - 1; i = i + 2)
        {
            const auto first = model()->index(selection[i], 0);
            const auto last = model()->index(selection[i + 1], 0);

            QItemSelectionRange range(first, last);
            _selection.append(range);
        }
    }

    QItemSelectionModel::select(_selection, command);
}

void ListSelectionModel::selectAll()
{
    select(0, QItemSelectionModel::Columns | QItemSelectionModel::Select);
}

void ListSelectionModel::updateSelection(Qt::KeyboardModifiers modifiers, int oldIndex, int newIndex)
{
    if (modifiers & Qt::ShiftModifier)
    {
        if (m_shiftIndex == -1)
            m_shiftIndex = oldIndex;
        select({m_shiftIndex, newIndex}, ClearAndSelect);
    }
    else if (modifiers & Qt::ControlModifier)
    {
        m_shiftIndex = -1;
        select(newIndex, Toggle);
    }
    else
    {
        m_shiftIndex = -1;
        select(newIndex, ClearAndSelect);
    }
}

void ListSelectionModel::updateSelectedIndexes() const
{
    const QModelIndexList& indexes = QItemSelectionModel::selectedIndexes();
    m_cache.selectedIndexes = QVector<int>(indexes.size());
    assert(m_cache.selectedIndexes.size() == indexes.size());

    for (int i = 0; i < indexes.size(); ++i)
    {
        m_cache.selectedIndexes[i] = indexes[i].row();
    }

    m_cache.status = Cache::Unsorted;
}

void ListSelectionModel::updateSortedSelectedIndexes() const
{
    if (m_cache.status == Cache::Invalid)
        updateSelectedIndexes();

    std::sort(m_cache.selectedIndexes.begin(), m_cache.selectedIndexes.end());

    m_cache.status = Cache::Sorted;
}

void ListSelectionModel::invalidateCache() const
{
    m_cache.status = Cache::Invalid;
}
