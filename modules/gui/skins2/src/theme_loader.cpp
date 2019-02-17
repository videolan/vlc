/*****************************************************************************
 * theme_loader.cpp
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fstream>
#include <memory>

#include <vlc_common.h>
#include <vlc_fs.h>
#include <vlc_url.h>
#include <vlc_stream_extractor.h>

#include "theme_loader.hpp"
#include "theme.hpp"
#include "../parser/builder.hpp"
#include "../parser/skin_parser.hpp"
#include "../src/os_factory.hpp"
#include "../src/vlcproc.hpp"
#include "../src/window_manager.hpp"

#define DEFAULT_XML_FILE "theme.xml"
#define WINAMP2_XML_FILE "winamp2.xml"

/* Recursive make directory
 * Abort if you get an ENOENT errno somewhere in the middle
 * e.g. ignore error "mkdir on existing directory"
 *
 * return 1 if OK, 0 on error
 */
static int makedir( const char *newdir )
{
    char *p, *buffer = strdup( newdir );
    int  len = strlen( buffer );

    if( len <= 0 )
    {
        free( buffer );
        return 0;
    }

    if( buffer[len-1] == '/' )
    {
        buffer[len-1] = '\0';
    }

    if( vlc_mkdir( buffer, 0775 ) == 0 )
    {
        free( buffer );
        return 1;
    }

    p = buffer + 1;
    while( 1 )
    {
        char hold;

        while( *p && *p != '\\' && *p != '/' ) p++;
        hold = *p;
        *p = 0;
        if( ( vlc_mkdir( buffer, 0775 ) == -1 ) && ( errno == ENOENT ) )
        {
            fprintf( stderr, "couldn't create directory %s\n", buffer );
            free( buffer );
            return 0;
        }
        if( hold == 0 ) break;
        *p++ = hold;
    }
    free( buffer );
    return 1;
}

bool ThemeLoader::load( const std::string &fileName )
{
    std::string path = getFilePath( fileName );

    //Before all, let's see if the file is present
    struct stat p_stat;
    if( vlc_stat( fileName.c_str(), &p_stat ) )
        return false;

    // First, we try to un-targz the file, and if it fails we hope it's a XML
    // file...

    if( ! extract( fileName ) && ! parse( path, fileName ) )
        return false;

    Theme *pNewTheme = getIntf()->p_sys->p_theme;
    if( !pNewTheme )
        return false;

    // Restore the theme configuration
    getIntf()->p_sys->p_theme->loadConfig();

    // Retain new loaded skins in config
    config_PutPsz( "skins2-last", fileName.c_str() );

    return true;
}

bool ThemeLoader::extract( const std::string &fileName )
{
    bool result = true;
    std::string tempPath = getTmpDir();
    if( tempPath.empty() )
        return false;

    if( unarchive( fileName, tempPath ) == false )
    {
        msg_Err( getIntf(), "extraction from %s failed", fileName.c_str() );
        return false;
    }

    std::string path;
    std::string xmlFile;
    OSFactory *pOsFactory = OSFactory::instance( getIntf() );
    // Find the XML file in the theme
    if( findFile( tempPath, DEFAULT_XML_FILE, xmlFile ) )
    {
        path = getFilePath( xmlFile );
    }
    else
    {
        // No XML file, check if it is a winamp2 skin
        std::string mainBmp;
        if( findFile( tempPath, "main.bmp", mainBmp ) )
        {
            msg_Dbg( getIntf(), "trying to load a winamp2 skin" );
            path = getFilePath( mainBmp );

            // Look for winamp2.xml in the resource path
            std::list<std::string> resPath = pOsFactory->getResourcePath();
            std::list<std::string>::const_iterator it;
            for( it = resPath.begin(); it != resPath.end(); ++it )
            {
                if( findFile( *it, WINAMP2_XML_FILE, xmlFile ) )
                    break;
            }
        }
    }

    if( !xmlFile.empty() )
    {
        // Parse the XML file
        if (! parse( path, xmlFile ) )
        {
            msg_Err( getIntf(), "error while parsing %s", xmlFile.c_str() );
            result = false;
        }
    }
    else
    {
        msg_Err( getIntf(), "no XML found in theme %s", fileName.c_str() );
        result = false;
    }

    // Clean-up
    deleteTempFiles( tempPath );
    return result;
}

