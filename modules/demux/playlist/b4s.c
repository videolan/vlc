/*****************************************************************************
 * b4s.c : B4S playlist format import
 *****************************************************************************
 * Copyright (C) 2005 VideoLAN
 * $Id$
 *
 * Authors: Sigmund Augdal <sigmunau@idi.ntnu.no>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>                                      /* malloc(), free() */

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
    int b_shout;
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int Demux( demux_t *p_demux);
static int Control( demux_t *p_demux, int i_query, va_list args );
static int IsWhitespace( char *psz_string );
static void ShoutcastAdd( playlist_t *p_playlist, playlist_item_t* p_genre,
                          playlist_item_t *p_bitrate, playlist_item_t *p_item,
                          char *psz_genre, char *psz_bitrate );

/*****************************************************************************
 * Import_B4S: main import function
 *****************************************************************************/
int Import_B4S( vlc_object_t *p_this )
{
    demux_t *p_demux = (demux_t *)p_this;
    demux_sys_t *p_sys;

    char    *psz_ext;

    psz_ext = strrchr ( p_demux->psz_path, '.' );

    if( ( psz_ext && !strcasecmp( psz_ext, ".b4s") ) ||
        ( p_demux->psz_demux && !strcmp(p_demux->psz_demux, "b4s-open") ) ||
        ( p_demux->psz_demux && !strcmp(p_demux->psz_demux, "shout-b4s") ) )
    {
        ;
    }
    else
    {
        return VLC_EGENERIC;
    }
    msg_Dbg( p_demux, "using b4s playlist import");

    p_demux->pf_control = Control;
    p_demux->pf_demux = Demux;
    p_demux->p_sys = p_sys = malloc( sizeof(demux_sys_t) );
    if( p_sys == NULL )
    {
        msg_Err( p_demux, "Out of memory" );
        return VLC_ENOMEM;
    }
    p_sys->b_shout = p_demux->psz_demux &&
        !strcmp(p_demux->psz_demux, "shout-b4s");
    p_sys->psz_prefix = FindPrefix( p_demux );
    p_sys->p_playlist = NULL;
    p_sys->p_xml = NULL;
    p_sys->p_xml_reader = NULL;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Deactivate: frees unused data
 *****************************************************************************/
void Close_B4S( vlc_object_t *p_this )
{
    demux_t *p_demux = (demux_t *)p_this;
    demux_sys_t *p_sys = p_demux->p_sys;

    if( p_sys->psz_prefix ) free( p_sys->psz_prefix );
    if( p_sys->p_playlist ) vlc_object_release( p_sys->p_playlist );
    if( p_sys->p_xml_reader ) xml_ReaderDelete( p_sys->p_xml, p_sys->p_xml_reader );
    if( p_sys->p_xml ) xml_Delete( p_sys->p_xml );
    free( p_sys );
}

static int Demux( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    playlist_t *p_playlist;
    playlist_item_t *p_item, *p_current;
    playlist_item_t *p_bitrate = NULL, *p_genre = NULL;

    vlc_bool_t b_play;
    int i_ret;

    xml_t *p_xml;
    xml_reader_t *p_xml_reader;
    char *psz_elname = NULL;
    int i_type, b_shoutcast;
    char *psz_mrl = NULL, *psz_name = NULL, *psz_genre = NULL;
    char *psz_now = NULL, *psz_listeners = NULL, *psz_bitrate = NULL;


    b_shoutcast = p_sys->b_shout;

    p_playlist = (playlist_t *) vlc_object_find( p_demux, VLC_OBJECT_PLAYLIST,
                                                 FIND_PARENT );
    if( !p_playlist )
    {
        msg_Err( p_demux, "can't find playlist" );
        return -1;
    }
    p_sys->p_playlist = p_playlist;

    b_play = FindItem( p_demux, p_playlist, &p_current );

    playlist_ItemToNode( p_playlist, p_current );
    p_current->input.i_type = ITEM_TYPE_PLAYLIST;
    if( b_shoutcast )
    {
        p_genre = playlist_NodeCreate( p_playlist, p_current->pp_parents[0]->i_view, "Genre", p_current );
        playlist_CopyParents( p_current, p_genre );

        p_bitrate = playlist_NodeCreate( p_playlist, p_current->pp_parents[0]->i_view, "Bitrate", p_current );
        playlist_CopyParents( p_current, p_bitrate );
    }
    
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
        strcmp( psz_elname, "WinampXML" ) )
    {
        msg_Err( p_demux, "invalid root node %i, %s",
                 xml_ReaderNodeType( p_xml_reader ), psz_elname );
        if( psz_elname ) free( psz_elname );
        return -1;
    }
    free( psz_elname );

    /* root node should not have any attributes, and should only
     * contain the "playlist node */

    /* Skip until 1st child node */
    while( (i_ret = xml_ReaderRead( p_xml_reader )) == 1 &&
           xml_ReaderNodeType( p_xml_reader ) != XML_READER_STARTELEM );
    if( i_ret != 1 )
    {
        msg_Err( p_demux, "invalid file (no child node)" );
        return -1;
    }

    if( ( psz_elname = xml_ReaderName( p_xml_reader ) ) == NULL ||
        strcmp( psz_elname, "playlist" ) )
    {
        msg_Err( p_demux, "invalid child node %s", psz_elname );
        if( psz_elname ) free( psz_elname );
        return -1;
    }
    free( psz_elname ); psz_elname = 0;

    // Read the attributes
    while( xml_ReaderNextAttr( p_xml_reader ) == VLC_SUCCESS )
    {
        char *psz_name = xml_ReaderName( p_xml_reader );
        char *psz_value = xml_ReaderValue( p_xml_reader );
        if( !psz_name || !psz_value ) return -1;
        if( !strcmp( psz_name, "num_entries" ) )
        {
            msg_Dbg( p_demux, "playlist has %d entries", atoi(psz_value) );
        }
        else if( !strcmp( psz_name, "label" ) )
        {
            playlist_ItemSetName( p_current, psz_value );
        }
        else
        {
            msg_Warn( p_demux, "stray attribute %s with value %s in element"
                      " 'playlist'", psz_name, psz_value );
        }
        free( psz_name );
        free( psz_value );
    }

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
                

                // Read the attributes
                while( xml_ReaderNextAttr( p_xml_reader ) == VLC_SUCCESS )
                {
                    char *psz_name = xml_ReaderName( p_xml_reader );
                    char *psz_value = xml_ReaderValue( p_xml_reader );
                    if( !psz_name || !psz_value ) return -1;
                    if( !strcmp( psz_elname, "entry" ) &&
                        !strcmp( psz_name, "Playstring" ) )
                    {
                        psz_mrl = strdup( psz_value );
                    }
                    else
                    {
                        msg_Warn( p_demux, "unexpected attribure %s in element %s",
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
                if( IsWhitespace( psz_text ) )
                {
                    free( psz_text );
                    break;
                }
                if( !strcmp( psz_elname, "Name" ) )
                {
                    psz_name = strdup( psz_text );
                }
                else if( !strcmp( psz_elname, "Genre" ) )
                {
                    psz_genre = strdup( psz_text );
                }
                else if( !strcmp( psz_elname, "Nowplaying" ) )
                {
                    psz_now = strdup( psz_text );
                }
                else if( !strcmp( psz_elname, "Listeners" ) )
                {
                    psz_listeners = strdup( psz_text );
                }
                else if( !strcmp( psz_elname, "Bitrate" ) )
                {
                    psz_bitrate = strdup( psz_text );
                }
                else if( !strcmp( psz_elname, "" ) )
                {
                    ;
                }
                else
                {
                    msg_Warn( p_demux, "unexpected text in element '%s'",
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
                if( !strcmp( psz_elname, "entry" ) )
                {
                    p_item = playlist_ItemNew( p_playlist, psz_mrl, psz_name );
                    if( psz_now )
                    {
                        vlc_input_item_AddInfo( &(p_item->input),
                                                _("Meta-information"),
                                                _( VLC_META_NOW_PLAYING ),
                                                "%s",
                                                psz_now );
                    }
                    if( psz_genre )
                    {
                        vlc_input_item_AddInfo( &p_item->input,
                                                _("Meta-information"),
                                                _( VLC_META_GENRE ),
                                                "%s",
                                                psz_genre );
                    }
                    if( psz_listeners )
                    {
                        vlc_input_item_AddInfo( &p_item->input,
                                                _("Meta-information"),
                                                _( "Listeners" ),
                                                "%s",
                                                psz_listeners );
                    }
                    if( psz_bitrate )
                    {
                        vlc_input_item_AddInfo( &p_item->input,
                                                _("Meta-information"),
                                                _( "Bitrate" ),
                                                "%s",
                                                psz_bitrate );
                    }
                    playlist_NodeAddItem( p_playlist, p_item,
                                          p_current->pp_parents[0]->i_view,
                                          p_current, PLAYLIST_APPEND,
                                          PLAYLIST_END );

                    /* We need to declare the parents of the node as the
                     *                  * same of the parent's ones */
                    playlist_CopyParents( p_current, p_item );
                    
                    vlc_input_item_CopyOptions( &p_current->input,
                                                &p_item->input );
                    if( b_shoutcast )
                        ShoutcastAdd( p_playlist, p_genre, p_bitrate, p_item,
                                      psz_genre, psz_bitrate );
#define FREE(a) if( a ) free( a ); a = NULL;
                    FREE( psz_name );
                    FREE( psz_mrl );
                    FREE( psz_genre );
                    FREE( psz_bitrate );
                    FREE( psz_listeners );
                    FREE( psz_now );
#undef FREE
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
    if( b_shoutcast )
    {
        vlc_mutex_lock( &p_playlist->object_lock );
        playlist_NodeSort( p_playlist, p_bitrate, SORT_TITLE_NUMERIC, ORDER_NORMAL );
        vlc_mutex_unlock( &p_playlist->object_lock );
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

static int IsWhitespace( char *psz_string )
{
    while( *psz_string )
    {
        if( *psz_string != ' ' && *psz_string != '\t' && *psz_string != '\r' &&
            *psz_string != '\n' )
        {
            return VLC_FALSE;
        }
        psz_string++;
    }
    return VLC_TRUE;
}

static void ShoutcastAdd( playlist_t *p_playlist, playlist_item_t* p_genre,
                          playlist_item_t *p_bitrate, playlist_item_t *p_item,
                          char *psz_genre, char *psz_bitrate )
{
    playlist_item_t *p_parent;
    if( psz_bitrate )
    {
        playlist_item_t *p_copy = playlist_ItemCopy(p_playlist,p_item);
        p_parent = playlist_ChildSearchName( p_bitrate, psz_bitrate );
        if( !p_parent )
        {
            p_parent = playlist_NodeCreate( p_playlist, p_genre->pp_parents[0]->i_view, psz_bitrate,
                                            p_bitrate );
            playlist_CopyParents( p_bitrate, p_parent );
        }
        playlist_NodeAddItem( p_playlist, p_copy, p_parent->pp_parents[0]->i_view, p_parent, PLAYLIST_APPEND, PLAYLIST_END  );
        playlist_CopyParents( p_parent, p_copy );

    }

    if( psz_genre )
    {
        playlist_item_t *p_copy = playlist_ItemCopy(p_playlist,p_item);
        p_parent = playlist_ChildSearchName( p_genre, psz_genre );
        if( !p_parent )
        {
            p_parent = playlist_NodeCreate( p_playlist, p_genre->pp_parents[0]->i_view, psz_genre,
                                            p_genre );
            playlist_CopyParents( p_genre, p_parent );
        }
        playlist_NodeAddItem( p_playlist, p_copy, p_parent->pp_parents[0]->i_view, p_parent, PLAYLIST_APPEND, PLAYLIST_END );
        playlist_CopyParents( p_parent, p_copy );
    }
}
