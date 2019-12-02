/*****************************************************************************
 * Access to vlc_gettext from QML
 ****************************************************************************
 * Copyright (C) 2019 the VideoLAN team
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

#ifndef I18N_HPP
#define I18N_HPP

#include <QString>
#include <QObject>

class I18n : public QObject
{
    Q_OBJECT
public:
    I18n(QObject* parent = nullptr);
public:
#ifdef qtr
#undef qtr
    Q_INVOKABLE QString qtr(QString msgid) const;
#define qtr(i) QString::fromUtf8( vlc_gettext(i) )
#else
    Q_INVOKABLE QString qtr(const QString msgid) const;
#endif

};

#endif // I18N_HPP
