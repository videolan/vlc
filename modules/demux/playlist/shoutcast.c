/*****************************************************************************
 * shoutcast.c: Winamp >=5.2 shoutcast demuxer
 *****************************************************************************
 * Copyright (C) 2006 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Antoine Cellerier <dionoea -@t- videolan -Dot- org>
 *          based on b4s.c by Sigmund Augdal Helberg <dnumgis@videolan.org>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_demux.h>

#include "playlist.h"
#include <vlc_xml.h>

/* duplicate from modules/services_discovery/shout.c */
#define SHOUTCAST_BASE_URL "http/shout-winamp://www.shoutcast.com/sbin/newxml.phtml"
#define SHOUTCAST_TUNEIN_BASE_URL "http://www.shoutcast.com"
#define SHOUTCAST_TV_TUNEIN_URL "http://www.shoutcast.com/sbin/tunein-tvstation.pls?id="

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int Demux( demux_t *p_demux);

static int DemuxGenre( demux_t *p_demux, xml_reader_t *p_xml_reader,
                       input_item_node_t *p_input_node );
static int DemuxStation( demux_t *p_demux, xml_reader_t *p_xml_reader,
                         input_item_node_t *p_input_node, bool b_adult );

/*****************************************************************************
 * Import_Shoutcast: main import function
 *****************************************************************************/
int Import_Shoutcast( vlc_object_t *p_this )
{
    demux_t *p_demux = (demux_t *)p_this;

    if( !demux_IsForced( p_demux, "shout-winamp" ) )
        return VLC_EGENERIC;

    p_demux->pf_demux = Demux;
    p_demux->pf_control = Control;
    msg_Dbg( p_demux, "using shoutcast playlist reader" );

    return VLC_SUCCESS;
}

static int Demux( demux_t *p_demux )
{
    xml_reader_t *p_xml_reader = NULL;
    const char *node;
    int i_ret = -1;
    input_item_t *p_current_input = GetCurrentItem(p_demux);
    input_item_node_t *p_input_node = NULL;

    p_xml_reader = xml_ReaderCreate( p_demux, p_demux->s );
    if( !p_xml_reader )
        goto error;

    /* check root node */
    if( xml_ReaderNextNode( p_xml_reader, &node ) != XML_READER_STARTELEM )
    {
        msg_Err( p_demux, "invalid file (no root node)" );
        goto error;
    }

    if( strcmp( node, "genrelist" ) && strcmp( node, "stationlist" ) )
    {
        msg_Err( p_demux, "invalid root node <%s>", node );
        goto error;
    }

    p_input_node = input_item_node_Create( p_current_input );

    if( !strcmp( node, "genrelist" ) )
    {
        /* we're reading a genre list */
        if( DemuxGenre( p_demux, p_xml_reader, p_input_node ) )
            goto error;
    }
    else
    {
        /* we're reading a station list */
        if( DemuxStation( p_demux, p_xml_reader, p_input_node,
                var_InheritBool( p_demux, "shoutcast-show-adult" ) ) )
            goto error;
    }

    input_item_node_PostAndDelete( p_input_node );
    p_input_node = NULL;

    i_ret = 0; /* Needed for correct operation of go back */

error:
    if( p_xml_reader )
        xml_ReaderDelete( p_xml_reader );
    if( p_input_node ) input_item_node_Delete( p_input_node );
    vlc_gc_decref(p_current_input);
    return i_ret;
}

/* <genrelist>
 *   <genre name="the name"></genre>
 *   ...
 * </genrelist>
 **/
