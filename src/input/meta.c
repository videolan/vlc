/*****************************************************************************
 * meta.c : Metadata handling
 *****************************************************************************
 * Copyright (C) 1998-2004 the VideoLAN team
 * $Id$
 *
 * Authors: Antoine Cellerier <dionoea@videolan.org>
 *          Clément Stenac <zorglub@videolan.org
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

#include <vlc/vlc.h>
#include <vlc_input.h>
#include <vlc_stream.h>
#include <vlc_meta.h>
#include <vlc_playlist.h>
#include <vlc_charset.h>
#include "../playlist/playlist_internal.h"
#include <errno.h>

#ifdef HAVE_SYS_STAT_H
#   include <sys/stat.h>
#endif

// FIXME be sure to not touch p_meta without lock on p_item

static const char * meta_type_to_string[VLC_META_TYPE_COUNT] =
{
    [vlc_meta_Title]            = N_("Title"),
    [vlc_meta_Artist]           = N_("Artist"),
    [vlc_meta_Genre]            = N_("Genre"),
    [vlc_meta_Copyright]        = N_("Copyright"),
    [vlc_meta_Album]            = N_("Album/movie/show title"),
    [vlc_meta_TrackNumber]      = N_("Track number/position in set"),
    [vlc_meta_Description]      = N_("Description"),
    [vlc_meta_Rating]           = N_("Rating"),
    [vlc_meta_Date]             = N_("Date"),
    [vlc_meta_Setting]          = N_("Setting"),
    [vlc_meta_URL]              = N_("URL"),
    [vlc_meta_Language]         = N_("Language"),
    [vlc_meta_NowPlaying]       = N_("Language"),
    [vlc_meta_Publisher]        = N_("Publisher"),
    [vlc_meta_EncodedBy]        = N_("Encoded by"),
    [vlc_meta_ArtworkURL]       = N_("Artwork URL"),
    [vlc_meta_TrackID]          = N_("Track ID"),
};

const char *
input_MetaTypeToLocalizedString( vlc_meta_type_t meta_type )
{
    return _(meta_type_to_string[meta_type]);
}

#define input_FindArtInCache(a,b) __input_FindArtInCache(VLC_OBJECT(a),b)
static int __input_FindArtInCache( vlc_object_t *, input_item_t *p_item );

vlc_bool_t input_MetaSatisfied( playlist_t *p_playlist, input_item_t *p_item,
                                uint32_t *pi_mandatory, uint32_t *pi_optional )
{
    (void)p_playlist;
    *pi_mandatory = VLC_META_ENGINE_TITLE | VLC_META_ENGINE_ARTIST;

    uint32_t i_meta = input_CurrentMetaFlags( p_item->p_meta );
    *pi_mandatory &= ~i_meta;
    *pi_optional = 0; /// Todo
    return *pi_mandatory ? VLC_FALSE:VLC_TRUE;
}

int input_MetaFetch( playlist_t *p_playlist, input_item_t *p_item )
{
    struct meta_engine_t *p_me;
    uint32_t i_mandatory, i_optional;

    input_MetaSatisfied( p_playlist, p_item, &i_mandatory, &i_optional );
    // Meta shouldn't magically appear
    assert( i_mandatory );

    /* FIXME: object creation is overkill, use p_private */
    p_me = vlc_object_create( p_playlist, VLC_OBJECT_META_ENGINE );
    p_me->i_flags |= OBJECT_FLAGS_NOINTERACT;
    p_me->i_flags |= OBJECT_FLAGS_QUIET;
    p_me->i_mandatory = i_mandatory;
    p_me->i_optional = i_optional;

    p_me->p_item = p_item;
    p_me->p_module = module_Need( p_me, "meta fetcher", 0, VLC_FALSE );
    if( !p_me->p_module )
    {
        vlc_object_destroy( p_me );
        return VLC_EGENERIC;
    }
    module_Unneed( p_me, p_me->p_module );
    vlc_object_destroy( p_me );

    input_item_SetMetaFetched( p_item, VLC_TRUE );

    return VLC_SUCCESS;
}

