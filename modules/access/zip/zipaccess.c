/*****************************************************************************
 * zipaccess.c: Module (access) to extract different archives, based on zlib
 *****************************************************************************
 * Copyright (C) 2009 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Jean-Philippe André <jpeg@videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/** @todo:
 * - implement crypto (using url zip://user:password@path-to-archive!/file)
 * - read files in zip with long name (use unz_file_info.size_filename)
 * - multi-volume archive support ?
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "zip.h"
#include <vlc_access.h>

/** **************************************************************************
 * This is our own access_sys_t for zip files
 *****************************************************************************/
struct access_sys_t
{
    /* zlib / unzip members */
    unzFile            zipFile;
    zlib_filefunc_def *fileFunctions;

    /* file in zip information */
    char              *psz_fileInzip;
};

static int AccessControl( access_t *p_access, int i_query, va_list args );
static ssize_t AccessRead( access_t *, uint8_t *, size_t );
static int AccessSeek( access_t *, uint64_t );
static int OpenFileInZip( access_t *p_access );
static char *unescapeXml( const char *psz_text );

/** **************************************************************************
 * \brief Unescape valid XML string
 * The exact reverse of escapeToXml (zipstream.c)
 *****************************************************************************/
static char *unescapeXml( const char *psz_text )
{
    char *psz_ret = malloc( strlen( psz_text ) + 1 );
    if( unlikely( !psz_ret ) ) return NULL;

    char *psz_tmp = psz_ret;
    for( char *psz_iter = (char*) psz_text; *psz_iter; ++psz_iter, ++psz_tmp )
    {
        if( *psz_iter == '?' )
        {
            int i_value;
            if( !sscanf( ++psz_iter, "%02x", &i_value ) )
            {
                /* Invalid number: URL incorrectly encoded */
                free( psz_ret );
                return NULL;
            }
            *psz_tmp = (char) i_value;
            psz_iter++;
        }
        else if( isAllowedChar( *psz_iter ) )
        {
            *psz_tmp = *psz_iter;
        }
        else
        {
            /* Invalid character encoding for the URL */
            free( psz_ret );
            return NULL;
        }
    }
    *psz_tmp = '\0';

    return psz_ret;
}

/** **************************************************************************
 * \brief Open access
 *****************************************************************************/
int AccessOpen( vlc_object_t *p_this )
{
    access_t     *p_access = (access_t*)p_this;
    access_sys_t *p_sys;
    int i_ret              = VLC_EGENERIC;
    unzFile file           = 0;

    char *psz_pathToZip = NULL, *psz_path = NULL, *psz_sep = NULL;

    if( !strstr( p_access->psz_location, ZIP_SEP ) )
    {
        msg_Dbg( p_access, "location does not contain separator " ZIP_SEP );
        return VLC_EGENERIC;
    }

    p_access->p_sys = p_sys = (access_sys_t*)
            calloc( 1, sizeof( access_sys_t ) );
    if( unlikely( !p_sys ) )
        return VLC_ENOMEM;

    /* Split the MRL */
    psz_path = strdup( p_access->psz_location );
    psz_sep = strstr( psz_path, ZIP_SEP );

    *psz_sep = '\0';
    psz_pathToZip = unescapeXml( psz_path );
    if( !psz_pathToZip )
    {
        /* Maybe this was not an encoded string */
        msg_Dbg( p_access, "not an encoded URL  Trying file '%s'",
                 psz_path );
        psz_pathToZip = strdup( psz_path );
        if( unlikely( !psz_pathToZip ) )
        {
            i_ret = VLC_ENOMEM;
            goto exit;
        }
    }
    p_sys->psz_fileInzip = unescapeXml( psz_sep + ZIP_SEP_LEN );
    if( unlikely( !p_sys->psz_fileInzip ) )
    {
        p_sys->psz_fileInzip = strdup( psz_sep + ZIP_SEP_LEN );
        if( unlikely( !p_sys->psz_fileInzip ) )
        {
            i_ret = VLC_ENOMEM;
            goto exit;
        }
    }

    /* Define IO functions */
    zlib_filefunc_def *p_func = (zlib_filefunc_def*)
                                    calloc( 1, sizeof( zlib_filefunc_def ) );
    if( unlikely( !p_func ) )
    {
        i_ret = VLC_ENOMEM;
        goto exit;
    }
    p_func->zopen_file   = ZipIO_Open;
    p_func->zread_file   = ZipIO_Read;
    p_func->zwrite_file  = ZipIO_Write; // see comment
    p_func->ztell_file   = ZipIO_Tell;
    p_func->zseek_file   = ZipIO_Seek;
    p_func->zclose_file  = ZipIO_Close;
    p_func->zerror_file  = ZipIO_Error;
    p_func->opaque       = p_access;

    /* Open zip archive */
    file = p_access->p_sys->zipFile = unzOpen2( psz_pathToZip, p_func );
    if( !file )
    {
        msg_Err( p_access, "not a valid zip archive: '%s'", psz_pathToZip );
        i_ret = VLC_EGENERIC;
        goto exit;
    }

    /* Open file in zip */
    if( ( i_ret = OpenFileInZip( p_access ) ) != VLC_SUCCESS )
        goto exit;

    /* Set callback */
    ACCESS_SET_CALLBACKS( AccessRead, NULL, AccessControl, AccessSeek );

    /* Get some infos about current file. Maybe we could want some more ? */
    unz_file_info z_info;
    unzGetCurrentFileInfo( file, &z_info, NULL, 0, NULL, 0, NULL, 0 );

    /* Set access information: size is needed for AccessSeek */
    p_access->info.i_size = z_info.uncompressed_size;
    p_access->info.i_pos  = 0;
    p_access->info.b_eof  = false;

    i_ret = VLC_SUCCESS;

exit:
    if( i_ret != VLC_SUCCESS )
    {
        if( file )
        {
            unzCloseCurrentFile( file );
            unzClose( file );
        }
        free( p_sys->psz_fileInzip );
        free( p_sys->fileFunctions );
        free( p_sys );
    }

    free( psz_pathToZip );
    free( psz_path );
    return i_ret;
}

