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

#include <QAbstractListModel>

#include <memory>

class ColorSchemeModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QString current READ currentText NOTIFY currentChanged FINAL)
    Q_PROPERTY(ColorScheme scheme READ currentScheme NOTIFY currentChanged FINAL)

public:
    enum ColorScheme
    {
        System,
        Day,
        Night,

        Auto
    };

    struct Item
    {
        QString text;
        ColorScheme scheme;
    };

    Q_ENUM(ColorScheme);

    explicit ColorSchemeModel(QObject* parent = nullptr);

    virtual int rowCount(const QModelIndex& parent) const override;
    virtual Qt::ItemFlags flags (const QModelIndex& index) const override;
    virtual QVariant data(const QModelIndex &index, int role) const override;
    virtual bool setData(const QModelIndex &index, const QVariant &value, int role) override;

    int currentIndex() const;
    void setCurrentIndex(int newIndex);

    QString currentText() const;
    QVector<Item> getSchemes() const;

    ColorScheme currentScheme() const;

signals:
    void currentChanged();

private:
    class SchemeList
    {
    public:
        virtual ~SchemeList() = default;
        virtual ColorScheme scheme(int i) const = 0;
        virtual QString text(int i) const = 0;
        virtual int size() const = 0;
    };

    class DefaultSchemeList;
    class WinColorSchemeList;

    static std::unique_ptr<SchemeList> createList(ColorSchemeModel *parent);

    // \internal used by SchemeList to notify scheme changed
    void indexChanged(int i);

    const std::unique_ptr<SchemeList> m_list;
    int m_currentIndex;
};

#endif // COLORSCHEMEMODEL_HPP
