/*****************************************************************************
 * zipstream.c: stream_filter that creates a XSPF playlist from a Zip archive
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

/** **************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "zip.h"
#include <stddef.h>

/* FIXME remove */
#include <vlc_input.h>

#define FILENAME_TEXT N_( "Media in Zip" )
#define FILENAME_LONGTEXT N_( "Path to the media in the Zip archive" )

/** **************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin()
    set_shortname( "Zip" )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_STREAM_FILTER )
    set_description( N_( "Zip files filter" ) )
    set_capability( "stream_filter", 1 )
    set_callbacks( StreamOpen, StreamClose )
    add_submodule()
        set_subcategory( SUBCAT_INPUT_ACCESS )
        set_description( N_( "Zip access" ) )
        set_capability( "access", 0 )
        add_shortcut( "unzip", "zip" )
        set_callbacks( AccessOpen, AccessClose )
vlc_module_end()

/** *************************************************************************
 * Local prototypes
 ****************************************************************************/
static int Read   ( stream_t *, void *p_read, unsigned int i_read );
static int Peek   ( stream_t *, const uint8_t **pp_peek, unsigned int i_peek );
static int Control( stream_t *, int i_query, va_list );

typedef struct node node;
typedef struct item item;

static int Fill( stream_t * );
static int CreatePlaylist( stream_t *s, char **pp_buffer );
static int GetFilesInZip( stream_t*, unzFile, vlc_array_t*, vlc_array_t* );
static node* findOrCreateParentNode( node *root, const char *fullpath );
static int WriteXSPF( char **pp_buffer, vlc_array_t *p_filenames,
                      const char *psz_zippath );
static int nodeToXSPF( char **pp_buffer, node *n, bool b_root );
static node* findOrCreateParentNode( node *root, const char *fullpath );

/** **************************************************************************
 * Struct definitions
 *****************************************************************************/
struct stream_sys_t
{
    /* zlib / unzip members */
    unzFile zipFile;
    zlib_filefunc_def *fileFunctions;
    char *psz_path;

    /* xspf data */
    char *psz_xspf;
    size_t i_len;
    size_t i_pos;
};

struct item {
    int id;
    item *next;
};

struct node {
    char *name;
    item *media;
    node *child;
    node *next;
};

/** **************************************************************************
 * Some helpers
 *****************************************************************************/

inline static node* new_node( char *name )
{
    node *n = (node*) calloc( 1, sizeof(node) );
    n->name = convert_xml_special_chars( name );
    return n;
}

inline static item* new_item( int id )
{
    item *media = (item*) calloc( 1, sizeof(item) );
    media->id = id;
    return media;
}

inline static void free_all_node( node *root )
{
    while( root )
    {
        free_all_node( root->child );
        free( root->name );
        node *tmp = root->next;
        free( root );
        root = tmp;
    }
}

/* Allocate strcat and format */
static int astrcatf( char **ppsz_dest, const char *psz_fmt_src, ... )
{
    char *psz_tmp;
    va_list args;

    va_start( args, psz_fmt_src );
    int i_ret = vasprintf( &psz_tmp, psz_fmt_src, args );
    va_end( args );

    if( i_ret == -1 ) return -1;

    int i_len = strlen( *ppsz_dest ) + strlen( psz_tmp ) + 1;
    char *psz_out = realloc( *ppsz_dest, i_len );
    if( !psz_out ) return -1;

    strcat( psz_out, psz_tmp );
    free( psz_tmp );

    *ppsz_dest = psz_out;
    return i_len;
}

/** **************************************************************************
 * Zip file identifier
 *****************************************************************************/
static const uint8_t p_zip_marker[] = { 0x50, 0x4b, 0x03, 0x04 }; // "PK^C^D"
static const int i_zip_marker = 4;


/** **************************************************************************
 * Open
 *****************************************************************************/
