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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "selectable_list_model.hpp"

namespace vlc {

void SelectableListModel::setSelected(int row, bool selected)
{
    assert( row >= 0 && row < rowCount() );

    setRowSelected(row, selected);

    QModelIndex modelIndex = index(row);
    emit dataChanged(modelIndex, modelIndex, { getSelectedRole() });
    emit selectedCountChanged();
}

bool SelectableListModel::isSelected(int row) const
{
    assert( row >= 0 && row < rowCount() );

    return isRowSelected(row);
}

void SelectableListModel::toggleSelected(int row)
{
    assert( row >= 0 && row < rowCount() );

    setRowSelected(row, !isRowSelected(row));

    QModelIndex modelIndex = index(row);
    emit dataChanged(modelIndex, modelIndex, { getSelectedRole() });
    emit selectedCountChanged();
}

void SelectableListModel::setSelection(const QList<int> &sortedIndexes)
{
    int itemsCount = rowCount();

    if (!itemsCount)
        return;

    int indexesCount = sortedIndexes.size();
    int p = 0; /* points to the current index in sortedIndexes */
    for (int i = 0; i < itemsCount; ++i)
    {
        /* the indexes are sorted, so only check the next one */
        if (p < indexesCount && i == sortedIndexes[p])
        {
            setRowSelected(i, true);
            ++p;
        }
        else
        {
            setRowSelected(i, false);
        }
    }

    QModelIndex first = index(0);
    QModelIndex last = index(itemsCount - 1);
    emit dataChanged(first, last, { getSelectedRole() });
    emit selectedCountChanged();
}

QList<int> SelectableListModel::getSelection() const
{
    int itemsCount = rowCount();

    QList<int> indexes;
    for (int i = 0; i < itemsCount; ++i)
    {
        if (isRowSelected(i))
            indexes.push_back(i);
    }
    return indexes;
}

void SelectableListModel::setRangeSelected(int start, int count, bool selected)
{
    for (int i = start; i < start + count; ++i)
        setRowSelected(i, selected);

    QModelIndex first = index(start);
    QModelIndex last = index(start + count - 1);
    emit dataChanged(first, last, { getSelectedRole() });
    emit selectedCountChanged();
}

void SelectableListModel::selectAll()
{
    int count = rowCount();

    if (!count)
        return;

    setRangeSelected(0, count, true);
}

void SelectableListModel::deselectAll()
{
    int count = rowCount();

    if (!count)
        return;

    setRangeSelected(0, count, false);
}

int SelectableListModel::getSelectedCount() const
{
    int itemsCount = rowCount();
    int count = 0;
    for (int i = 0 ; i < itemsCount; i++) {
        if (isRowSelected(i))
            count++;
    }
    return count;
}

} // namespace vlc