bool ThemeLoader::unarchive( const std::string& fileName, const std::string &tempPath )
{
#define UPTR_HELPER(type,deleter) []( type * data ) { \
        return std::unique_ptr< type, decltype( deleter )> ( data, deleter ); }

    auto make_input_node_ptr = UPTR_HELPER( input_item_node_t, &input_item_node_Delete );
    auto make_input_item_ptr = UPTR_HELPER( input_item_t, &input_item_Release );
    auto make_stream_ptr = UPTR_HELPER( stream_t, &vlc_stream_Delete );
    auto make_cstr_ptr = UPTR_HELPER( char, &std::free );

#undef UPTR_HELPER

    auto uri = make_cstr_ptr( vlc_path2uri( fileName.c_str(), "file" ) );
    if( !uri )
    {
        msg_Err( getIntf(), "unable to convert %s to local URI",
                            fileName.c_str() );
        return false;
    }

    auto input = make_stream_ptr( vlc_stream_NewURL( getIntf(), uri.get() ) );
    if( !input )
    {
        msg_Err( getIntf(), "unable to open %s", uri.get() );
        return false;
    }

    stream_t* stream = input.get();
    if( vlc_stream_directory_Attach( &stream, NULL ) )
    {
        msg_Err( getIntf(), "unable to attach stream_directory, treat as XML!" );
    }
    else
    {
        input.release();
        input.reset( stream );

        auto item = make_input_item_ptr( input_item_New( "vlc://dummy", "vlc://dummy" ) );
        auto node = make_input_node_ptr( (input_item_node_t*)std::calloc( 1, sizeof( input_item_node_t ) ) );

        if( !item || !node )
            return false;

        input_item_AddOption( item.get(), "ignore-filetypes=\"\"", VLC_INPUT_OPTION_TRUSTED );
        input_item_AddOption( item.get(), "extractor-flatten", VLC_INPUT_OPTION_TRUSTED );
        node->p_item = item.release();

        if( vlc_stream_ReadDir( input.get(), node.get() ) )
        {
            msg_Err( getIntf(), "unable to read items in %s", uri.get() );
            return false;
        }

        for( int i = 0; i < node->i_children; ++i )
        {
            auto child = node->pp_children[i]->p_item;
            auto child_stream = make_stream_ptr( vlc_stream_NewMRL( getIntf(), child->psz_uri ) );
            if( !child_stream )
            {
                msg_Err( getIntf(), "unable to open %s for reading", child->psz_name );
                return false;
            }

            auto out_path = tempPath + "/" + child->psz_name;

            { /* create directory tree */
                auto out_directory = out_path.substr( 0, out_path.find_last_of( '/' ) );

                if( makedir( out_directory.c_str() ) == false )
                {
                    msg_Err( getIntf(), "failed to create directory tree for %s (%s)",
                             out_path.c_str(), out_directory.c_str() );

                    return false;
                }
            }

            { /* write data to disk */
                std::string contents;

                char buf[1024];
                ssize_t n;

                while( ( n = vlc_stream_Read( child_stream.get(), buf, sizeof buf ) ) > 0 )
                    contents.append( buf, n );

                std::ofstream out_stream( out_path, std::ios::binary );

                if( out_stream.write( contents.data(), contents.size() ) )
                {
                    msg_Dbg( getIntf(), "finished writing %zu bytes to %s",
                        size_t{ contents.size() }, out_path.c_str() );
                }
                else
                {
                    msg_Err( getIntf(), "unable to write %zu bytes to %s",
                        size_t{ contents.size() }, out_path.c_str() );
                    return false;
                }
            }
        }
    }

    return true;
}