int StreamOpen( vlc_object_t *p_this )
{
    stream_t *s = (stream_t*) p_this;
    stream_sys_t *p_sys;

    /* Verify file format */
    const uint8_t *p_peek;
    if( stream_Peek( s->p_source, &p_peek, i_zip_marker ) < i_zip_marker )
        return VLC_EGENERIC;
    if( memcmp( p_peek, p_zip_marker, i_zip_marker ) )
        return VLC_EGENERIC;

    s->p_sys = p_sys = calloc( 1, sizeof( *p_sys ) );
    if( !p_sys )
        return VLC_ENOMEM;

    s->pf_read = Read;
    s->pf_peek = Peek;
    s->pf_control = Control;

    p_sys->fileFunctions = ( zlib_filefunc_def * )
            calloc( 1, sizeof( zlib_filefunc_def ) );
    if( !p_sys->fileFunctions )
    {
        free( p_sys );
        return VLC_ENOMEM;
    }
    p_sys->fileFunctions->zopen_file   = ZipIO_Open;
    p_sys->fileFunctions->zread_file   = ZipIO_Read;
    p_sys->fileFunctions->zwrite_file  = ZipIO_Write;
    p_sys->fileFunctions->ztell_file   = ZipIO_Tell;
    p_sys->fileFunctions->zseek_file   = ZipIO_Seek;
    p_sys->fileFunctions->zclose_file  = ZipIO_Close;
    p_sys->fileFunctions->zerror_file  = ZipIO_Error;
    p_sys->fileFunctions->opaque       = ( void * ) s;
    p_sys->zipFile = unzOpen2( NULL /* path */, p_sys->fileFunctions );
    if( !p_sys->zipFile )
    {
        msg_Warn( s, "unable to open file" );
        free( p_sys->fileFunctions );
        free( p_sys );
        return VLC_EGENERIC;
    }

    /* Find the stream uri */
    char *psz_tmp;
    if( asprintf( &psz_tmp, "%s.xspf", s->psz_path ) == -1 )
    {
        free( p_sys->fileFunctions );
        free( p_sys );
        return VLC_ENOMEM;
    }
    p_sys->psz_path = s->psz_path;
    s->psz_path = psz_tmp;

    return VLC_SUCCESS;
}

/** *************************************************************************
 * Close
 ****************************************************************************/
void StreamClose( vlc_object_t *p_this )
{
    stream_t *s = (stream_t*)p_this;
    stream_sys_t *p_sys = s->p_sys;

    if( p_sys->zipFile )
        unzClose( p_sys->zipFile );

    free( p_sys->fileFunctions );
    free( p_sys->psz_xspf );
    free( p_sys->psz_path );
    free( p_sys );
}

/** *************************************************************************
 * Stream filters functions
 ****************************************************************************/

/** *************************************************************************
 * Read
 ****************************************************************************/
static int Read( stream_t *s, void *p_read, unsigned int i_read )
{
    stream_sys_t *p_sys = s->p_sys;

    /* Fill the buffer */
    if( Fill( s ) )
        return -1;

    /* Read the buffer */
    unsigned i_len = __MIN( i_read, p_sys->i_len - p_sys->i_pos );
    if( p_read )
        memcpy( p_read, p_sys->psz_xspf + p_sys->i_pos, i_len );
    p_sys->i_pos += i_len;

    return i_len;
}

/** *************************************************************************
 * Peek
 ****************************************************************************/
static int Peek( stream_t *s, const uint8_t **pp_peek, unsigned int i_peek )
{
    stream_sys_t *p_sys = s->p_sys;

    /* Fill the buffer */
    if( Fill( s ) )
        return -1;

    /* Point to the buffer */
    int i_len = __MIN( i_peek, p_sys->i_len - p_sys->i_pos );
    *pp_peek = (uint8_t*) p_sys->psz_xspf + p_sys->i_pos;

    return i_len;
}

/** *************************************************************************
 * Control
 ****************************************************************************/
