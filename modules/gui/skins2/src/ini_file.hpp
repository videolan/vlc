/*****************************************************************************
 * ini_file.hpp
 *****************************************************************************
 * Copyright (C) 2006 the VideoLAN team
 * $Id$
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef INI_FILE_HPP
#define INI_FILE_HPP

#include "skin_common.hpp"
#include <string>
#include <map>


/// INI file parser
class IniFile: public SkinObject
{
public:
    IniFile( intf_thread_t *pIntf, const string &rName,
             const string &rPath );
    virtual ~IniFile() { }

    /// Parse the INI file and fill the VarManager
    void parseFile();

private:
    string m_name;
    string m_path;
};


#endif
