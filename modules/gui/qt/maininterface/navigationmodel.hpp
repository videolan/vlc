/*****************************************************************************
 * Copyright (C) 2020 VLC authors and VideoLAN
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

#ifndef NAVIGATIONMODEL_H
#define NAVIGATIONMODEL_H

#include <QObject>
#include <QQmlEngine>
#include <QAbstractListModel>

class NavigationModelPrivate;
class NavigationModel : public QAbstractListModel, public QQmlParserStatus
{
    Q_OBJECT
    QML_ELEMENT
    Q_INTERFACES(QQmlParserStatus)

    Q_PROPERTY(bool hasMedialib READ hasMedialib WRITE setHasMedialib NOTIFY hasMedialibChanged FINAL)

public:
    enum Roles {
        TITLE = Qt::UserRole,
        URI,
        DEPTH,
        ICON,
        EXPANDABLE,
        EXPANDED,
    };
    Q_ENUM(Roles)

public:
    explicit NavigationModel(QObject *parent = nullptr);
    ~NavigationModel();

public:  //QAbstractListModel
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    QHash<int,QByteArray> roleNames() const override;

public: //QQmlParserStatus
    void classBegin() override;
    void componentComplete() override;

    //properties
public:
    bool hasMedialib() const;
public slots:
    void setHasMedialib(bool);
signals:
    void hasMedialibChanged(bool);

protected:
    QScopedPointer<NavigationModelPrivate> d_ptr;
    Q_DECLARE_PRIVATE(NavigationModel)
};

#endif // NAVIGATIONMODEL_H
