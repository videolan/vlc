/*******************************************************************************
 * itml.c : iTunes Music Library import functions
 *******************************************************************************
 * Copyright (C) 2007 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Yoann Peronneau <yoann@videolan.org>
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
#include <vlc_xml.h>
#include <vlc_strings.h>
#include <vlc_url.h>

#include "itml.h"
#include "playlist.h"

struct demux_sys_t
{
    int i_ntracks;
};

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
    xml_reader_t *p_xml_reader;
    const char *node;

    input_item_t *p_current_input = GetCurrentItem(p_demux);
    p_demux->p_sys->i_ntracks = 0;

    /* create new xml parser from stream */
    p_xml_reader = xml_ReaderCreate( p_demux, p_demux->s );
    if( !p_xml_reader )
        goto end;

    /* locating the root node */
    int type;
    do
    {
        type = xml_ReaderNextNode( p_xml_reader, &node );
        if( type <= 0 )
        {
            msg_Err( p_demux, "can't read xml stream" );
            goto end;
        }
    }
    while( type != XML_READER_STARTELEM );

    /* checking root node name */
    if( strcmp( node, "plist" ) )
    {
        msg_Err( p_demux, "invalid root node <%s>", node );
        goto end;
    }

    input_item_node_t *p_subitems = input_item_node_Create( p_current_input );
    xml_elem_hnd_t pl_elements[] =
        { {"dict",    COMPLEX_CONTENT, {.cmplx = parse_plist_dict} } };
    parse_plist_node( p_demux, p_subitems, NULL, p_xml_reader, "plist",
                      pl_elements );
    input_item_node_PostAndDelete( p_subitems );

    vlc_gc_decref(p_current_input);

end:
    if( p_xml_reader )
        xml_ReaderDelete( p_xml_reader );

    /* Needed for correct operation of go back */
    return 0;
}

/**
 * \brief parse the root node of the playlist
 */
static bool parse_plist_node( demux_t *p_demux, input_item_node_t *p_input_node,
                              track_elem_t *p_track, xml_reader_t *p_xml_reader,
                              const char *psz_element,
                              xml_elem_hnd_t *p_handlers )
{
    VLC_UNUSED(p_track); VLC_UNUSED(psz_element);
    const char *attr, *value;
    bool b_version_found = false;

    /* read all playlist attributes */
    while( (attr = xml_ReaderNextAttr( p_xml_reader, &value )) != NULL )
    {
        /* attribute: version */
        if( !strcmp( attr, "version" ) )
        {
            b_version_found = true;
            if( strcmp( value, "1.0" ) )
                msg_Warn( p_demux, "unsupported iTunes Media Library version" );
        }
        /* unknown attribute */
        else
            msg_Warn( p_demux, "invalid <plist> attribute:\"%s\"", attr );
    }

    /* attribute version is mandatory !!! */
    if( !b_version_found )
        msg_Warn( p_demux, "<plist> requires \"version\" attribute" );

    return parse_dict( p_demux, p_input_node, NULL, p_xml_reader,
                       "plist", p_handlers );
}

/**
 * \brief parse a <dict>
 * \param COMPLEX_INTERFACE
 */
static bool parse_dict( demux_t *p_demux, input_item_node_t *p_input_node,
                        track_elem_t *p_track, xml_reader_t *p_xml_reader,
                        const char *psz_element, xml_elem_hnd_t *p_handlers )
{
    int i_node;
    const char *node;
    char *psz_value = NULL;
    char *psz_key = NULL;
    xml_elem_hnd_t *p_handler = NULL;
    bool b_ret = false;

    while( (i_node = xml_ReaderNextNode( p_xml_reader, &node )) > 0 )
    {
        switch( i_node )
        {
        /*  element start tag  */
        case XML_READER_STARTELEM:
            if( !*node )
            {
                msg_Err( p_demux, "invalid XML stream" );
                goto end;
            }
            /* choose handler */
            for( p_handler = p_handlers;
                     p_handler->name && strcmp( node, p_handler->name );
                     p_handler++ );
            if( !p_handler->name )
            {
                msg_Err( p_demux, "unexpected element <%s>", node );
                goto end;
            }
            /* complex content is parsed in a separate function */
            if( p_handler->type == COMPLEX_CONTENT )
            {
                if( p_handler->pf_handler.cmplx( p_demux, p_input_node, NULL,
                                                 p_xml_reader, p_handler->name,
                                                 NULL ) )
                {
                    p_handler = NULL;
                    FREE_ATT_KEY();
                }
                else
                    goto end;
            }
            break;

        /* simple element content */
        case XML_READER_TEXT:
            free( psz_value );
            psz_value = strdup( node );
            if( unlikely(!psz_value) )
                goto end;
            break;

        /* element end tag */
        case XML_READER_ENDELEM:
            /* leave if the current parent node <track> is terminated */
            if( !strcmp( node, psz_element ) )
            {
                b_ret = true;
                goto end;
            }
            /* there MUST have been a start tag for that element name */
            if( !p_handler || !p_handler->name
                || strcmp( p_handler->name, node ) )
            {
                msg_Err( p_demux, "there's no open element left for <%s>",
                         node );
                goto end;
            }
            /* special case: key */
            if( !strcmp( p_handler->name, "key" ) )
            {
                free( psz_key );
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
        }
    }
    msg_Err( p_demux, "unexpected end of XML data" );

end:
    free( psz_value );
    free( psz_key );
    return b_ret;
}

static bool parse_plist_dict( demux_t *p_demux, input_item_node_t *p_input_node,
                              track_elem_t *p_track, xml_reader_t *p_xml_reader,
                              const char *psz_element,
                              xml_elem_hnd_t *p_handlers )
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

    return parse_dict( p_demux, p_input_node, NULL, p_xml_reader,
                       "dict", pl_elements );
}

