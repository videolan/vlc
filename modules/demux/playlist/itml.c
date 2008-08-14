/*******************************************************************************
 * itml.c : iTunes Music Library import functions
 *******************************************************************************
 * Copyright (C) 2007 the VideoLAN team
 * $Id$
 *
 * Authors: Yoann Peronneau <yoann@videolan.org>
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
 *******************************************************************************/
/**
 * \file modules/demux/playlist/itml.c
 * \brief iTunes Music Library import functions
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_demux.h>

#include "playlist.h"
#include "vlc_xml.h"
#include "vlc_strings.h"
#include "vlc_url.h"
#include "itml.h"

struct demux_sys_t
{
    int i_ntracks;
};

static int Control( demux_t *, int, va_list );
static int Demux( demux_t * );

/**
 * \brief iTML submodule initialization function
 */
int Import_iTML( vlc_object_t *p_this )
{
    DEMUX_BY_EXTENSION_OR_FORCED_MSG( ".xml", "itml",
                                      "using iTunes Media Library reader" );
    return VLC_SUCCESS;
}

void Close_iTML( vlc_object_t *p_this )
{
    demux_t *p_demux = (demux_t *)p_this;
    free( p_demux->p_sys );
}

/**
 * \brief demuxer function for iTML parsing
 */
int Demux( demux_t *p_demux )
{
    int i_ret = VLC_SUCCESS;
    xml_t *p_xml = NULL;
    xml_reader_t *p_xml_reader = NULL;
    char *psz_name = NULL;
    INIT_PLAYLIST_STUFF;
    p_demux->p_sys->i_ntracks = 0;

    /* create new xml parser from stream */
    p_xml = xml_Create( p_demux );
    if( !p_xml )
        i_ret = VLC_ENOMOD;
    else
    {
        p_xml_reader = xml_ReaderCreate( p_xml, p_demux->s );
        if( !p_xml_reader )
            i_ret = VLC_EGENERIC;
    }

    /* locating the root node */
    if( i_ret == VLC_SUCCESS )
    {
        do
        {
            if( xml_ReaderRead( p_xml_reader ) != 1 )
            {
                msg_Err( p_demux, "can't read xml stream" );
                i_ret = VLC_EGENERIC;
            }
        } while( i_ret == VLC_SUCCESS &&
                 xml_ReaderNodeType( p_xml_reader ) != XML_READER_STARTELEM );
    }
    /* checking root node name */
    if( i_ret == VLC_SUCCESS )
    {
        psz_name = xml_ReaderName( p_xml_reader );
        if( !psz_name || strcmp( psz_name, "plist" ) )
        {
            msg_Err( p_demux, "invalid root node name: %s", psz_name );
            i_ret = VLC_EGENERIC;
        }
        FREE_NAME();
    }

    if( i_ret == VLC_SUCCESS )
    {
        xml_elem_hnd_t pl_elements[] =
            { {"dict",    COMPLEX_CONTENT, {.cmplx = parse_plist_dict} } };
        i_ret = parse_plist_node( p_demux, p_current_input,
                                     NULL, p_xml_reader, "plist",
                                     pl_elements );
        HANDLE_PLAY_AND_RELEASE;
    }

    if( p_xml_reader )
        xml_ReaderDelete( p_xml, p_xml_reader );
    if( p_xml )
        xml_Delete( p_xml );
    return 0; /* Needed for correct operation of go back */
}

/** \brief dummy function for demux callback interface */
static int Control( demux_t *p_demux, int i_query, va_list args )
{
    VLC_UNUSED(p_demux); VLC_UNUSED(i_query); VLC_UNUSED(args);
    return VLC_EGENERIC;
}

/**
 * \brief parse the root node of the playlist
 */
