/*****************************************************************************
 * themeloader.cpp: ThemeLoader class
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: themeloader.cpp,v 1.13 2003/06/08 16:56:48 gbazin Exp $
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
#if !defined( WIN32 )
#   include <unistd.h>
#else
#   include <direct.h>
#   ifndef PATH_MAX
#       define PATH_MAX MAX_PATH
#   endif
#endif

//--- VLC -------------------------------------------------------------------
#include <vlc/vlc.h>
#include <vlc/intf.h>

#if defined( HAVE_ZLIB_H )
#   include <zlib.h>
#   include <errno.h>
#endif

#if defined( HAVE_ZLIB_H ) && defined( HAVE_LIBTAR_H )
#   include <libtar.h>
#elif defined( HAVE_ZLIB_H )
    typedef gzFile TAR;
    int tar_open         ( TAR **t, char *pathname, int oflags );
    int tar_extract_all  ( TAR *t, char *prefix );
    int tar_close        ( TAR *t );
#endif

//--- SKIN ------------------------------------------------------------------
#include "../os_api.h"
#include "theme.h"
#include "../os_theme.h"
#include "themeloader.h"
#include "skin_common.h"

#include "anchor.h"
#include "window.h"

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
#if defined( HAVE_ZLIB_H )
int gzopen_frontend( char *pathname, int oflags, int mode )
{
    char *gzflags;
    gzFile gzf;

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

    gzf = gzopen( pathname, gzflags );
    if( !gzf )
    {
        errno = ENOMEM;
        return -1;
    }

    return (int)gzf;
}
#endif

//---------------------------------------------------------------------------
#if defined( HAVE_ZLIB_H )
bool ThemeLoader::ExtractTarGz( const string tarfile, const string rootdir )
{
    TAR *t;
#if defined( HAVE_LIBTAR_H )
    tartype_t gztype = { (openfunc_t) gzopen_frontend, (closefunc_t) gzclose,
        (readfunc_t) gzread, (writefunc_t) gzwrite };

    if( tar_open( &t, (char *)tarfile.c_str(), &gztype, O_RDONLY, 0,
                  TAR_GNU ) == -1 )
#else
    if( tar_open( &t, (char *)tarfile.c_str(), O_RDONLY ) == -1 )
#endif
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
        msg_Err( p_intf, "%s doesn't contain a " DEFAULT_XML_FILE " file",
                 FileName.c_str() );
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
    char *cwd = new char[PATH_MAX];
    getcwd( cwd, PATH_MAX );

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
#if defined( HAVE_ZLIB_H )
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
        config_SaveConfigFile( p_intf, "skins" );
    }

    return true;
}
//---------------------------------------------------------------------------
#if !defined( HAVE_LIBTAR_H ) && defined( HAVE_ZLIB_H )

#ifdef WIN32
#  define mkdir(dirname,mode) _mkdir(dirname)
#endif

/* Values used in typeflag field.  */
#define REGTYPE  '0'            /* regular file */
#define AREGTYPE '\0'           /* regular file */
#define DIRTYPE  '5'            /* directory */

#define BLOCKSIZE 512

struct tar_header
{                               /* byte offset */
  char name[100];               /*   0 */
  char mode[8];                 /* 100 */
  char uid[8];                  /* 108 */
  char gid[8];                  /* 116 */
  char size[12];                /* 124 */
  char mtime[12];               /* 136 */
  char chksum[8];               /* 148 */
  char typeflag;                /* 156 */
  char linkname[100];           /* 157 */
  char magic[6];                /* 257 */
  char version[2];              /* 263 */
  char uname[32];               /* 265 */
  char gname[32];               /* 297 */
  char devmajor[8];             /* 329 */
  char devminor[8];             /* 337 */
  char prefix[155];             /* 345 */
                                /* 500 */
};

union tar_buffer {
  char               buffer[BLOCKSIZE];
  struct tar_header  header;
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
    union  tar_buffer buffer;
    int    len, err, getheader = 1, remaining = 0;
    FILE   *outfile = NULL;
    char   fname[BLOCKSIZE + PATH_MAX];

    while( 1 )
    {
        len = gzread( *t, &buffer, BLOCKSIZE );
        if(len < 0) fprintf(stderr, "%s", gzerror(*t, &err) );

        /*
         * Always expect complete blocks to process
         * the tar information.
         */
        if( len != 0 && len != BLOCKSIZE )
        {
            fprintf(stderr, "gzread: incomplete block read");
            return -1;
        }

        /*
         * If we have to get a tar header
         */
        if( getheader == 1 )
        {
            /*
             * if we met the end of the tar
             * or the end-of-tar block,
             * we are done
             */
            if( (len == 0)  || (buffer.header.name[0]== 0) ) break;

            sprintf( fname, "%s/%s", prefix, buffer.header.name );
          
            switch (buffer.header.typeflag)
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
                                fprintf( stderr, "tar couldn't create %s",
                                         fname );
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
    if( gzclose( *t ) != Z_OK ) fprintf( stderr, "failed gzclose" );
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
        if (c == ' ') continue;
        if (c == 0) break;
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
    char *p, *buffer = strdup(newdir);
    int  len = strlen(buffer);
  
    if( len <= 0 ) { free(buffer); return 0; }

    if( buffer[len-1] == '/' ) buffer[len-1] = '\0';

    if( mkdir( buffer, 0775 ) == 0 ) { free(buffer); return 1; }

    p = buffer+1;
    while( 1 )
    {
        char hold;

        while( *p && *p != '\\' && *p != '/' ) p++;
        hold = *p;
        *p = 0;
        if( ( mkdir( buffer, 0775 ) == -1 ) && ( errno == ENOENT ) )
        {
            fprintf(stderr,"couldn't create directory %s\n", buffer);
            free(buffer);
            return 0;
        }
        if( hold == 0 ) break;
        *p++ = hold;
    }
    free(buffer);
    return 1;
}

#endif
//---------------------------------------------------------------------------
