/*****************************************************************************
 * archive.c: libarchive based stream filter
 *****************************************************************************
 * Copyright (C) 2016 VLC authors and VideoLAN
 *
 * Authors: Filip Roséen <filip@atch.se>
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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_stream.h>
#include <vlc_stream_extractor.h>
#include <vlc_dialog.h>
#include <vlc_input_item.h>

#include <assert.h>
#include <archive.h>
#include <archive_entry.h>

#if ARCHIVE_VERSION_NUMBER < 3002000
typedef __LA_INT64_T la_int64_t;
typedef __LA_SSIZE_T la_ssize_t;
#endif

static  int ExtractorOpen( vlc_object_t* );
static void ExtractorClose( vlc_object_t* );

static  int DirectoryOpen( vlc_object_t* );
static void DirectoryClose( vlc_object_t* );

vlc_module_begin()
    set_subcategory( SUBCAT_INPUT_STREAM_FILTER )
    set_capability( "stream_directory", 99 )
    set_description( N_( "libarchive based stream directory" ) )
    set_callbacks( DirectoryOpen, DirectoryClose )

    add_submodule()
        set_description( N_( "libarchive based stream extractor" ) )
        set_capability( "stream_extractor", 99 )
        set_callbacks( ExtractorOpen, ExtractorClose )

vlc_module_end()

typedef struct libarchive_callback_t libarchive_callback_t;
typedef struct private_sys_t private_sys_t;
typedef struct archive libarchive_t;

struct private_sys_t
{
    libarchive_t* p_archive;
    vlc_object_t* p_obj;
    stream_t* source;

    struct archive_entry* p_entry;
    bool b_dead;
    bool b_eof;

    uint64_t i_offset;

    uint8_t buffer[ 8192 ];
    bool b_seekable_source;
    bool b_seekable_archive;

    libarchive_callback_t** pp_callback_data;
    size_t i_callback_data;

    const uint8_t *last_arcbuf;
    size_t last_arcsize;
};

struct libarchive_callback_t {
    private_sys_t* p_sys;
    stream_t* p_source;
    char* psz_url;
};

/* ------------------------------------------------------------------------- */

static int libarchive_exit_cb( libarchive_t* p_arc, void* p_obj )
{
    VLC_UNUSED( p_arc );

    libarchive_callback_t* p_cb = (libarchive_callback_t*)p_obj;

    if( p_cb->p_sys->source == p_cb->p_source )
    {  /* DO NOT CLOSE OUR MOTHER STREAM */
        if( !p_cb->p_sys->b_dead && vlc_stream_Seek( p_cb->p_source, 0 ) )
            return ARCHIVE_FATAL;
    }
    else if( p_cb->p_source )
    {
        vlc_stream_Delete( p_cb->p_source );
        p_cb->p_source = NULL;
    }

    return ARCHIVE_OK;
}

static int libarchive_jump_cb( libarchive_t* p_arc, void* p_obj_current,
  void* p_obj_next )
{
    libarchive_callback_t* p_current = (libarchive_callback_t*)p_obj_current;
    libarchive_callback_t* p_next    = (libarchive_callback_t*)p_obj_next;

    if( libarchive_exit_cb( p_arc, p_current ) )
        return ARCHIVE_FATAL;

    if( p_next->p_source == NULL )
        p_next->p_source = vlc_stream_NewURL( p_next->p_sys->p_obj,
                                              p_next->psz_url );

    return p_next->p_source ? ARCHIVE_OK : ARCHIVE_FATAL;
}


static la_int64_t libarchive_skip_cb( libarchive_t* p_arc, void* p_obj,
                                      la_int64_t i_request )
{
    VLC_UNUSED( p_arc );

    libarchive_callback_t* p_cb = (libarchive_callback_t*)p_obj;

    stream_t*  p_source = p_cb->p_source;
    private_sys_t* p_sys = p_cb->p_sys;

    /* TODO: fix b_seekable_source on libarchive_callback_t */

    if( p_sys->b_seekable_source )
    {
        if( vlc_stream_Seek( p_source, vlc_stream_Tell( p_source ) + i_request ) )
            return ARCHIVE_FATAL;

        return i_request;
    }

    ssize_t i_read = vlc_stream_Read( p_source, NULL, i_request );
    return  i_read >= 0 ? i_read : ARCHIVE_FATAL;
}