/* Return codes:
 *   0 : Art is in cache
 *   1 : Art found, need to download
 *  -X : Error/not found
 */
int input_ArtFind( playlist_t *p_playlist, input_item_t *p_item )
{
    int i_ret = VLC_EGENERIC;
    module_t *p_module;
    char * psz_album = input_item_GetAlbum( p_item );
    char * psz_artist = input_item_GetArtist( p_item );
    char * psz_title = input_item_GetAlbum( p_item );

    if( !p_item->p_meta )
        return VLC_EGENERIC;

    if(  !p_item->psz_name && !input_item_GetTitle( p_item ) &&
        (!input_item_GetArtist( p_item ) || !input_item_GetAlbum( p_item )) )
        return VLC_EGENERIC;

    /* If we already checked this album in this session, skip */
    if( input_item_GetArtist( p_item ) && input_item_GetAlbum( p_item ) )
    {
        FOREACH_ARRAY( playlist_album_t album, p_playlist->p_fetcher->albums )
            if( !strcmp( album.psz_artist, input_item_GetArtist( p_item ) ) &&
                !strcmp( album.psz_album, input_item_GetAlbum( p_item ) ) )
            {
                msg_Dbg( p_playlist, " %s - %s has already been searched",
                         input_item_GetArtist( p_item ),  input_item_GetAlbum( p_item ) );
        /* TODO-fenrir if we cache art filename too, we can go faster */
                if( album.b_found )
                {
                    /* Actually get URL from cache */
                    input_FindArtInCache( p_playlist, p_item );
                    return 0;
                }
                else
                {
                    return VLC_EGENERIC;
                }
            }
        FOREACH_END();
    }

    input_FindArtInCache( p_playlist, p_item );
    if( !EMPTY_STR(input_item_GetArtURL( p_item )) )
        return 0;

    PL_LOCK;
    p_playlist->p_private = p_item;
    if( input_item_GetAlbum( p_item ) && input_item_GetArtist( p_item ) )
    {
        msg_Dbg( p_playlist, "searching art for %s - %s",
             input_item_GetArtist( p_item ),  input_item_GetAlbum( p_item ) );
    }
    else
    {
        msg_Dbg( p_playlist, "searching art for %s",
             input_item_GetTitle( p_item ) ? input_item_GetTitle( p_item ) : p_item->psz_name );
    }

    p_module = module_Need( p_playlist, "art finder", 0, VLC_FALSE );

    if( p_module )
        i_ret = 1;
    else
        msg_Dbg( p_playlist, "unable to find art" );

    /* Record this album */
    if( input_item_GetArtist( p_item ) && input_item_GetAlbum( p_item ) )
    {
        playlist_album_t a;
        a.psz_artist = strdup( input_item_GetArtist( p_item ) );
        a.psz_album = strdup( input_item_GetAlbum( p_item ) );
        a.b_found = (i_ret == VLC_EGENERIC ? VLC_FALSE : VLC_TRUE );
        ARRAY_APPEND( p_playlist->p_fetcher->albums, a );
    }

    if( p_module )
        module_Unneed( p_playlist, p_module );
    p_playlist->p_private = NULL;
    PL_UNLOCK;

    return i_ret;
}

