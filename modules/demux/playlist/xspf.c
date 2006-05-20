/******************************************************************************
 * Copyright (C) 2006 Daniel Stränger <vlc at schmaller dot de>
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
 * \file modules/demux/playlist/xspf.c
 * \brief XSPF playlist import functions
 */

#include <vlc/vlc.h>
#include <vlc/input.h>
#include <vlc/intf.h>

#include "playlist.h"
#include "vlc_xml.h"
#include "vlc_strings.h"
#include "xspf.h"

/**
 * \brief XSPF submodule initialization function
 */
int E_(xspf_import_Activate)( vlc_object_t *p_this )
{
    demux_t *p_demux = (demux_t *)p_this;
    char    *psz_ext;

    psz_ext = strrchr ( p_demux->psz_path, '.' );

    if( ( psz_ext && !strcasecmp( psz_ext, ".xspf") ) ||
        ( p_demux->psz_demux && !strcmp(p_demux->psz_demux, "xspf-open") ) )
    {
        ;
    }
    else
    {
        return VLC_EGENERIC;
    }
    msg_Dbg( p_demux, "using xspf playlist import");

    p_demux->pf_control = xspf_import_Control;
    p_demux->pf_demux = xspf_import_Demux;

    return VLC_SUCCESS;
}

/**
 * \brief demuxer function for XSPF parsing
 */
int xspf_import_Demux( demux_t *p_demux )
{
#if 0
    playlist_t *p_playlist = NULL;
    playlist_item_t *p_current = NULL;

    vlc_bool_t b_play;
    int i_ret = VLC_SUCCESS;

    xml_t *p_xml = NULL;
    xml_reader_t *p_xml_reader = NULL;
    char *psz_name = NULL;

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

    /* start with parsing the root node */
    if ( i_ret == VLC_SUCCESS )
        if ( xml_ReaderRead( p_xml_reader ) != 1 )
        {
            msg_Err( p_demux, "can't read xml stream" );
            i_ret = VLC_EGENERIC;
        }
    /* checking root nody type */
    if ( i_ret == VLC_SUCCESS )
        if( xml_ReaderNodeType( p_xml_reader ) != XML_READER_STARTELEM )
        {
            msg_Err( p_demux, "invalid root node type: %i", xml_ReaderNodeType( p_xml_reader ) );
            i_ret = VLC_EGENERIC;
        }
    /* checking root node name */
    if ( i_ret == VLC_SUCCESS )
        psz_name = xml_ReaderName( p_xml_reader );
    if ( !psz_name || strcmp( psz_name, "playlist" ) )
    {
        msg_Err( p_demux, "invalid root node name: %s", psz_name );
        i_ret = VLC_EGENERIC;
    }
    FREE_NAME();

    /* get the playlist ... */
    if ( i_ret == VLC_SUCCESS )
    {
        p_playlist = (playlist_t *) vlc_object_find( p_demux, VLC_OBJECT_PLAYLIST, FIND_PARENT );
        if( !p_playlist )
        {
            msg_Err( p_demux, "can't find playlist" );
            i_ret = VLC_ENOOBJ;
        }
    }
    /* ... and its current item (to convert it to a node) */
    if ( i_ret == VLC_SUCCESS )
    {
        b_play = E_(FindItem)( p_demux, p_playlist, &p_current );
        playlist_ItemToNode( p_playlist, p_current );
        p_current->input.i_type = ITEM_TYPE_PLAYLIST;
        /* parse the playlist node */
        i_ret = parse_playlist_node( p_demux, p_playlist, p_current,
                                     p_xml_reader, "playlist" );
        /* true/false - success/egeneric mapping */
        i_ret = ( i_ret==VLC_TRUE ? VLC_SUCCESS : VLC_EGENERIC );

        if( b_play )
        {
            playlist_Control( p_playlist, PLAYLIST_VIEWPLAY,
                              p_playlist->status.i_view,
                              p_playlist->status.p_item, NULL );
        }
    }

    if ( p_playlist )
        vlc_object_release( p_playlist );
    if ( p_xml_reader )
        xml_ReaderDelete( p_xml, p_xml_reader );
    if ( p_xml )
        xml_Delete( p_xml );

    return i_ret;
#endif
    return 0;
}

