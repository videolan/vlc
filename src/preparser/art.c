/*****************************************************************************
 * art.c : Art metadata handling
 *****************************************************************************
 * Copyright (C) 1998-2008 VLC authors and VideoLAN
 *
 * Authors: Antoine Cellerier <dionoea@videolan.org>
 *          Clément Stenac <zorglub@videolan.org
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
# include "config.h"
#endif

#include <sys/stat.h>
#include <errno.h>

#include <vlc_common.h>
#include <vlc_configuration.h>
#include <vlc_input_item.h>
#include <vlc_fs.h>
#include <vlc_strings.h>
#include <vlc_url.h>
#include <vlc_hash.h>

#include "art.h"

static void ArtCacheCreateDir( const char *psz_dir )
{
    vlc_mkdir_parent(psz_dir, 0700);
}

static char* ArtCacheGetDirPath( const char *psz_arturl, const char *psz_artist,
                                 const char *psz_album,  const char *psz_date,
                                 const char *psz_title )
{
    char *psz_dir;
    char *psz_cachedir = config_GetUserDir(VLC_CACHE_DIR);

    if (unlikely(psz_cachedir == NULL))
        return NULL;

    if( !EMPTY_STR(psz_artist) && !EMPTY_STR(psz_album) )
    {
        char *psz_album_sanitized = strdup( psz_album );
        if (!psz_album_sanitized)
        {
            free( psz_cachedir );
            return NULL;
        }
        filename_sanitize( psz_album_sanitized );

        char *psz_artist_sanitized = strdup( psz_artist );
        if (!psz_artist_sanitized)
        {
            free( psz_cachedir );
            free( psz_album_sanitized );
            return NULL;
        }
        filename_sanitize( psz_artist_sanitized );

        char *psz_date_sanitized = !EMPTY_STR(psz_date) ? strdup( psz_date ) : NULL;
        if (psz_date_sanitized)
            filename_sanitize(psz_date_sanitized);

        if( asprintf( &psz_dir, "%s" DIR_SEP "art" DIR_SEP "artistalbum"
                      DIR_SEP "%s" DIR_SEP "%s" DIR_SEP "%s", psz_cachedir,
                      psz_artist_sanitized,
                      psz_date_sanitized ? psz_date_sanitized : "0000",
                      psz_album_sanitized ) == -1 )
            psz_dir = NULL;
        free( psz_album_sanitized );
        free( psz_artist_sanitized );
        free( psz_date_sanitized );
    }
    else
    {
        /* If artist or album are missing, cache by art download URL.
         * If the URL is an attachment://, add the title to the cache name.
         * It will be md5 hashed to form a valid cache filename.
         * We assume that psz_arturl is always the download URL and not the
         * already hashed filename.
         * (We should never need to call this function if art has already been
         * downloaded anyway).
         */
        char psz_arturl_sanitized[VLC_HASH_MD5_DIGEST_HEX_SIZE];

        vlc_hash_md5_t md5;
        vlc_hash_md5_Init( &md5 );
        vlc_hash_md5_Update( &md5, psz_arturl, strlen( psz_arturl ) );
        if( !strncmp( psz_arturl, "attachment://", 13 ) && psz_title != NULL )
            vlc_hash_md5_Update( &md5, psz_title, strlen( psz_title ) );
        vlc_hash_FinishHex( &md5, psz_arturl_sanitized );
        if( asprintf( &psz_dir, "%s" DIR_SEP "art" DIR_SEP "arturl" DIR_SEP
                      "%s", psz_cachedir, psz_arturl_sanitized ) == -1 )
            psz_dir = NULL;
    }
    free( psz_cachedir );
    return psz_dir;
}

