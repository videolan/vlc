/*****************************************************************************
 * variables.cpp : VLC variable class
 ****************************************************************************
 * Copyright (C) 2019 VLC authors and VideoLAN
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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "qt.hpp"
#include "variables.hpp"

void QVLCBool::setValue(bool value)
{
    setValueInternal(value);
}

void QVLCString::setValue(QString value)
{
    setValueInternal(value);
}

void QVLCFloat::setValue(float value)
{
    setValueInternal(value);
}

void QVLCInteger::setValue(int64_t value)
{
    setValueInternal(value);
}