#ifndef MAX_PATH
#   define MAX_PATH 250
#endif
#define ArtCacheCreateName(a,b,c,d,e,f) __ArtCacheCreateName(VLC_OBJECT(a),b,c,d,e,f)
static void __ArtCacheCreateName( vlc_object_t *p_obj,
                                  char psz_filename[MAX_PATH+1],
                                  const char *psz_title,
                                  const char *psz_artist, const char *psz_album,
                                  const char *psz_extension )
{
    if( psz_artist && psz_artist )
    {
        snprintf( psz_filename, MAX_PATH,
              "file://%s" DIR_SEP CONFIG_DIR DIR_SEP "art" DIR_SEP "artistalbum"
              DIR_SEP "%s" DIR_SEP "%s" DIR_SEP "art%s",
              p_obj->p_libvlc->psz_homedir,
              psz_artist, psz_album, psz_extension ? psz_extension : "" );
    }
    else
    {
        /* We will use the psz_title name to store the art */
        snprintf( psz_filename, MAX_PATH,
              "file://%s" DIR_SEP CONFIG_DIR DIR_SEP "art" DIR_SEP "title"
              DIR_SEP "%s" DIR_SEP "art%s",
              p_obj->p_libvlc->psz_homedir,
              psz_title, psz_extension ? psz_extension : "" );
    }
}
#define ArtCacheCreatePath(a,b,c,d) __ArtCacheCreatePath(VLC_OBJECT(a),b,c,d)
static void __ArtCacheCreatePath( vlc_object_t *p_obj,
                                  const char *psz_title,
                                  const char *psz_artist, const char *psz_album )
{
    char psz_dir[MAX_PATH+1];
    snprintf( psz_dir, MAX_PATH, "%s" DIR_SEP CONFIG_DIR,
              p_obj->p_libvlc->psz_homedir );
    utf8_mkdir( psz_dir );
    snprintf( psz_dir, MAX_PATH, "%s" DIR_SEP CONFIG_DIR DIR_SEP "art",
              p_obj->p_libvlc->psz_homedir );
    utf8_mkdir( psz_dir );

    if( psz_artist && psz_artist )
    {
        snprintf( psz_dir, MAX_PATH, "%s" DIR_SEP CONFIG_DIR DIR_SEP
                  "art" DIR_SEP "artistalbum",
                       p_obj->p_libvlc->psz_homedir );
        utf8_mkdir( psz_dir );
        snprintf( psz_dir, MAX_PATH, "%s" DIR_SEP CONFIG_DIR DIR_SEP
                  "art" DIR_SEP "artistalbum" DIR_SEP "%s",
                      p_obj->p_libvlc->psz_homedir, psz_artist );
        utf8_mkdir( psz_dir );
        snprintf( psz_dir, MAX_PATH, "%s" DIR_SEP CONFIG_DIR DIR_SEP
                  "art" DIR_SEP "artistalbum" DIR_SEP "%s" DIR_SEP "%s",
                      p_obj->p_libvlc->psz_homedir,
                      psz_artist, psz_album );
        utf8_mkdir( psz_dir );
    }
    else
    {
        snprintf( psz_dir, MAX_PATH, "%s" DIR_SEP CONFIG_DIR DIR_SEP
                  "art" DIR_SEP "title",
                      p_obj->p_libvlc->psz_homedir );
        utf8_mkdir( psz_dir );
        snprintf( psz_dir, MAX_PATH, "%s" DIR_SEP CONFIG_DIR DIR_SEP
                  "art" DIR_SEP "title" DIR_SEP "%s",
                      p_obj->p_libvlc->psz_homedir, psz_title );
        utf8_mkdir( psz_dir );
    }
}
static char *ArtCacheCreateString( const char *psz )
{
    char *dup = strdup(psz);
    int i;

    /* Doesn't create a filename with invalid characters
     * TODO: several filesystems forbid several characters: list them all
     */
    for( i = 0; dup[i] != '\0'; i++ )
    {
        if( dup[i] == '/' )
            dup[i] = ' ';
    }
    return dup;
}