static char *ArtCachePath( input_item_t *p_item )
{
    char* psz_path = NULL;
    const char *psz_artist;
    const char *psz_album;
    const char *psz_arturl;
    const char *psz_title;
    const char *psz_date;

    vlc_mutex_lock( &p_item->lock );

    if( !p_item->p_meta )
        p_item->p_meta = vlc_meta_New();
    if( !p_item->p_meta )
        goto end;

    psz_artist = vlc_meta_Get( p_item->p_meta, vlc_meta_Artist );
    psz_album = vlc_meta_Get( p_item->p_meta, vlc_meta_Album );
    psz_arturl = vlc_meta_Get( p_item->p_meta, vlc_meta_ArtworkURL );
    psz_title = vlc_meta_Get( p_item->p_meta, vlc_meta_Title );
    psz_date = vlc_meta_Get( p_item->p_meta, vlc_meta_Date );
    if( !psz_title )
        psz_title = p_item->psz_name;

    if( (EMPTY_STR(psz_artist) || EMPTY_STR(psz_album) ) && !psz_arturl )
        goto end;

    psz_path = ArtCacheGetDirPath( psz_arturl, psz_artist, psz_album, psz_date, psz_title );

end:
    vlc_mutex_unlock( &p_item->lock );
    return psz_path;
}

static char *ArtCacheName( input_item_t *p_item, const char *psz_type )
{
    char *psz_path = ArtCachePath( p_item );
    char *psz_ext = strdup( psz_type ? psz_type : "" );
    char *psz_filename = NULL;

    if( unlikely( !psz_path || !psz_ext ) )
        goto end;

    ArtCacheCreateDir( psz_path );
    filename_sanitize( psz_ext );

    if( asprintf( &psz_filename, "%s" DIR_SEP "art%s", psz_path, psz_ext ) < 0 )
        psz_filename = NULL;

end:
    free( psz_ext );
    free( psz_path );

    return psz_filename;
}

static char *ArtCacheNameFromDirPath( const char *psz_path, const char *psz_type )
{
    char *psz_ext = strdup( psz_type ? psz_type : "" );
    char *psz_filename = NULL;

    if( unlikely( !psz_path || !psz_ext ) )
    goto end;

    ArtCacheCreateDir( psz_path );
    filename_sanitize( psz_ext );

    if( asprintf( &psz_filename, "%s" DIR_SEP "art%s", psz_path, psz_ext ) < 0 )
        psz_filename = NULL;

end:
    free( psz_ext );

    return psz_filename;
}

static char *ArtCacheFilePath( const char *psz_dir, const char *psz_name )
{
    char *psz_file = NULL;

    if( !psz_dir || !psz_name )
        return NULL;

    if( asprintf( &psz_file, "%s" DIR_SEP "%s", psz_dir, psz_name ) == -1 )
        psz_file = NULL;

    return psz_file;
}

static char *ArtCacheReadUriFromFile( const char *psz_file )
{
    if( !psz_file )
        return NULL;

    FILE *fd = vlc_fopen( psz_file, "rb" );
    if( !fd )
        return NULL;

    char *psz_uri = NULL;
    char sz_cachefile[2049];

    if( fgets( sz_cachefile, sizeof( sz_cachefile ), fd ) != NULL )
    {
        size_t i_len = strlen( sz_cachefile );
        while( i_len > 0 &&
               ( sz_cachefile[i_len - 1] == '\n' ||
                 sz_cachefile[i_len - 1] == '\r' ) )
            sz_cachefile[--i_len] = '\0';

        if( i_len > 0 )
            psz_uri = strdup( sz_cachefile );
    }

    fclose( fd );
    return psz_uri;
}

static char *ArtCacheReadUriFromDirPath( const char *psz_dir )
{
    char *psz_uri = NULL;
    char *psz_file = ArtCacheFilePath( psz_dir, "arturl" );

    if( psz_file )
    {
        psz_uri = ArtCacheReadUriFromFile( psz_file );
        free( psz_file );
    }

    return psz_uri;
}

static int ArtCacheWriteUriToFile( vlc_object_t *obj, const char *psz_file,
                                   const char *psz_uri )
{
    if( !psz_file || !psz_uri )
        return VLC_EGENERIC;

    FILE *f = vlc_fopen( psz_file, "wb" );
    if( !f )
        return VLC_EGENERIC;

    int ret = VLC_SUCCESS;
    if( fputs( psz_uri, f ) < 0 )
    {
        msg_Err( obj, "Error writing %s: %s", psz_file, vlc_strerror_c(errno) );
        ret = VLC_EGENERIC;
    }

    fclose( f );
    return ret;
}