static bool parse_plist_node COMPLEX_INTERFACE
{
    VLC_UNUSED(p_track); VLC_UNUSED(psz_element);
    char *psz_name = NULL;
    char *psz_value = NULL;
    bool b_version_found = false;

    /* read all playlist attributes */
    while( xml_ReaderNextAttr( p_xml_reader ) == VLC_SUCCESS )
    {
        psz_name = xml_ReaderName( p_xml_reader );
        psz_value = xml_ReaderValue( p_xml_reader );
        if( !psz_name || !psz_value )
        {
            msg_Err( p_demux, "invalid xml stream @ <plist>" );
            FREE_ATT();
            return false;
        }
        /* attribute: version */
        if( !strcmp( psz_name, "version" ) )
        {
            b_version_found = true;
            if( strcmp( psz_value, "1.0" ) )
                msg_Warn( p_demux, "unsupported iTunes Media Library version" );
        }
        /* unknown attribute */
        else
            msg_Warn( p_demux, "invalid <plist> attribute:\"%s\"", psz_name);

        FREE_ATT();
    }
    /* attribute version is mandatory !!! */
    if( !b_version_found )
        msg_Warn( p_demux, "<plist> requires \"version\" attribute" );

    return parse_dict( p_demux, p_input_item, NULL, p_xml_reader,
                       "plist", p_handlers );
}

/**
 * \brief parse a <dict>
 * \param COMPLEX_INTERFACE
 */
static bool parse_dict COMPLEX_INTERFACE
{
    int i_node;
    char *psz_name = NULL;
    char *psz_value = NULL;
    char *psz_key = NULL;
    xml_elem_hnd_t *p_handler = NULL;

    while( xml_ReaderRead( p_xml_reader ) == 1 )
    {
        i_node = xml_ReaderNodeType( p_xml_reader );
        switch( i_node )
        {
            case XML_READER_NONE:
                break;

            case XML_READER_STARTELEM:
                /*  element start tag  */
                psz_name = xml_ReaderName( p_xml_reader );
                if( !psz_name || !*psz_name )
                {
                    msg_Err( p_demux, "invalid xml stream" );
                    FREE_ATT_KEY();
                    return false;
                }
                /* choose handler */
                for( p_handler = p_handlers;
                     p_handler->name && strcmp( psz_name, p_handler->name );
                     p_handler++ );
                if( !p_handler->name )
                {
                    msg_Err( p_demux, "unexpected element <%s>", psz_name );
                    FREE_ATT_KEY();
                    return false;
                }
                FREE_NAME();
                /* complex content is parsed in a separate function */
                if( p_handler->type == COMPLEX_CONTENT )
                {
                    if( p_handler->pf_handler.cmplx( p_demux,
                                                     p_input_item,
                                                     NULL,
                                                     p_xml_reader,
                                                     p_handler->name,
                                                     NULL ) )
                    {
                        p_handler = NULL;
                        FREE_ATT_KEY();
                    }
                    else
                    {
                        FREE_ATT_KEY();
                        return false;
                    }
                }
                break;

            case XML_READER_TEXT:
                /* simple element content */
                FREE_ATT();
                psz_value = xml_ReaderValue( p_xml_reader );
                if( !psz_value )
                {
                    msg_Err( p_demux, "invalid xml stream" );
                    FREE_ATT_KEY();
                    return false;
                }
                break;

            case XML_READER_ENDELEM:
                /* element end tag */
                psz_name = xml_ReaderName( p_xml_reader );
                if( !psz_name )
                {
                    msg_Err( p_demux, "invalid xml stream" );
                    FREE_ATT_KEY();
                    return false;
                }
                /* leave if the current parent node <track> is terminated */
                if( !strcmp( psz_name, psz_element ) )
                {
                    FREE_ATT_KEY();
                    return true;
                }
                /* there MUST have been a start tag for that element name */
                if( !p_handler || !p_handler->name
                    || strcmp( p_handler->name, psz_name ))
                {
                    msg_Err( p_demux, "there's no open element left for <%s>",
                             psz_name );
                    FREE_ATT_KEY();
                    return false;
                }
                /* special case: key */
                if( !strcmp( p_handler->name, "key" ) )
                {
                    psz_key = strdup( psz_value );
                }
                /* call the simple handler */
                else if( p_handler->pf_handler.smpl )
                {
                    p_handler->pf_handler.smpl( p_track, psz_key, psz_value );
                }
                FREE_ATT();
                p_handler = NULL;
                break;

            default:
                /* unknown/unexpected xml node */
                msg_Err( p_demux, "unexpected xml node %i", i_node );
                FREE_ATT_KEY();
                return false;
        }
        FREE_NAME();
    }
    msg_Err( p_demux, "unexpected end of xml data" );
    FREE_ATT_KEY();
    return false;
}