static la_int64_t libarchive_seek_cb( libarchive_t* p_arc, void* p_obj,
  la_int64_t offset, int whence )
{
    VLC_UNUSED( p_arc );

    libarchive_callback_t* p_cb = (libarchive_callback_t*)p_obj;
    stream_t* p_source = p_cb->p_source;

    ssize_t whence_pos;

    switch( whence )
    {
        case SEEK_SET: whence_pos = 0;                           break;
        case SEEK_CUR: whence_pos = vlc_stream_Tell( p_source ); break;
        case SEEK_END: whence_pos = stream_Size( p_source ); break;
              default: vlc_assert_unreachable();

    }

    if( whence_pos < 0 || vlc_stream_Seek( p_source, whence_pos + offset ) )
        return ARCHIVE_FATAL;

    return vlc_stream_Tell( p_source );
}

static la_ssize_t libarchive_read_cb( libarchive_t* p_arc, void* p_obj,
  const void** pp_dst )
{
    VLC_UNUSED( p_arc );

    libarchive_callback_t* p_cb = (libarchive_callback_t*)p_obj;

    stream_t*  p_source = p_cb->p_source;
    private_sys_t* p_sys = p_cb->p_sys;

    ssize_t i_ret = vlc_stream_Read( p_source, p_sys->buffer,
                                     sizeof( p_sys->buffer ) );

    if( i_ret < 0 )
    {
        archive_set_error( p_sys->p_archive, ARCHIVE_FATAL,
          "libarchive_read_cb failed = %zd", i_ret );

        return ARCHIVE_FATAL;
    }

    *pp_dst = &p_sys->buffer;
    return i_ret;
}

/* ------------------------------------------------------------------------- */

static int archive_push_resource( private_sys_t* p_sys,
  stream_t* p_source, char const* psz_url )
{
    libarchive_callback_t** pp_callback_data;
    libarchive_callback_t*   p_callback_data;

    /* INCREASE BUFFER SIZE */

    pp_callback_data = realloc( p_sys->pp_callback_data,
      sizeof( *p_sys->pp_callback_data ) * ( p_sys->i_callback_data + 1 ) );

    if( unlikely( !pp_callback_data ) )
        goto error;

    /* CREATE NEW NODE */

    p_callback_data = malloc( sizeof( *p_callback_data ) );

    if( unlikely( !p_callback_data ) )
        goto error;

    /* INITIALIZE AND APPEND */

    p_callback_data->psz_url  = psz_url ? strdup( psz_url ) : NULL;
    p_callback_data->p_source = p_source;
    p_callback_data->p_sys    = p_sys;

    if( unlikely( !p_callback_data->psz_url && psz_url ) )
    {
        free( p_callback_data );
        goto error;
    }

    pp_callback_data[ p_sys->i_callback_data++ ] = p_callback_data;
    p_sys->pp_callback_data = pp_callback_data;

    return VLC_SUCCESS;

error:
    free( pp_callback_data );
    return VLC_ENOMEM;
}

