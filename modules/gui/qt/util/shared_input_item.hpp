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

#ifndef SHAREDINPUTITEM_HPP
#define SHAREDINPUTITEM_HPP

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <QMetaType>

#include <vlc_common.h>
#include <vlc_input_item.h>
#include <vlc_cxx_helpers.hpp>


using SharedInputItem = ::vlc::vlc_shared_data_ptr<input_item_t,
                                                   &input_item_Hold,
                                                   &input_item_Release>;

Q_DECLARE_METATYPE(SharedInputItem)

#endif // SHAREDINPUTITEM_HPP
