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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_demux.h>

#include "playlist.h"
#include "vlc_xml.h"

struct demux_sys_t
{
    input_item_t *p_current_input;

    xml_t *p_xml;
    xml_reader_t *p_xml_reader;

    bool b_adult;
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
int Import_Shoutcast( vlc_object_t *p_this )
{
    demux_t *p_demux = (demux_t *)p_this;

    if( !demux_IsForced( p_demux, "shout-winamp" ) )
        return VLC_EGENERIC;

    STANDARD_DEMUX_INIT_MSG( "using shoutcast playlist reader" );
    p_demux->p_sys->p_xml = NULL;
    p_demux->p_sys->p_xml_reader = NULL;

    /* Do we want to list adult content ? */
    var_Create( p_demux, "shoutcast-show-adult",
                VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
    p_demux->p_sys->b_adult = var_GetBool( p_demux, "shoutcast-show-adult" );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Deactivate: frees unused data
 *****************************************************************************/
void Close_Shoutcast( vlc_object_t *p_this )
{
    demux_t *p_demux = (demux_t *)p_this;
    demux_sys_t *p_sys = p_demux->p_sys;

    if( p_sys->p_xml_reader )
        xml_ReaderDelete( p_sys->p_xml, p_sys->p_xml_reader );
    if( p_sys->p_xml )
        xml_Delete( p_sys->p_xml );
    free( p_sys );
}

static int Demux( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    xml_t *p_xml;
    xml_reader_t *p_xml_reader;
    char *psz_eltname = NULL;
    INIT_PLAYLIST_STUFF;
    p_sys->p_current_input = p_current_input;

    p_xml = p_sys->p_xml = xml_Create( p_demux );
    if( !p_xml ) return -1;

    p_xml_reader = xml_ReaderCreate( p_xml, p_demux->s );
    if( !p_xml_reader ) return -1;
    p_sys->p_xml_reader = p_xml_reader;

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
        free( psz_eltname );
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

    HANDLE_PLAY_AND_RELEASE;
    return 0; /* Needed for correct operation of go back */
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
    input_item_t *p_input;

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
                            FREENULL(psz_attrname);
                            FREENULL(psz_attrvalue);
                            free(psz_eltname);
                            /*FIXME: isn't return a bit too much. what about break*/
                            return -1;
                        }

                        GET_VALUE( name )
                        else
                        {
                            msg_Warn( p_demux,
                                      "unexpected attribure %s in element %s",
                                      psz_attrname,psz_eltname );
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
                    char* psz_mrl;
                    if( asprintf( &psz_mrl, SHOUTCAST_BASE_URL "?genre=%s",
                             psz_name ) != -1 )
                    {
                        p_input = input_item_NewExt( p_demux, psz_mrl,
                                                    psz_name, 0, NULL, -1 );
                        input_item_CopyOptions( p_sys->p_current_input, p_input );
                        free( psz_mrl );
                        input_item_AddSubItem( p_sys->p_current_input, p_input );
                        vlc_gc_decref( p_input );
                    }
                    FREENULL( psz_name );
                }
                FREENULL( psz_eltname );
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
    input_item_t *p_input;

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
                            free( psz_eltname );
                            free( psz_attrname );
                            free( psz_attrvalue );
                            return -1;
                        }

                        GET_VALUE( base )
                        else
                        {
                            msg_Warn( p_demux,
                                      "unexpected attribure %s in element %s",
                                      psz_attrname, psz_eltname );
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
                            free( psz_eltname );
                            free( psz_attrname );
                            free( psz_attrvalue );
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
                                      "unexpected attribute %s in element %s",
                                      psz_attrname, psz_eltname );
                        }
                        free( psz_attrname );
                        free( psz_attrvalue );
                    }
                }
                free( psz_eltname );
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
                    p_input = input_item_NewExt( p_demux, psz_mrl,
                                                psz_name , 0, NULL, -1 );
                    free( psz_mrl );

                    input_item_CopyOptions( p_sys->p_current_input,
                                                p_input );

#define SADD_INFO( type, field ) if( field ) { input_item_AddInfo( \
                    p_input, _("Shoutcast"), _(type), "%s", field ) ; }
                    SADD_INFO( "Mime type", psz_mt );
                    SADD_INFO( "Bitrate", psz_br );
                    SADD_INFO( "Listeners", psz_lc );
                    SADD_INFO( "Load", psz_load );
                    if( psz_genre )
                        input_item_SetGenre( p_input, psz_genre );
                    if( psz_ct )
                        input_item_SetNowPlaying( p_input, psz_ct );
                    if( psz_rt )
                        input_item_SetRating( p_input, psz_rt );
                    input_item_AddSubItem( p_sys->p_current_input, p_input );
                    vlc_gc_decref( p_input );
                    FREENULL( psz_name );
                    FREENULL( psz_mt );
                    FREENULL( psz_id );
                    FREENULL( psz_br );
                    FREENULL( psz_genre );
                    FREENULL( psz_ct );
                    FREENULL( psz_lc );
                    FREENULL( psz_rt );
                }
                free( psz_eltname );
                break;
        }
    }
    return 0;
}

static int Control( demux_t *p_demux, int i_query, va_list args )
{
    VLC_UNUSED(p_demux); VLC_UNUSED(i_query); VLC_UNUSED(args);
    return VLC_EGENERIC;
}
