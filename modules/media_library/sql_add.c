/*****************************************************************************
 * sql_add.c: SQL-based media library
 *****************************************************************************
 * Copyright (C) 2008-2010 the VideoLAN Team and AUTHORS
 * $Id$
 *
 * Authors: Antoine Lejeune <phytos@videolan.org>
 *          Jean-Philippe André <jpeg@videolan.org>
 *          Rémi Duraffort <ivoire@videolan.org>
 *          Adrien Maglo <magsoft@videolan.org>
 *          Srikanth Raju <srikiraju at gmail dot com>
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

#include "sql_media_library.h"

/*****************************************************************************
 * ADD FUNCTIONS
 *****************************************************************************/

/**
 * @brief Add element to ML based on a ml_media_t (media ID ignored)
 * @param p_ml This media_library_t object
 * @param p_media media item to add in the DB. The media_id is ignored
 * @return VLC_SUCCESS or VLC_EGENERIC
 * @note This function is threadsafe
 */
int AddMedia( media_library_t *p_ml, ml_media_t *p_media )
{
    int i_ret = VLC_SUCCESS;
    int i_album_artist = 0;

    Begin( p_ml );
    ml_LockMedia( p_media );
    assert( p_media->i_id == 0 );
    /* Add any people */
    ml_person_t* person = p_media->p_people;
    while( person )
    {
        if( person->i_id <= 0 )
        {
            if( person->psz_name )
            {
                person->i_id = ml_GetInt( p_ml, ML_PEOPLE_ID, person->psz_role,
                                            ML_PEOPLE, person->psz_role,
                                            person->psz_name );
                if( person->i_id <= 0 )
                {
                    /* Create person */
                    AddPeople( p_ml, person->psz_name, person->psz_role );
                    person->i_id = ml_GetInt( p_ml, ML_PEOPLE_ID, person->psz_role,
                                            ML_PEOPLE, person->psz_role,
                                            person->psz_name );
                }

            }
        }
        if( strcmp( person->psz_role, ML_PERSON_ALBUM_ARTIST ) == 0 )
            i_album_artist = person->i_id;
        person = person->p_next;
    }

    /* Album id */
    if( p_media->i_album_id <= 0 )
    {
        if( p_media->psz_album )
        {
            /* TODO:Solidly incorporate Album artist */
            int i_album_id = ml_GetAlbumId( p_ml, p_media->psz_album );
            if( i_album_id <= 0 )
            {
                /* Create album */
                i_ret = AddAlbum( p_ml, p_media->psz_album, p_media->psz_cover,
                                    i_album_artist );
                if( i_ret != VLC_SUCCESS )
                    return i_ret;
                i_album_id = ml_GetAlbumId( p_ml, p_media->psz_album );
                if( i_album_id <= 0 )
                    return i_ret;
            }
            p_media->i_album_id = i_album_id;
        }
    }


    if( !p_media->psz_uri || !*p_media->psz_uri )
    {
        msg_Dbg( p_ml, "cannot add a media without uri (%s)", __func__ );
        return VLC_EGENERIC;
    }

    i_ret = QuerySimple( p_ml,
            "INSERT INTO media ( uri, title, original_title, genre, type, "
            "comment, cover, preview, year, track, disc, album_id, vote, score, "
            "duration, first_played, played_count, last_played, "
            "skipped_count, last_skipped, import_time, filesize ) "
            "VALUES ( %Q, %Q, %Q, %Q, '%d',%Q, %Q, %Q, '%d', '%d', '%d', '%d',"
            "'%d', '%d', '%d', '%d', '%d', '%d', '%d', '%d', '%d', '%d' )",
                p_media->psz_uri,
                p_media->psz_title,
                p_media->psz_orig_title,
                p_media->psz_genre,
                (int)p_media->i_type,
                p_media->psz_comment,
                p_media->psz_cover,
                p_media->psz_preview,
                (int)p_media->i_year,
                (int)p_media->i_track_number,
                (int)p_media->i_disc_number,
                (int)p_media->i_album_id,
                (int)p_media->i_vote,
                (int)p_media->i_score,
                (int)p_media->i_duration,
                (int)p_media->i_first_played,
                (int)p_media->i_played_count,
                (int)p_media->i_last_played,
                (int)p_media->i_skipped_count,
                (int)p_media->i_last_skipped,
                (int)p_media->i_import_time,
                (int)p_media->i_filesize );
    if( i_ret != VLC_SUCCESS )
        goto quit_addmedia;

    int id = GetMediaIdOfURI( p_ml, p_media->psz_uri );
    if( id <= 0 )
    {
        i_ret = VLC_EGENERIC;
        goto quit_addmedia;
    }

    p_media->i_id = id;
    person = p_media->p_people;
    if( !person )
    {
        /* If there is no person, set it to "Unknown", ie. people_id=0 */
        i_ret = QuerySimple( p_ml, "INSERT into media_to_people ( media_id, "
                                     "people_id ) VALUES ( %d, %d )",
                                     id, 0 );
        if( i_ret != VLC_SUCCESS )
            goto quit_addmedia;
    } else {
        while( person )
        {
            i_ret = QuerySimple( p_ml, "INSERT into media_to_people ( media_id, "
                                         "people_id ) VALUES ( %d, %d )",
                                         id, person->i_id );
            if( i_ret != VLC_SUCCESS )
                goto quit_addmedia;
            person = person->p_next;
        }
    }

    i_ret = QuerySimple( p_ml, "INSERT into extra ( id, extra, language, bitrate, "
            "samplerate, bpm ) VALUES ( '%d', %Q, %Q, '%d', '%d', '%d' )",
            id, p_media->psz_extra, p_media->psz_language,
            p_media->i_bitrate, p_media->i_samplerate, p_media->i_bpm );
    if( i_ret != VLC_SUCCESS )
        goto quit_addmedia;
    i_ret = pool_InsertMedia( p_ml, p_media, true );

quit_addmedia:
    if( i_ret == VLC_SUCCESS )
    {
        Commit( p_ml );
    }
    else
        Rollback( p_ml );
    ml_UnlockMedia( p_media );
    if( i_ret == VLC_SUCCESS )
        var_SetInteger( p_ml, "media-added", id );
    return i_ret;
}


