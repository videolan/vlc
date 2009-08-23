/******************************************************************************
 * xspf.c : XSPF playlist export functions
 ******************************************************************************
 * Copyright (C) 2006-2009 the VideoLAN team
 * $Id$
 *
 * Authors: Daniel Stränger <vlc at schmaller dot de>
 *          Yoann Peronneau <yoann@videolan.org>
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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_playlist.h>
#include <vlc_input.h>
#include <vlc_strings.h>
#include <vlc_url.h>
#include "xspf.h"

#include <assert.h>

static void xspf_export_item( playlist_item_t *, FILE *, int * );
static void xspf_extension_item( playlist_item_t *, FILE *, int * );

/**
 * \brief Prints the XSPF header to file, writes each item by xspf_export_item()
 * and closes the open xml elements
 * \param p_this the VLC playlist object
 * \return VLC_SUCCESS if some memory is available, otherwise VLC_ENONMEM
 */
int xspf_export_playlist( vlc_object_t *p_this )
{
    const playlist_export_t *p_export = (playlist_export_t *)p_this;
    int               i, i_count;
    char             *psz_temp;
    playlist_item_t  *p_node = p_export->p_root;

    /* write XSPF XML header */
    fprintf( p_export->p_file, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n" );
    fprintf( p_export->p_file,
             "<playlist version=\"1\" xmlns=\"http://xspf.org/ns/0/\" " \
             "xmlns:vlc=\"http://www.videolan.org/vlc/playlist/ns/0/\">\n" );

    if( !p_node ) return VLC_SUCCESS;

    /* save name of the playlist node */
    psz_temp = convert_xml_special_chars( p_node->p_input->psz_name );
    if( *psz_temp )
    {
        fprintf(  p_export->p_file, "\t<title>%s</title>\n", psz_temp );
    }
    free( psz_temp );

    /* export all items in a flat format */
    fprintf( p_export->p_file, "\t<trackList>\n" );
    i_count = 0;
    for( i = 0; i < p_node->i_children; i++ )
    {
        xspf_export_item( p_node->pp_children[i], p_export->p_file,
                          &i_count );
    }
    fprintf( p_export->p_file, "\t</trackList>\n" );

    /* export the tree structure in <extension> */
    fprintf( p_export->p_file, "\t<extension application=\"" \
             "http://www.videolan.org/vlc/playlist/0\">\n" );
    i_count = 0;
    for( i = 0; i < p_node->i_children; i++ )
    {
        xspf_extension_item( p_node->pp_children[i], p_export->p_file,
                             &i_count );
    }
    fprintf( p_export->p_file, "\t</extension>\n" );

    /* close the header elements */
    fprintf( p_export->p_file, "</playlist>\n" );

    return VLC_SUCCESS;
}

/**
 * \brief exports one item to file or traverse if item is a node
 * \param p_item playlist item to export
 * \param p_file file to write xml-converted item to
 * \param p_i_count counter for track identifiers
 */
static void xspf_export_item( playlist_item_t *p_item, FILE *p_file,
                              int *p_i_count )
{
    char *psz;
    char *psz_temp;
    int i;
    mtime_t i_duration;

    if( !p_item ) return;

    /* if we get a node here, we must traverse it */
    if( p_item->i_children > 0 )
    {
        int i;
        for( i = 0; i < p_item->i_children; i++ )
        {
            xspf_export_item( p_item->pp_children[i], p_file, p_i_count );
        }
        return;
    }

    /* don't write empty nodes */
    if( p_item->i_children == 0 )
    {
        return;
    }

    /* leaves can be written directly */
    fprintf( p_file, "\t\t<track>\n" );

    /* -> the location */

    char *psz_uri = input_item_GetURI( p_item->p_input );

    if( psz_uri && *psz_uri )
        fprintf( p_file, "\t\t\t<location>%s</location>\n", psz_uri );

    /* -> the name/title (only if different from uri)*/
    char *psz_name = input_item_GetTitle( p_item->p_input );
    if( psz_name && psz_uri && strcmp( psz_uri, psz_name ) )
    {
        psz_temp = convert_xml_special_chars( psz_name );
        if( *psz_temp )
            fprintf( p_file, "\t\t\t<title>%s</title>\n", psz_temp );
        free( psz_temp );
    }
    free( psz_name );
    free( psz_uri );

    if( p_item->p_input->p_meta == NULL )
    {
        goto xspfexportitem_end;
    }

    /* -> the artist/creator */
    psz = input_item_GetArtist( p_item->p_input );
    if( psz == NULL ) psz = strdup( "" );
    psz_temp = convert_xml_special_chars( psz );
    free( psz );
    if( *psz_temp )
    {
        fprintf( p_file, "\t\t\t<creator>%s</creator>\n", psz_temp );
    }
    free( psz_temp );

    /* -> the album */
    psz = input_item_GetAlbum( p_item->p_input );
    if( psz == NULL ) psz = strdup( "" );
    psz_temp = convert_xml_special_chars( psz );
    free( psz );
    if( *psz_temp )
    {
        fprintf( p_file, "\t\t\t<album>%s</album>\n", psz_temp );
    }
    free( psz_temp );

    /* -> the track number */
    psz = input_item_GetTrackNum( p_item->p_input );
    if( psz == NULL ) psz = strdup( "" );
    if( psz && *psz )
    {
        int i_tracknum = atoi( psz );
        if( i_tracknum > 0 )
            fprintf( p_file, "\t\t\t<trackNum>%i</trackNum>\n", i_tracknum );
    }
    free( psz );

    /* -> the description */
    psz = input_item_GetDescription( p_item->p_input );
    if( psz == NULL ) psz = strdup( "" );
    psz_temp = convert_xml_special_chars( psz );
    free( psz );
    if( *psz_temp )
    {
        fprintf( p_file, "\t\t\t<annotation>%s</annotation>\n", psz_temp );
    }
    free( psz_temp );

    psz = input_item_GetArtURL( p_item->p_input );
    if( psz == NULL ) psz = strdup( "" );
    if( !EMPTY_STR( psz ) )
    {
        fprintf( p_file, "\t\t\t<image>%s</image>\n", psz );
    }
    free( psz );

xspfexportitem_end:
    /* -> the duration */
    i_duration = input_item_GetDuration( p_item->p_input );
    if( i_duration > 0 )
    {
        fprintf( p_file, "\t\t\t<duration>%ld</duration>\n",
                 (long)(i_duration / 1000) );
    }

    /* export the intenal id and the input's options (bookmarks, ...)
     * in <extension> */
    fprintf( p_file, "\t\t\t<extension application=\"" \
             "http://www.videolan.org/vlc/playlist/0\">\n" );

    /* print the id and increase the counter */
    fprintf( p_file, "\t\t\t\t<vlc:id>%i</vlc:id>\n", *p_i_count );
    ( *p_i_count )++;

    for( i = 0; i < p_item->p_input->i_options; i++ )
    {
        fprintf( p_file, "\t\t\t\t<vlc:option>%s</vlc:option>\n",
                 p_item->p_input->ppsz_options[i][0] == ':' ?
                 p_item->p_input->ppsz_options[i] + 1 :
                 p_item->p_input->ppsz_options[i] );
    }
    fprintf( p_file, "\t\t\t</extension>\n" );

    fprintf( p_file, "\t\t</track>\n" );

    return;
}

/**
 * \brief exports one item in extension to file and traverse if item is a node
 * \param p_item playlist item to export
 * \param p_file file to write xml-converted item to
 * \param p_i_count counter for track identifiers
 */
static void xspf_extension_item( playlist_item_t *p_item, FILE *p_file,
                                 int *p_i_count )
{
    if( !p_item ) return;

    /* if we get a node here, we must traverse it */
    if( p_item->i_children >= 0 )
    {
        int i;
        char *psz_temp = NULL;
        if( p_item->p_input->psz_name )
            psz_temp = convert_xml_special_chars( p_item->p_input->psz_name );
        fprintf( p_file, "\t\t<vlc:node title=\"%s\">\n",
                 psz_temp ? psz_temp : "" );
        free( psz_temp );

        for( i = 0; i < p_item->i_children; i++ )
        {
            xspf_extension_item( p_item->pp_children[i], p_file, p_i_count );
        }

        fprintf( p_file, "\t\t</vlc:node>\n" );
        return;
    }


    /* print leaf and increase the counter */
    fprintf( p_file, "\t\t\t<vlc:item tid=\"%i\" />\n", *p_i_count );
    ( *p_i_count )++;

    return;
}
