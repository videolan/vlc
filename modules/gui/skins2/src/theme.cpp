/*****************************************************************************
 * theme.cpp
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: theme.cpp,v 1.1 2004/01/03 23:31:34 asmax Exp $
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

#include "theme.hpp"


Theme::~Theme()
{
    // Be sure things are destroyed in the right order (XXX check)
    m_layouts.clear();
    m_controls.clear();
    m_windows.clear();
    m_bitmaps.clear();
    m_fonts.clear();
    m_commands.clear();
    m_vars.clear();
}


// Useful macro
#define FIND_OBJECT( mapData, mapName ) \
    map<string, mapData>::const_iterator it; \
    it = mapName.find( id ); \
    if( it == mapName.end() ) \
    { \
        return NULL; \
    } \
    return (*it).second.get();


GenericBitmap *Theme::getBitmapById( const string &id )
{
    FIND_OBJECT( GenericBitmapPtr, m_bitmaps );
}

GenericFont *Theme::getFontById( const string &id )
{
    FIND_OBJECT( GenericFontPtr, m_fonts );
}

GenericWindow *Theme::getWindowById( const string &id )
{
    FIND_OBJECT( GenericWindowPtr, m_windows );
}

GenericLayout *Theme::getLayoutById( const string &id )
{
    FIND_OBJECT( GenericLayoutPtr, m_layouts );
}

CtrlGeneric *Theme::getControlById( const string &id )
{
    FIND_OBJECT( CtrlGenericPtr, m_controls );
}

