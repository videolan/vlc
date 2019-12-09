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

#ifndef VLC_QT_SELECTABLE_LIST_MODEL_HPP_
#define VLC_QT_SELECTABLE_LIST_MODEL_HPP_

#include <QAbstractListModel>

namespace vlc {

class SelectableListModel : public QAbstractListModel
{
    Q_OBJECT

    Q_PROPERTY(int selectedCount READ getSelectedCount NOTIFY selectedCountChanged)
public:
    SelectableListModel(QObject *parent = nullptr) :
        QAbstractListModel(parent) {}

    Q_INVOKABLE bool isSelected(int index) const;
    Q_INVOKABLE void setSelected(int index, bool selected);
    Q_INVOKABLE void toggleSelected(int index);
    Q_INVOKABLE void setSelection(const QList<int> &sortedIndexes);
    Q_INVOKABLE QList<int> getSelection() const;
    Q_INVOKABLE void setRangeSelected(int first, int count, bool selected);
    Q_INVOKABLE void selectAll();
    Q_INVOKABLE void deselectAll();

    virtual int getSelectedCount() const;

signals:
    void selectedCountChanged();

protected:
    virtual bool isRowSelected(int row) const = 0;
    virtual void setRowSelected(int row, bool selected) = 0;

    /* return the role to notify when selection changes */
    virtual int getSelectedRole() const = 0;
};

} // namespace vlc

#endif
