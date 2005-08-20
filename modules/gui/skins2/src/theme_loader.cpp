/*****************************************************************************
 * theme_loader.cpp
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

#include "theme_loader.hpp"
#include "theme.hpp"
#include "../parser/builder.hpp"
#include "../parser/skin_parser.hpp"
#include "../src/os_factory.hpp"
#include "../src/vlcproc.hpp"
#include "../src/window_manager.hpp"

#ifdef HAVE_FCNTL_H
#   include <fcntl.h>
#endif
#ifdef HAVE_SYS_STAT_H
#   include <sys/stat.h>
#endif
#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#elif defined( WIN32 ) && !defined( UNDER_CE )
#   include <direct.h>
#endif

#ifdef HAVE_DIRENT_H
#   include <dirent.h>
#endif


#if defined( HAVE_ZLIB_H )
#   include <zlib.h>
#   include <errno.h>
int gzopen_frontend ( char *pathname, int oflags, int mode );
int gzclose_frontend( int );
int gzread_frontend ( int, void *, size_t );
int gzwrite_frontend( int, const void *, size_t );
#if defined( HAVE_LIBTAR_H )
#   include <libtar.h>
#else
typedef gzFile TAR;
int tar_open        ( TAR **t, char *pathname, int oflags );
int tar_extract_all ( TAR *t, char *prefix );
int tar_close       ( TAR *t );
#endif
#endif

#define DEFAULT_XML_FILE "theme.xml"


bool ThemeLoader::load( const string &fileName )
{
    // First, we try to un-targz the file, and if it fails we hope it's a XML
    // file...
#if defined( HAVE_ZLIB_H )
    if( ! extract( fileName ) && ! parse( fileName ) )
        return false;
#else
    if( ! parse( fileName ) )
        return false;
#endif

    Theme *pNewTheme = getIntf()->p_sys->p_theme;
    if( !pNewTheme )
    {
        return false;
    }

    // Check if the skin to load is in the config file, to load its config
    char *skin_last = config_GetPsz( getIntf(), "skins2-last" );
    if( skin_last != NULL && fileName == (string)skin_last )
    {
        // Restore the theme configuration
        getIntf()->p_sys->p_theme->loadConfig();
        // Used to anchor the windows at the beginning
        pNewTheme->getWindowManager().stopMove();
    }
    else
    {
        config_PutPsz( getIntf(), "skins2-last", fileName.c_str() );
        // Show the windows
        pNewTheme->getWindowManager().showAll();
    }
    if( skin_last ) free( skin_last );

    // The new theme cannot embed a video output yet
    VlcProc::instance( getIntf() )->dropVout();

    return true;
}


#if defined( HAVE_ZLIB_H )
bool ThemeLoader::extractTarGz( const string &tarFile, const string &rootDir )
{
    TAR *t;
#if defined( HAVE_LIBTAR_H )
    tartype_t gztype = { (openfunc_t) gzopen_frontend,
                         (closefunc_t) gzclose_frontend,
                         (readfunc_t) gzread_frontend,
                         (writefunc_t) gzwrite_frontend };

    if( tar_open( &t, (char *)tarFile.c_str(), &gztype, O_RDONLY, 0,
                  TAR_GNU ) == -1 )
#else
    if( tar_open( &t, (char *)tarFile.c_str(), O_RDONLY ) == -1 )
#endif
    {
        return false;
    }

    if( tar_extract_all( t, (char *)rootDir.c_str() ) != 0 )
    {
        tar_close( t );
        return false;
    }

    if( tar_close( t ) != 0 )
    {
        return false;
    }

    return true;
}


bool ThemeLoader::extract( const string &fileName )
{
    char *tmpdir = tempnam( NULL, "vlt" );
    string tempPath = tmpdir;
    free( tmpdir );

    // Extract the file in a temporary directory
    if( ! extractTarGz( fileName, tempPath ) )
        return false;

    // Find the XML file and parse it
    string xmlFile;
    if( ! findThemeFile( tempPath, xmlFile ) || ! parse( xmlFile ) )
    {
        msg_Err( getIntf(), "%s doesn't contain a " DEFAULT_XML_FILE " file",
                 fileName.c_str() );
        deleteTempFiles( tempPath );
        return false;
    }

    // Clean-up
    deleteTempFiles( tempPath );
    return true;
}


void ThemeLoader::deleteTempFiles( const string &path )
{
    OSFactory::instance( getIntf() )->rmDir( path );
}
#endif // HAVE_ZLIB_H


bool ThemeLoader::parse( const string &xmlFile )
{
    // File loaded
    msg_Dbg( getIntf(), "Using skin file: %s", xmlFile.c_str() );

    // Extract the path of the XML file
    string path;
    const string &sep = OSFactory::instance( getIntf() )->getDirSeparator();
    string::size_type p = xmlFile.rfind( sep, xmlFile.size() );
    if( p != string::npos )
    {
        path = xmlFile.substr( 0, p + 1 );
    }
    else
    {
        path = "";
    }

    // Start the parser
    SkinParser parser( getIntf(), xmlFile, path );
    if( ! parser.parse() )
    {
        msg_Err( getIntf(), "Failed to parse %s", xmlFile.c_str() );
        return false;
    }

    // Build and store the theme
    Builder builder( getIntf(), parser.getData() );
    getIntf()->p_sys->p_theme = builder.build();

    return true;
}


bool ThemeLoader::findThemeFile( const string &rootDir, string &themeFilePath )
{
    // Path separator
    const string &sep = OSFactory::instance( getIntf() )->getDirSeparator();

    DIR *pCurrDir;
    struct dirent *pDirContent;

    // Open the dir
    pCurrDir = opendir( rootDir.c_str() );

    if( pCurrDir == NULL )
    {
        // An error occurred
        msg_Dbg( getIntf(), "Cannot open directory %s", rootDir.c_str() );
        return false;
    }

    // Get the first directory entry
    pDirContent = (dirent*)readdir( pCurrDir );

    // While we still have entries in the directory
    while( pDirContent != NULL )
    {
        string newURI = rootDir + sep + pDirContent->d_name;

        // Skip . and ..
        if( string( pDirContent->d_name ) != "." &&
            string( pDirContent->d_name ) != ".." )
        {
#if defined( S_ISDIR )
            struct stat stat_data;
            stat( newURI.c_str(), &stat_data );
            if( S_ISDIR(stat_data.st_mode) )
#elif defined( DT_DIR )
            if( pDirContent->d_type & DT_DIR )
#else
            if( 0 )
#endif
            {
                // Can we find the theme file in this subdirectory?
                if( findThemeFile( newURI, themeFilePath ) )
                {
                    closedir( pCurrDir );
                    return true;
                }
            }
            else
            {
                // Found the theme file?
                if( string( DEFAULT_XML_FILE ) ==
                    string( pDirContent->d_name ) )
                {
                    themeFilePath = newURI;
                    closedir( pCurrDir );
                    return true;
                }
            }
        }

        pDirContent = (dirent*)readdir( pCurrDir );
    }

    closedir( pCurrDir );
    return false;
}


#if !defined( HAVE_LIBTAR_H ) && defined( HAVE_ZLIB_H )

#ifdef WIN32
#  define mkdir(dirname,mode) _mkdir(dirname)
#endif

/* Values used in typeflag field */
#define REGTYPE  '0'            /* regular file */
#define AREGTYPE '\0'           /* regular file */
#define DIRTYPE  '5'            /* directory */

