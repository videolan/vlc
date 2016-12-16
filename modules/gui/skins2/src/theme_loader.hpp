/*****************************************************************************
 * theme_loader.hpp
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

#ifndef THEME_LOADER_HPP
#define THEME_LOADER_HPP

#include "skin_common.hpp"
#include <string>
#if defined( HAVE_ZLIB_H )
#   include "unzip.h"
#endif

class ThemeLoader: public SkinObject
{
public:
    ThemeLoader( intf_thread_t *pIntf ): SkinObject( pIntf ) { }
    virtual ~ThemeLoader() { }

    /// The expected fileName must be an UTF-8 string
    bool load( const std::string &fileName );

private:
#if defined( HAVE_ZLIB_H )
    /// Extract files from an archive (handles tar.gz and zip)
    /**
     * Expects a string from the current locale.
     */
    bool extract( const std::string &fileName );

    bool unarchive( const std::string &fileName, const std::string &tempPath );

    /// Extract files from a tar.gz archive
    /**
     * Expects strings from the current locale.
     */
    bool extractTarGz( const std::string &tarFile, const std::string &rootDir );

    /// Extract files from a .zip archive
    /**
     * Expects strings from the current locale.
     */
    bool extractZip( const std::string &zipFile, const std::string &rootDir );

    /// Extract the current file from a .zip archive
    /**
     * Expects a string from the current locale.
     */
    bool extractFileInZip( unzFile file, const std::string &rootDir, bool isWsz );

    /// Clean up the temporary files created by the extraction
    /**
     * Expects a string from the current locale.
     */
    void deleteTempFiles( const std::string &path );

    /// Get a unique temporary directory
    std::string getTmpDir( );
#endif

    /// Parse the XML file given as a parameter and build the skin
    /**
     * Expects UTF8 strings
     */
    bool parse( const std::string &path, const std::string &xmlFile );

    /// Recursively look for the XML file from rootDir.
    /**
     * The first corresponding file found will be chosen and themeFilePath
     * will be updated accordingly.
     * The method returns true if a theme file was found, false otherwise.
     * rootDir and rFilename must both be strings in the current locale,
     * whereas themeFilePath will be in UTF8.
     */
    bool findFile( const std::string &rootDir, const std::string &rFileName,
                   std::string &themeFilePath );

    /// Get the base path of a file
    std::string getFilePath( const std::string &rFullPath );

    /// Replace '/' separators by the actual separator of the OS
    std::string fixDirSeparators( const std::string &rPath );
};

#endif
