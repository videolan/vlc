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

#ifndef BASEMODEL_HPP
#define BASEMODEL_HPP

#include <QAbstractListModel>
#include <QQmlParserStatus>
#include <QString>


class BaseModelPrivate;

class BaseModel : public QAbstractListModel, public QQmlParserStatus
{
    Q_OBJECT
    Q_INTERFACES(QQmlParserStatus)

    Q_PROPERTY(QString searchPattern READ searchPattern WRITE setSearchPattern NOTIFY searchPatternChanged FINAL)

    Q_PROPERTY(Qt::SortOrder sortOrder READ getSortOrder WRITE setSortOrder NOTIFY sortOrderChanged FINAL)

    Q_PROPERTY(QString sortCriteria READ getSortCriteria WRITE setSortCriteria
                   NOTIFY sortCriteriaChanged RESET unsetSortCriteria FINAL)

    //maximum number of element to load
    //limit = 0 means all elements are loaded
    Q_PROPERTY(unsigned int limit READ getLimit WRITE setLimit NOTIFY limitChanged FINAL)

    //skip in N first elements
    Q_PROPERTY(unsigned int offset READ getOffset WRITE setOffset NOTIFY offsetChanged FINAL)

    //number of elements in the model (limit is accounted)
    Q_PROPERTY(unsigned int count READ getCount NOTIFY countChanged FINAL)

    //number of elements in the model (limit not accounted)
    Q_PROPERTY(unsigned int maximumCount READ getMaximumCount NOTIFY maximumCountChanged FINAL)

    /**
     * @brief loading
     * @return true till no data is available
     */
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged FINAL)

public:
    BaseModel(BaseModelPrivate* priv, QObject* parent = nullptr);
    virtual ~BaseModel();

public:
    virtual const QString& searchPattern() const;
    virtual void setSearchPattern( const QString& pattern );
    virtual Qt::SortOrder getSortOrder() const;
    virtual void setSortOrder(Qt::SortOrder order);
    virtual const QString getSortCriteria() const;
    virtual void setSortCriteria(const QString& criteria);
    virtual void unsetSortCriteria();

    unsigned int getLimit() const;
    void setLimit(unsigned int limit);
    unsigned int getOffset() const;
    void setOffset(unsigned int offset);

    unsigned int getCount() const;
    unsigned int getLoadedCount() const;
    unsigned int getMaximumCount() const;

    virtual bool loading() const;

    Q_INVOKABLE QMap<QString, QVariant> getDataAt(int idx) const;
    Q_INVOKABLE QMap<QString, QVariant> getDataAt(const QModelIndex & index) const;

signals:
    void resetRequested();
    void sortOrderChanged();
    void sortCriteriaChanged();
    void searchPatternChanged();
    void limitChanged() const;
    void offsetChanged() const;
    void countChanged(unsigned int) const;
    void maximumCountChanged(unsigned int) const;
    void loadingChanged() const;

public:
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

public:
    void classBegin() override;
    void componentComplete() override;

public:
    void onCacheDataChanged(int first, int last);
    void onCacheBeginInsertRows(int first, int last);
    void onCacheBeginRemoveRows(int first, int last);
    void onCacheBeginMoveRows(int first, int last, int destination);

    void onResetRequested();
    void onLocalSizeChanged(size_t queryCount, size_t maximumCount);

    void validateCache() const;
    void resetCache();
    void invalidateCache();

protected:
    QScopedPointer<BaseModelPrivate> d_ptr;
    Q_DECLARE_PRIVATE(BaseModel)
};

#endif // BASEMODEL_HPP