/**
 * @brief Add generic album to ML
 *
 * @param p_ml this Media Library
 * @param psz_title album title, cannot be null
 * @param psz_cover album cover, can be null
 * @return VLC_SUCCESS or a VLC error code
 *
 * This will add a new in the album table, without checking if album is
 * already present (or another album with same title)
 */
int AddAlbum( media_library_t *p_ml, const char *psz_title,
        const char *psz_cover, const int i_album_artist )
{
    assert( p_ml );

    if( !psz_title || !*psz_title )
    {
        msg_Warn( p_ml, "tried to add an album without title" );
        return VLC_EGENERIC;
    }
    msg_Dbg( p_ml, "New album: '%s'", psz_title );

    int i_ret = QuerySimple( p_ml,
                        "INSERT INTO album ( title, cover, album_artist_id ) "
                        "VALUES ( %Q, %Q, '%d' )",
                        psz_title , psz_cover, i_album_artist );

    return i_ret;
}


/**
 * @brief Add generic people to ML
 *
 * @param p_ml this Media Library
 * @param psz_title name
 * @param i_role role: 1 for artist, 2 for publisher
 * @return VLC_SUCCESS or a VLC error code
 *
 * This will add a new in the album table, without checking if album is
 * already present (or another album with same title)
 */
int AddPeople( media_library_t *p_ml, const char *psz_name,
        const char* psz_role )
{
    assert( p_ml );
    assert( psz_role && *psz_role );

    if( !psz_name || !*psz_name )
    {
        msg_Warn( p_ml, "tried to add an artist without name" );
        return VLC_EGENERIC;
    }
    msg_Dbg( p_ml, "New people: (%s) '%s'", psz_role, psz_name );

    int i_ret = QuerySimple( p_ml,
                             "INSERT INTO people ( name, role ) "
                             "VALUES ( %Q, %Q )",
                             psz_name, psz_role );

    return i_ret;
}

/**
 * @brief Add element to ML based on an Input Item
 * @param p_ml This media_library_t object
 * @param p_input input item to add
 * @return VLC_SUCCESS or VLC_EGENERIC
 */
int AddInputItem( media_library_t *p_ml, input_item_t *p_input )
{
    assert( p_ml );
    if( !p_input || !p_input->psz_uri )
        return VLC_EGENERIC;
    int i_ret = VLC_SUCCESS;

    vlc_gc_incref( p_input );

    /* Check input item is not already in the ML */
    i_ret = GetMediaIdOfInputItem( p_ml, p_input );
    if( i_ret > 0 )
    {
        msg_Dbg( p_ml, "Item already in Media Library (id: %d)", i_ret );
        vlc_gc_decref( p_input );
        return VLC_SUCCESS;
    }

    ml_media_t* p_media = media_New( p_ml, 0, ML_MEDIA, false );

    /* Add media to the database */
    CopyInputItemToMedia( p_media, p_input );
    i_ret = AddMedia( p_ml, p_media );
    if( i_ret == VLC_SUCCESS )
        watch_add_Item( p_ml, p_input, p_media );
    ml_gc_decref( p_media );
    vlc_gc_decref( p_input );
    return i_ret;
}


/**
 * @brief Add element to ML based on a Playlist Item
 *
 * @param p_ml the media library object
 * @param p_playlist_item playlist_item to add
 * @return VLC_SUCCESS or VLC_EGENERIC
 */
int AddPlaylistItem( media_library_t *p_ml, playlist_item_t *p_playlist_item )
{
    if( !p_playlist_item )
        return VLC_EGENERIC;

    return AddInputItem( p_ml, p_playlist_item->p_input );
}

