/*****************************************************************************
 * native.c : Native playlist format import
 *****************************************************************************
 * Copyright (C) 2004 VideoLAN
 * $Id: old.c 6961 2004-03-05 17:34:23Z sam $
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
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
#include <vlc/intf.h>

#include <errno.h>                                                 /* ENOMEM */

#include <libxml/xmlreader.h>
#include <libxml/encoding.h>

#define HEADER "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<playlist"

#define CHUNK_SIZE 256

typedef struct attribute_s {
    xmlChar *pxsz_attrname;
    xmlChar *pxsz_attrvalue;
} attribute_t;


struct demux_sys_t
{
    /* Playlist elements */
    playlist_t      *p_playlist;
    playlist_item_t *p_item;
    char            *psz_category;

    /* XML Parsing elements */
    xmlChar      *pxsz_elemname;
    xmlChar      *pxsz_attrname;
    xmlChar      *pxsz_attrvalue;
    int           i_attributes;
    attribute_t **pp_attributes;
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
int Import_Native ( vlc_object_t * );
int HandleBeginningElement( demux_t *p_demux );
int HandleEndingElement( demux_t *p_demux );
char *SearchAttribute( int i_attribute,attribute_t **pp_attributes,
                       char *psz_name );
static int Demux( demux_t *p_demux);
static int Control( demux_t *p_demux, int i_query, va_list args );

/*****************************************************************************
 * Import_Native : main import function
 *****************************************************************************/
int Import_Native(vlc_object_t *p_this )
{
    demux_t *p_demux = (demux_t *)p_this;

    demux_sys_t *p_sys = (demux_sys_t *)malloc( sizeof( demux_sys_t ) );

    p_demux->p_sys = p_sys;

    uint8_t *p_peek;

    if( stream_Peek( p_demux->s, &p_peek, 8 ) < 8 )
    {
        msg_Err( p_demux, "cannot peek" );
        return VLC_EGENERIC;
    }

    if( strncmp( p_peek, HEADER , 40 ) )
    {
        msg_Warn(p_demux, "native import module discarded: invalid file");
        return VLC_EGENERIC;
    }
    msg_Dbg( p_demux, "found valid native playlist file");

    p_sys->p_playlist = (playlist_t*)vlc_object_find( p_demux,
                         VLC_OBJECT_PLAYLIST, FIND_PARENT );

    if( !p_sys->p_playlist )
    {
        msg_Err( p_demux, "cannot attach playlist" );
        return VLC_EGENERIC;
    }
    p_sys->p_playlist->pp_items[p_sys->p_playlist->i_index]->b_autodeletion =
                                                        VLC_TRUE;

    p_sys->p_item = NULL;
    p_sys->psz_category = NULL;

    p_sys->pp_attributes = NULL;
    p_sys->i_attributes = 0;

    p_demux->pf_control = Control;
    p_demux->pf_demux = Demux;

    return VLC_SUCCESS;
}

void Close_Native( vlc_object_t *p_this )
{
    demux_t *p_demux = (demux_t *)p_this;
    demux_sys_t *p_sys = p_demux->p_sys;

    if( p_sys )
    {
        if( p_sys->psz_category != NULL ) free( p_sys->psz_category );
        if( p_sys->pxsz_elemname != NULL) free(  p_sys->pxsz_elemname );

        if( p_sys->p_playlist )
        {
            vlc_object_release( p_sys->p_playlist );
        }
        free( p_sys );
    }
}

static int Demux( demux_t *p_demux)
{
    char *psz_file=(char *)malloc( sizeof( char ) );
    int i_size = 0;
    int i_ret, i;
    xmlTextReaderPtr p_reader;

    uint8_t p_peek[CHUNK_SIZE];

    demux_sys_t *p_sys = p_demux->p_sys;

    msg_Dbg( p_demux, "building playlist string" );

    /* Build the string containing the playlist */
    while( 1 )
    {
        i_ret = stream_Read( p_demux->s, p_peek, CHUNK_SIZE );
        i_size+= i_ret;
        psz_file = (char *)realloc( psz_file, i_size + 1242 );
        memcpy( psz_file + i_size - i_ret, p_peek, CHUNK_SIZE );
        if( i_ret < CHUNK_SIZE )
        {
            break;
        }
    }
    psz_file[i_size]=0;

    msg_Dbg( p_demux, "parsing playlist");
    /* Create the XML parser */
    p_reader = xmlReaderForMemory( psz_file, i_size, NULL, "UTF-8", 0 );

    if( !p_reader )
    {
        msg_Warn( p_demux, "Unable to parse");
                return VLC_EGENERIC;
    }

    i_ret = xmlTextReaderRead( p_reader );

    /* Start the main parsing loop */
    while( i_ret == 1)
    {
        int i_type = xmlTextReaderNodeType( p_reader );
        switch( i_type )
        {
            /* -1 = error */
            case -1:
                return VLC_EGENERIC;
                break;

            /* 1 = Beginning of an element */
            case 1:
            {
                /* Clean attributes of previous element */
                for(i = 0 ; i < p_sys->i_attributes ; i++ )
                {
                    if( p_sys->pp_attributes[i]->pxsz_attrname )
                    {
                        free( p_sys->pp_attributes[i]->pxsz_attrname );
                    }
                    if( p_sys->pp_attributes[i]->pxsz_attrvalue )
                    {
                        free( p_sys->pp_attributes[i]->pxsz_attrvalue );
                    }
                    free( p_sys->pp_attributes[i] );
                }
                p_sys->i_attributes = 0;
                p_sys->pp_attributes = NULL;

                p_sys->pxsz_elemname = xmlTextReaderName( p_reader );
                if( !p_sys->pxsz_elemname )
                {
                    return VLC_EGENERIC;
                }
                /* Get all attributes */
                while( xmlTextReaderMoveToNextAttribute( p_reader ) == 1 )
                {
                    attribute_t *p_attribute =
                            (attribute_t *)malloc( sizeof(attribute_t) );
                    p_attribute->pxsz_attrname  = xmlTextReaderName( p_reader );
                    p_attribute->pxsz_attrvalue = xmlTextReaderValue( p_reader );

                    if( !p_attribute->pxsz_attrname ||
                        !p_attribute->pxsz_attrvalue )
                    {
                        return VLC_EGENERIC;
                    }
                    INSERT_ELEM( p_sys->pp_attributes,
                                 p_sys->i_attributes,
                                 p_sys->i_attributes,
                                 p_attribute );
                }
                HandleBeginningElement( p_demux );
                break;
            }

            /* 15 = End of an element */
            case 15:
                p_sys->pxsz_elemname = xmlTextReaderName( p_reader );
                if( !p_sys->pxsz_elemname )
                {
                    return VLC_EGENERIC;
                }
                HandleEndingElement( p_demux );
                break;
        }
        i_ret= xmlTextReaderRead( p_reader );
    }
    if( p_sys->p_item )
    {
        /* We still have an item. Add it */
        playlist_AddItem( p_sys->p_playlist, p_sys->p_item,
                          PLAYLIST_APPEND, PLAYLIST_END );
        p_sys->p_item = NULL;
    }

    p_demux->b_die = VLC_TRUE;
    return VLC_SUCCESS;
}

static int Control( demux_t *p_demux, int i_query, va_list args )
{
    return VLC_EGENERIC;
}

int HandleBeginningElement( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    if( !strcmp( p_sys->pxsz_elemname, "item" ) )
    {
        char *psz_uri,*psz_name,*psz_duration;
        if( p_sys->p_item )
        {
            /* We already have an item. Assume that it is because
             * we have <item name="" uri="" /> */
            playlist_AddItem( p_sys->p_playlist, p_sys->p_item,
                              PLAYLIST_APPEND, PLAYLIST_END );
            p_sys->p_item = NULL;
        }
        psz_uri  = SearchAttribute( p_sys->i_attributes,
                                    p_sys->pp_attributes, "uri" );
        psz_name = SearchAttribute( p_sys->i_attributes,
                                    p_sys->pp_attributes, "name" );
        psz_duration = SearchAttribute( p_sys->i_attributes,
                                        p_sys->pp_attributes, "duration");
        if( !psz_uri )
        {
            return VLC_EGENERIC;
        }
        p_sys->p_item = playlist_ItemNew( p_sys->p_playlist ,
                                          psz_uri, psz_name );
    }
    else if( !strcmp( p_sys->pxsz_elemname, "category" ) )
    {
        if( !p_sys->p_item)
        {
            msg_Warn( p_demux, "trying to set category without item" );
            return VLC_EGENERIC;
        }
        p_sys->psz_category = SearchAttribute( p_sys->i_attributes,
                                               p_sys->pp_attributes,
                                               "name" );
    }
    else if( !strcmp( p_sys->pxsz_elemname, "info" ) )
    {
        char *psz_name, *psz_value;
        if( !p_sys->psz_category || !p_sys->p_item )
        {
            msg_Warn( p_demux, "trying to set info without item or category" );
        }
        psz_name  = SearchAttribute( p_sys->i_attributes,
                                        p_sys->pp_attributes,
                                        "name" );
        psz_value = SearchAttribute( p_sys->i_attributes,
                                        p_sys->pp_attributes,
                                        "value" );
        playlist_ItemAddInfo( p_sys->p_item, p_sys->psz_category,
                              psz_name, psz_value );
    }
    else if( !strcmp( p_sys->pxsz_elemname, "option" ) )
    {
        char *psz_name;
        if( !p_sys->p_item )
        {
            msg_Warn( p_demux, "trying to set category without item" );
            return VLC_EGENERIC;
        }
        psz_name = SearchAttribute( p_sys->i_attributes,
                        p_sys->pp_attributes,
                        "name" );
        if( psz_name )
        {
            playlist_ItemAddOption( p_sys->p_item, psz_name );
        }
    }
    return VLC_SUCCESS;
}

int HandleEndingElement( demux_t *p_demux )
{
    int i;
    demux_sys_t *p_sys = p_demux->p_sys;

    if( !strcmp( p_sys->pxsz_elemname, "item" ) )
    {
        /* Add the item to the playlist */
        if( p_sys->p_item )
        {
            playlist_AddItem( p_sys->p_playlist, p_sys->p_item,
                            PLAYLIST_APPEND, PLAYLIST_END );
            p_sys->p_item = NULL;
        }
    }
    else if (!strcmp( p_sys->pxsz_elemname, "category" ) )
    {
        p_sys->psz_category = NULL;
    }

    /* Clear attribute list */
    for(i = 0 ; i < p_sys->i_attributes ; i++ )
    {
        if( p_sys->pp_attributes[i]->pxsz_attrname )
        {
            free( p_sys->pp_attributes[i]->pxsz_attrname );
        }
        if( p_sys->pp_attributes[i]->pxsz_attrvalue )
        {
            free( p_sys->pp_attributes[i]->pxsz_attrvalue );
        }
        free( p_sys->pp_attributes[i] );
    }
    p_sys->i_attributes = 0;
    return VLC_SUCCESS;
}

char *SearchAttribute( int i_attribute,attribute_t **pp_attributes, char *psz_name )
{
    int i, i_size, i_iso_size, i_read;
    char *psz_iso, *psz_value;
    for( i = 0 ; i < i_attribute ; i++ )
    {
        if( !strcmp( (char *)pp_attributes[i]->pxsz_attrname , psz_name ) )
        {
            psz_value = (char *)pp_attributes[i]->pxsz_attrvalue;
            i_size = sizeof( char ) * strlen(psz_value);
            i_iso_size = 2*i_size + 1;
            psz_iso = (char *)malloc( sizeof(char) * i_iso_size );
            i_read = UTF8Toisolat1( psz_iso, &i_iso_size, psz_value, &i_size) ;
            psz_iso[i_iso_size ]=0;
            return psz_iso;
        }
    }
    return NULL;
}