static int Control( stream_t *s, int i_query, va_list args )
{
    stream_sys_t *p_sys = s->p_sys;

    switch( i_query )
    {
        case STREAM_SET_POSITION:
        {
            uint64_t i_position = va_arg( args, uint64_t );
            if( i_position >= p_sys->i_len )
                return VLC_EGENERIC;
            else
            {
                p_sys->i_pos = (size_t) i_position;
                return VLC_SUCCESS;
            }
        }

        case STREAM_GET_POSITION:
        {
            uint64_t *pi_position = va_arg( args, uint64_t* );
            *pi_position = p_sys->i_pos;
            return VLC_SUCCESS;
        }

        case STREAM_GET_SIZE:
        {
            uint64_t *pi_size = va_arg( args, uint64_t* );
            *pi_size = p_sys->i_len;
            return VLC_SUCCESS;
        }

        case STREAM_GET_CONTENT_TYPE:
            return VLC_EGENERIC;

        case STREAM_UPDATE_SIZE:
        case STREAM_CONTROL_ACCESS:
        case STREAM_CAN_SEEK:
        case STREAM_CAN_FASTSEEK:
        case STREAM_SET_RECORD_STATE:
            return stream_vaControl( s->p_source, i_query, args );

        default:
            return VLC_EGENERIC;
    }
}

static int Fill( stream_t *s )
{
    stream_sys_t *p_sys = s->p_sys;

    if( p_sys->psz_xspf )
        return VLC_SUCCESS;

    if( CreatePlaylist( s, &p_sys->psz_xspf ) < 0 )
        return VLC_EGENERIC;

    p_sys->i_len = strlen( p_sys->psz_xspf );
    p_sys->i_pos = 0;
    return VLC_SUCCESS;
}

static int CreatePlaylist( stream_t *s, char **pp_buffer )
{
    stream_sys_t *p_sys = s->p_sys;

    unzFile file = p_sys->zipFile;
    if( !file )
        return -1;

    /* Get some infos about zip archive */
    int i_ret = 0;
    vlc_array_t *p_filenames = vlc_array_new(); /* Will contain char* */

    /* List all file names in Zip archive */
    i_ret = GetFilesInZip( s, file, p_filenames, NULL );
    if( i_ret < 0 )
    {
        i_ret = -1;
        goto exit;
    }

    /* Construct the xspf playlist */
    i_ret = WriteXSPF( pp_buffer, p_filenames, p_sys->psz_path );
    if( i_ret > 0 )
        i_ret = 1;
    else if( i_ret < 0 )
        i_ret = -1;

exit:
    /* Close archive */
    unzClose( file );
    p_sys->zipFile = NULL;

    for( int i = 0; i < vlc_array_count( p_filenames ); i++ )
    {
        free( vlc_array_item_at_index( p_filenames, i ) );
    }
    vlc_array_destroy( p_filenames );
    return i_ret;
}


/** **************************************************************************
 * Zip utility functions
 *****************************************************************************/

/** **************************************************************************
 * \brief List files in zip and append their names to p_array
 * \param p_this
 * \param file Opened zip file
 * \param p_array vlc_array_t which will receive all filenames
 *
 * In case of error, returns VLC_EGENERIC.
 * In case of success, returns number of files found, and goes back to first file.
 *****************************************************************************/
static int GetFilesInZip( stream_t *p_this, unzFile file,
                          vlc_array_t *p_filenames, vlc_array_t *p_fileinfos )
{
    if( !p_filenames || !p_this )
        return VLC_EGENERIC;

    int i_ret = 0;

    /* Get global info */
    unz_global_info info;

    if( unzGetGlobalInfo( file, &info ) != UNZ_OK )
    {
        msg_Warn( p_this, "this is not a valid zip archive" );
        return VLC_EGENERIC;
    }

    /* Go to first file in archive */
    unzGoToFirstFile( file );

    /* Get info about each file */
    for( unsigned long i = 0; i < info.number_entry; i++ )
    {
        char *psz_fileName = calloc( ZIP_FILENAME_LEN, 1 );
        unz_file_info *p_fileInfo = calloc( 1, sizeof( unz_file_info ) );

        if( !p_fileInfo || !psz_fileName )
        {
            free( psz_fileName );
            free( p_fileInfo );
            return VLC_ENOMEM;
        }

        if( unzGetCurrentFileInfo( file, p_fileInfo, psz_fileName,
                                   ZIP_FILENAME_LEN, NULL, 0, NULL, 0 )
            != UNZ_OK )
        {
            msg_Warn( p_this, "can't get info about file in zip" );
            return VLC_EGENERIC;
        }

        if( p_filenames )
            vlc_array_append( p_filenames, strdup( psz_fileName ) );
        free( psz_fileName );

        if( p_fileinfos )
            vlc_array_append( p_fileinfos, p_fileInfo );
        else
            free( p_fileInfo );

        if( i < ( info.number_entry - 1 ) )
        {
            /* Go the next file in the archive */
            if( unzGoToNextFile( file ) != UNZ_OK )
            {
                msg_Warn( p_this, "can't go to next file in zip" );
                return VLC_EGENERIC;
            }
        }

        i_ret++;
    }

    /* i_ret should be equal to info.number_entry */
    unzGoToFirstFile( file );
    return i_ret;
}