/** **************************************************************************
 * \brief Close access: free structures
 *****************************************************************************/
void AccessClose( vlc_object_t *p_this )
{
    access_t     *p_access = (access_t*)p_this;
    access_sys_t *p_sys = p_access->p_sys;
    if( p_sys )
    {
        unzFile file = p_sys->zipFile;
        if( file )
        {
            unzCloseCurrentFile( file );
            unzClose( file );
        }
        free( p_sys->psz_fileInzip );
        free( p_sys->fileFunctions );
        free( p_sys );
    }
}

/** **************************************************************************
 * \brief Control access
 *****************************************************************************/
static int AccessControl( access_t *p_access, int i_query, va_list args )
{
    bool         *pb_bool;
    int64_t      *pi_64;

    switch( i_query )
    {
        /* */
        case ACCESS_CAN_SEEK:
        case ACCESS_CAN_PAUSE:
        case ACCESS_CAN_CONTROL_PACE:
            pb_bool = (bool*)va_arg( args, bool* );
            *pb_bool = true;
            break;

        case ACCESS_CAN_FASTSEEK:
            pb_bool = (bool*)va_arg( args, bool* );
            *pb_bool = false;
            break;

        case ACCESS_GET_PTS_DELAY:
            pi_64 = (int64_t*)va_arg( args, int64_t * );
            *pi_64 = DEFAULT_PTS_DELAY;
            break;

        /* */
        case ACCESS_SET_PAUSE_STATE:
            /* Nothing to do */
            break;

        case ACCESS_GET_TITLE_INFO:
        case ACCESS_SET_TITLE:
        case ACCESS_SET_SEEKPOINT:
        case ACCESS_SET_PRIVATE_ID_STATE:
        case ACCESS_GET_META:
        case ACCESS_GET_PRIVATE_ID_STATE:
        case ACCESS_GET_CONTENT_TYPE:
            return VLC_EGENERIC;

        default:
            msg_Warn( p_access, "unimplemented query %d in control", i_query );
            return VLC_EGENERIC;

    }
    return VLC_SUCCESS;
}

/** **************************************************************************
 * \brief Read access
 * Reads current opened file in zip. This does not open the file in zip.
 * Return -1 if no data yet, 0 if no more data, else real data read
 *****************************************************************************/
static ssize_t AccessRead( access_t *p_access, uint8_t *p_buffer, size_t sz )
{
    access_sys_t *p_sys = p_access->p_sys;
    assert( p_sys );
    unzFile file = p_sys->zipFile;
    if( !file )
    {
        msg_Err( p_access, "archive not opened !" );
        return VLC_EGENERIC;
    }

    int i_read = 0;
    i_read = unzReadCurrentFile( file, p_buffer, sz );

    p_access->info.i_pos = unztell( file );
    return ( i_read >= 0 ? i_read : VLC_EGENERIC );
}

/** **************************************************************************
 * \brief Seek inside zip file
 *****************************************************************************/
static int AccessSeek( access_t *p_access, uint64_t seek_len )
{
    access_sys_t *p_sys = p_access->p_sys;
    assert( p_sys );
    unzFile file = p_sys->zipFile;

    if( !file )
    {
        msg_Err( p_access, "archive not opened !" );
        return VLC_EGENERIC;
    }

    /* Reopen file in zip if needed */
    if( p_access->info.i_pos > seek_len )
    {
        OpenFileInZip( p_access );
    }

    /* Read seek_len data and drop it */
    unsigned i_seek = 0;
    int i_read = 1;
    char *p_buffer = ( char* ) calloc( 1, ZIP_BUFFER_LEN );
    if( unlikely( !p_buffer ) )
        return VLC_EGENERIC;
    while( ( i_seek < seek_len ) && ( i_read > 0 ) )
    {
        i_read = ( seek_len - i_seek < ZIP_BUFFER_LEN )
               ? ( seek_len - i_seek ) : ZIP_BUFFER_LEN;
        i_read = unzReadCurrentFile( file, p_buffer, i_read );
        if( i_read < 0 )
        {
            msg_Warn( p_access, "could not seek in file" );
            free( p_buffer );
            return VLC_EGENERIC;
        }
        else
        {
            i_seek += i_read;
        }
    }
    free( p_buffer );

    p_access->info.i_pos = unztell( file );
    return VLC_SUCCESS;
}

