/*****************************************************************************
 * theme.cpp
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN (Centrale RÃ©seaux) and its contributors
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
    m_curves.clear();
}


void Theme::loadConfig()
{
    msg_Dbg( getIntf(), "Loading theme configuration");

    // Get config from vlcrc file
    char *save = config_GetPsz( getIntf(), "skins2-config" );
    if( !save ) return;

    // Is there an existing config?
    if( !strcmp( save, "" ) )
    {
        // Show the windows
        m_windowManager.showAll();
        return;
    }

    // Initialization
    map<string, TopWindowPtr>::const_iterator it;
    int i = 0;
    int x, y, visible, scan;

    // Get config for each window
    for( it = m_windows.begin(); it != m_windows.end(); it++ )
    {
        TopWindow *pWin = (*it).second.get();
        // Get config
        scan = sscanf( &save[i * 13], "(%4d,%4d,%1d)", &x, &y, &visible );

        // If config has the correct number of arguments
        if( scan > 2 )
        {
            m_windowManager.startMove( *pWin );
            m_windowManager.move( *pWin, x, y );
            m_windowManager.stopMove();
            if( visible )
            {
                m_windowManager.show( *pWin );
            }
        }

        // Next window
        i++;
    }
    free( save );
}


void Theme::saveConfig()
{
    msg_Dbg( getIntf(), "Saving theme configuration");

    // Initialize char where config is stored
    char *save  = new char[400];
    map<string, TopWindowPtr>::const_iterator it;
    int i = 0;
    int x, y;

    // Save config of every window
    for( it = m_windows.begin(); it != m_windows.end(); it++ )
    {
        TopWindow *pWin = (*it).second.get();
        // Print config
        x = pWin->getLeft();
        y = pWin->getTop();
        sprintf( &save[i * 13], "(%4d,%4d,%1d)", x, y,
            pWin->getVisibleVar().get() );
        i++;
    }

    // Save config to file
    config_PutPsz( getIntf(), "skins2-config", save );

    // Free memory
    delete[] save;
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

TopWindow *Theme::getWindowById( const string &id )
{
    FIND_OBJECT( TopWindowPtr, m_windows );
}

GenericLayout *Theme::getLayoutById( const string &id )
{
    FIND_OBJECT( GenericLayoutPtr, m_layouts );
}

CtrlGeneric *Theme::getControlById( const string &id )
{
    FIND_OBJECT( CtrlGenericPtr, m_controls );
}