static bool parse_tracks_dict( demux_t *p_demux, input_item_node_t *p_input_node,
                               track_elem_t *p_track, xml_reader_t *p_xml_reader,
                               const char *psz_element,
                               xml_elem_hnd_t *p_handlers )
{
    VLC_UNUSED(p_track); VLC_UNUSED(psz_element); VLC_UNUSED(p_handlers);
    xml_elem_hnd_t tracks_elements[] =
        { {"dict",    COMPLEX_CONTENT, {.cmplx = parse_track_dict} },
          {"key",     SIMPLE_CONTENT,  {NULL} },
          {NULL,      UNKNOWN_CONTENT, {NULL} }
        };

    parse_dict( p_demux, p_input_node, NULL, p_xml_reader,
                "dict", tracks_elements );

    msg_Info( p_demux, "added %i tracks successfully",
              p_demux->p_sys->i_ntracks );

    return true;
}

static bool parse_track_dict( demux_t *p_demux, input_item_node_t *p_input_node,
                              track_elem_t *p_track, xml_reader_t *p_xml_reader,
                              const char *psz_element,
                              xml_elem_hnd_t *p_handlers )
{
    VLC_UNUSED(psz_element); VLC_UNUSED(p_handlers);
    input_item_t *p_new_input = NULL;
    int i_ret;
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

    i_ret = parse_dict( p_demux, p_input_node, p_track,
                        p_xml_reader, "dict", track_elements );

    msg_Dbg( p_demux, "name: %s, artist: %s, album: %s, genre: %s, trackNum: %s, location: %s",
             p_track->name, p_track->artist, p_track->album, p_track->genre, p_track->trackNum, p_track->location );

    if( !p_track->location )
    {
        msg_Err( p_demux, "Track needs Location" );
        free_track( p_track );
        return false;
    }

    msg_Info( p_demux, "Adding '%s'", p_track->location );
    p_new_input = input_item_New( p_track->location, NULL );
    input_item_node_AppendItem( p_input_node, p_new_input );

    /* add meta info */
    add_meta( p_new_input, p_track );
    vlc_gc_decref( p_new_input );

    p_demux->p_sys->i_ntracks++;

    free_track( p_track );
    return i_ret;
}

static track_elem_t *new_track()
{
    track_elem_t *p_track;
    p_track = malloc( sizeof( track_elem_t ) );
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

    FREENULL( p_track->name );
    FREENULL( p_track->artist );
    FREENULL( p_track->album );
    FREENULL( p_track->genre );
    FREENULL( p_track->trackNum );
    FREENULL( p_track->location );
    p_track->duration = 0;
    free( p_track );
}

static bool save_data( track_elem_t *p_track, const char *psz_name,
                       char *psz_value)
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
#undef SAVE_INFO
    return true;
}

/**
 * \brief handles the supported <track> sub-elements
 */
static bool add_meta( input_item_t *p_input_item, track_elem_t *p_track )
{
    /* exit if setting is impossible */
    if( !p_input_item || !p_track )
        return false;

#define SET_INFO( type, prop ) \
    if( p_track->prop ) {input_item_Set##type( p_input_item, p_track->prop );}
    SET_INFO( Title, name )
    SET_INFO( Artist, artist )
    SET_INFO( Album, album )
    SET_INFO( Genre, genre )
    SET_INFO( TrackNum, trackNum )
    SET_INFO( Duration, duration )
#undef SET_INFO
    return true;
}

/**
 * \brief skips complex element content that we can't manage
 */
static bool skip_element( demux_t *p_demux, input_item_node_t *p_input_node,
                          track_elem_t *p_track, xml_reader_t *p_xml_reader,
                          const char *psz_element, xml_elem_hnd_t *p_handlers )
{
    VLC_UNUSED(p_demux); VLC_UNUSED(p_input_node);
    VLC_UNUSED(p_track); VLC_UNUSED(p_handlers);
    const char *node;
    int type;

    while( (type = xml_ReaderNextNode( p_xml_reader, &node )) > 0 )
        if( type == XML_READER_ENDELEM && !strcmp( psz_element, node ) )
            return true;
    return false;
}
