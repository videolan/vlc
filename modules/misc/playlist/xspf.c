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
 * \file modules/misc/playlist/xspf.c
 * \brief XSPF playlist export functions
 */
#include <stdio.h>
#include <vlc/vlc.h>
#include <vlc/intf.h>
#include "vlc_meta.h"
#include "vlc_strings.h"
#include "xspf.h"

/**
 * \brief Prints the XSPF header to file, writes each item by xspf_export_item()
 * and closes the open xml elements
 * \param p_this the VLC playlist object
 * \return VLC_SUCCESS if some memory is available, otherwise VLC_ENONMEM
 */
int E_(xspf_export_playlist)( vlc_object_t *p_this )
{
    const playlist_t *p_playlist = (playlist_t *)p_this;
    const playlist_export_t *p_export =
        (playlist_export_t *)p_playlist->p_private;
    int              i;
    char             *psz;
    char             *psz_temp;
    playlist_item_t **pp_items = NULL;
    int               i_size;
    playlist_item_t  *p_node;

    /* write XSPF XML header - since we don't use <extension>,
     * we get by with version 0 */
    fprintf( p_export->p_file, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n" );
    fprintf( p_export->p_file,
             "<playlist version=\"0\" xmlns=\"http://xspf.org/ns/0/\">\n" );

    /* save tho whole playlist or only the current node */
#define p_item p_playlist->status.p_item
    if ( p_item )
    {
        for (i = 0; i < p_item->i_parents; i++ )
        {
            if ( p_item->pp_parents[i]->p_parent->input.i_type
                 == ITEM_TYPE_PLAYLIST )
            {
                /* set the current node and its children */
                p_node   = p_item->pp_parents[i]->p_parent;
                pp_items = p_node->pp_children;
                i_size   = p_node->i_children;
#undef p_item

                /* save name of the playlist node */
                psz_temp = convert_xml_special_chars( p_node->input.psz_name );
                if ( *psz_temp )
                    fprintf(  p_export->p_file, "\t<title>%s</title>\n",
                              psz_temp );
                free( psz_temp );

                /* save the creator of the playlist node */
                psz = vlc_input_item_GetInfo( &p_node->input,
                                              _(VLC_META_INFO_CAT),
                                              _(VLC_META_ARTIST) );
                if ( psz && !*psz )
                {
                    free ( psz );
                    psz = NULL;
                }

                if ( !psz )
                    psz = vlc_input_item_GetInfo( &p_node->input,
                                                  _(VLC_META_INFO_CAT),
                                                  _(VLC_META_AUTHOR) );

                psz_temp = convert_xml_special_chars( psz );

                if ( psz ) free( psz );
                if ( *psz_temp )
                    fprintf(  p_export->p_file, "\t<creator>%s</creator>\n",
                              psz_temp );
                free( psz_temp );

                /* save location of the playlist node */
                psz = assertUTF8URI( p_export->psz_filename );
                if ( psz && *psz )
                {
                    fprintf( p_export->p_file, "\t<location>%s</location>\n",
                             psz );
                    free( psz );
                }
                break;
            }
        }
    }

    /* prepare all the playlist children for export */
    if ( !pp_items )
    {
        pp_items = p_playlist->pp_items;
        i_size   = p_playlist->i_size;
    }

    /* export all items */
    fprintf( p_export->p_file, "\t<trackList>\n" );
    for ( i = 0; i < i_size; i++ )
    {
        xspf_export_item( pp_items[i], p_export->p_file );
    }

    /* close the header elements */
    fprintf( p_export->p_file, "\t</trackList>\n" );
    fprintf( p_export->p_file, "</playlist>\n" );

    return VLC_SUCCESS;
}

/**
 * \brief exports one item to file or traverse if item is a node
 * \param p_item playlist item to export
 * \param p_file file to write xml-converted item to
 */
static void xspf_export_item( playlist_item_t *p_item, FILE *p_file )
{
    int i;       /**< iterator for all children if the current item is a node */
    char *psz;
    char *psz_temp;

    if ( !p_item )
        return;

    /** \todo only "flat" playlists supported at this time.
     *  extend to save the tree structure.
     */
    /* if we get a node here, we must traverse it */
    if ( p_item->i_children > 0 )
    {
        for ( i = 0; i < p_item->i_children; i++ )
        {
            xspf_export_item( p_item->pp_children[i], p_file );
        }
        return;
    }

    /* leaves can be written directly */
    fprintf( p_file, "\t\t<track>\n" );

    /* -> the location */
    if ( p_item->input.psz_uri && *p_item->input.psz_uri )
    {
        psz = assertUTF8URI( p_item->input.psz_uri );
        fprintf( p_file, "\t\t\t<location>%s</location>\n", psz );
        free( psz );
    }

    /* -> the name/title (only if different from uri)*/
    if ( p_item->input.psz_name &&
         p_item->input.psz_uri &&
         strcmp( p_item->input.psz_uri, p_item->input.psz_name ) )
    {
        psz_temp = convert_xml_special_chars( p_item->input.psz_name );
        if ( *psz_temp )
            fprintf( p_file, "\t\t\t<title>%s</title>\n", psz_temp );
        free( psz_temp );
    }

    /* -> the artist/creator */
    psz = vlc_input_item_GetInfo( &p_item->input,
                                  _(VLC_META_INFO_CAT),
                                  _(VLC_META_ARTIST) );
    if ( psz && !*psz )
    {
        free ( psz );
        psz = NULL;
    }
    if ( !psz )
        psz = vlc_input_item_GetInfo( &p_item->input,
                                      _(VLC_META_INFO_CAT),
                                      _(VLC_META_AUTHOR) );
    psz_temp = convert_xml_special_chars( psz );
    if ( psz ) free( psz );
    if ( *psz_temp )
        fprintf( p_file, "\t\t\t<creator>%s</creator>\n", psz_temp );
    free( psz_temp );

    /* -> the album */
    psz = vlc_input_item_GetInfo( &p_item->input,
                                  _(VLC_META_INFO_CAT),
                                  _(VLC_META_COLLECTION) );
    psz_temp = convert_xml_special_chars( psz );
    if ( psz ) free( psz );
    if ( *psz_temp )
        fprintf( p_file, "\t\t\t<album>%s</album>\n", psz_temp );
    free( psz_temp );

    /* -> the track number */
    psz = vlc_input_item_GetInfo( &p_item->input,
                                  _(VLC_META_INFO_CAT),
                                  _(VLC_META_SEQ_NUM) );
    if ( psz )
    {
        if ( *psz )
            fprintf( p_file, "\t\t\t<trackNum>%i</trackNum>\n", atoi( psz ) );
        free( psz );
    }

    /* -> the duration */
    if ( p_item->input.i_duration > 0 )
    {
        fprintf( p_file, "\t\t\t<duration>%ld</duration>\n",
                 (long)(p_item->input.i_duration / 1000) );
    }

    fprintf( p_file, "\t\t</track>\n" );

    return;
}

/**
 * \param psz_name the location of the media ressource (e.g. local file,
 *        device, network stream, etc.)
 * \return a new char buffer which asserts that the location is valid UTF-8
 *         and a valid URI
 * \note the returned buffer must be freed, when it isn't used anymore
 */
static char *assertUTF8URI( char *psz_name )
{
    char *psz_ret = NULL;              /**< the new result buffer to return */
    char *psz_s = NULL, *psz_d = NULL; /**< src & dest pointers for URI conversion */
    vlc_bool_t b_name_is_uri = VLC_FALSE;

    if ( !psz_name || !*psz_name )
        return NULL;

    /* check that string is valid UTF-8 */
    /* XXX: Why do we even need to do that ? (all strings in core are UTF-8 encoded */
    if( !( psz_s = EnsureUTF8( psz_name ) ) )
        return NULL;

    /* max. 3x for URI conversion (percent escaping) and
       8 bytes for "file://" and NULL-termination */
    psz_ret = (char *)malloc( sizeof(char)*strlen(psz_name)*6*3+8 );
    if ( !psz_ret )
        return NULL;

    /** \todo check for a valid scheme part preceding the colon */
    if ( strchr( psz_s, ':' ) )
    {
        psz_d = psz_ret;
        b_name_is_uri = VLC_TRUE;
    }
    /* assume "file" scheme if no scheme-part is included */
    else
    {
        strcpy( psz_ret, "file://" );
        psz_d = psz_ret + 7;
    }

    while ( *psz_s )
    {
        /* percent-encode all non-ASCII and the XML special characters and the percent sign itself */
        if ( *psz_s & B10000000 ||
             *psz_s == '<' ||
             *psz_s == '>' ||
             *psz_s == '&' ||
             *psz_s == ' ' ||
             ( *psz_s == '%' && !b_name_is_uri ) )
        {
            *psz_d++ = '%';
            *psz_d++ = hexchars[(*psz_s >> 4) & B00001111];
            *psz_d++ = hexchars[*psz_s & B00001111];
        } else
            *psz_d++ = *psz_s;

        psz_s++;
    }
    *psz_d = '\0';

    return (char *)realloc( psz_ret, sizeof(char)*strlen( psz_ret ) + 1 );
}
