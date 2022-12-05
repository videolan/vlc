/*****************************************************************************
 * Copyright (C) 2021 VLC authors and VideoLAN
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

#ifndef MODELITEMS_HPP
#define MODELITEMS_HPP

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

// Qt includes
#include <QAbstractListModel>

// NOTE: Qt won't let us inherit from QAbstractListModel when declaring a template class. So we
//       specify a base class for properties and signals.
class BaseClipListModel : public QAbstractListModel
{
    Q_OBJECT

    Q_PROPERTY(int count READ count NOTIFY countChanged)

    Q_PROPERTY(int maximumCount READ maximumCount WRITE setMaximumCount NOTIFY maximumCountChanged)

    Q_PROPERTY(bool hasMoreItems READ hasMoreItems NOTIFY countChanged)

    Q_PROPERTY(QByteArray searchRole READ searchRole WRITE setSearchRole NOTIFY searchRoleChanged)

    Q_PROPERTY(QString searchPattern READ searchPattern WRITE setSearchPattern
               NOTIFY searchPatternChanged)

    Q_PROPERTY(QString sortCriteria READ sortCriteria WRITE setSortCriteria
               NOTIFY sortCriteriaChanged)

    Q_PROPERTY(Qt::SortOrder sortOrder READ sortOrder WRITE setSortOrder NOTIFY sortOrderChanged)

public:
    BaseClipListModel(QObject * parent = nullptr);

public: // Interface
    // Convenience function for showing or hiding items according to the implicitCount.
    void updateItems();

public: // Abstract functions
    virtual int implicitCount() const = 0;

    virtual bool hasMoreItems() const = 0;

public: // QAbstractItemModel implementation
    int rowCount(const QModelIndex & parent = QModelIndex()) const override;

protected: // Abstract functions
    virtual void expandItems(int count) = 0;
    virtual void shrinkItems(int count) = 0;

    virtual void updateSort() = 0;

signals:
    void countChanged();

    void maximumCountChanged();

    void searchPatternChanged();
    void searchRoleChanged();

    void sortCriteriaChanged();
    void sortOrderChanged();

public: // Properties
    int count() const;

    int maximumCount() const;
    void setMaximumCount(int count);

    QString searchPattern() const;
    void setSearchPattern(const QString & pattern);

    QByteArray searchRole() const;
    void setSearchRole(const QByteArray & role);

    QString sortCriteria() const;
    void setSortCriteria(const QString & criteria);

    Qt::SortOrder sortOrder() const;
    void setSortOrder(Qt::SortOrder order);

protected:
    int m_count = 0;

    int m_maximumCount = -1;

    QString m_searchPattern;
    QByteArray m_searchRole;

    QString m_sortCriteria = "name";

    Qt::SortOrder m_sortOrder = Qt::AscendingOrder;
};

// NOTE: This helper adds support for capping, sorting and filtering QAbstractListModel item(s).
//       We tried implementing capping from a QSortFilterProxyModel and QML, but both
//       implementations had flaws.
template <typename T>
class ClipListModel : public BaseClipListModel
{
public:
    typedef typename std::vector<T>::iterator iterator;

public:
    explicit ClipListModel(QObject * parent = nullptr);

public: // Interface
    // NOTE: Convenience function that inserts an item and 'clears' model row(s) depending on the
    //       maximum count to preserve item capping.
    void insertItem(iterator it, const T & item);

    // NOTE: Convenience function that removes an item at a given index and updates the iterator.
    void removeItem(iterator & it, int index);

    // NOTE: Convenience function that clears the items and resets the model.
    void clearItems();

public: // BaseClipListModel implementation
    int implicitCount() const override;

    bool hasMoreItems() const override;

protected: // Abstract functions
    // NOTE: This function is called when we need to update 'm_comparator' based on the current
    //       sorting parameters.
    virtual void onUpdateSort(const QString & criteria, Qt::SortOrder order) = 0;

protected: // BaseClipListModel implementation
    void expandItems(int count) override;
    void shrinkItems(int count) override;

    void updateSort() override;

protected:
    std::vector<T> m_items;

    std::function<bool(const T &, const T &)> m_comparator;
};

// Ctor / dtor

template <typename T>
/* explicit */ ClipListModel<T>::ClipListModel(QObject * parent) : BaseClipListModel(parent) {}

// Interface

template <typename T>
void ClipListModel<T>::insertItem(iterator it, const T & item)
{
    int pos = std::distance(m_items.begin(), it);

    if (m_maximumCount != -1 && m_count >= m_maximumCount)
    {
        // NOTE: When the position is beyond the maximum count we don't notify the view.
        if (pos >= m_maximumCount)
        {
            m_items.insert(it, std::move(item));

            return;
        }

        // NOTE: Removing the last item to make room for the new one.

        int index = m_count - 1;

        beginRemoveRows({}, index, index);

        m_count--;

        endRemoveRows();

        emit countChanged();
    }

    beginInsertRows({}, pos, pos);

    m_items.insert(it, std::move(item));

    m_count++;

    endInsertRows();

    emit countChanged();
}

template <typename T>
void ClipListModel<T>::removeItem(iterator & it, int index)
{
    if (index < 0 || index >= count())
        return;

    beginRemoveRows({}, index, index);

    it = m_items.erase(it);

    m_count--;

    endRemoveRows();

    emit countChanged();
}

template <typename T>
void ClipListModel<T>::clearItems()
{
    if (m_items.empty())
        return;

    beginResetModel();

    m_items.clear();

    m_count = 0;

    endResetModel();

    emit countChanged();
}

// BaseClipListModel implementation

template <typename T>
int ClipListModel<T>::implicitCount() const /* override */
{
    assert(m_items.size() < INT32_MAX);

    if (m_maximumCount == -1)
        return (int) m_items.size();
    else
        return qMin((int) m_items.size(), m_maximumCount);
}

template <typename T>
bool ClipListModel<T>::hasMoreItems() const /* override */
{
    if (m_maximumCount == -1)
        return false;
    else
        return (m_count < (int) m_items.size());
}

// Protected BaseClipListModel implementation

template <typename T>
void ClipListModel<T>::expandItems(int count) /* override */
{
    beginInsertRows({}, m_count, count - 1);

    m_count = count;

    endInsertRows();

    emit countChanged();
}

template <typename T>
void ClipListModel<T>::shrinkItems(int count) /* override */
{
    beginRemoveRows({}, count, m_count - 1);

    m_count = count;

    endRemoveRows();

    emit countChanged();
}

template <typename T>
void ClipListModel<T>::updateSort() /* override */
{
    onUpdateSort(m_sortCriteria, m_sortOrder);

    beginResetModel();

    std::sort(m_items.begin(), m_items.end(), m_comparator);

    endResetModel();
}

#endif // MODELITEMS_HPP
