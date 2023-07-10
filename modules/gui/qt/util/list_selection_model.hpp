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
#ifndef LISTSELECTIONMODEL_HPP
#define LISTSELECTIONMODEL_HPP

#include <QItemSelectionModel>
#include <QVector>
#include <QPointer>

class ListSelectionModel : public QItemSelectionModel
{
    // ListSelectionModel is a specialization of QItemSelectionModel.
    // It provides convenient properties and methods for models that
    // are flat. If your model uses columns or item parents, use
    // QItemSelectionModel instead.
    // If you use QAbstractListModel, you can use ListSelectionModel
    // for easier integration.

    Q_OBJECT

    // TODO: Use size_t when Qt switches from int in its abstract models.

    Q_PROPERTY(int currentIndexInt READ currentIndexInt NOTIFY _currentChanged STORED false DESIGNABLE false FINAL)
    Q_PROPERTY(QVector<int> selectedIndexesFlat READ selectedIndexesFlat NOTIFY _selectionChanged STORED false DESIGNABLE false FINAL)
    // Flattened QList<QPair<int (first), int (last)>> (convenient for QML):
    // NOTE: Is not cached, avoid many reads:
    Q_PROPERTY(QVector<int> selectionFlat READ selectionFlat NOTIFY _selectionChanged STORED false DESIGNABLE false FINAL)
    // For convenience:
    Q_PROPERTY(QVector<int> sortedSelectedIndexesFlat READ sortedSelectedIndexesFlat NOTIFY _selectionChanged STORED false DESIGNABLE false FINAL)
    
    Q_PROPERTY(bool cache MEMBER m_caching NOTIFY cacheChanged FINAL)
    
public:
    explicit ListSelectionModel(QAbstractItemModel *model = nullptr, QObject *parent = nullptr);

    int currentIndexInt() const;
    QVector<int> selectionFlat() const;
    QVector<int> selectedIndexesFlat() const;
    QVector<int> sortedSelectedIndexesFlat() const;

    Q_INVOKABLE bool isSelected(int index) const;

public slots:
    void setCurrentIndex(const QModelIndex &index, QItemSelectionModel::SelectionFlags command) override
    {
        QItemSelectionModel::setCurrentIndex(index, command);
    }

    void select(const QModelIndex &index, QItemSelectionModel::SelectionFlags command) override
    {
        QItemSelectionModel::select(index, command);
    }

    void select(const QItemSelection &selection, QItemSelectionModel::SelectionFlags command) override
    {
        QItemSelectionModel::select(selection, command);
    }

    void setCurrentIndex(int index, QItemSelectionModel::SelectionFlags command);
    void select(int index, QItemSelectionModel::SelectionFlags command);
    void select(const QVector<int> &selection, QItemSelectionModel::SelectionFlags command);
    void selectAll();

    // NOTE: Deprecated. Do not use.
    //       Added only for compatibility reasons.
    void updateSelection(Qt::KeyboardModifiers modifiers, int oldIndex, int newIndex);

signals:
    void _selectionChanged();
    void _currentChanged();
    void cacheChanged();

private:
    int m_shiftIndex = -1;
	
    bool m_caching = true;

    struct Cache
    {
        enum {
            Invalid,
            Unsorted,
            Sorted
        } status = Invalid;

        QVector<int> selectedIndexes;
    } mutable m_cache;

    mutable QPointer<const QAbstractItemModel> m_oldModel;

    void updateSelectedIndexes() const;
    void updateSortedSelectedIndexes() const;

private slots:
    void invalidateCache() const;
};

#endif // LISTSELECTIONMODEL_HPP
