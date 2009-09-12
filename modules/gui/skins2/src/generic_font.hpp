/*****************************************************************************
 * generic_font.hpp
 *****************************************************************************
 * Copyright (C) 2003 the VideoLAN team
 * $Id$
 *
 * Authors: Cyril Deguet     <asmax@via.ecp.fr>
 *          Olivier Teulière <ipkiss@via.ecp.fr>
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef GENERIC_FONT_HPP
#define GENERIC_FONT_HPP

#include "skin_common.hpp"
#include "../utils/pointer.hpp"

class GenericBitmap;
class UString;

/// Base class for fonts
class GenericFont: public SkinObject
{
public:
    virtual ~GenericFont() { }

    virtual bool init() = 0;

    /// Render a string on a bitmap.
    /// If maxWidth != -1, the text is truncated with '...'
    /// The Bitmap is _not_ owned by this object
    virtual GenericBitmap *drawString( const UString &rString,
        uint32_t color, int maxWidth = -1 ) const = 0;

    /// Get the font size
    virtual int getSize() const = 0;

protected:
    GenericFont( intf_thread_t *pIntf ): SkinObject( pIntf ) { }
};

typedef CountedPtr<GenericFont> GenericFontPtr;


#endif