static int DemuxGenre( demux_t *p_demux, xml_reader_t *p_xml_reader,
                       input_item_node_t *p_input_node )
{
    const char *node;
    char *psz_name = NULL; /* genre name */
    int type;

    while( (type = xml_ReaderNextNode( p_xml_reader, &node )) > 0 )
    {
        switch( type )
        {
            case XML_READER_STARTELEM:
            {
                if( !strcmp( node, "genre" ) )
                {
                    // Read the attributes
                    const char *name, *value;
                    while( (name = xml_ReaderNextAttr( p_xml_reader, &value )) )
                    {
                        if( !strcmp( name, "name" ) )
                        {
                            free(psz_name);
                            psz_name = strdup( value );
                        }
                        else
                            msg_Warn( p_demux,
                                      "unexpected attribute %s in <%s>",
                                      name, node );
                    }
                }
                break;
            }

            case XML_READER_ENDELEM:
                if( !strcmp( node, "genre" ) && psz_name != NULL )
                {
                    char* psz_mrl;

                    if( asprintf( &psz_mrl, SHOUTCAST_BASE_URL "?genre=%s",
                                  psz_name ) != -1 )
                    {
                        input_item_t *p_input;
                        p_input = input_item_New( psz_mrl, psz_name );
                        input_item_CopyOptions( p_input_node->p_item, p_input );
                        free( psz_mrl );
                        input_item_node_AppendItem( p_input_node, p_input );
                        vlc_gc_decref( p_input );
                    }
                    FREENULL( psz_name );
                }
                break;
        }
    }

    free( psz_name );
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
static int DemuxStation( demux_t *p_demux, xml_reader_t *p_xml_reader,
                         input_item_node_t *p_input_node, bool b_adult )
{
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

    const char *node; /* tag name */
    int i_type;

    while( (i_type = xml_ReaderNextNode( p_xml_reader, &node )) > 0 )
    {
        switch( i_type )
        {
            case XML_READER_STARTELEM:
                // Read the attributes
                if( !strcmp( node, "tunein" ) )
                {
                    const char *name, *value;
                    while( (name = xml_ReaderNextAttr( p_xml_reader, &value )) )
                    {
                        if( !strcmp( name, "base" ) )
                        {
                            free( psz_base );
                            psz_base = strdup( value );
                        }
                        else
                            msg_Warn( p_demux,
                                      "unexpected attribute %s in <%s>",
                                      name, node );
                    }
                }
                else if( !strcmp( node, "station" ) )
                {
                    const char *name, *value;
                    while( (name = xml_ReaderNextAttr( p_xml_reader, &value )) )
                    {
                        char **p = NULL;
                        if( !strcmp( name, "name" ) )
                            p = &psz_name;
                        else if ( !strcmp( name, "mt" ) )
                            p = &psz_mt;
                        else if ( !strcmp( name, "id" ) )
                            p = &psz_id;
                        else if ( !strcmp( name, "br" ) )
                            p = &psz_br;
                        else if ( !strcmp( name, "genre" ) )
                            p = &psz_genre;
                        else if ( !strcmp( name, "ct" ) )
                            p = &psz_ct;
                        else if ( !strcmp( name, "lc" ) )
                            p = &psz_lc;
                        else if ( !strcmp( name, "rt" ) )
                            p = &psz_rt;
                        else if ( !strcmp( name, "load" ) )
                            p = &psz_load;
                        if( p != NULL )
                        {
                            free( *p );
                            *p = strdup( value );
                        }
                        else
                            msg_Warn( p_demux,
                                      "unexpected attribute %s in <%s>",
                                      name, node );
                    }
                }
                break;

            // End element
            case XML_READER_ENDELEM:
                if( !strcmp( node, "station" ) &&
                    ( psz_base || ( psz_rt && psz_load &&
                    ( b_adult || strcmp( psz_rt, "NC17" ) ) ) ) )
                {
                    char *psz_mrl = NULL;
                    if( psz_rt || psz_load )
                    {
                        /* tv */
                        if( asprintf( &psz_mrl, SHOUTCAST_TV_TUNEIN_URL "%s",
                                 psz_id ) == -1)
                            psz_mrl = NULL;
                    }
                    else
                    {
                        /* radio */
                        if( asprintf( &psz_mrl, SHOUTCAST_TUNEIN_BASE_URL "%s?id=%s",
                             psz_base, psz_id ) == -1 )
                            psz_mrl = NULL;
                    }

                    /* Create the item */
                    input_item_t *p_input;
                    p_input = input_item_New( psz_mrl, psz_name );
                    input_item_CopyOptions( p_input_node->p_item, p_input );
                    free( psz_mrl );

#define SADD_INFO( type, field ) \
                    if( field ) \
                        input_item_AddInfo( p_input, _("Shoutcast"), \
                                            vlc_gettext(type), "%s", field )
                    SADD_INFO( N_("Mime"), psz_mt );
                    SADD_INFO( N_("Bitrate"), psz_br );
                    SADD_INFO( N_("Listeners"), psz_lc );
                    SADD_INFO( N_("Load"), psz_load );
                    if( psz_genre )
                        input_item_SetGenre( p_input, psz_genre );
                    if( psz_ct )
                        input_item_SetNowPlaying( p_input, psz_ct );
                    if( psz_rt )
                        input_item_SetRating( p_input, psz_rt );
                    input_item_node_AppendItem( p_input_node, p_input );
                    vlc_gc_decref( p_input );
                    FREENULL( psz_base );
                    FREENULL( psz_name );
                    FREENULL( psz_mt );
                    FREENULL( psz_id );
                    FREENULL( psz_br );
                    FREENULL( psz_genre );
                    FREENULL( psz_ct );
                    FREENULL( psz_lc );
                    FREENULL( psz_rt );
                    FREENULL( psz_load );
                }
                break;
        }
    }
    /* FIXME: leaks on missing ENDELEMENT? */
    return 0;
}