/** \brief dummy function for demux callback interface */
int xspf_import_Control( demux_t *p_demux, int i_query, va_list args )
{
    return VLC_EGENERIC;
}
#if 0
/**
 * \brief parse the root node of a XSPF playlist
 * \param p_demux demuxer instance
 * \param p_playlist playlist instance
 * \param p_item current playlist node
 * \param p_xml_reader xml reader instance
 * \param psz_element name of element to parse
 */
static vlc_bool_t parse_playlist_node COMPLEX_INTERFACE
{
    char *psz_name=NULL;
    char *psz_value=NULL;
    vlc_bool_t b_version_found = VLC_FALSE;
    int i_node;
    xml_elem_hnd_t *p_handler=NULL;

    xml_elem_hnd_t pl_elements[] =
        { {"title",        SIMPLE_CONTENT,  {.smpl = set_item_info} },
          {"creator",      SIMPLE_CONTENT,  {.smpl = set_item_info} },
          {"annotation",   SIMPLE_CONTENT,  {NULL} },
          {"info",         SIMPLE_CONTENT,  {NULL} },
          {"location",     SIMPLE_CONTENT,  {NULL} },
          {"identifier",   SIMPLE_CONTENT,  {NULL} },
          {"image",        SIMPLE_CONTENT,  {NULL} },
          {"date",         SIMPLE_CONTENT,  {NULL} },
          {"license",      SIMPLE_CONTENT,  {NULL} },
          {"attribution",  COMPLEX_CONTENT, {.cmplx = skip_element} },
          {"link",         SIMPLE_CONTENT,  {NULL} },
          {"meta",         SIMPLE_CONTENT,  {NULL} },
          {"extension",    COMPLEX_CONTENT, {.cmplx = skip_element} },
          {"trackList",    COMPLEX_CONTENT, {.cmplx = parse_tracklist_node} },
          {NULL,           UNKNOWN_CONTENT, {NULL} }
        };

    /* read all playlist attributes */
    while ( xml_ReaderNextAttr( p_xml_reader ) == VLC_SUCCESS )
    {
        psz_name = xml_ReaderName ( p_xml_reader );
        psz_value = xml_ReaderValue ( p_xml_reader );
        if ( !psz_name || !psz_value )
        {
            msg_Err( p_demux, "invalid xml stream @ <playlist>" );
            FREE_ATT();
            return VLC_FALSE;
        }
        /* attribute: version */
        if ( !strcmp( psz_name, "version" ) )
        {
            b_version_found = VLC_TRUE;
            if ( strcmp( psz_value, "0" ) && strcmp( psz_value, "1" ) )
                msg_Warn( p_demux, "unsupported XSPF version" );
        }
        /* attribute: xmlns */
        else if ( !strcmp ( psz_name, "xmlns" ) )
            ;
        /* unknown attribute */
        else
            msg_Warn( p_demux, "invalid <playlist> attribute:\"%s\"", psz_name);

        FREE_ATT();
    }
    /* attribute version is mandatory !!! */
    if ( !b_version_found )
        msg_Warn( p_demux, "<playlist> requires \"version\" attribute" );

    /* parse the child elements - we only take care of <trackList> */
    while ( xml_ReaderRead( p_xml_reader ) == 1 )
    {
        i_node = xml_ReaderNodeType( p_xml_reader );
        switch ( i_node )
        {
            case XML_READER_NONE:
                break;
            case XML_READER_STARTELEM:
                /*  element start tag  */
                psz_name = xml_ReaderName( p_xml_reader );
                if ( !psz_name || !*psz_name )
                {
                    msg_Err( p_demux, "invalid xml stream" );
                    FREE_ATT();
                    return VLC_FALSE;
                }
                /* choose handler */
                for( p_handler = pl_elements;
                     p_handler->name && strcmp( psz_name, p_handler->name );
                     p_handler++ );
                if ( !p_handler->name )
                {
                    msg_Err( p_demux, "unexpected element <%s>", psz_name );
                    FREE_ATT();
                    return VLC_FALSE;
                }
                FREE_NAME();
                /* complex content is parsed in a separate function */
                if ( p_handler->type == COMPLEX_CONTENT )
                {
                    if ( p_handler->pf_handler.cmplx( p_demux,
                                                      p_playlist,
                                                      p_item,
                                                      p_xml_reader,
                                                      p_handler->name ) )
                    {
                        p_handler = NULL;
                        FREE_ATT();
                    }
                    else
                    {
                        FREE_ATT();
                        return VLC_FALSE;
                    }
                }
                break;

            case XML_READER_TEXT:
                /* simple element content */
                FREE_ATT();
                psz_value = xml_ReaderValue( p_xml_reader );
                if ( !psz_value )
                {
                    msg_Err( p_demux, "invalid xml stream" );
                    FREE_ATT();
                    return VLC_FALSE;
                }
                break;

            case XML_READER_ENDELEM:
                /* element end tag */
                psz_name = xml_ReaderName( p_xml_reader );
                if ( !psz_name )
                {
                    msg_Err( p_demux, "invalid xml stream" );
                    FREE_ATT();
                    return VLC_FALSE;
                }
                /* leave if the current parent node <playlist> is terminated */
                if ( !strcmp( psz_name, psz_element ) )
                {
                    FREE_ATT();
                    return VLC_TRUE;
                }
                /* there MUST have been a start tag for that element name */
                if ( !p_handler || !p_handler->name
                     || strcmp( p_handler->name, psz_name ))
                {
                    msg_Err( p_demux, "there's no open element left for <%s>",
                             psz_name );
                    FREE_ATT();
                    return VLC_FALSE;
                }

                if ( p_handler->pf_handler.smpl )
                {
                    p_handler->pf_handler.smpl( p_item, p_handler->name,
                                                psz_value );
                }
                FREE_ATT();
                p_handler = NULL;
                break;

            default:
                /* unknown/unexpected xml node */
                msg_Err( p_demux, "unexpected xml node %i", i_node );
                FREE_ATT();
                return VLC_FALSE;
        }
        FREE_NAME();
    }
    return VLC_FALSE;
}

