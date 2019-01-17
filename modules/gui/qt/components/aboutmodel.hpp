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
#ifndef ABOUTMODEL_HPP
#define ABOUTMODEL_HPP

#include <QObject>

class AboutModel : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString license READ getLicense CONSTANT)
    Q_PROPERTY(QString authors READ getAuthors CONSTANT)
    Q_PROPERTY(QString thanks  READ getThanks  CONSTANT)
    Q_PROPERTY(QString version  READ getVersion  CONSTANT)
public:
    explicit AboutModel(QObject *parent = nullptr);

    QString getLicense() const;
    QString getAuthors() const;
    QString getThanks() const;
    QString getVersion() const;
};

#endif // ABOUTMODEL_HPP