static bool parse_plist_dict COMPLEX_INTERFACE
{
    VLC_UNUSED(p_track); VLC_UNUSED(psz_element); VLC_UNUSED(p_handlers);
    xml_elem_hnd_t pl_elements[] =
        { {"dict",    COMPLEX_CONTENT, {.cmplx = parse_tracks_dict} },
          {"array",   SIMPLE_CONTENT,  {NULL} },
          {"key",     SIMPLE_CONTENT,  {NULL} },
          {"integer", SIMPLE_CONTENT,  {NULL} },
          {"string",  SIMPLE_CONTENT,  {NULL} },
          {"date",    SIMPLE_CONTENT,  {NULL} },
          {"true",    SIMPLE_CONTENT,  {NULL} },
          {"false",   SIMPLE_CONTENT,  {NULL} },
          {NULL,      UNKNOWN_CONTENT, {NULL} }
        };

    return parse_dict( p_demux, p_input_item, NULL, p_xml_reader,
                       "dict", pl_elements );
}

static bool parse_tracks_dict COMPLEX_INTERFACE
{
    VLC_UNUSED(p_track); VLC_UNUSED(psz_element); VLC_UNUSED(p_handlers);
    xml_elem_hnd_t tracks_elements[] =
        { {"dict",    COMPLEX_CONTENT, {.cmplx = parse_track_dict} },
          {"key",     SIMPLE_CONTENT,  {NULL} },
          {NULL,      UNKNOWN_CONTENT, {NULL} }
        };

    parse_dict( p_demux, p_input_item, NULL, p_xml_reader,
                "dict", tracks_elements );

    msg_Info( p_demux, "added %i tracks successfully",
              p_demux->p_sys->i_ntracks );

    return true;
}

static bool parse_track_dict COMPLEX_INTERFACE
{
    VLC_UNUSED(psz_element); VLC_UNUSED(p_handlers);
    input_item_t *p_new_input = NULL;
    int i_ret = -1;
    char *psz_uri = NULL;
    p_track = new_track();

    xml_elem_hnd_t track_elements[] =
        { {"array",   COMPLEX_CONTENT, {.cmplx = skip_element} },
          {"key",     SIMPLE_CONTENT,  {.smpl = save_data} },
          {"integer", SIMPLE_CONTENT,  {.smpl = save_data} },
          {"string",  SIMPLE_CONTENT,  {.smpl = save_data} },
          {"date",    SIMPLE_CONTENT,  {.smpl = save_data} },
          {"true",    SIMPLE_CONTENT,  {NULL} },
          {"false",   SIMPLE_CONTENT,  {NULL} },
          {NULL,      UNKNOWN_CONTENT, {NULL} }
        };

    i_ret = parse_dict( p_demux, p_input_item, p_track,
                        p_xml_reader, "dict", track_elements );

    msg_Dbg( p_demux, "name: %s, artist: %s, album: %s, genre: %s, trackNum: %s, location: %s",
             p_track->name, p_track->artist, p_track->album, p_track->genre, p_track->trackNum, p_track->location );

    if( !p_track->location )
    {
        msg_Err( p_demux, "Track needs Location" );
        free_track( p_track );
        return false;
    }

    psz_uri = decode_URI_duplicate( p_track->location );

    if( psz_uri )
    {
        if( strlen( psz_uri ) > 17 &&
            !strncmp( psz_uri, "file://localhost/", 17 ) )
        {
            /* remove 'localhost/' */
            memmove( psz_uri + 7, psz_uri + 17, strlen( psz_uri ) - 9 );
            msg_Info( p_demux, "Adding '%s'", psz_uri );

            p_new_input = input_item_NewExt( p_demux, psz_uri,
                                            NULL, 0, NULL, -1 );
            input_item_AddSubItem( p_input_item, p_new_input );

            /* add meta info */
            add_meta( p_new_input, p_track );
            vlc_gc_decref( p_new_input );
    
            p_demux->p_sys->i_ntracks++;
        }
        else
        {
            msg_Err( p_demux, "Don't know how to handle %s", psz_uri );
        }
        free( psz_uri );
    }

    free_track( p_track );
    return i_ret;
}

