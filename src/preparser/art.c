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
#include <vlc_input_item.h>
#include <vlc_fs.h>
#include <vlc_strings.h>
#include <vlc_url.h>
#include <vlc_hash.h>

#include "art.h"

static void ArtCacheCreateDir( const char *psz_dir )
{
    char newdir[strlen( psz_dir ) + 1];
    strcpy( newdir, psz_dir );
    char * psz_newdir = newdir;
    char * psz = psz_newdir;

    while( *psz )
    {
        while( *psz && *psz != DIR_SEP_CHAR) psz++;
        if( !*psz ) break;
        *psz = 0;
        if( !EMPTY_STR( psz_newdir ) )
            vlc_mkdir( psz_newdir, 0700 );
        *psz = DIR_SEP_CHAR;
        psz++;
    }
    vlc_mkdir( psz_dir, 0700 );
}

static char* ArtCacheGetDirPath( const char *psz_arturl, const char *psz_artist,
                                 const char *psz_album,  const char *psz_date,
                                 const char *psz_title )
{
    char *psz_dir;
    char *psz_cachedir = config_GetUserDir(VLC_CACHE_DIR);

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
        if( !strncmp( psz_arturl, "attachment://", 13 ) )
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

/* */
int input_FindArtInCache( input_item_t *p_item )
{
    char *psz_path = ArtCachePath( p_item );

    if( !psz_path )
        return VLC_EGENERIC;

    /* Check if file exists */
    DIR *p_dir = vlc_opendir( psz_path );
    if( !p_dir )
    {
        free( psz_path );
        return VLC_EGENERIC;
    }

    bool b_found = false;
    const char *psz_filename;
    while( !b_found && (psz_filename = vlc_readdir( p_dir )) )
    {
        if( !strncmp( psz_filename, "art", 3 ) )
        {
            char *psz_file;
            if( asprintf( &psz_file, "%s" DIR_SEP "%s",
                          psz_path, psz_filename ) != -1 )
            {
                char *psz_uri = vlc_path2uri( psz_file, "file" );
                if( psz_uri )
                {
                    input_item_SetArtURL( p_item, psz_uri );
                    free( psz_uri );
                }
                free( psz_file );
            }

            b_found = true;
        }
    }

    /* */
    closedir( p_dir );
    free( psz_path );
    return b_found ? VLC_SUCCESS : VLC_EGENERIC;
}

static char * GetDirByItemUIDs( char *psz_uid )
{
    char *psz_cachedir = config_GetUserDir(VLC_CACHE_DIR);
    char *psz_dir;
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

static char * GetFileByItemUID( char *psz_dir, const char *psz_type )
{
    char *psz_file;
    if( asprintf( &psz_file, "%s" DIR_SEP "%s", psz_dir, psz_type ) == -1 )
    {
        psz_file = NULL;
    }
    return psz_file;
}

int input_FindArtInCacheUsingItemUID( input_item_t *p_item )
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
    char *psz_byuidfile = GetFileByItemUID( psz_byuiddir, "arturl" );
    free( psz_byuiddir );
    if( psz_byuidfile )
    {
        FILE *fd = vlc_fopen( psz_byuidfile, "rb" );
        if ( fd )
        {
            char sz_cachefile[2049];
            /* read the cache hash url */
            if ( fgets( sz_cachefile, 2048, fd ) != NULL )
            {
                input_item_SetArtURL( p_item, sz_cachefile );
                b_done = true;
            }
            fclose( fd );
        }
        free( psz_byuidfile );
    }
    free( uid );
    if ( b_done ) return VLC_SUCCESS;

    return VLC_EGENERIC;
}

/* */
int input_SaveArt( vlc_object_t *obj, input_item_t *p_item,
                   const void *data, size_t length, const char *psz_type )
{
    char *psz_filename = ArtCacheName( p_item, psz_type );

    if( !psz_filename )
        return VLC_EGENERIC;

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
        free( psz_filename );
        free( psz_uri );
        return VLC_SUCCESS;
    }

    /* Dump it otherwise */
    FILE *f = vlc_fopen( psz_filename, "wb" );
    if( f )
    {
        if( fwrite( data, 1, length, f ) != length )
        {
            msg_Err( obj, "%s: %s", psz_filename, vlc_strerror_c(errno) );
        }
        else
        {
            msg_Dbg( obj, "album art saved to %s", psz_filename );
            input_item_SetArtURL( p_item, psz_uri );
        }
        fclose( f );
    }
    free( psz_uri );

    /* save uid info */
    char *uid = input_item_GetInfo( p_item, "uid", "md5" );
    if ( ! *uid )
    {
        free( uid );
        goto end;
    }

    char *psz_byuiddir = GetDirByItemUIDs( uid );
    char *psz_byuidfile = GetFileByItemUID( psz_byuiddir, "arturl" );
    ArtCacheCreateDir( psz_byuiddir );
    free( psz_byuiddir );

    if ( psz_byuidfile )
    {
        f = vlc_fopen( psz_byuidfile, "wb" );
        if ( f )
        {
            if( fputs( "file://", f ) < 0 || fputs( psz_filename, f ) < 0 )
                msg_Err( obj, "Error writing %s: %s", psz_byuidfile,
                         vlc_strerror_c(errno) );
            fclose( f );
        }
        free( psz_byuidfile );
    }
    free( uid );
    /* !save uid info */
end:
    free( psz_filename );
    return VLC_SUCCESS;
}