/**
 * \brief parses the tracklist node which only may contain <track>s
 */
static vlc_bool_t parse_tracklist_node COMPLEX_INTERFACE
{
    char *psz_name=NULL;
    int i_node;
    int i_ntracks = 0;

    /* now parse the <track>s */
    while ( xml_ReaderRead( p_xml_reader ) == 1 )
    {
        i_node = xml_ReaderNodeType( p_xml_reader );
        if ( i_node == XML_READER_STARTELEM )
        {
            psz_name = xml_ReaderName( p_xml_reader );
            if ( !psz_name )
            {
                msg_Err( p_demux, "unexpected end of xml data" );
                FREE_NAME();
                return VLC_FALSE;
            }
            if ( strcmp( psz_name, "track") )
            {
                msg_Err( p_demux, "unexpected child of <trackList>: <%s>",
                         psz_name );
                FREE_NAME();
                return VLC_FALSE;
            }
            FREE_NAME();

            /* parse the track data in a separate function */
            if ( parse_track_node( p_demux, p_playlist, p_item, p_xml_reader,
                                   "track" ) == VLC_TRUE )
                i_ntracks++;
        }
        else if ( i_node == XML_READER_ENDELEM )
            break;
    }

    /* the <trackList> has to be terminated */
    if ( xml_ReaderNodeType( p_xml_reader ) != XML_READER_ENDELEM )
    {
        msg_Err( p_demux, "there's a missing </trackList>" );
        FREE_NAME();
        return VLC_FALSE;
    }
    psz_name = xml_ReaderName( p_xml_reader );
    if ( !psz_name || strcmp( psz_name, "trackList" ) )
    {
        msg_Err( p_demux, "expected: </trackList>, found: </%s>", psz_name );
        FREE_NAME();
        return VLC_FALSE;
    }
    FREE_NAME();

    msg_Dbg( p_demux, "parsed %i tracks successfully", i_ntracks );

    return VLC_TRUE;
}

/**
 * \brief parse one track element
 * \param COMPLEX_INTERFACE
 */
