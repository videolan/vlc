/*****************************************************************************
 * ini_file.cpp
 *****************************************************************************
 * Copyright (C) 2006 the VideoLAN team
 *
 * Authors: Cyril Deguet     <asmax@via.ecp.fr>
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

#include "ini_file.hpp"
#include "var_manager.hpp"
#include <fstream>


IniFile::IniFile( intf_thread_t *pIntf, const std::string &rName,
                  const std::string &rPath ):
    SkinObject( pIntf ), m_name( rName ), m_path( rPath )
{
}


void IniFile::parseFile()
{
    VarManager *pVarManager = VarManager::instance( getIntf() );

    // Open the file
    std::fstream fs( m_path.c_str(), std::fstream::in );
    if( fs.is_open() )
    {
        std::string section;
        std::string line;
        while( !fs.eof() )
        {
            // Read the next line
            fs >> line;

            switch( line[0] )
            {
            // "[section]" line ?
            case '[':
                section = line.substr( 1, line.size() - 2);
                break;

            // Comment
            case ';':
            case '#':
                break;

            // Variable declaration
            default:
                size_t eqPos = line.find( '=' );
                std::string var = line.substr( 0, eqPos );
                std::string val = line.substr( eqPos + 1, line.size() - eqPos - 1);

                std::string name = m_name + "." + section + "." + var;

                // Convert to lower case because of some buggy winamp2 skins
                for( size_t i = 0; i < name.size(); i++ )
                {
                    name[i] = tolower( (unsigned char)name[i] );
                }

                // Register the value in the var manager
                pVarManager->registerConst( name, val );
            }
        }
        fs.close();
    }
    else
    {
        msg_Err( getIntf(), "Failed to open INI file %s", m_path.c_str() );
    }
}

