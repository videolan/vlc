/*****************************************************************************
 * Copyright (C) 2021 VLC authors and VideoLAN
 *
 * Authors: Benjamin Arnaud <bunjee@omega.gg>
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

#include "mlitemcover.hpp"

//-------------------------------------------------------------------------------------------------
// Ctor / dtor
//-------------------------------------------------------------------------------------------------

MLItemCover::MLItemCover(const MLItemId & id)
    : MLItem(id)
    , m_generator(nullptr) {}

//-------------------------------------------------------------------------------------------------
// Interface
//-------------------------------------------------------------------------------------------------

bool MLItemCover::hasGenerator() const
{
    return m_generator.get();
}

void MLItemCover::setGenerator(CoverGenerator * generator)
{
    m_generator.reset(generator);
}

//-------------------------------------------------------------------------------------------------

QString MLItemCover::getCover() const
{
    return m_cover;
}

void MLItemCover::setCover(const QString & fileName)
{
    m_cover = fileName;
}