/** **************************************************************************
 * XSPF generation functions
 *****************************************************************************/

/** **************************************************************************
 * \brief Check a character for allowance in the Xml.
 * Allowed chars are: a-z, A-Z, 0-9, \, /, ., ' ', _ and :
 *****************************************************************************/
bool isAllowedChar( char c )
{
    return ( c >= 'a' && c <= 'z' )
           || ( c >= 'A' && c <= 'Z' )
           || ( c >= '0' && c <= '9' )
           || ( c == ':' ) || ( c == '/' )
           || ( c == '\\' ) || ( c == '.' )
           || ( c == ' ' ) || ( c == '_' );
}

/** **************************************************************************
 * \brief Escape string to be XML valid
 * Allowed chars are defined by the above function isAllowedChar()
 * Invalid chars are escaped using non standard '?XX' notation.
 * NOTE: We cannot trust VLC internal Web encoding functions
 *       because they are not able to encode and decode some rare utf-8
 *       characters properly. Also, we don't control exactly when they are
 *       called (from this module).
 *****************************************************************************/
static int escapeToXml( char **ppsz_encoded, const char *psz_url )
{
    char *psz_iter, *psz_tmp;

    /* Count number of unallowed characters in psz_url */
    size_t i_num = 0, i_len = 0;
    for( psz_iter = (char*) psz_url; *psz_iter; ++psz_iter )
    {
        if( isAllowedChar( *psz_iter ) )
        {
            i_len++;
        }
        else
        {
            i_len++;
            i_num++;
        }
    }

    /* Special case */
    if( i_num == 0 )
    {
        *ppsz_encoded = malloc( i_len + 1 );
        memcpy( *ppsz_encoded, psz_url, i_len + 1 );
        return VLC_SUCCESS;
    }

    /* Copy string, replacing invalid characters */
    char *psz_ret = malloc( i_len + 3*i_num + 2 );
    if( !psz_ret ) return VLC_ENOMEM;

    for( psz_iter = (char*) psz_url, psz_tmp = psz_ret;
         *psz_iter; ++psz_iter, ++psz_tmp )
    {
        if( isAllowedChar( *psz_iter ) )
        {
            *psz_tmp = *psz_iter;
        }
        else
        {
            *(psz_tmp++) = '?';
            snprintf( psz_tmp, 3, "%02x", ( *psz_iter & 0x000000FF ) );
            psz_tmp++;
        }
    }
    *psz_tmp = '\0';

    /* Return success */
    *ppsz_encoded = psz_ret;
    return VLC_SUCCESS;
}

/** **************************************************************************
 * \brief Write the XSPF playlist given the list of files
 *****************************************************************************/