static int __input_FindArtInCache( vlc_object_t *p_obj, input_item_t *p_item )
{
    const char *psz_artist;
    const char *psz_album;
    const char *psz_title;
    char psz_filename[MAX_PATH+1];
    int i;
    struct stat a;
    const char *ppsz_type[] = { ".jpg", ".png", ".gif", ".bmp", "" };

    if( !p_item->p_meta ) return VLC_EGENERIC;

    psz_artist = input_item_GetArtist( p_item );
    psz_album = input_item_GetAlbum( p_item );
    psz_title = input_item_GetTitle( p_item );
    if( !psz_title ) psz_title = p_item->psz_name;

    if( (!psz_artist || !psz_album) && !psz_title ) return VLC_EGENERIC;

    for( i = 0; i < 5; i++ )
    {
        ArtCacheCreateName( p_obj, psz_filename, psz_title /* Used if none artist nor album is defined */,
                            psz_artist, psz_album, ppsz_type[i] );

        /* Check if file exists */
        if( utf8_stat( psz_filename+7, &a ) == 0 )
        {
            input_item_SetArtURL( p_item, psz_filename );
            return VLC_SUCCESS;
        }
    }
    return VLC_EGENERIC;
}

/**
 * Download the art using the URL or an art downloaded
 * This function should be called only if data is not already in cache
 */
int input_DownloadAndCacheArt( playlist_t *p_playlist, input_item_t *p_item )
{
    int i_status = VLC_EGENERIC;
    stream_t *p_stream;
    char psz_filename[MAX_PATH+1];
    char *psz_artist = NULL;
    char *psz_album = NULL;
    char *psz_title = NULL;
    char *psz_type;
    if( input_item_GetArtist( p_item ) )
        psz_artist = ArtCacheCreateString( input_item_GetArtist( p_item ) );
    if( input_item_GetAlbum( p_item ) )
        psz_album = ArtCacheCreateString( input_item_GetAlbum( p_item ) );
    if( input_item_GetTitle( p_item ) )
        psz_title = ArtCacheCreateString( input_item_GetTitle( p_item ) );
    else if( p_item->psz_name )
        psz_title = ArtCacheCreateString( p_item->psz_name );

    if( !psz_title || (!psz_album && !psz_artist) )
    {
        free( psz_title );
        free( psz_album );
        free( psz_artist );
        return VLC_EGENERIC;
    }

    assert( !EMPTY_STR(input_item_GetArtURL( p_item )) );

    psz_type = strrchr( input_item_GetArtURL( p_item ), '.' );

    /* */
    ArtCacheCreateName( p_playlist, psz_filename, psz_title /* Used only if needed*/,
                        psz_artist, psz_album, psz_type );

    /* */
    ArtCacheCreatePath( p_playlist, psz_title, psz_artist, psz_album );

    /* */
    free( psz_artist );
    free( psz_album );
    free( psz_title );

    if( !strncmp( input_item_GetArtURL( p_item ) , "APIC", 4 ) )
    {
        msg_Warn( p_playlist, "APIC fetch not supported yet" );
        return VLC_EGENERIC;
    }

    p_stream = stream_UrlNew( p_playlist, input_item_GetArtURL( p_item ) );
    if( p_stream )
    {
        uint8_t p_buffer[65536];
        long int l_read;
        FILE *p_file = utf8_fopen( psz_filename+7, "w" );
        int err = 0;
        while( ( l_read = stream_Read( p_stream, p_buffer, sizeof (p_buffer) ) ) )
        {
            if( fwrite( p_buffer, l_read, 1, p_file ) != 1 )
            {
                err = errno;
                break;
            }
        }
        if( fclose( p_file ) && !err )
            err = errno;
        stream_Delete( p_stream );

        if( err )
            msg_Err( p_playlist, "%s: %s", psz_filename, strerror( err ) );
        else
            msg_Dbg( p_playlist, "album art saved to %s\n", psz_filename );

        input_item_SetArtURL( p_item, psz_filename );
        i_status = VLC_SUCCESS;
    }
    return i_status;
}

