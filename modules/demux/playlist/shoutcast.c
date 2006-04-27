/*****************************************************************************
 * shoutcast.c: Winamp >=5.2 shoutcast demuxer
 *****************************************************************************
 * Copyright (C) 2006 the VideoLAN team
 * $Id$
 *
 * Authors: Antoine Cellerier <dionoea -@t- videolan -Dot- org>
 *          based on b4s.c by Sigmund Augdal Helberg <dnumgis@videolan.org>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>                                      /* malloc(), free() */
#include <ctype.h>                                              /* isspace() */

#include <vlc/vlc.h>
#include <vlc/input.h>
#include <vlc/intf.h>

#include <errno.h>                                                 /* ENOMEM */
#include "playlist.h"
#include "vlc_xml.h"

struct demux_sys_t
{
    playlist_t *p_playlist;
    playlist_item_t *p_current;

    xml_t *p_xml;
    xml_reader_t *p_xml_reader;

    vlc_bool_t b_adult;
};

/* duplicate from modules/services_discovery/shout.c */
#define SHOUTCAST_BASE_URL "http/shout-winamp://www.shoutcast.com/sbin/newxml.phtml"
#define SHOUTCAST_TUNEIN_BASE_URL "http://www.shoutcast.com"
#define SHOUTCAST_TV_TUNEIN_URL "http://www.shoutcast.com/sbin/tunein-tvstation.pls?id="

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int Demux( demux_t *p_demux);
static int Control( demux_t *p_demux, int i_query, va_list args );

static int DemuxGenre( demux_t *p_demux );
static int DemuxStation( demux_t *p_demux );

/*****************************************************************************
 * Import_Shoutcast: main import function
 *****************************************************************************/
