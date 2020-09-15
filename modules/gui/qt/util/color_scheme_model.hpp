/*****************************************************************************
 * Copyright (C) 2020 the VideoLAN team
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

#ifndef COLORSCHEMEMODEL_HPP
#define COLORSCHEMEMODEL_HPP

#include <QStringListModel>

class ColorSchemeModel : public QStringListModel
{
    Q_OBJECT
    Q_PROPERTY(QString current READ getCurrent WRITE setCurrent NOTIFY currentChanged)
public:
    explicit ColorSchemeModel(QObject* parent = nullptr);

    Q_INVOKABLE void setAvailableColorSchemes(const QStringList& colorSchemeList);

    virtual Qt::ItemFlags flags (const QModelIndex& index) const override;
    virtual QVariant data(const QModelIndex &index, int role) const override;
    virtual bool setData(const QModelIndex &index, const QVariant &value, int role) override;

    inline QString getCurrent() const { return m_current; }
    void setCurrent(const QString& );

signals:
    void currentChanged(const QString& colorScheme);

private:
    QString               m_current;
    QPersistentModelIndex m_checkedItem;


};

#endif // COLORSCHEMEMODEL_HPP
