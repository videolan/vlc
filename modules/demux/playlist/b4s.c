/*****************************************************************************
 * b4s.c : B4S playlist format import
 *****************************************************************************
 * Copyright (C) 2005 the VideoLAN team
 * $Id$
 *
 * Authors: Sigmund Augdal Helberg <dnumgis@videolan.org>
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
    xml_t *p_xml;
    xml_reader_t *p_xml_reader;
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int Demux( demux_t *p_demux);
static int Control( demux_t *p_demux, int i_query, va_list args );
static char *GetNextToken(char *psz_cur_string);
static int IsWhitespace( char *psz_string );

/*****************************************************************************
 * Import_B4S: main import function
 *****************************************************************************/
int E_(Import_B4S)( vlc_object_t *p_this )
{
    demux_t *p_demux = (demux_t *)p_this;
    demux_sys_t *p_sys;

    char    *psz_ext;

    psz_ext = strrchr ( p_demux->psz_path, '.' );

    if( ( psz_ext && !strcasecmp( psz_ext, ".b4s") ) ||
        ( p_demux->psz_demux && !strcmp(p_demux->psz_demux, "b4s-open") ) )
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
        msg_Err( p_demux, "out of memory" );
        return VLC_ENOMEM;
    }
    p_sys->psz_prefix = E_(FindPrefix)( p_demux );
    p_sys->p_xml = NULL;
    p_sys->p_xml_reader = NULL;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Deactivate: frees unused data
 *****************************************************************************/
void E_(Close_B4S)( vlc_object_t *p_this )
{
    demux_t *p_demux = (demux_t *)p_this;
    demux_sys_t *p_sys = p_demux->p_sys;

    if( p_sys->psz_prefix ) free( p_sys->psz_prefix );
    if( p_sys->p_xml_reader ) xml_ReaderDelete( p_sys->p_xml, p_sys->p_xml_reader );
    if( p_sys->p_xml ) xml_Delete( p_sys->p_xml );
    free( p_sys );
}

static int Demux( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    int i_ret;

    xml_t *p_xml;
    xml_reader_t *p_xml_reader;
    char *psz_elname = NULL;
    int i_type;
    char *psz_mrl = NULL, *psz_name = NULL, *psz_genre = NULL;
    char *psz_now = NULL, *psz_listeners = NULL, *psz_bitrate = NULL;

    INIT_PLAYLIST_STUFF;

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
        vlc_object_release( p_playlist );
        return -1;
    }

    if( xml_ReaderNodeType( p_xml_reader ) != XML_READER_STARTELEM ||
        ( psz_elname = xml_ReaderName( p_xml_reader ) ) == NULL ||
        strcmp( psz_elname, "WinampXML" ) )
    {
        msg_Err( p_demux, "invalid root node %i, %s",
                 xml_ReaderNodeType( p_xml_reader ), psz_elname );
        if( psz_elname ) free( psz_elname );
        vlc_object_release( p_playlist );
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
                    p_input = input_ItemNewExt( p_playlist, psz_mrl, psz_name,
                                                0, NULL, -1 );
                    if( psz_now )
                        vlc_meta_SetNowPlaying( p_input->p_meta, psz_now );
                    if( psz_genre )
                        vlc_meta_SetGenre( p_input->p_meta, psz_genre );
                    if( psz_listeners )
                        msg_Err( p_playlist, "Unsupported meta listeners" );
                    if( psz_bitrate )
                        msg_Err( p_playlist, "Unsupported meta bitrate" );

                    playlist_AddWhereverNeeded( p_playlist, p_input, p_current,
                         p_item_in_category, (i_parent_id > 0 ) ? VLC_TRUE:
                                                 VLC_FALSE, PLAYLIST_APPEND );

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

    HANDLE_PLAY_AND_RELEASE;
    return VLC_SUCCESS;
}

static int Control( demux_t *p_demux, int i_query, va_list args )
{
    return VLC_EGENERIC;
}

/**
 * Get a in-string pointer to the start of the next token from a
 * string terminating the pointer returned by a previous call.
 *
 * \param psz_cur_string The string to search for the token from
 * \return a pointer to withing psz_cur_string, or NULL if no token
 * was found
 * \note The returned pointer may contain more than one
 * token, Run GetNextToken once more to terminate the token properly
 */
static char *GetNextToken(char *psz_cur_string) {
    while (*psz_cur_string && !isspace(*psz_cur_string))
        psz_cur_string++;
    if (!*psz_cur_string)
        return NULL;
    *psz_cur_string++ = '\0';
    while (*psz_cur_string && isspace(*psz_cur_string))
        psz_cur_string++;
    return psz_cur_string;
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