#define BLOCKSIZE 512

struct tar_header
{                               /* byte offset */
    char name[100];             /*   0 */
    char mode[8];               /* 100 */
    char uid[8];                /* 108 */
    char gid[8];                /* 116 */
    char size[12];              /* 124 */
    char mtime[12];             /* 136 */
    char chksum[8];             /* 148 */
    char typeflag;              /* 156 */
    char linkname[100];         /* 157 */
    char magic[6];              /* 257 */
    char version[2];            /* 263 */
    char uname[32];             /* 265 */
    char gname[32];             /* 297 */
    char devmajor[8];           /* 329 */
    char devminor[8];           /* 337 */
    char prefix[155];           /* 345 */
                                /* 500 */
};


union tar_buffer {
    char              buffer[BLOCKSIZE];
    struct tar_header header;
};


/* helper functions */
int getoct( char *p, int width );
int makedir( char *newdir );

int tar_open( TAR **t, char *pathname, int oflags )
{
    gzFile f = gzopen( pathname, "rb" );
    if( f == NULL )
    {
        fprintf( stderr, "Couldn't gzopen %s\n", pathname );
        return -1;
    }

    *t = (gzFile *)malloc( sizeof(gzFile) );
    **t = f;
    return 0;
}


int tar_extract_all( TAR *t, char *prefix )
{
    union tar_buffer buffer;
    int   len, err, getheader = 1, remaining = 0;
    FILE  *outfile = NULL;
    char  fname[BLOCKSIZE + PATH_MAX];

    while( 1 )
    {
        len = gzread( *t, &buffer, BLOCKSIZE );
        if( len < 0 )
        {
            fprintf( stderr, "%s\n", gzerror(*t, &err) );
        }

        /*
         * Always expect complete blocks to process
         * the tar information.
         */
        if( len != 0 && len != BLOCKSIZE )
        {
            fprintf( stderr, "gzread: incomplete block read\n" );
            return -1;
        }

        /*
         * If we have to get a tar header
         */
        if( getheader == 1 )
        {
            /*
             * If we met the end of the tar
             * or the end-of-tar block, we are done
             */
            if( (len == 0) || (buffer.header.name[0] == 0) )
            {
                break;
            }

            sprintf( fname, "%s/%s", prefix, buffer.header.name );

            /* Check magic value in header */
            if( strncmp( buffer.header.magic, "GNUtar", 6 ) &&
                strncmp( buffer.header.magic, "ustar", 5 ) )
            {
                //fprintf(stderr, "not a tar file\n");
                return -1;
            }

            switch( buffer.header.typeflag )
            {
                case DIRTYPE:
                    makedir( fname );
                    break;
                case REGTYPE:
                case AREGTYPE:
                    remaining = getoct( buffer.header.size, 12 );
                    if( remaining )
                    {
                        outfile = fopen( fname, "wb" );
                        if( outfile == NULL )
                        {
                            /* try creating directory */
                            char *p = strrchr( fname, '/' );
                            if( p != NULL )
                            {
                                *p = '\0';
                                makedir( fname );
                                *p = '/';
                                outfile = fopen( fname, "wb" );
                                if( !outfile )
                                {
                                    fprintf( stderr, "tar couldn't create %s\n",
                                             fname );
                                }
                            }
                        }
                    }
                    else outfile = NULL;

                /*
                 * could have no contents
                 */
                getheader = (remaining) ? 0 : 1;
                break;
            default:
                break;
            }
        }
        else
        {
            unsigned int bytes = (remaining > BLOCKSIZE)?BLOCKSIZE:remaining;

            if( outfile != NULL )
            {
                if( fwrite( &buffer, sizeof(char), bytes, outfile ) != bytes )
                {
                    fprintf( stderr, "error writing %s skipping...\n", fname );
                    fclose( outfile );
                    unlink( fname );
                }
            }
            remaining -= bytes;
            if( remaining == 0 )
            {
                getheader = 1;
                if( outfile != NULL )
                {
                    fclose(outfile);
                    outfile = NULL;
                }
            }
        }
    }

    return 0;
}