/** **************************************************************************
 * \brief Open file in zip
 *****************************************************************************/
static int OpenFileInZip( access_t *p_access )
{
    access_sys_t *p_sys = p_access->p_sys;
    unzFile file = p_sys->zipFile;
    if( !p_sys->psz_fileInzip )
    {
        return VLC_EGENERIC;
    }

    p_access->info.i_pos = 0;

    unzCloseCurrentFile( file ); /* returns UNZ_PARAMERROR if file not opened */
    if( unzLocateFile( file, p_sys->psz_fileInzip, 0 ) != UNZ_OK )
    {
        msg_Err( p_access, "could not [re]locate file in zip: '%s'",
                 p_sys->psz_fileInzip );
        return VLC_EGENERIC;
    }
    if( unzOpenCurrentFile( file ) != UNZ_OK )
    {
        msg_Err( p_access, "could not [re]open file in zip: '%s'",
                 p_sys->psz_fileInzip );
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

/** **************************************************************************
 * \brief I/O functions for the ioapi: open (read only)
 *****************************************************************************/
static void* ZCALLBACK ZipIO_Open( void* opaque, const char* file, int mode )
{
    assert(opaque != NULL);
    assert(mode == (ZLIB_FILEFUNC_MODE_READ | ZLIB_FILEFUNC_MODE_EXISTING));

    access_t *p_access = (access_t*) opaque;

    char *fileUri = malloc( strlen(file) + 8 );
    if( unlikely( !fileUri ) )
        return NULL;
    if( !strstr( file, "://" ) )
    {
        strcpy( fileUri, "file://" );
        strcat( fileUri, file );
    }
    else
    {
        strcpy( fileUri, file );
    }

    stream_t *s = stream_UrlNew( p_access, fileUri );
    free( fileUri );
    return s;
}

/** **************************************************************************
 * \brief I/O functions for the ioapi: read
 *****************************************************************************/
static uLong ZCALLBACK ZipIO_Read( void* opaque, void* stream,
                                   void* buf, uLong size )
{
    (void)opaque;
    //access_t *p_access = (access_t*) opaque;
    //msg_Dbg(p_access, "read %d", size);
    return stream_Read( (stream_t*) stream, buf, size );
}

/** **************************************************************************
 * \brief I/O functions for the ioapi: write (assert insteadof segfault)
 *****************************************************************************/
static uLong ZCALLBACK ZipIO_Write( void* opaque, void* stream,
                                    const void* buf, uLong size )
{
    (void)opaque; (void)stream; (void)buf; (void)size;
    int zip_access_cannot_write_this_should_not_happen = 0;
    assert(zip_access_cannot_write_this_should_not_happen);
    return 0;
}

/** **************************************************************************
 * \brief I/O functions for the ioapi: tell
 *****************************************************************************/
static long ZCALLBACK ZipIO_Tell( void* opaque, void* stream )
{
    (void)opaque;
    int64_t i64_tell = stream_Tell( (stream_t*) stream );
    //access_t *p_access = (access_t*) opaque;
    //msg_Dbg(p_access, "tell %" PRIu64, i64_tell);
    return (long)i64_tell;
}

/** **************************************************************************
 * \brief I/O functions for the ioapi: seek
 *****************************************************************************/
static long ZCALLBACK ZipIO_Seek( void* opaque, void* stream,
                                  uLong offset, int origin )
{
    (void)opaque;
    int64_t pos = offset;
    switch( origin )
    {
        case SEEK_CUR:
            pos += stream_Tell( (stream_t*) stream );
            break;
        case SEEK_SET:
            break;
        case SEEK_END:
            pos += stream_Size( (stream_t*) stream );
            break;
        default:
            return -1;
    }
    if( pos < 0 )
        return -1;
    stream_Seek( (stream_t*) stream, pos );
    /* Note: in unzip.c, unzlocal_SearchCentralDir seeks to the end of
             the stream, which is doable but returns an error in VLC.
             That's why we always assume this was OK. FIXME */
    return 0;
}

/** **************************************************************************
 * \brief I/O functions for the ioapi: close
 *****************************************************************************/
static int ZCALLBACK ZipIO_Close( void* opaque, void* stream )
{
    (void)opaque;
    stream_Delete( (stream_t*) stream );
    return 0;
}

/** **************************************************************************
 * \brief I/O functions for the ioapi: test error (man 3 ferror)
 *****************************************************************************/
static int ZCALLBACK ZipIO_Error( void* opaque, void* stream )
{
    (void)opaque;
    (void)stream;
    //msg_Dbg( p_access, "error" );
    return 0;
}