static int WriteXSPF( char **pp_buffer, vlc_array_t *p_filenames,
                      const char *psz_zippath )
{
    char *psz_zip = strrchr( psz_zippath, DIR_SEP_CHAR );
    psz_zip = convert_xml_special_chars( psz_zip ? (psz_zip+1) : psz_zippath );

    if( asprintf( pp_buffer, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<playlist version=\"1\" xmlns=\"http://xspf.org/ns/0/\" "
                "xmlns:vlc=\"http://www.videolan.org/vlc/playlist/ns/0/\">\n"
                " <title>%s</title>\n"
                " <trackList>\n", psz_zip ) == -1)
        return -1;

    /* Root node */
    node *playlist = new_node( psz_zip );

    /* Encode the URI and append ZIP_SEP */
    char *psz_pathtozip;
    escapeToXml( &psz_pathtozip, psz_zippath );
    if( astrcatf( &psz_pathtozip, "%s", ZIP_SEP ) < 0 ) return -1;

    int i_track = 0;
    for( int i = 0; i < vlc_array_count( p_filenames ); ++i )
    {
        char *psz_name = (char*) vlc_array_item_at_index( p_filenames, i );
        int i_len = strlen( psz_name );

        if( !i_len ) continue;

        /* Is it a folder ? */
        if( psz_name[i_len-1] == '/' )
        {
            /* Do nothing */
        }
        else /* File */
        {
            /* Extract file name */
            char *psz_file = strrchr( psz_name, '/' );
            psz_file = convert_xml_special_chars( psz_file ?
                    (psz_file+1) : psz_name );

            /* Build full MRL */
            char *psz_path = strdup( psz_pathtozip );
            char *psz_escapedName;
            escapeToXml( &psz_escapedName, psz_name );
            if( astrcatf( &psz_path, "%s", psz_escapedName ) < 0 ) return -1;

            /* Track information */
            if( astrcatf( pp_buffer,
                        "  <track>\n"
                        "   <location>zip://%s</location>\n"
                        "   <title>%s</title>\n"
                        "   <extension application=\"http://www.videolan.org/vlc/playlist/0\">\n"
                        "    <vlc:id>%d</vlc:id>\n"
                        "   </extension>\n"
                        "  </track>\n",
                        psz_path, psz_file, i_track ) < 0 ) return -1;

            free( psz_file );
            free( psz_path );

            /* Find the parent node */
            node *parent = findOrCreateParentNode( playlist, psz_name );
            assert( parent );

            /* Add the item to this node */
            item *tmp = parent->media;
            if( !tmp )
            {
                parent->media = new_item( i_track );
            }
            else
            {
                while( tmp->next )
                {
                    tmp = tmp->next;
                }
                tmp->next = new_item( i_track );
            }

            ++i_track;
        }
    }

    free( psz_pathtozip );

    /* Close tracklist, open the extension */
    if( astrcatf( pp_buffer,
        " </trackList>\n"
        " <extension application=\"http://www.videolan.org/vlc/playlist/0\">\n"
                ) < 0 ) return -1;

    /* Write the tree */
    if( nodeToXSPF( pp_buffer, playlist, true ) < 0 ) return -1;

    /* Close extension and playlist */
    if( astrcatf( pp_buffer, " </extension>\n</playlist>\n" ) < 0 ) return -1;

    /* printf( "%s", *pp_buffer ); */

    free_all_node( playlist );

    return VLC_SUCCESS;
}

/** **************************************************************************
 * \brief Recursively convert a node to its XSPF representation
 *****************************************************************************/
static int nodeToXSPF( char **pp_buffer, node *n, bool b_root )
{
    if( !b_root )
    {
        if( astrcatf( pp_buffer, "  <vlc:node title=\"%s\">\n", n->name ) < 0 )
            return -1;
    }
    if( n->child )
        nodeToXSPF( pp_buffer, n->child, false );
    item *i = n->media;
    while( i )
    {
        if( astrcatf( pp_buffer, "   <vlc:item tid=\"%d\" />\n", i->id ) < 0 )
            return -1;
        i = i->next;
    }
    if( !b_root )
    {
        if( astrcatf( pp_buffer, "  </vlc:node>\n" ) < 0 )
            return -1;
    }
    return VLC_SUCCESS;
}

/** **************************************************************************
 * \brief Either create or find the parent node of the item
 *****************************************************************************/
static node* findOrCreateParentNode( node *root, const char *fullpath )
{
    char *folder;
    char *path = strdup( fullpath );
    folder = path;

    assert( root );

    char *sep = strchr( folder, '/' );
    if( !sep )
    {
        free( path );
        return root;
    }

    *sep = '\0';
    ++sep;

    node *current = root->child;

    while( current )
    {
        if( !strcmp( current->name, folder ) )
        {
            /* We found the folder, go recursively deeper */
            return findOrCreateParentNode( current, sep );
        }
        current = current->next;
    }

    /* If we are here, then it means that we did not find the parent */
    node *ret = new_node( folder );
    if( !root->child )
        root->child = ret;
    else
    {
        current = root->child;
        while( current->next )
        {
            current = current->next;
        }
        current->next = ret;
    }

    /* And now, create the subfolders */
    ret = findOrCreateParentNode( ret, sep );

    free( path );
    return ret;
}