static track_elem_t *new_track()
{
    track_elem_t *p_track = NULL;
    p_track = (track_elem_t *)malloc( sizeof( track_elem_t ) );
    if( p_track )
    {
        p_track->name = NULL;
        p_track->artist = NULL;
        p_track->album = NULL;
        p_track->genre = NULL;
        p_track->trackNum = NULL;
        p_track->location = NULL;
        p_track->duration = 0;
    }
    return p_track;
}

static void free_track( track_elem_t *p_track )
{
    fprintf( stderr, "free track\n" );
    if ( !p_track )
        return;

    FREE( p_track->name )
    FREE( p_track->artist )
    FREE( p_track->album )
    FREE( p_track->genre )
    FREE( p_track->trackNum )
    FREE( p_track->location )
    p_track->duration = 0;
    free( p_track );
}

static bool save_data SIMPLE_INTERFACE
{
    /* exit if setting is impossible */
    if( !psz_name || !psz_value || !p_track )
        return false;

    /* re-convert xml special characters inside psz_value */
    resolve_xml_special_chars( psz_value );

#define SAVE_INFO( name, value ) \
    if( !strcmp( psz_name, name ) ) { p_track->value = strdup( psz_value ); }

    SAVE_INFO( "Name", name )
    else SAVE_INFO( "Artist", artist )
    else SAVE_INFO( "Album", album )
    else SAVE_INFO( "Genre", genre )
    else SAVE_INFO( "Track Number", trackNum )
    else SAVE_INFO( "Location", location )
    else if( !strcmp( psz_name, "Total Time" ) )
    {
        long i_num = atol( psz_value );
        p_track->duration = (mtime_t) i_num*1000;
    }
    return true;
}

/**
 * \brief handles the supported <track> sub-elements
 */
static bool add_meta( input_item_t *p_input_item,
                            track_elem_t *p_track )
{
    /* exit if setting is impossible */
    if( !p_input_item || !p_track )
        return false;

#define SET_INFO( func, prop ) \
    if( p_track->prop ) { func( p_input_item, p_track->prop ); }

    SET_INFO( input_item_SetTitle, name )
    SET_INFO( input_item_SetArtist, artist )
    SET_INFO( input_item_SetAlbum, album )
    SET_INFO( input_item_SetGenre, genre )
    SET_INFO( input_item_SetTrackNum, trackNum )
    SET_INFO( input_item_SetDuration, duration )
    return true;
}

/**
 * \brief skips complex element content that we can't manage
 */
static bool skip_element COMPLEX_INTERFACE
{
    VLC_UNUSED(p_demux); VLC_UNUSED(p_input_item);
    VLC_UNUSED(p_track); VLC_UNUSED(p_handlers);
    char *psz_endname;

    while( xml_ReaderRead( p_xml_reader ) == 1 )
    {
        if( xml_ReaderNodeType( p_xml_reader ) == XML_READER_ENDELEM )
        {
            psz_endname = xml_ReaderName( p_xml_reader );
            if( !psz_endname )
                return false;
            if( !strcmp( psz_element, psz_endname ) )
            {
                free( psz_endname );
                return true;
            }
            else
                free( psz_endname );
        }
    }
    return false;
}
