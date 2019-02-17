/*****************************************************************************
 * cmd_change_skin.cpp
 *****************************************************************************
 * Copyright (C) 2003 the VideoLAN team
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include "cmd_change_skin.hpp"
#include "cmd_quit.hpp"
#include "../src/os_factory.hpp"
#include "../src/os_loop.hpp"
#include "../src/theme.hpp"
#include "../src/theme_loader.hpp"
#include "../src/theme_repository.hpp"
#include "../src/window_manager.hpp"
#include "../src/vout_manager.hpp"
#include "../src/vlcproc.hpp"


void CmdChangeSkin::execute()
{
    // Save the old theme to restore it in case of problem
    Theme *pOldTheme = getIntf()->p_sys->p_theme;

    if( pOldTheme )
    {
        pOldTheme->getWindowManager().saveVisibility();
        pOldTheme->getWindowManager().hideAll();
    }

    VoutManager::instance( getIntf() )->saveVoutConfig();

    ThemeLoader loader( getIntf() );
    if( loader.load( m_file ) )
    {
        // Everything went well
        msg_Info( getIntf(), "new theme successfully loaded (%s)",
                 m_file.c_str() );
        delete pOldTheme;

        // restore vout config
        VoutManager::instance( getIntf() )->restoreVoutConfig( true );
    }
    else if( pOldTheme )
    {
        msg_Warn( getIntf(), "a problem occurred when loading the new theme,"
                  " restoring the previous one" );
        getIntf()->p_sys->p_theme = pOldTheme;
        VoutManager::instance( getIntf() )->restoreVoutConfig( false );
        pOldTheme->getWindowManager().restoreVisibility();
    }
    else
    {
        msg_Err( getIntf(), "cannot load the theme, aborting" );
        // Quit
        CmdQuit cmd( getIntf() );
        cmd.execute();
    }

   // update the repository
   ThemeRepository::instance( getIntf() )->updateRepository();
}