/** **************************************************************************
 * ZipIO function definitions : how to use vlc_stream to read the zip
 *****************************************************************************/

/** **************************************************************************
 * \brief interface for unzip module to open a file using a vlc_stream
 * \param opaque
 * \param filename
 * \param mode how to open the file (read/write ?). We support only read
 * \return opaque
 *****************************************************************************/
static void ZCALLBACK *ZipIO_Open( void *opaque, const char *file, int mode )
{
    (void) file;
    stream_t *s = (stream_t*) opaque;
    if( mode & ( ZLIB_FILEFUNC_MODE_CREATE | ZLIB_FILEFUNC_MODE_WRITE ) )
    {
        msg_Dbg( s, "ZipIO_Open: we cannot write into zip files" );
        return NULL;
    }
    return s;
}

/** **************************************************************************
 * \brief read something from stream into buffer
 * \param opaque should be the stream
 * \param stream stream created by ZipIO_Open
 * \param buf buffer to read the file
 * \param size length of this buffer
 * \return return the number of bytes read (<= size)
 *****************************************************************************/
static unsigned long ZCALLBACK ZipIO_Read( void *opaque, void *stream,
                                           void *buf, unsigned long size )
{
    (void) stream;
    stream_t *s = (stream_t*) opaque;
    return (unsigned long) stream_Read( s->p_source, buf, (int) size );
}

/** **************************************************************************
 * \brief tell size of stream
 * \param opaque should be the stream
 * \param stream stream created by ZipIO_Open
 * \return size of the file / stream
 * ATTENTION: this is not stream_Tell, but stream_Size !
 *****************************************************************************/
static long ZCALLBACK ZipIO_Tell( void *opaque, void *stream )
{
    (void) stream;
    stream_t *s = (stream_t*) opaque;
    return (long) stream_Size( s->p_source ); /* /!\ not stream_Tell /!\ */
}

/** **************************************************************************
 * \brief seek in the stream
 * \param opaque should be the stream
 * \param stream stream created by ZipIO_Open
 * \param offset positive offset to seek
 * \param origin current position in stream
 * \return ¿ VLC_SUCCESS or an error code ?
 *****************************************************************************/
static long ZCALLBACK ZipIO_Seek ( void *opaque, void *stream,
                                   unsigned long offset, int origin )
{
    (void) stream;
    stream_t *s = (stream_t*) opaque;
    long l_ret;

    uint64_t pos = offset + origin;
    l_ret = (long) stream_Seek( s->p_source, pos );
    return l_ret;
}

/** **************************************************************************
 * \brief close the stream
 * \param opaque should be the stream
 * \param stream stream created by ZipIO_Open
 * \return always VLC_SUCCESS
 * This closes zip archive
 *****************************************************************************/
static int ZCALLBACK ZipIO_Close ( void *opaque, void *stream )
{
    (void) stream;
    (void) opaque;
//     stream_t *s = (stream_t*) opaque;
//    if( p_demux->p_sys && p_demux->p_sys->zipFile )
//        p_demux->p_sys->zipFile = NULL;
//     stream_Seek( s->p_source, 0 );
    return VLC_SUCCESS;
}

/** **************************************************************************
 * \brief I/O functions for the ioapi: write (assert insteadof segfault)
 *****************************************************************************/
static uLong ZCALLBACK ZipIO_Write( void* opaque, void* stream,
                                    const void* buf, uLong size )
{
    (void)opaque; (void)stream; (void)buf; (void)size;
    int ERROR_zip_cannot_write_this_should_not_happen = 0;
    assert( ERROR_zip_cannot_write_this_should_not_happen );
    return 0;
}

/** **************************************************************************
 * \brief I/O functions for the ioapi: test error (man 3 ferror)
 *****************************************************************************/
static int ZCALLBACK ZipIO_Error( void* opaque, void* stream )
{
    (void)opaque;
    (void)stream;
    return 0;
}