static vlc_bool_t parse_track_node COMPLEX_INTERFACE
{
    playlist_item_t *p_new=NULL;
    int i_node;
    char *psz_name=NULL;
    char *psz_value=NULL;
    xml_elem_hnd_t *p_handler=NULL;

    xml_elem_hnd_t track_elements[] =
        { {"location",     SIMPLE_CONTENT,  {NULL} },
          {"identifier",   SIMPLE_CONTENT,  {NULL} },
          {"title",        SIMPLE_CONTENT,  {.smpl = set_item_info} },
          {"creator",      SIMPLE_CONTENT,  {.smpl = set_item_info} },
          {"annotation",   SIMPLE_CONTENT,  {NULL} },
          {"info",         SIMPLE_CONTENT,  {NULL} },
          {"image",        SIMPLE_CONTENT,  {NULL} },
          {"album",        SIMPLE_CONTENT,  {.smpl = set_item_info} },
          {"trackNum",     SIMPLE_CONTENT,  {.smpl = set_item_info} },
          {"duration",     SIMPLE_CONTENT,  {.smpl = set_item_info} },
          {"link",         SIMPLE_CONTENT,  {NULL} },
          {"meta",         SIMPLE_CONTENT,  {NULL} },
          {"extension",    COMPLEX_CONTENT, {.cmplx = skip_element} },
          {NULL,           UNKNOWN_CONTENT, {NULL} }
        };

    while ( xml_ReaderRead( p_xml_reader ) == 1 )
    {
        i_node = xml_ReaderNodeType( p_xml_reader );
        switch ( i_node )
        {
            case XML_READER_NONE:
                break;

            case XML_READER_STARTELEM:
                /*  element start tag  */
                psz_name = xml_ReaderName( p_xml_reader );
                if ( !psz_name || !*psz_name )
                {
                    msg_Err( p_demux, "invalid xml stream" );
                    FREE_ATT();
                    return VLC_FALSE;
                }
                /* choose handler */
                for( p_handler = track_elements;
                     p_handler->name && strcmp( psz_name, p_handler->name );
                     p_handler++ );
                if ( !p_handler->name )
                {
                    msg_Err( p_demux, "unexpected element <%s>", psz_name );
                    FREE_ATT();
                    return VLC_FALSE;
                }
                FREE_NAME();
                /* complex content is parsed in a separate function */
                if ( p_handler->type == COMPLEX_CONTENT )
                {
                    if ( !p_new )
                    {
                        msg_Err( p_demux,
                                 "at <%s> level no new item has been allocated",
                                 p_handler->name );
                        FREE_ATT();
                        return VLC_FALSE;
                    }
                    if ( p_handler->pf_handler.cmplx( p_demux,
                                                      p_playlist,
                                                      p_new,
                                                      p_xml_reader,
                                                      p_handler->name ) )
                    {
                        p_handler = NULL;
                        FREE_ATT();
                    }
                    else
                    {
                        FREE_ATT();
                        return VLC_FALSE;
                    }
                }
                break;

            case XML_READER_TEXT:
                /* simple element content */
                FREE_ATT();
                psz_value = xml_ReaderValue( p_xml_reader );
                if ( !psz_value )
                {
                    msg_Err( p_demux, "invalid xml stream" );
                    FREE_ATT();
                    return VLC_FALSE;
                }
                break;

            case XML_READER_ENDELEM:
                /* element end tag */
                psz_name = xml_ReaderName( p_xml_reader );
                if ( !psz_name )
                {
                    msg_Err( p_demux, "invalid xml stream" );
                    FREE_ATT();
                    return VLC_FALSE;
                }
                /* leave if the current parent node <track> is terminated */
                if ( !strcmp( psz_name, psz_element ) )
                {
                    FREE_ATT();
                    return VLC_TRUE;
                }
                /* there MUST have been a start tag for that element name */
                if ( !p_handler || !p_handler->name
                     || strcmp( p_handler->name, psz_name ))
                {
                    msg_Err( p_demux, "there's no open element left for <%s>",
                             psz_name );
                    FREE_ATT();
                    return VLC_FALSE;
                }

                /* special case: location */
                if ( !strcmp( p_handler->name, "location" ) )
                {
                    /* there MUST NOT be an item */
                    if ( p_new )
                    {
                        msg_Err( p_demux,
                                 "a new item has just been created <%s>",
                                 psz_name );
                        FREE_ATT();
                        return VLC_FALSE;
                    }
                    /* create it now */
                    if ( insert_new_item( p_playlist, p_item,
                                          &p_new, psz_value ) )
                    {
                        FREE_ATT();
                        p_handler = NULL;
                    }
                    else
                    {
                        FREE_ATT();
                        return VLC_FALSE;
                    }
                }
                else
                {
                    /* there MUST be an item */
                    if ( !p_new )
                    {
                        msg_Err( p_demux,
                                 "an item hasn't been created yet <%s>",
                                 psz_name );
                        FREE_ATT();
                        return VLC_FALSE;
                    }
                    if ( p_handler->pf_handler.smpl )
                    {
                        p_handler->pf_handler.smpl( p_new, p_handler->name,
                                                    psz_value );
                        FREE_ATT();
                    }
                }
                FREE_ATT();
                p_handler = NULL;
                break;

            default:
                /* unknown/unexpected xml node */
                msg_Err( p_demux, "unexpected xml node %i", i_node );
                FREE_ATT();
                return VLC_FALSE;
        }
        FREE_NAME();
    }
    msg_Err( p_demux, "unexpected end of xml data" );
    FREE_ATT();
    return VLC_FALSE;
}