static int archive_init( private_sys_t* p_sys, stream_t* source )
{
    /* CREATE ARCHIVE HANDLE */

    p_sys->p_archive = archive_read_new();

    if( unlikely( !p_sys->p_archive ) )
    {
        msg_Dbg( p_sys->p_obj, "unable to create libarchive handle" );
        return VLC_EGENERIC;
    }

    /* SETUP SEEKING */

    p_sys->b_seekable_archive = false;

    if( vlc_stream_Control( source, STREAM_CAN_SEEK,
        &p_sys->b_seekable_source ) )
    {
        msg_Warn( p_sys->p_obj, "unable to query whether source stream can seek" );
        p_sys->b_seekable_source = false;
    }

    if( p_sys->b_seekable_source )
    {
        if( archive_read_set_seek_callback( p_sys->p_archive,
            libarchive_seek_cb ) )
        {
            msg_Err( p_sys->p_obj, "archive_read_set_callback failed, aborting." );
            return VLC_EGENERIC;
        }
    }

    /* ENABLE ALL FORMATS/FILTERS */

    archive_read_support_filter_all( p_sys->p_archive );
    archive_read_support_format_all( p_sys->p_archive );

    /* REGISTER CALLBACK DATA */

    if( archive_read_set_switch_callback( p_sys->p_archive,
        libarchive_jump_cb ) )
    {
        msg_Err( p_sys->p_obj, "archive_read_set_switch_callback failed, aborting." );
        return VLC_EGENERIC;
    }

    for( size_t i = 0; i < p_sys->i_callback_data; ++i )
    {
        if( archive_read_append_callback_data( p_sys->p_archive,
            p_sys->pp_callback_data[i] ) )
        {
            return VLC_EGENERIC;
        }
    }

    /* OPEN THE ARCHIVE */

    if( archive_read_open2( p_sys->p_archive, p_sys->pp_callback_data[0], NULL,
        libarchive_read_cb, libarchive_skip_cb, libarchive_exit_cb ) )
    {
        msg_Dbg( p_sys->p_obj, "libarchive: %s",
          archive_error_string( p_sys->p_archive ) );

        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

static int archive_clean( private_sys_t* p_sys )
{
    libarchive_t* p_arc = p_sys->p_archive;

    if( p_sys->p_entry )
        archive_entry_free( p_sys->p_entry );

    if( p_arc )
        archive_read_free( p_arc );

    p_sys->p_entry   = NULL;
    p_sys->p_archive = NULL;

    return VLC_SUCCESS;
}

static int archive_seek_subentry( private_sys_t* p_sys, char const* psz_subentry )
{
    libarchive_t* p_arc = p_sys->p_archive;

    struct archive_entry* entry;
    int archive_status;

    while( !( archive_status = archive_read_next_header( p_arc, &entry ) ) )
    {
        char const* entry_path = archive_entry_pathname( entry );

        if( strcmp( entry_path, psz_subentry ) == 0 )
        {
            p_sys->p_entry = archive_entry_clone( entry );

            if( unlikely( !p_sys->p_entry ) )
                return VLC_ENOMEM;

            break;
        }

        archive_read_data_skip( p_arc );
    }

    switch( archive_status )
    {
        case ARCHIVE_WARN:
            msg_Warn( p_sys->p_obj,
              "libarchive: %s", archive_error_string( p_arc ) );
            /* fall through */
        case ARCHIVE_EOF:
        case ARCHIVE_FATAL:
        case ARCHIVE_RETRY:
            archive_set_error( p_arc, ARCHIVE_FATAL,
                "archive does not contain >>> %s <<<", psz_subentry );

            return VLC_EGENERIC;
    }

    /* check if seeking is supported */

    if( p_sys->b_seekable_source )
    {
        if( archive_seek_data( p_sys->p_archive, 0, SEEK_CUR ) >= 0 )
            p_sys->b_seekable_archive = true;
    }

    return VLC_SUCCESS;
}

static int archive_extractor_reset( stream_extractor_t* p_extractor )
{
    private_sys_t* p_sys = p_extractor->p_sys;

    if( vlc_stream_Seek( p_extractor->source, 0 )
        || archive_clean( p_sys )
        || archive_init( p_sys, p_extractor->source )
        || archive_seek_subentry( p_sys, p_extractor->identifier ) )
    {
        p_sys->b_dead = true;
        return VLC_EGENERIC;
    }

    p_sys->i_offset = 0;
    p_sys->b_eof = false;
    p_sys->b_dead = false;
    p_sys->last_arcbuf = NULL;
    p_sys->last_arcsize = 0;
    return VLC_SUCCESS;
}

/* ------------------------------------------------------------------------- */

static private_sys_t* setup( vlc_object_t* obj, stream_t* source,
                             char **volumes, size_t volumes_count )
{
    private_sys_t* p_sys  = calloc( 1, sizeof( *p_sys ) );

    if( unlikely( !p_sys ) )
        goto error;

    if( archive_push_resource( p_sys, source, NULL ) )
        goto error;


    for( size_t i=0; i<volumes_count; i++ )
    {
        if( archive_push_resource( p_sys, NULL, volumes[i] ) )
            goto error;
    }

    p_sys->source = source;
    p_sys->p_obj = obj;
    p_sys->last_arcbuf = NULL;
    p_sys->last_arcsize = 0;

    return p_sys;

error:
    free( p_sys );
    return NULL;
}

static int probe( stream_t* source )
{
    struct
    {
        uint16_t i_offset;
        uint8_t  i_length;
        char const * p_bytes;
    } const magicbytes[] = {
        /* keep heaviest at top */
        { 257, 5, "ustar" },              //TAR
#if ARCHIVE_VERSION_NUMBER >= 3004000
        { 0,   8, "Rar!\x1A\x07\x01" },   //RAR5.0
#endif
        { 0,   7, "Rar!\x1A\x07" },       //RAR4.x
        { 0,   6, "7z\xBC\xAF\x27\x1C" }, //7z
        { 0,   4, "xar!" },               //XAR
        { 0,   4, "PK\x03\x04" },         //ZIP
        { 0,   4, "PK\x05\x06" },         //ZIP
        { 0,   4, "PK\x07\x08" },         //ZIP
        { 2,   3, "-lh" },                //LHA/LHZ
        { 0,   3, "\x1f\x8b\x08" },       // Gzip
        { 0,   3, "PAX" },                //PAX
        { 0,   6, "070707" },             //CPIO
        { 0,   6, "070701" },             //CPIO
        { 0,   6, "070702" },             //CPIO
        { 0,   4, "MSCH" },               //CAB
    };

    const uint8_t *p_peek;

    int i_peek = vlc_stream_Peek( source, &p_peek,
      magicbytes[0].i_offset + magicbytes[0].i_length);

    for(unsigned i=0; i < ARRAY_SIZE( magicbytes ); i++)
    {
        if (i_peek < magicbytes[i].i_offset + magicbytes[i].i_length)
            continue;

        if ( !memcmp(p_peek + magicbytes[i].i_offset,
            magicbytes[i].p_bytes, magicbytes[i].i_length) )
            return VLC_SUCCESS;
    }

    return VLC_EGENERIC;
}

/* ------------------------------------------------------------------------- */

static int Control( stream_extractor_t* p_extractor, int i_query, va_list args )
{
    private_sys_t* p_sys = p_extractor->p_sys;

    switch( i_query )
    {
        case STREAM_CAN_FASTSEEK:
            *va_arg( args, bool* ) = false;
            break;

        case STREAM_CAN_SEEK:
            *va_arg( args, bool* ) = p_sys->b_seekable_source;
            break;

        case STREAM_GET_SIZE:
            if( p_sys->p_entry == NULL )
                return VLC_EGENERIC;

            if( !archive_entry_size_is_set( p_sys->p_entry ) )
                return VLC_EGENERIC;

            *va_arg( args, uint64_t* ) = archive_entry_size( p_sys->p_entry );
            break;

        case STREAM_GET_MTIME:
            if( p_sys->p_entry == NULL )
                return VLC_EGENERIC;

            if( !archive_entry_mtime_is_set( p_sys->p_entry ) )
                return VLC_EGENERIC;

            *va_arg( args, uint64_t* ) = archive_entry_mtime( p_sys->p_entry );
            break;

        default:
            return vlc_stream_vaControl( p_extractor->source, i_query, args );
    }

    return VLC_SUCCESS;
}

static int ReadDir( stream_directory_t* p_directory, input_item_node_t* p_node )
{
    private_sys_t* p_sys = p_directory->p_sys;
    libarchive_t* p_arc = p_sys->p_archive;

    struct vlc_readdir_helper rdh;
    vlc_readdir_helper_init( &rdh, p_directory, p_node);
    struct archive_entry* entry;
    int archive_status;
    unsigned item_count = 0;

    while( !( archive_status = archive_read_next_header( p_arc, &entry ) ) )
    {
        if( archive_entry_filetype( entry ) == AE_IFDIR )
            continue;

        char const* path = archive_entry_pathname( entry );

        if( unlikely( !path ) )
            break;

        char* mrl = vlc_stream_extractor_CreateMRL( p_directory, path,
                                                   (char const**) p_directory->volumes,
                                                   p_directory->volumes_count );

        if( unlikely( !mrl ) )
            break;

        input_item_t *p_item;
        if( vlc_readdir_helper_additem( &rdh, mrl, path, NULL, ITEM_TYPE_FILE,
                                        ITEM_LOCAL, &p_item ) )
        {
            free( mrl );
            break;
        }
        if ( p_item )
        {
            time_t mtime = archive_entry_mtime( entry );
            if( mtime >= 0 )
                input_item_AddStat( p_item, "mtime", mtime );

            int64_t size = archive_entry_size( entry );
            if( size >= 0 )
                input_item_AddStat( p_item, "size", size );
            item_count++;
        }
        free( mrl );

        if( archive_read_data_skip( p_arc ) )
            break;
    }

    bool success = item_count > 0 || archive_status == ARCHIVE_EOF;

    vlc_readdir_helper_finish( &rdh, success );
    return success ? VLC_SUCCESS : VLC_EGENERIC;
}

static ssize_t Read( stream_extractor_t *p_extractor, void* p_data, size_t i_size )
{
    private_sys_t* p_sys = p_extractor->p_sys;
    libarchive_t* p_arc = p_sys->p_archive;
    ssize_t       i_ret;

    if( p_sys->b_dead || p_sys->p_entry == NULL )
        return 0;

    if( p_sys->b_eof )
        return 0;

    const void *arcbuf = NULL;
    size_t arcsize = 0;
    la_int64_t arcoffset = 0;
    if( p_sys->last_arcbuf == NULL )
    {
        i_ret = archive_read_data_block( p_arc, &arcbuf, &arcsize, &arcoffset);
        assert(arcoffset == (la_int64_t) p_sys->i_offset); (void)arcoffset;
    }
    else
    {
        i_ret = ARCHIVE_OK;
        arcbuf = p_sys->last_arcbuf;
        arcsize = p_sys->last_arcsize;
    }

    switch( i_ret )
    {
        case ARCHIVE_OK:
        case ARCHIVE_EOF:
            if( i_size >= arcsize )
            {
                i_size = arcsize;
                p_sys->last_arcbuf = NULL;
                p_sys->last_arcsize = 0;
            }
            else
            {
                p_sys->last_arcbuf = (uint8_t *)arcbuf + i_size;
                p_sys->last_arcsize = arcsize - i_size;
            }
            if( p_data != NULL)
                memcpy( p_data, arcbuf, i_size );
            break;
        case ARCHIVE_RETRY:
        case ARCHIVE_FAILED:
            msg_Dbg( p_extractor, "libarchive: %s", archive_error_string( p_arc ) );
            goto eof;

        case ARCHIVE_WARN:
            msg_Warn( p_extractor, "libarchive: %s", archive_error_string( p_arc ) );
            goto eof;

        case ARCHIVE_FATAL:
            msg_Err( p_extractor, "libarchive: %s", archive_error_string( p_arc ) );
            goto fatal_error;
    }

    p_sys->i_offset += i_size;
    return i_size;

fatal_error:
    p_sys->b_dead = true;

eof:
    p_sys->b_eof = true;
    return 0;
}

static int archive_skip_decompressed( stream_extractor_t* p_extractor, uint64_t *pi_skip )
{
    while( *pi_skip )
    {
        ssize_t i_read = Read( p_extractor, NULL, *pi_skip );

        if( i_read < 1 )
            return VLC_EGENERIC;

        *pi_skip -= i_read;
    }

    return VLC_SUCCESS;
}

static int Seek( stream_extractor_t* p_extractor, uint64_t i_req )
{
    private_sys_t* p_sys = p_extractor->p_sys;

    if( !p_sys->p_entry || !p_sys->b_seekable_source )
        return VLC_EGENERIC;

    if( archive_entry_size_is_set( p_sys->p_entry ) &&
        (uint64_t)archive_entry_size( p_sys->p_entry ) <= i_req )
    {
        p_sys->b_eof = true;
        return VLC_SUCCESS;
    }

    p_sys->b_eof = false;
    int ret = VLC_SUCCESS;

    if( !p_sys->b_seekable_archive || p_sys->b_dead
      || archive_seek_data( p_sys->p_archive, i_req, SEEK_SET ) < 0 )
    {
        msg_Dbg( p_extractor,
            "intrinsic seek failed: '%s' (falling back to dumb seek)",
            archive_error_string( p_sys->p_archive ) );

        uint64_t i_skip = i_req - p_sys->i_offset;


        /* The 1st try is the original skip.
         * The 2nd try is to go back to the previous position in case of
         * failure. */
        uint64_t preskip_offset = p_sys->i_offset;
        for (unsigned i = 0; i < 2; ++i)
        {
            if( i_req < p_sys->i_offset )
            {
                /* RECREATE LIBARCHIVE HANDLE IF WE ARE SEEKING BACKWARDS */
                if( archive_extractor_reset( p_extractor ) )
                {
                    msg_Err( p_extractor, "unable to reset libarchive handle" );
                    return VLC_EGENERIC;
                }

                i_skip = i_req;
            }

            if( archive_skip_decompressed( p_extractor, &i_skip ) == 0 )
                break; /* Success */
            msg_Warn( p_extractor, "failed to skip to seek position %"
                      PRIu64 "/%" PRId64, i_req,
                      archive_entry_size( p_sys->p_entry ) );
            ret = VLC_EGENERIC;

            /* Seek back to the original offset before failure */
            i_req = preskip_offset;
            if( i_req == p_sys->i_offset )
                break; /* no data was skipped, no need for a 2nd try */
            assert( i_req < p_sys->i_offset );
        }
    }
    else
    {
        p_sys->last_arcbuf = NULL;
        p_sys->last_arcsize = 0;
    }

    p_sys->i_offset = i_req;
    return ret;
}


static void CommonClose( private_sys_t* p_sys )
{
    p_sys->b_dead = true;
    archive_clean( p_sys );

    for( size_t i = 0; i < p_sys->i_callback_data; ++i )
    {
        free( p_sys->pp_callback_data[i]->psz_url );
        free( p_sys->pp_callback_data[i] );
    }

    free( p_sys->pp_callback_data );
    free( p_sys );
}

static void DirectoryClose( vlc_object_t* p_obj )
{
    stream_directory_t* p_directory = (void*)p_obj;
    return CommonClose( p_directory->p_sys );
}

static void ExtractorClose( vlc_object_t* p_obj )
{
    stream_extractor_t* p_extractor = (void*)p_obj;
    return CommonClose( p_extractor->p_sys );
}

static private_sys_t* CommonOpen( vlc_object_t* p_obj, stream_t* source,
                                  char **volumes, size_t volumes_count  )
{
    if( probe( source ) )
        return NULL;

    private_sys_t* p_sys = setup( p_obj, source, volumes, volumes_count );

    if( p_sys == NULL )
        return NULL;

    if( archive_init( p_sys, source ) )
    {
        CommonClose( p_sys );
        return NULL;
    }

    return p_sys;
}

static int DirectoryOpen( vlc_object_t* p_obj )
{
    stream_directory_t* p_directory = (void*)p_obj;
    private_sys_t* p_sys = CommonOpen( p_obj, p_directory->source,
                                       p_directory->volumes, p_directory->volumes_count );

    if( p_sys == NULL )
        return VLC_EGENERIC;

    p_directory->p_sys = p_sys;
    p_directory->pf_readdir = ReadDir;

    return VLC_SUCCESS;
}

static int ExtractorOpen( vlc_object_t* p_obj )
{
    stream_extractor_t* p_extractor = (void*)p_obj;
    private_sys_t* p_sys = CommonOpen( p_obj, p_extractor->source,
                                       p_extractor->volumes, p_extractor->volumes_count );

    if( p_sys == NULL )
        return VLC_EGENERIC;

    if( archive_seek_subentry( p_sys, p_extractor->identifier ) )
    {
        CommonClose( p_sys );
        return VLC_EGENERIC;
    }

    p_extractor->p_sys = p_sys;
    p_extractor->pf_read = Read;
    p_extractor->pf_control = Control;
    p_extractor->pf_seek = Seek;

    return VLC_SUCCESS;
}
