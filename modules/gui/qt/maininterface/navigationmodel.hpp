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

class NavigationModelItem
{
    Q_GADGET

    Q_PROPERTY(QString name READ name CONSTANT FINAL)
    Q_PROPERTY(QString icon READ icon CONSTANT FINAL)
    Q_PROPERTY(QString icon_svg READ icon_svg CONSTANT FINAL)
    Q_PROPERTY(QStringList uri READ uri CONSTANT FINAL)

    QML_VALUE_TYPE(navigationModelItem)

public:
    NavigationModelItem() = default;

    NavigationModelItem(const QString& name,
                        const QString& icon,
                        const QString& icon_svg,
                        const QStringList& uri)
        : m_name(name)
        , m_icon(icon)
        , m_icon_svg(icon_svg)
        , m_uri(uri)
    { }

    QString name() const { return m_name; }
    QString icon() const { return m_icon; }
    QString icon_svg() const { return m_icon_svg; }
    QStringList uri() const { return m_uri; }

private:
    QString m_name, m_icon, m_icon_svg;
    QStringList m_uri;
};

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
        ICON_SVG,
        EXPANDABLE,
        EXPANDED,
        CHILDREN
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
