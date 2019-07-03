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

#ifndef VLC_SETTINGS_HPP
#define VLC_SETTINGS_HPP

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "qt.hpp"
#include <QObject>
#include <QSettings>

#define QML_AVAILABLE_PROPERTY(PropertyName) \
    Q_PROPERTY(QVariant PropertyName READ get##PropertyName WRITE set##PropertyName NOTIFY PropertyName##Changed) \
    public: \
        QVariant get##PropertyName() const; \
        void set##PropertyName(QVariant); \
        Q_SIGNAL void PropertyName##Changed();

class Settings : public QObject
{
    Q_OBJECT

    QML_AVAILABLE_PROPERTY(VLCStyle_colors_state)
    QML_AVAILABLE_PROPERTY(medialib_gridView)

public:
    Settings(intf_thread_t *_p_intf,QObject * parent = nullptr);

private:
    intf_thread_t *p_intf;

};
#undef QML_AVAILABLE_PROPERTY
#endif