void input_ExtractAttachmentAndCacheArt( input_thread_t *p_input )
{
    input_item_t *p_item = p_input->p->input.p_item;
    char *psz_arturl;
    char *psz_artist = NULL;
    char *psz_album = NULL;
    char *psz_title = NULL;
    char *psz_type = NULL;
    char psz_filename[MAX_PATH+1];
    FILE *f;
    input_attachment_t *p_attachment;
    struct stat s;
    int i_idx;

    /* TODO-fenrir merge input_ArtFind with download and make it set the flags FETCH
     * and then set it here to to be faster */

    psz_arturl = strdup( input_item_GetArtURL( p_item ) );
    if( !psz_arturl || strncmp( psz_arturl, "attachment://", strlen("attachment://") ) )
    {
        msg_Err( p_input, "internal input error with input_ExtractAttachmentAndCacheArt" );
        return;
    }
    input_item_SetArtURL( p_item, NULL );

    if( input_item_IsArtFetched( p_item ) )
    {
        /* XXX Weird, we should not have end up with attachment:// art url unless there is a race
         * condition */
        msg_Warn( p_input, "internal input error with input_ExtractAttachmentAndCacheArt" );
        input_FindArtInCache( p_input, p_item );
        free( psz_arturl );
        return;
    }

    /* */
    for( i_idx = 0, p_attachment = NULL; i_idx < p_input->p->i_attachment; i_idx++ )
    {
        if( !strcmp( p_input->p->attachment[i_idx]->psz_name,
                     &psz_arturl[strlen("attachment://")] ) )
        {
            p_attachment = p_input->p->attachment[i_idx];
            break;
        }
    }
    if( !p_attachment || p_attachment->i_data <= 0 )
    {
        msg_Warn( p_input, "internal input error with input_ExtractAttachmentAndCacheArt" );
        goto end;
    }

    if( input_item_GetArtist( p_item ) )
        psz_artist = ArtCacheCreateString( input_item_GetArtist( p_item ) );
    if( input_item_GetAlbum( p_item ) )
        psz_album = ArtCacheCreateString( input_item_GetAlbum( p_item ) );
    if( input_item_GetTitle( p_item ) )
        psz_title = ArtCacheCreateString( input_item_GetTitle( p_item ) );
    else if( p_item->psz_name )
        psz_title = ArtCacheCreateString( p_item->psz_name );

    if( (!psz_artist || !psz_album ) && !psz_title )
        goto end;

    /* */
    psz_type = strrchr( psz_arturl, '.' );
    ArtCacheCreateName( p_input, psz_filename, psz_title, psz_artist, psz_album, psz_type );

    /* Check if we already dumped it */
    if( !utf8_stat( psz_filename+7, &s ) )
        goto end;

    ArtCacheCreatePath( p_input, psz_title, psz_artist, psz_album );

    f = utf8_fopen( psz_filename+7, "w" );
    if( f )
    {
        if( fwrite( p_attachment->p_data, p_attachment->i_data, 1, f ) != 1 )
            msg_Err( p_input, "%s: %s", psz_filename, strerror( errno ) );
        else
            msg_Dbg( p_input, "album art saved to %s\n", psz_filename );
        fclose( f );
    }

end:
    if( psz_artist ) free( psz_artist );
    if( psz_album ) free( psz_album );
    if( psz_title ) free( psz_title );
    if( psz_arturl ) free( psz_arturl );
}


uint32_t input_CurrentMetaFlags( vlc_meta_t *p_meta )
{
    uint32_t i_meta = 0;

    if( !p_meta )
        return 0;

#define CHECK( a, b ) \
    if( !EMPTY_STR( vlc_meta_Get( p_meta, vlc_meta_ ## a ) ) ) \
        i_meta |= VLC_META_ENGINE_ ## b;

    CHECK( Title, TITLE )
    CHECK( Artist, ARTIST )
    CHECK( Album, COLLECTION )
#if 0
    /* As this is not used at the moment, don't uselessly check for it.
     * Re-enable this when it is used */
    CHECK( Genre, GENRE )
    CHECK( Copyright, COPYRIGHT )
    CHECK( Tracknum, SEQ_NUM )
    CHECK( Description, DESCRIPTION )
    CHECK( Rating, RATING )
    CHECK( Date, DATE )
    CHECK( URL, URL )
    CHECK( Language, LANGUAGE )
#endif
    CHECK( ArtworkURL, ART_URL )

    return i_meta;
}
