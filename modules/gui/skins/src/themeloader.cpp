/*****************************************************************************
 * themeloader.cpp: ThemeLoader class
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: themeloader.cpp,v 1.1 2003/03/18 02:21:47 ipkiss Exp $
 *
 * Authors: Olivier Teulière <ipkiss@via.ecp.fr>
 *          Emmanuel Puig    <karibu@via.ecp.fr>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111,
 * USA.
 *****************************************************************************/


//--- GENERAL ---------------------------------------------------------------
#include <string>
#include <fcntl.h>

#if defined( HAVE_LIBTAR_H ) && defined( HAVE_ZLIB_H )
#   include <zlib.h>
#   include <libtar.h>
#endif

//--- VLC -------------------------------------------------------------------
#include <vlc/vlc.h>
#include <vlc/intf.h>

//--- SKIN ------------------------------------------------------------------
#include "os_api.h"
#include "theme.h"
#include "os_theme.h"
#include "themeloader.h"
#include "skin_common.h"

#define DEFAULT_XML_FILE "theme.xml"

//---------------------------------------------------------------------------
extern "C"
{
    extern FILE *yyin;
    int yylex();
}

//---------------------------------------------------------------------------
ThemeLoader::ThemeLoader( intf_thread_t *_p_intf )
{
    p_intf = _p_intf;
}
//---------------------------------------------------------------------------
ThemeLoader::~ThemeLoader()
{
}
//---------------------------------------------------------------------------
int gzopen_frontend( char *pathname, int oflags, int mode )
{
#if defined( HAVE_LIBTAR_H ) && defined( HAVE_ZLIB_H )
    char *gzflags;
    gzFile gzf;
    int fd;

    switch( oflags & O_ACCMODE )
    {
        case O_WRONLY:
            gzflags = "wb";
            break;
        case O_RDONLY:
            gzflags = "rb";
            break;
        case O_RDWR:
        default:
            errno = EINVAL;
            return -1;
    }

    fd = open( pathname, oflags, mode );
    if( fd == -1 )
        return -1;

//    if( ( oflags & O_CREAT ) && fchmod( fd, mode ) )
//        return -1;

    gzf = gzdopen( fd, gzflags );
    if( !gzf )
    {
        errno = ENOMEM;
        return -1;
    }

    return (int)gzf;
#else
    return 0;
#endif
}
//---------------------------------------------------------------------------
#if defined( HAVE_LIBTAR_H ) && defined( HAVE_ZLIB_H )
bool ThemeLoader::ExtractTarGz( const string tarfile, const string rootdir )
{
    TAR *t;
    tartype_t gztype = { (openfunc_t) gzopen_frontend, (closefunc_t) gzclose,
        (readfunc_t) gzread, (writefunc_t) gzwrite };

    if( tar_open( &t, (char *)tarfile.c_str(), &gztype, O_RDONLY, 0,
                  TAR_GNU ) == -1 )
    {
        return false;
    }

    if( tar_extract_all( t, (char *)rootdir.c_str() ) != 0 )
    {
        return false;
    }

    if( tar_close( t ) != 0 )
    {
        return false;
    }

    return true;
}
//---------------------------------------------------------------------------
bool ThemeLoader::Extract( const string FileName )
{
    char *tmpdir = tempnam( NULL, "vlt" );
    string TempPath = tmpdir;
    free( tmpdir );

    if( ! ExtractTarGz( FileName, TempPath ) )
        return false;

    if( ! Parse( TempPath + DIRECTORY_SEPARATOR + string( DEFAULT_XML_FILE ) ) )
    {
        DeleteTempFiles( TempPath );
        return false;
    }

    DeleteTempFiles( TempPath );
    return true;
}
//---------------------------------------------------------------------------
void ThemeLoader::DeleteTempFiles( const string Path )
{
    OSAPI_RmDir( Path );
}
#endif
//---------------------------------------------------------------------------
void ThemeLoader::CleanTheme()
{
    delete (OSTheme *)p_intf->p_sys->p_theme;
    p_intf->p_sys->p_theme = (Theme *)new OSTheme( p_intf );
}
//---------------------------------------------------------------------------
bool ThemeLoader::Parse( const string XmlFile )
{
    // Things to do before loading theme
    p_intf->p_sys->p_theme->OnLoadTheme();

    // Set the file to parse
    yyin = fopen( XmlFile.c_str(), "r" );
    if( yyin == NULL )
    {
        // Skin cannot be opened
        msg_Warn( p_intf, "Cannot open the specified skin file: %s",
                  XmlFile.c_str() );
        return false;
    }

    // File loaded
    msg_Dbg( p_intf, "Using skin file: %s", XmlFile.c_str() );

    // Save current working directory
    char *cwd = new char[MAX_PATH];
    getcwd( cwd, MAX_PATH );

    // Directory separator is different in each OS !
    int p = XmlFile.rfind( DIRECTORY_SEPARATOR, XmlFile.size() );

    // Change current working directory to xml file
    string path = "";
    if( p > 0 )
        path = XmlFile.substr( 0, p );
    chdir( path.c_str() );

    p_intf->p_sys->b_all_win_closed = false;

    // Start the parser
    int lex = yylex();
    fclose( yyin );
    if( lex )
    {
        // Set old working directory to current
        chdir( cwd );
        delete[] cwd;

        msg_Warn( p_intf, "yylex failed: %i", lex );
        CleanTheme();
        return false;
    }

    // Set old working directory to current
    chdir( cwd );
    delete[] cwd;

    return true;
}
//---------------------------------------------------------------------------
bool ThemeLoader::Load( const string FileName )
{
    // First, we try to un-targz the file, and if it fails we hope it's a XML
    // file...
#if defined( HAVE_LIBTAR_H ) && defined( HAVE_ZLIB_H )
    if( ! Extract( FileName ) && ! Parse( FileName ) )
        return false;
#else
    if( ! Parse( FileName ) )
        return false;
#endif

    // Check if the skin to load is in the config file, to load its config
    char *skin_last = config_GetPsz( p_intf, "skin_last" );
    if( skin_last != NULL && FileName == (string)skin_last )
    {
        p_intf->p_sys->p_theme->LoadConfig();
    }
    else
    {
        config_PutPsz( p_intf, "skin_last", FileName.c_str() );
        config_SaveConfigFile( p_intf, "skin" );
    }

    return true;
}
//---------------------------------------------------------------------------