void ThemeLoader::deleteTempFiles( const std::string &path )
{
    OSFactory::instance( getIntf() )->rmDir( path );
}

bool ThemeLoader::parse( const std::string &path, const std::string &xmlFile )
{
    // File loaded
    msg_Dbg( getIntf(), "using skin file: %s", xmlFile.c_str() );

    // Start the parser
    SkinParser parser( getIntf(), xmlFile, path );
    if( ! parser.parse() )
        return false;

    // Build and store the theme
    Builder builder( getIntf(), parser.getData(), path );
    getIntf()->p_sys->p_theme = builder.build();

    return true;
}


std::string ThemeLoader::getFilePath( const std::string &rFullPath )
{
    OSFactory *pOsFactory = OSFactory::instance( getIntf() );
    const std::string &sep = pOsFactory->getDirSeparator();
    // Find the last separator ('/' or '\')
    std::string::size_type p = rFullPath.rfind( sep, rFullPath.size() );
    std::string basePath;
    if( p != std::string::npos )
    {
        if( p < rFullPath.size() - 1)
        {
            basePath = rFullPath.substr( 0, p );
        }
        else
        {
            basePath = rFullPath;
        }
    }
    return basePath;
}

bool ThemeLoader::findFile( const std::string &rootDir, const std::string &rFileName,
                            std::string &themeFilePath )
{
    // Path separator
    const std::string &sep = OSFactory::instance( getIntf() )->getDirSeparator();

    const char *pszDirContent;

    // Open the dir
    DIR *pCurrDir = vlc_opendir( rootDir.c_str() );

    if( pCurrDir == NULL )
    {
        // An error occurred
        msg_Dbg( getIntf(), "cannot open directory %s", rootDir.c_str() );
        return false;
    }

    // While we still have entries in the directory
    while( ( pszDirContent = vlc_readdir( pCurrDir ) ) != NULL )
    {
        std::string newURI = rootDir + sep + pszDirContent;

        // Skip . and ..
        if( std::string( pszDirContent ) != "." &&
            std::string( pszDirContent ) != ".." )
        {
#if defined( S_ISDIR )
            struct stat stat_data;

            if( ( vlc_stat( newURI.c_str(), &stat_data ) == 0 )
             && S_ISDIR(stat_data.st_mode) )
#elif defined( DT_DIR )
            if( pDirContent->d_type & DT_DIR )
#else
            if( 0 )
#endif
            {
                // Can we find the file in this subdirectory?
                if( findFile( newURI, rFileName, themeFilePath ) )
                {
                    closedir( pCurrDir );
                    return true;
                }
            }
            else
            {
                // Found the theme file?
                if( rFileName == std::string( pszDirContent ) )
                {
                    themeFilePath = newURI;
                    closedir( pCurrDir );
                    return true;
                }
            }
        }
    }

    closedir( pCurrDir );
    return false;
}

// FIXME: could become a skins2 OS factory function or a vlc core function
std::string ThemeLoader::getTmpDir( )
{
#if defined( _WIN32 )
    wchar_t *tmpdir = _wtempnam( NULL, L"vlt" );
    if( tmpdir == NULL )
        return "";
    char* utf8 = FromWide( tmpdir );
    free( tmpdir );
    std::string tempPath( utf8 ? utf8 : "" );
    free( utf8 );
    return tempPath;

#elif defined( __OS2__ )
    char *tmpdir = tempnam( NULL, "vlt" );
    if( tmpdir == NULL )
        return "";
    std::string tempPath( sFromLocale( tmpdir ));
    free( tmpdir );
    return tempPath;

#else
    char templ[] = "/tmp/vltXXXXXX";
    char *tmpdir = mkdtemp( templ );
    return std::string( tmpdir ? tmpdir : "");
#endif
}


