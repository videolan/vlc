/*****************************************************************************
 * theme_loader.hpp
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

#ifndef THEME_LOADER_HPP
#define THEME_LOADER_HPP

#include "skin_common.hpp"
#include <string>


class ThemeLoader: public SkinObject
{
    public:
        ThemeLoader( intf_thread_t *pIntf ): SkinObject( pIntf ) {}
        virtual ~ThemeLoader() {}

        bool load( const string &fileName );

    private:
#if defined( HAVE_ZLIB_H )
        /// Extract files from an archive (currently only handles tar.gz)
        bool extract( const string &fileName );

        /// Extract files from a tar.gz archive
        bool extractTarGz( const string &tarFile, const string &rootDir );

        /// Clean up the temporary files created by the extraction
        void deleteTempFiles( const string &path );
#endif

        /// Parse the XML file given as a parameter and build the skin
        bool parse( const string &xmlFile );

        /// Recursively look for the XML file from rootDir.
        /// The first corresponding file found will be chosen and themeFilePath
        /// will be updated accordingly.
        /// The method returns true if a theme file was found, false otherwise
        bool findThemeFile( const string &rootDir, string &themeFilePath );
};

#endif
