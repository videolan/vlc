/*****************************************************************************
 * Copyright (C) 2021 VLC authors and VideoLAN
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

#ifndef QMLINPUTITEM_HPP
#define QMLINPUTITEM_HPP

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

// VLC includes
#include <vlc_media_source.h>
#include <vlc_cxx_helpers.hpp>

// Qt includes
#include <QObject>

class QmlInputItem
{
    Q_GADGET

    using InputItemPtr = vlc_shared_data_ptr_type(input_item_t,
                                                  input_item_Hold,
                                                  input_item_Release);

public:
    QmlInputItem() : item(nullptr) {}

    QmlInputItem(input_item_t * item, bool hold) : item(item, hold) {}

public: // Operators
    QmlInputItem(const QmlInputItem &)  = default;
    QmlInputItem(QmlInputItem       &&) = default;

    QmlInputItem & operator=(const QmlInputItem &)  = default;
    QmlInputItem & operator=(QmlInputItem       &&) = default;

public: // Variables
    InputItemPtr item;
};

Q_DECLARE_METATYPE(QmlInputItem)

#endif // QMLINPUTITEM_HPP
