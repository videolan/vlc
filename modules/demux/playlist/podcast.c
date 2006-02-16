/*****************************************************************************
 * podcast.c : podcast playlist imports
 *****************************************************************************
 * Copyright (C) 2005 the VideoLAN team
 * $Id$
 *
 * Authors: Antoine Cellerier <dionoea -at- videolan -dot- org>
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
    char *psz_prefix;
    playlist_t *p_playlist;
    xml_t *p_xml;
    xml_reader_t *p_xml_reader;
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int Demux( demux_t *p_demux);
static int Control( demux_t *p_demux, int i_query, va_list args );

/*****************************************************************************
 * Import_podcast: main import function
 *****************************************************************************/
int E_(Import_podcast)( vlc_object_t *p_this )
{
    demux_t *p_demux = (demux_t *)p_this;
    demux_sys_t *p_sys;

    char    *psz_ext;

    psz_ext = strrchr ( p_demux->psz_path, '.' );

    if( p_demux->psz_demux && !strcmp(p_demux->psz_demux, "podcast") )
    {
        ;
    }
    else
    {
        return VLC_EGENERIC;
    }
    msg_Dbg( p_demux, "using podcast playlist import");

    p_demux->pf_control = Control;
    p_demux->pf_demux = Demux;
    p_demux->p_sys = p_sys = malloc( sizeof(demux_sys_t) );
    if( p_sys == NULL )
    {
        msg_Err( p_demux, "Out of memory" );
        return VLC_ENOMEM;
    }
    p_sys->psz_prefix = E_(FindPrefix)( p_demux );
    p_sys->p_playlist = NULL;
    p_sys->p_xml = NULL;
    p_sys->p_xml_reader = NULL;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Deactivate: frees unused data
 *****************************************************************************/
void E_(Close_podcast)( vlc_object_t *p_this )
{
    demux_t *p_demux = (demux_t *)p_this;
    demux_sys_t *p_sys = p_demux->p_sys;

    if( p_sys->psz_prefix ) free( p_sys->psz_prefix );
    if( p_sys->p_playlist ) vlc_object_release( p_sys->p_playlist );
    if( p_sys->p_xml_reader ) xml_ReaderDelete( p_sys->p_xml, p_sys->p_xml_reader );
    if( p_sys->p_xml ) xml_Delete( p_sys->p_xml );
    free( p_sys );
}

/* "specs" : http://phobos.apple.com/static/iTunesRSS.html */
static int Demux( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    playlist_t *p_playlist;
    playlist_item_t *p_item, *p_current;

    vlc_bool_t b_play;
    vlc_bool_t b_item = VLC_FALSE;
    vlc_bool_t b_image = VLC_FALSE;
    int i_ret;

    xml_t *p_xml;
    xml_reader_t *p_xml_reader;
    char *psz_elname = NULL;
    char *psz_item_mrl = NULL;
    char *psz_item_size = NULL;
    char *psz_item_type = NULL;
    char *psz_item_name = NULL;
    char *psz_item_date = NULL;
    char *psz_item_author = NULL;
    char *psz_item_category = NULL;
    char *psz_item_duration = NULL;
    char *psz_item_keywords = NULL;
    char *psz_item_subtitle = NULL;
    char *psz_item_summary = NULL;
    int i_type;

    p_playlist = (playlist_t *) vlc_object_find( p_demux, VLC_OBJECT_PLAYLIST,
                                                 FIND_ANYWHERE );
    if( !p_playlist )
    {
        msg_Err( p_demux, "can't find playlist" );
        return -1;
    }
    p_sys->p_playlist = p_playlist;

    b_play = E_(FindItem)( p_demux, p_playlist, &p_current );

    playlist_ItemToNode( p_playlist, p_current );
    p_current->input.i_type = ITEM_TYPE_PLAYLIST;

    p_xml = p_sys->p_xml = xml_Create( p_demux );
    if( !p_xml ) return -1;

    psz_elname = stream_ReadLine( p_demux->s );
    if( psz_elname ) free( psz_elname );
    psz_elname = 0;

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
        ( psz_elname = xml_ReaderName( p_xml_reader ) ) == NULL ||
        strcmp( psz_elname, "rss" ) )
    {
        msg_Err( p_demux, "invalid root node %i, %s",
                 xml_ReaderNodeType( p_xml_reader ), psz_elname );
        if( psz_elname ) free( psz_elname );
        return -1;
    }
    free( psz_elname ); psz_elname = NULL;

    while( (i_ret = xml_ReaderRead( p_xml_reader )) == 1 )
    {
        // Get the node type
        i_type = xml_ReaderNodeType( p_xml_reader );
        switch( i_type )
        {
            // Error
            case -1:
                return -1;
                break;

            case XML_READER_STARTELEM:
            {
                // Read the element name
                if( psz_elname ) free( psz_elname );
                psz_elname = xml_ReaderName( p_xml_reader );
                if( !psz_elname ) return -1;

                if( !strcmp( psz_elname, "item" ) )
                {
                    b_item = VLC_TRUE;
                }
                else if( !strcmp( psz_elname, "image" ) )
                {
                    b_item = VLC_TRUE;
                }

                // Read the attributes
                while( xml_ReaderNextAttr( p_xml_reader ) == VLC_SUCCESS )
                {
                    char *psz_name = xml_ReaderName( p_xml_reader );
                    char *psz_value = xml_ReaderValue( p_xml_reader );
                    if( !psz_name || !psz_value ) return -1;
                    if( !strcmp( psz_elname, "enclosure" ) &&
                        !strcmp( psz_name, "url" ) )
                    {
                        psz_item_mrl = strdup( psz_value );
                    }
                    else if( !strcmp( psz_elname, "enclosure" ) &&
                        !strcmp( psz_name, "length" ) )
                    {
                        psz_item_size = strdup( psz_value );
                    }
                    else if( !strcmp( psz_elname, "enclosure" ) &&
                        !strcmp( psz_name, "type" ) )
                    {
                        psz_item_type = strdup( psz_value );
                    }
                    else
                    {
                        msg_Dbg( p_demux,"unhandled attribure %s in element %s",
                                  psz_name, psz_elname );
                    }
                    free( psz_name );
                    free( psz_value );
                }
                break;
            }
            case XML_READER_TEXT:
            {
                char *psz_text = xml_ReaderValue( p_xml_reader );
                /* item specific meta data */
                if( b_item == VLC_TRUE && !strcmp( psz_elname, "title" ) )
                {
                    psz_item_name = strdup( psz_text );
                }
                else if( b_item == VLC_TRUE
                         && !strcmp( psz_elname, "pubDate" ) )
                {
                    psz_item_date = strdup( psz_text );
                }
                else if( b_item == VLC_TRUE
                         && ( !strcmp( psz_elname, "itunes:author" )
                            ||!strcmp( psz_elname, "author" ) ) )
                { /* <author> isn't standard iTunes podcast stuff */
                    psz_item_author = strdup( psz_text );
                }
                else if( b_item == VLC_TRUE
                         && !strcmp( psz_elname, "itunes:category" ) )
                {
                    psz_item_category = strdup( psz_text );
                }
                else if( b_item == VLC_TRUE
                         && !strcmp( psz_elname, "itunes:duration" ) )
                {
                    psz_item_duration = strdup( psz_text );
                }
                else if( b_item == VLC_TRUE
                         && !strcmp( psz_elname, "itunes:keywords" ) )
                {
                    psz_item_keywords = strdup( psz_text );
                }
                else if( b_item == VLC_TRUE
                         && !strcmp( psz_elname, "itunes:subtitle" ) )
                {
                    psz_item_subtitle = strdup( psz_text );
                }
                else if( b_item == VLC_TRUE
                         && ( !strcmp( psz_elname, "itunes:summary" )
                            ||!strcmp( psz_elname, "description" ) ) )
                { /* <description> isn't standard iTunes podcast stuff */
                    psz_item_summary = strdup( psz_text );
                }
                /* toplevel meta data */
                else if( b_item == VLC_FALSE && b_image == VLC_FALSE
                         && !strcmp( psz_elname, "title" ) )
                {
                    playlist_ItemSetName( p_current, psz_text );
                }
                else if( b_item == VLC_FALSE && b_image == VLC_FALSE
                         && !strcmp( psz_elname, "link" ) )
                {
                    vlc_input_item_AddInfo( &(p_current->input),
                                            _( "Podcast Info" ),
                                            _( "Podcast Link" ),
                                            "%s",
                                            psz_text );
                }
                else if( b_item == VLC_FALSE && b_image == VLC_FALSE
                         && !strcmp( psz_elname, "copyright" ) )
                {
                    vlc_input_item_AddInfo( &(p_current->input),
                                            _( "Podcast Info" ),
                                            _( "Podcast Copyright" ),
                                            "%s",
                                            psz_text );
                }
                else if( b_item == VLC_FALSE && b_image == VLC_FALSE
                         && !strcmp( psz_elname, "itunes:category" ) )
                {
                    vlc_input_item_AddInfo( &(p_current->input),
                                            _( "Podcast Info" ),
                                            _( "Podcast Category" ),
                                            "%s",
                                            psz_text );
                }
                else if( b_item == VLC_FALSE && b_image == VLC_FALSE
                         && !strcmp( psz_elname, "itunes:keywords" ) )
                {
                    vlc_input_item_AddInfo( &(p_current->input),
                                            _( "Podcast Info" ),
                                            _( "Podcast Keywords" ),
                                            "%s",
                                            psz_text );
                }
                else if( b_item == VLC_FALSE && b_image == VLC_FALSE
                         && !strcmp( psz_elname, "itunes:subtitle" ) )
                {
                    vlc_input_item_AddInfo( &(p_current->input),
                                            _( "Podcast Info" ),
                                            _( "Podcast Subtitle" ),
                                            "%s",
                                            psz_text );
                }
                else if( b_item == VLC_FALSE && b_image == VLC_FALSE
                         && ( !strcmp( psz_elname, "itunes:summary" )
                            ||!strcmp( psz_elname, "description" ) ) )
                { /* <description> isn't standard iTunes podcast stuff */
                    vlc_input_item_AddInfo( &(p_current->input),
                                            _( "Podcast Info" ),
                                            _( "Podcast Summary" ),
                                            "%s",
                                            psz_text );
                }
                else
                {
                    msg_Dbg( p_demux, "unhandled text in element '%s'",
                              psz_elname );
                }
                free( psz_text );
                break;
            }
            // End element
            case XML_READER_ENDELEM:
            {
                // Read the element name
                free( psz_elname );
                psz_elname = xml_ReaderName( p_xml_reader );
                if( !psz_elname ) return -1;
                if( !strcmp( psz_elname, "item" ) )
                {
                    p_item = playlist_ItemNew( p_playlist, psz_item_mrl,
                                               psz_item_name );
                    if( p_item == NULL ) break;
                    playlist_NodeAddItem( p_playlist, p_item,
                                          p_current->pp_parents[0]->i_view,
                                          p_current, PLAYLIST_APPEND,
                                          PLAYLIST_END );

                    /* We need to declare the parents of the node as the
                     *                  * same of the parent's ones */
                    playlist_CopyParents( p_current, p_item );

                    if( psz_item_date )
                    {
                        vlc_input_item_AddInfo( &p_item->input,
                                                _( "Podcast Info" ),
                                                _( "Podcast Publication Date" ),
                                                "%s",
                                                psz_item_date );
                    }
                    if( psz_item_author )
                    {
                        vlc_input_item_AddInfo( &p_item->input,
                                                _( "Podcast Info" ),
                                                _( "Podcast Author" ),
                                                "%s",
                                                psz_item_author );
                    }
                    if( psz_item_category )
                    {
                        vlc_input_item_AddInfo( &p_item->input,
                                                _( "Podcast Info" ),
                                                _( "Podcast Subcategory" ),
                                                "%s",
                                                psz_item_category );
                    }
                    if( psz_item_duration )
                    {
                        vlc_input_item_AddInfo( &p_item->input,
                                                _( "Podcast Info" ),
                                                _( "Podcast Duration" ),
                                                "%s",
                                                psz_item_duration );
                    }
                    if( psz_item_keywords )
                    {
                        vlc_input_item_AddInfo( &p_item->input,
                                                _( "Podcast Info" ),
                                                _( "Podcast Keywords" ),
                                                "%s",
                                                psz_item_keywords );
                    }
                    if( psz_item_subtitle )
                    {
                        vlc_input_item_AddInfo( &p_item->input,
                                                _( "Podcast Info" ),
                                                _( "Podcast Subtitle" ),
                                                "%s",
                                                psz_item_subtitle );
                    }
                    if( psz_item_summary )
                    {
                        vlc_input_item_AddInfo( &p_item->input,
                                                _( "Podcast Info" ),
                                                _( "Podcast Summary" ),
                                                "%s",
                                                psz_item_summary );
                    }
                    if( psz_item_size )
                    {
                        vlc_input_item_AddInfo( &p_item->input,
                                                _( "Podcast Info" ),
                                                _( "Podcast Size" ),
                                                "%s bytes",
                                                psz_item_size );
                    }
                    if( psz_item_type )
                    {
                        vlc_input_item_AddInfo( &p_item->input,
                                                _( "Podcast Info" ),
                                                _( "Podcast Type" ),
                                                "%s",
                                                psz_item_type );
                    }

#define FREE(a) if( a ) free( a ); a = NULL;
                    FREE( psz_item_name );
                    FREE( psz_item_mrl );
                    FREE( psz_item_size );
                    FREE( psz_item_type );
                    FREE( psz_item_date );
                    FREE( psz_item_author );
                    FREE( psz_item_category );
                    FREE( psz_item_duration );
                    FREE( psz_item_keywords );
                    FREE( psz_item_subtitle );
                    FREE( psz_item_summary );
#undef FREE

                    b_item = VLC_FALSE;
                }
                else if( !strcmp( psz_elname, "image" ) )
                {
                    b_image = VLC_FALSE;
                }
                free( psz_elname );
                psz_elname = strdup("");

                break;
            }
        }
    }

    if( i_ret != 0 )
    {
        msg_Warn( p_demux, "error while parsing data" );
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

static int Control( demux_t *p_demux, int i_query, va_list args )
{
    return VLC_EGENERIC;
}