int E_(Import_Shoutcast)( vlc_object_t *p_this )
{
    demux_t *p_demux = (demux_t *)p_this;
    demux_sys_t *p_sys;

    char    *psz_ext;

    psz_ext = strrchr ( p_demux->psz_path, '.' );

    if( !p_demux->psz_demux || strcmp(p_demux->psz_demux, "shout-winamp") )
    {
        return VLC_EGENERIC;
    }
    msg_Dbg( p_demux, "using shoutcast playlist import");

    p_demux->pf_control = Control;
    p_demux->pf_demux = Demux;
    p_demux->p_sys = p_sys = malloc( sizeof(demux_sys_t) );
    if( p_sys == NULL )
    {
        msg_Err( p_demux, "out of memory" );
        return VLC_ENOMEM;
    }

    p_sys->p_playlist = NULL;
    p_sys->p_xml = NULL;
    p_sys->p_xml_reader = NULL;

    /* Do we want to list adult content ? */
    var_Create( p_demux, "shoutcast-show-adult",
                VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
    p_sys->b_adult = var_GetBool( p_demux, "shoutcast-show-adult" );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Deactivate: frees unused data
 *****************************************************************************/
void E_(Close_Shoutcast)( vlc_object_t *p_this )
{
    demux_t *p_demux = (demux_t *)p_this;
    demux_sys_t *p_sys = p_demux->p_sys;

    if( p_sys->p_playlist )
        vlc_object_release( p_sys->p_playlist );
    if( p_sys->p_xml_reader )
        xml_ReaderDelete( p_sys->p_xml, p_sys->p_xml_reader );
    if( p_sys->p_xml )
        xml_Delete( p_sys->p_xml );
    free( p_sys );
}

static int Demux( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    playlist_t *p_playlist;

    vlc_bool_t b_play;

    xml_t *p_xml;
    xml_reader_t *p_xml_reader;

    char *psz_eltname = NULL;


    p_playlist = (playlist_t *) vlc_object_find( p_demux, VLC_OBJECT_PLAYLIST,
                                                 FIND_ANYWHERE );
    if( !p_playlist )
    {
        msg_Err( p_demux, "can't find playlist" );
        return -1;
    }
    p_sys->p_playlist = p_playlist;

    b_play = E_(FindItem)( p_demux, p_playlist, &p_sys->p_current );

    msg_Warn( p_demux, "item: %s", p_sys->p_current->input.psz_name );
    playlist_ItemToNode( p_playlist, p_sys->p_current );
    p_sys->p_current->input.i_type = ITEM_TYPE_PLAYLIST;

    p_xml = p_sys->p_xml = xml_Create( p_demux );
    if( !p_xml ) return -1;

    p_xml_reader = xml_ReaderCreate( p_xml, p_demux->s );
    if( !p_xml_reader ) return -1;
    p_sys->p_xml_reader = p_xml_reader;

    /* xml */
    /* check root node */
    if( xml_ReaderRead( p_xml_reader ) != 1 )
    {
        msg_Err( p_demux, "invalid file (no root node)" );
        return -1;
    }

    if( xml_ReaderNodeType( p_xml_reader ) != XML_READER_STARTELEM ||
        ( psz_eltname = xml_ReaderName( p_xml_reader ) ) == NULL ||
        ( strcmp( psz_eltname, "genrelist" )
          && strcmp( psz_eltname, "stationlist" ) ) )
    {
        msg_Err( p_demux, "invalid root node %i, %s",
                 xml_ReaderNodeType( p_xml_reader ), psz_eltname );
        if( psz_eltname ) free( psz_eltname );
        return -1;
    }

    if( !strcmp( psz_eltname, "genrelist" ) )
    {
        /* we're reading a genre list */
        free( psz_eltname );
        if( DemuxGenre( p_demux ) ) return -1;
    }
    else
    {
        /* we're reading a station list */
        free( psz_eltname );
        if( DemuxStation( p_demux ) ) return -1;
    }

    /* Go back and play the playlist */
    if( b_play && p_playlist->status.p_item &&
        p_playlist->status.p_item->i_children > 0 )
    {
        playlist_Control( p_playlist, PLAYLIST_VIEWPLAY,
                          p_playlist->status.i_view,
                          p_playlist->status.p_item,
                          p_playlist->status.p_item->pp_children[0] );
    }

    vlc_object_release( p_playlist );
    p_sys->p_playlist = NULL;
    return VLC_SUCCESS;
}

#define GET_VALUE( a ) \
                        if( !strcmp( psz_attrname, #a ) ) \
                        { \
                            psz_ ## a = strdup( psz_attrvalue ); \
                        }
/* <genrelist>
 *   <genre name="the name"></genre>
 *   ...
 * </genrelist>
 **/
static int DemuxGenre( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    char *psz_name = NULL; /* genre name */
    char *psz_eltname = NULL; /* tag name */

#define FREE(a) if( a ) free( a ); a = NULL;
    while( xml_ReaderRead( p_sys->p_xml_reader ) == 1 )
    {
        int i_type;

        // Get the node type
        i_type = xml_ReaderNodeType( p_sys->p_xml_reader );
        switch( i_type )
        {
            // Error
            case -1:
                return -1;
                break;

            case XML_READER_STARTELEM:
                // Read the element name
                psz_eltname = xml_ReaderName( p_sys->p_xml_reader );
                if( !psz_eltname ) return -1;


                if( !strcmp( psz_eltname, "genre" ) )
                {
                    // Read the attributes
                    while( xml_ReaderNextAttr( p_sys->p_xml_reader ) == VLC_SUCCESS )
                    {
                        char *psz_attrname = xml_ReaderName( p_sys->p_xml_reader );
                        char *psz_attrvalue =
                            xml_ReaderValue( p_sys->p_xml_reader );
                        if( !psz_attrname || !psz_attrvalue )
                        {
                            FREE(psz_attrname);
                            FREE(psz_attrvalue);
                            free(psz_eltname);
                            /*FIXME: isn't return a bit too much. what about break*/
                            return -1;
                        }

                        GET_VALUE( name )
                        else
                        {
                            msg_Warn( p_demux,
                                      "unexpected attribure %s in element %s",
                                      psz_attrname,
                                      psz_eltname );
                        }
                        free( psz_attrname );
                        free( psz_attrvalue );
                    }
                }
                free( psz_eltname ); psz_eltname = NULL;
                break;

            case XML_READER_TEXT:
                break;

            // End element
            case XML_READER_ENDELEM:
                // Read the element name
                psz_eltname = xml_ReaderName( p_sys->p_xml_reader );
                if( !psz_eltname ) return -1;
                if( !strcmp( psz_eltname, "genre" ) )
                {
                    playlist_item_t *p_item;
                    char *psz_mrl = malloc( strlen( SHOUTCAST_BASE_URL )
                            + strlen( "?genre=" ) + strlen( psz_name ) + 1 );
                    sprintf( psz_mrl, SHOUTCAST_BASE_URL "?genre=%s",
                             psz_name );
                    p_item = playlist_ItemNew( p_sys->p_playlist, psz_mrl,
                                               psz_name );
                    free( psz_mrl );
                    playlist_NodeAddItem( p_sys->p_playlist, p_item,
                                          p_sys->p_current->pp_parents[0]->i_view,
                                          p_sys->p_current, PLAYLIST_APPEND,
                                          PLAYLIST_END );

                    /* We need to declare the parents of the node as the
                     *                  * same of the parent's ones */
                    playlist_CopyParents( p_sys->p_current, p_item );

                    vlc_input_item_CopyOptions( &p_sys->p_current->input,
                                                &p_item->input );

                    FREE( psz_name );
                }
                FREE( psz_eltname );
                break;
        }
    }
    return 0;
}

/* radio stations:
 * <stationlist>
 *   <tunein base="/sbin/tunein-station.pls"></tunein>
 *   <station name="the name"
 *            mt="mime type"
 *            id="the id"
 *            br="bit rate"
 *            genre="A big genre string"
 *            ct="current track name/author/..."
 *            lc="listener count"></station>
 * </stationlist>
 *
 * TV stations:
 * <stationlist>
 *   <tunein base="/sbin/tunein-station.pls"></tunein>
 *   <station name="the name"
 *            id="the id"
 *            br="bit rate"
 *            rt="rating"
 *            load="server load ?"
 *            ct="current track name/author/..."
 *            genre="A big genre string"
 *            lc="listener count"></station>
 * </stationlist>
 **/
static int DemuxStation( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    char *psz_base = NULL; /* */

    char *psz_name = NULL; /* genre name */
    char *psz_mt = NULL; /* mime type */
    char *psz_id = NULL; /* id */
    char *psz_br = NULL; /* bit rate */
    char *psz_genre = NULL; /* genre */
    char *psz_ct = NULL; /* current track */
    char *psz_lc = NULL; /* listener count */

    /* If these are set then it's *not* a radio but a TV */
    char *psz_rt = NULL; /* rating for shoutcast TV */
    char *psz_load = NULL; /* load for shoutcast TV */

    char *psz_eltname = NULL; /* tag name */

    while( xml_ReaderRead( p_sys->p_xml_reader ) == 1 )
    {
        int i_type;

        // Get the node type
        i_type = xml_ReaderNodeType( p_sys->p_xml_reader );
        switch( i_type )
        {
            // Error
            case -1:
                return -1;
                break;

            case XML_READER_STARTELEM:
                // Read the element name
                psz_eltname = xml_ReaderName( p_sys->p_xml_reader );
                if( !psz_eltname ) return -1;

                // Read the attributes
                if( !strcmp( psz_eltname, "tunein" ) )
                {
                    while( xml_ReaderNextAttr( p_sys->p_xml_reader ) == VLC_SUCCESS )
                    {
                        char *psz_attrname = xml_ReaderName( p_sys->p_xml_reader );
                        char *psz_attrvalue =
                            xml_ReaderValue( p_sys->p_xml_reader );
                        if( !psz_attrname || !psz_attrvalue )
                        {
                            free(psz_eltname);
                            FREE(psz_attrname);
                            FREE(psz_attrvalue);
                            return -1;
                        }

                        GET_VALUE( base )
                        else
                        {
                            msg_Warn( p_demux,
                                      "unexpected attribure %s in element %s",
                                      psz_attrname,
                                      psz_eltname );
                        }
                        free( psz_attrname );
                        free( psz_attrvalue );
                    }
                }
                else if( !strcmp( psz_eltname, "station" ) )
                {
                    while( xml_ReaderNextAttr( p_sys->p_xml_reader ) == VLC_SUCCESS )
                    {
                        char *psz_attrname = xml_ReaderName( p_sys->p_xml_reader );
                        char *psz_attrvalue =
                            xml_ReaderValue( p_sys->p_xml_reader );
                        if( !psz_attrname || !psz_attrvalue )
                        {
                            free(psz_eltname);
                            FREE(psz_attrname);
                            FREE(psz_attrvalue);
                            return -1;
                        }

                        GET_VALUE( name )
                        else GET_VALUE( mt )
                        else GET_VALUE( id )
                        else GET_VALUE( br )
                        else GET_VALUE( genre )
                        else GET_VALUE( ct )
                        else GET_VALUE( lc )
                        else GET_VALUE( rt )
                        else GET_VALUE( load )
                        else
                        {
                            msg_Warn( p_demux,
                                      "unexpected attribure %s in element %s",
                                      psz_attrname,
                                      psz_eltname );
                        }
                        free( psz_attrname );
                        free( psz_attrvalue );
                    }
                }
                free(psz_eltname);
                break;

            case XML_READER_TEXT:
                break;

            // End element
            case XML_READER_ENDELEM:
                // Read the element name
                psz_eltname = xml_ReaderName( p_sys->p_xml_reader );
                if( !psz_eltname ) return -1;
                if( !strcmp( psz_eltname, "station" ) &&
                    ( psz_base || ( psz_rt && psz_load &&
                    ( p_sys->b_adult || strcmp( psz_rt, "NC17" ) ) ) ) )
                {
                    playlist_item_t *p_item;
                    char *psz_mrl = NULL;
                    if( psz_rt || psz_load )
                    {
                        /* tv */
                        psz_mrl = malloc( strlen( SHOUTCAST_TV_TUNEIN_URL )
                                          + strlen( psz_id ) + 1 );
                        sprintf( psz_mrl, SHOUTCAST_TV_TUNEIN_URL "%s",
                                 psz_id );
                    }
                    else
                    {
                        /* radio */
                        psz_mrl = malloc( strlen( SHOUTCAST_TUNEIN_BASE_URL )
                            + strlen( psz_base ) + strlen( "?id=" )
                            + strlen( psz_id ) + 1 );
                        sprintf( psz_mrl, SHOUTCAST_TUNEIN_BASE_URL "%s?id=%s",
                             psz_base, psz_id );
                    }
                    p_item = playlist_ItemNew( p_sys->p_playlist, psz_mrl,
                                               psz_name );
                    free( psz_mrl );

                    if( psz_mt )
                    {
                        vlc_input_item_AddInfo( &p_item->input,
                                                _( "Shoutcast" ),
                                                _( "Mime type" ),
                                                "%s",
                                                psz_mt );
                    }
                    if( psz_br )
                    {
                        vlc_input_item_AddInfo( &p_item->input,
                                                _( "Shoutcast" ),
                                                _( "Bitrate" ),
                                                "%s",
                                                psz_br );
                    }
                    if( psz_genre )
                    {
                        vlc_input_item_AddInfo( &p_item->input,
                                                _(VLC_META_INFO_CAT),
                                                _(VLC_META_GENRE),
                                                "%s",
                                                psz_genre );
                    }
                    if( psz_ct )
                    {
                        vlc_input_item_AddInfo( &p_item->input,
                                                _(VLC_META_INFO_CAT),
                                                _(VLC_META_NOW_PLAYING),
                                                "%s",
                                                psz_ct );
                    }
                    if( psz_lc )
                    {
                        vlc_input_item_AddInfo( &p_item->input,
                                                _( "Shoutcast" ),
                                                _( "Listeners" ),
                                                "%s",
                                                psz_lc );
                    }
                    if( psz_rt )
                    {
                        vlc_input_item_AddInfo( &p_item->input,
                                                _(VLC_META_INFO_CAT),
                                                _(VLC_META_RATING),
                                                "%s",
                                                psz_rt );
                    }
                    if( psz_load )
                    {
                        vlc_input_item_AddInfo( &p_item->input,
                                                _( "Shoutcast" ),
                                                _( "Load" ),
                                                "%s",
                                                psz_load );
                    }

                    playlist_NodeAddItem( p_sys->p_playlist, p_item,
                                          p_sys->p_current->pp_parents[0]->i_view,
                                          p_sys->p_current, PLAYLIST_APPEND,
                                          PLAYLIST_END );

                    /* We need to declare the parents of the node as the
                     *                  * same of the parent's ones */
                    playlist_CopyParents( p_sys->p_current, p_item );

                    vlc_input_item_CopyOptions( &p_sys->p_current->input,
                                                &p_item->input );

                    FREE( psz_name );
                    FREE( psz_mt )
                    FREE( psz_id )
                    FREE( psz_br )
                    FREE( psz_genre )
                    FREE( psz_ct )
                    FREE( psz_lc )
                    FREE( psz_rt )
                }
                free( psz_eltname );
                break;
        }
    }
    return 0;
}
#undef FREE

static int Control( demux_t *p_demux, int i_query, va_list args )
{
    return VLC_EGENERIC;
}
