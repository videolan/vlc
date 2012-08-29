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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <fcntl.h>
#include <sys/stat.h>
#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#endif

#include <vlc_common.h>
#include <vlc_fs.h>

#include "theme_loader.hpp"
#include "theme.hpp"
#include "../parser/builder.hpp"
#include "../parser/skin_parser.hpp"
#include "../src/os_factory.hpp"
#include "../src/vlcproc.hpp"
#include "../src/window_manager.hpp"

#if defined( HAVE_ZLIB_H )
#   include <zlib.h>
#   include <errno.h>
int gzopen_frontend ( const char *pathname, int oflags, int mode );
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
int getoct( char *p, int width );
#endif
int makedir( const char *newdir );
#endif

#define DEFAULT_XML_FILE "theme.xml"
#define WINAMP2_XML_FILE "winamp2.xml"
#define ZIP_BUFFER_SIZE 4096


bool ThemeLoader::load( const string &fileName )
{
    string path = getFilePath( fileName );

    //Before all, let's see if the file is present
    struct stat p_stat;
    if( vlc_stat( fileName.c_str(), &p_stat ) )
        return false;

    // First, we try to un-targz the file, and if it fails we hope it's a XML
    // file...

#if defined( HAVE_ZLIB_H )
    if( ! extract( sToLocale( fileName ) ) && ! parse( path, fileName ) )
        return false;
#else
    if( ! parse( path, fileName ) )
        return false;
#endif

    Theme *pNewTheme = getIntf()->p_sys->p_theme;
    if( !pNewTheme )
        return false;

    // Restore the theme configuration
    getIntf()->p_sys->p_theme->loadConfig();

    // Retain new loaded skins in config
    config_PutPsz( getIntf(), "skins2-last", fileName.c_str() );

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


bool ThemeLoader::extractZip( const string &zipFile, const string &rootDir )
{
    bool b_isWsz = strstr( zipFile.c_str(), ".wsz" );

    // Try to open the ZIP file
    unzFile file = unzOpen( zipFile.c_str() );
    unz_global_info info;

    if( unzGetGlobalInfo( file, &info ) != UNZ_OK )
    {
        return false;
    }
    // Extract all the files in the archive
    for( unsigned long i = 0; i < info.number_entry; i++ )
    {
        if( !extractFileInZip( file, rootDir, b_isWsz ) )
        {
            msg_Warn( getIntf(), "error while unzipping %s",
                      zipFile.c_str() );
            unzClose( file );
            return false;
        }

        if( i < info.number_entry - 1 )
        {
            // Go the next file in the archive
            if( unzGoToNextFile( file ) !=UNZ_OK )
            {
                msg_Warn( getIntf(), "error while unzipping %s",
                          zipFile.c_str() );
                unzClose( file );
                return false;
            }
        }
    }
    unzClose( file );
    return true;
}


bool ThemeLoader::extractFileInZip( unzFile file, const string &rootDir,
                                    bool isWsz )
{
    // Read info for the current file
    char filenameInZip[256];
    unz_file_info fileInfo;
    if( unzGetCurrentFileInfo( file, &fileInfo, filenameInZip,
                               sizeof( filenameInZip), NULL, 0, NULL, 0 )
        != UNZ_OK )
    {
        return false;
    }

    // Convert the file name to lower case, because some winamp skins
    // use the wrong case...
    if( isWsz )
        for( size_t i = 0; i < strlen( filenameInZip ); i++ )
            filenameInZip[i] = tolower( (unsigned char)filenameInZip[i] );

    // Allocate the buffer
    void *pBuffer = malloc( ZIP_BUFFER_SIZE );
    if( !pBuffer )
        return false;

    // Get the path of the file
    OSFactory *pOsFactory = OSFactory::instance( getIntf() );
    string fullPath = rootDir
        + pOsFactory->getDirSeparator()
        + fixDirSeparators( filenameInZip );
    string basePath = getFilePath( fullPath );

    // Extract the file if is not a directory
    if( basePath != fullPath )
    {
        if( unzOpenCurrentFile( file ) )
        {
            free( pBuffer );
            return false;
        }
        makedir( basePath.c_str() );
        FILE *fout = fopen( fullPath.c_str(), "wb" );
        if( fout == NULL )
        {
            msg_Err( getIntf(), "error opening %s", fullPath.c_str() );
            free( pBuffer );
            return false;
        }

        // Extract the current file
        int n;
        do
        {
            n = unzReadCurrentFile( file, pBuffer, ZIP_BUFFER_SIZE );
            if( n < 0 )
            {
                msg_Err( getIntf(), "error while reading zip file" );
                fclose(fout);
                free( pBuffer );
                return false;
            }
            else if( n > 0 )
            {
                if( fwrite( pBuffer, n , 1, fout) != 1 )
                {
                    msg_Err( getIntf(), "error while writing %s",
                             fullPath.c_str() );
                    fclose(fout);
                    free( pBuffer );
                    return false;
                }
            }
        } while( n > 0 );

        fclose(fout);

        if( unzCloseCurrentFile( file ) != UNZ_OK )
        {
            free( pBuffer );
            return false;
        }
    }

    free( pBuffer );
    return true;
}


bool ThemeLoader::extract( const string &fileName )
{
    bool result = true;
    char *tmpdir = tempnam( NULL, "vlt" );
    string tempPath = sFromLocale( tmpdir );
    free( tmpdir );

    // Extract the file in a temporary directory
    if( ! extractTarGz( fileName, tempPath ) &&
        ! extractZip( fileName, tempPath ) )
    {
        deleteTempFiles( tempPath );
        return false;
    }

    string path;
    string xmlFile;
    OSFactory *pOsFactory = OSFactory::instance( getIntf() );
    // Find the XML file in the theme
    if( findFile( tempPath, DEFAULT_XML_FILE, xmlFile ) )
    {
        path = getFilePath( xmlFile );
    }
    else
    {
        // No XML file, check if it is a winamp2 skin
        string mainBmp;
        if( findFile( tempPath, "main.bmp", mainBmp ) )
        {
            msg_Dbg( getIntf(), "trying to load a winamp2 skin" );
            path = getFilePath( mainBmp );

            // Look for winamp2.xml in the resource path
            list<string> resPath = pOsFactory->getResourcePath();
            list<string>::const_iterator it;
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


void ThemeLoader::deleteTempFiles( const string &path )
{
    OSFactory::instance( getIntf() )->rmDir( path );
}
#endif // HAVE_ZLIB_H


bool ThemeLoader::parse( const string &path, const string &xmlFile )
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


string ThemeLoader::getFilePath( const string &rFullPath )
{
    OSFactory *pOsFactory = OSFactory::instance( getIntf() );
    const string &sep = pOsFactory->getDirSeparator();
    // Find the last separator ('/' or '\')
    string::size_type p = rFullPath.rfind( sep, rFullPath.size() );
    string basePath;
    if( p != string::npos )
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


string ThemeLoader::fixDirSeparators( const string &rPath )
{
    OSFactory *pOsFactory = OSFactory::instance( getIntf() );
    const string &sep = pOsFactory->getDirSeparator();
    string::size_type p = rPath.find( "/", 0 );
    string newPath = rPath;
    while( p != string::npos )
    {
        newPath = newPath.replace( p, 1, sep );
        p = newPath.find( "/", p + 1 );
    }
    return newPath;
}


bool ThemeLoader::findFile( const string &rootDir, const string &rFileName,
                            string &themeFilePath )
{
    // Path separator
    const string &sep = OSFactory::instance( getIntf() )->getDirSeparator();

    DIR *pCurrDir;
    char *pszDirContent;

    // Open the dir
    pCurrDir = vlc_opendir( rootDir.c_str() );

    if( pCurrDir == NULL )
    {
        // An error occurred
        msg_Dbg( getIntf(), "cannot open directory %s", rootDir.c_str() );
        return false;
    }

    // While we still have entries in the directory
    while( ( pszDirContent = vlc_readdir( pCurrDir ) ) != NULL )
    {
        string newURI = rootDir + sep + pszDirContent;

        // Skip . and ..
        if( string( pszDirContent ) != "." &&
            string( pszDirContent ) != ".." )
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
                    free( pszDirContent );
                    closedir( pCurrDir );
                    return true;
                }
            }
            else
            {
                // Found the theme file?
                if( rFileName == string( pszDirContent ) )
                {
                    themeFilePath = newURI;
                    free( pszDirContent );
                    closedir( pCurrDir );
                    return true;
                }
            }
        }

        free( pszDirContent );
    }

    closedir( pCurrDir );
    return false;
}


#if !defined( HAVE_LIBTAR_H ) && defined( HAVE_ZLIB_H )

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



int tar_open( TAR **t, char *pathname, int oflags )
{
    (void)oflags;

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
                if( !remaining ) outfile = NULL; else
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
                    outfile = NULL;
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

#endif

/* Recursive make directory
 * Abort if you get an ENOENT errno somewhere in the middle
 * e.g. ignore error "mkdir on existing directory"
 *
 * return 1 if OK, 0 on error
 */
int makedir( const char *newdir )
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

#ifdef HAVE_ZLIB_H

static int currentGzFd = -1;
static void * currentGzVp = NULL;

int gzopen_frontend( const char *pathname, int oflags, int mode )
{
    (void)mode;

    const char *gzflags;
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
        return gzclose( (gzFile) toClose );
    }
    return -1;
}

int gzread_frontend( int fd, void *p_buffer, size_t i_length )
{
    if( currentGzVp != NULL && fd != -1 )
    {
        return gzread( (gzFile) currentGzVp, p_buffer, i_length );
    }
    return -1;
}

int gzwrite_frontend( int fd, const void * p_buffer, size_t i_length )
{
    if( currentGzVp != NULL && fd != -1 )
    {
        return gzwrite( (gzFile) currentGzVp, const_cast<void*>(p_buffer), i_length );
    }
    return -1;
}

#endif