int tar_close( TAR *t )
{
    if( gzclose( *t ) != Z_OK ) fprintf( stderr, "failed gzclose\n" );
    free( t );
    return 0;
}


/* helper functions */
int getoct( char *p, int width )
{
    int result = 0;
    char c;

    while( width-- )
    {
        c = *p++;
        if( c == ' ' )
            continue;
        if( c == 0 )
            break;
        result = result * 8 + (c - '0');
    }
    return result;
}


/* Recursive make directory
 * Abort if you get an ENOENT errno somewhere in the middle
 * e.g. ignore error "mkdir on existing directory"
 *
 * return 1 if OK, 0 on error
 */
int makedir( char *newdir )
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

    if( mkdir( buffer, 0775 ) == 0 )
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
        if( ( mkdir( buffer, 0775 ) == -1 ) && ( errno == ENOENT ) )
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
#endif

#ifdef HAVE_ZLIB_H

static int currentGzFd = -1;
static void * currentGzVp = NULL;

int gzopen_frontend( char *pathname, int oflags, int mode )
{
    char *gzflags;
    gzFile gzf;

    switch( oflags )
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

    gzf = gzopen( pathname, gzflags );
    if( !gzf )
    {
        errno = ENOMEM;
        return -1;
    }

    /** Hum ... */
    currentGzFd = 42;
    currentGzVp = gzf;

    return currentGzFd;
}

int gzclose_frontend( int fd )
{
    if( currentGzVp != NULL && fd != -1 )
    {
        void *toClose = currentGzVp;
        currentGzVp = NULL;  currentGzFd = -1;
        return gzclose( toClose );
    }
    return -1;
}

int gzread_frontend( int fd, void *p_buffer, size_t i_length )
{
    if( currentGzVp != NULL && fd != -1 )
    {
        return gzread( currentGzVp, p_buffer, i_length );
    }
    return -1;
}

int gzwrite_frontend( int fd, const void * p_buffer, size_t i_length )
{
    if( currentGzVp != NULL && fd != -1 )
    {
        return gzwrite( currentGzVp, p_buffer, i_length );
    }
    return -1;
}

#endif
