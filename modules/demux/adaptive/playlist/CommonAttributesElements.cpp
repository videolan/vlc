/*****************************************************************************
 * CommonAttributesElements.cpp: Defines the common attributes and elements
 *                               for some Dash elements.
 *****************************************************************************
 * Copyright © 2011 VideoLAN
 *
 * Authors: Hugo Beauzée-Luyssen <hugo@beauzee.fr>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "CommonAttributesElements.h"

#include <vlc_common.h>
#include <vlc_arrays.h>

using namespace adaptive::playlist;

CommonAttributesElements::CommonAttributesElements() :
    width( -1 ),
    height( -1 )
{
}

CommonAttributesElements::~CommonAttributesElements()
{
}

const std::string& CommonAttributesElements::getMimeType() const
{
    return mimeType;
}

void CommonAttributesElements::setMimeType( const std::string &mimeType )
{
    this->mimeType = mimeType;
}

int     CommonAttributesElements::getWidth                () const
{
    return width;
}

void    CommonAttributesElements::setWidth( int width )
{
    if ( width > 0 )
        this->width = width;
}

int     CommonAttributesElements::getHeight               () const
{
    return height;
}

void    CommonAttributesElements::setHeight( int height )
{
    if ( height > 0 )
        this->height = height;
}