/**
 * \brief handles the supported <track> sub-elements
 */
static vlc_bool_t set_item_info SIMPLE_INTERFACE
{
    /* exit if setting is impossible */
    if ( !psz_name || !psz_value || !p_item )
        return VLC_FALSE;

    /* re-convert xml special characters inside psz_value */
    resolve_xml_special_chars ( psz_value );

    /* handle each info element in a separate "if" clause */
    if ( !strcmp( psz_name, "title" ) )
    {
        if ( playlist_ItemSetName ( p_item, (char *)psz_value ) == VLC_SUCCESS )
            return VLC_TRUE;
        return VLC_FALSE;
    }
    else if ( !strcmp( psz_name, "creator" ) )
    {
        if ( vlc_input_item_AddInfo( &(p_item->input),
                                     _(VLC_META_INFO_CAT), _(VLC_META_ARTIST),
                                     "%s", psz_value ) == VLC_SUCCESS )
            return VLC_TRUE;
        return VLC_FALSE;

    }
    else if ( !strcmp( psz_name, "album" ) )
    {
        if ( vlc_input_item_AddInfo( &(p_item->input),
                                     _(VLC_META_INFO_CAT),
                                     _(VLC_META_COLLECTION),
                                     "%s", psz_value ) == VLC_SUCCESS )
            return VLC_TRUE;
        return VLC_FALSE;

    } else if ( !strcmp( psz_name, "trackNum" ) )
    {
        long i_num = atol( psz_value );
        if ( i_num > 0
             && vlc_input_item_AddInfo( &(p_item->input),
                                         _(VLC_META_INFO_CAT),
                                         _(VLC_META_SEQ_NUM),
                                         "%s", psz_value ) == VLC_SUCCESS )
                return VLC_TRUE;
        return VLC_FALSE;

    } else if ( !strcmp( psz_name, "duration" ) )
    {
        long i_num = atol( psz_value );
        if ( i_num > 0
             && playlist_ItemSetDuration( p_item, i_num*1000 ) == VLC_SUCCESS )
                return VLC_TRUE;
        return VLC_FALSE;

    }

    return VLC_TRUE;
}

/**
 * \brief skips complex element content that we can't manage
 */
static vlc_bool_t skip_element COMPLEX_INTERFACE
{
    char *psz_endname;

    while ( xml_ReaderRead( p_xml_reader ) == 1 )
    {
        if ( xml_ReaderNodeType( p_xml_reader ) == XML_READER_ENDELEM )
        {
            psz_endname = xml_ReaderName( p_xml_reader );
            if ( !psz_endname )
                return VLC_FALSE;
            if ( !strcmp( psz_element, psz_endname ) )
            {
                free( psz_endname );
                return VLC_TRUE;
            }
            else
                free( psz_endname );
        }
    }
    return VLC_FALSE;
}

/**
 * \brief creates a new playlist item from the given mrl
 */
static vlc_bool_t insert_new_item( playlist_t *p_pl, playlist_item_t *p_cur,
                                   playlist_item_t **pp_new, char *psz_location )
{
    char *psz_uri=NULL;
    psz_uri = unescape_URI_duplicate( psz_location );

    if ( psz_uri )
    {
        *pp_new = playlist_ItemNew( p_pl, psz_uri, NULL );
        free( psz_uri );
        psz_uri = NULL;
    }

    if ( !*pp_new )
        return VLC_FALSE;

    playlist_NodeAddItem( p_pl,  *pp_new,         p_cur->pp_parents[0]->i_view,
                          p_cur, PLAYLIST_APPEND, PLAYLIST_END );

    playlist_CopyParents( p_cur, *pp_new );

    vlc_input_item_CopyOptions( &p_cur->input, &((*pp_new)->input) );

    return VLC_TRUE;
}
#endif
