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

#include "settings.hpp"

#define QML_AVAILABLE_PROPERTY_IMPL(PropertyName, key, defaultValue) \
    QVariant Settings::get##PropertyName() const { \
        return getSettings()->value(key, defaultValue); \
    } \
    void Settings::set##PropertyName(QVariant value) { \
        getSettings()->setValue(key, value); \
        emit PropertyName##Changed(); \
    }

Settings::Settings(intf_thread_t *_p_intf, QObject *parent)
    : QObject(parent),p_intf( _p_intf )
{
}


QML_AVAILABLE_PROPERTY_IMPL(VLCStyle_colors_state,"VLCStyle-colors-state","system")
QML_AVAILABLE_PROPERTY_IMPL(medialib_gridView,"medialib-gridView",true)