static char *GetFileByItemUID( char *psz_dir, const char *psz_type )
{
    return ArtCacheNameFromDirPath( psz_dir, psz_type );
}

static char * GetDirByItemUIDs( char *psz_uid )
{
    char *psz_cachedir = config_GetUserDir(VLC_CACHE_DIR);
    char *psz_dir;
    if (unlikely(psz_cachedir == NULL))
        return NULL;
    if( asprintf( &psz_dir, "%s" DIR_SEP
                  "by-iiuid" DIR_SEP
                  "%s",
                  psz_cachedir, psz_uid ) == -1 )
    {
        psz_dir = NULL;
    }
    free( psz_cachedir );
    return psz_dir;
}

int input_FindArtInCache( input_item_t *p_item )
{
    char *uid = input_item_GetInfo( p_item, "uid", "md5" );
    if ( ! *uid )
    {
        free( uid );
        return VLC_EGENERIC;
    }

    /* we have an input item uid set */
    bool b_done = false;
    char *psz_byuiddir = GetDirByItemUIDs( uid );
    char *psz_byuidfile = ArtCacheFilePath( psz_byuiddir, "arturl" );
    free( psz_byuiddir );
    if( psz_byuidfile )
    {
        char *psz_uri = ArtCacheReadUriFromFile( psz_byuidfile );
        if( psz_uri )
        {
            input_item_SetArtURL( p_item, psz_uri );
            free( psz_uri );
            b_done = true;
        }
        free( psz_byuidfile );
    }
    free( uid );
    if ( b_done ) return VLC_SUCCESS;

    return VLC_EGENERIC;
}

int input_FindArtInCacheUsingItemUID( input_item_t *p_item )
{
    return input_FindArtInCache( p_item );
}

/* */
int input_SaveArt( vlc_object_t *obj, input_item_t *p_item,
                   const void *data, size_t length, const char *psz_type )
{
    int i_ret = VLC_EGENERIC;
    char *psz_filename = ArtCacheName( p_item, psz_type );

    if( !psz_filename )
    {
        return VLC_EGENERIC;
    }

    char *psz_uri = vlc_path2uri( psz_filename, "file" );
    if( !psz_uri )
        {
            free( psz_filename );
            return VLC_EGENERIC;
        }

    /* Check if we already dumped it */
    struct stat s;
    if( !vlc_stat( psz_filename, &s ) )
    {
        input_item_SetArtURL( p_item, psz_uri );
        i_ret = VLC_SUCCESS;
        goto save_uid;
    }

    /* Dump it otherwise */
    FILE *f = vlc_fopen( psz_filename, "wb" );
    if( !f )
    {
        msg_Err( obj, "%s: %s", psz_filename, vlc_strerror_c(errno) );
        goto end;
    }

    if( fwrite( data, 1, length, f ) != length )
    {
        msg_Err( obj, "%s: %s", psz_filename, vlc_strerror_c(errno) );
        fclose( f );
        vlc_unlink( psz_filename );
        goto end;
    }

    if( fclose( f ) )
    {
        msg_Err( obj, "%s: %s", psz_filename, vlc_strerror_c(errno) );
        vlc_unlink( psz_filename );
        goto end;
    }

    msg_Dbg( obj, "album art saved to %s", psz_filename );
    input_item_SetArtURL( p_item, psz_uri );
    i_ret = VLC_SUCCESS;

save_uid:
    /* save uid info */
    if( i_ret != VLC_SUCCESS )
        goto end;

    char *uid = input_item_GetInfo( p_item, "uid", "md5" );
    if ( ! *uid )
    {
        free( uid );
        goto end;
    }

    char *psz_byuiddir = GetDirByItemUIDs( uid );
    char *psz_byuidfile = ArtCacheFilePath( psz_byuiddir, "arturl" );
    ArtCacheCreateDir( psz_byuiddir );
    free( psz_byuiddir );

    if ( psz_byuidfile )
    {
        ArtCacheWriteUriToFile( obj, psz_byuidfile, psz_uri );
        free( psz_byuidfile );
    }
    free( uid );
    /* !save uid info */
end:
    free( psz_uri );
    free( psz_filename );
return i_ret;
}
