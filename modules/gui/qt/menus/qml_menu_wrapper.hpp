/*****************************************************************************
 * Copyright (C) 2020 VLC authors and VideoLAN
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
#ifndef QMLMENUWRAPPER_HPP
#define QMLMENUWRAPPER_HPP

#include "qt.hpp"

#include <QObject>
#include <QPoint>

#include "menus.hpp"

class QmlMainContext;

#define SIMPLE_MENU_PROPERTY(type, name, defaultValue) \
    Q_PROPERTY(type name READ get##name WRITE set##name) \
    public: \
    inline void set##name( type data) { m_##name = data; } \
    inline type get##name() const { return m_##name; } \
    private: \
    type m_##name = defaultValue;

//inherit VLCMenuBar so we can access menu creation functions
class QmlGlobalMenu : public VLCMenuBar
{
    Q_OBJECT
    SIMPLE_MENU_PROPERTY(QmlMainContext*, ctx, nullptr)
public:
    explicit QmlGlobalMenu(QObject *parent = nullptr);

public slots:
    void popup( QPoint pos );
};

#undef SIMPLE_MENU_PROPERTY

#endif // QMLMENUWRAPPER_HPP
