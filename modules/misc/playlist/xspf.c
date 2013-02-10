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

#include <assert.h>

int xspf_export_playlist( vlc_object_t *p_this );

static char *input_xml( input_item_t *p_item, char *(*func)(input_item_t *) )
{
    char *tmp = func( p_item );
    if( tmp == NULL )
        return NULL;
    char *ret = convert_xml_special_chars( tmp );
    free( tmp );
    return ret;
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
    if( !p_item ) return;

    /* if we get a node here, we must traverse it */
    if( p_item->i_children > 0 )
    {
        for( int i = 0; i < p_item->i_children; i++ )
            xspf_export_item( p_item->pp_children[i], p_file, p_i_count );
        return;
    }

    /* don't write empty nodes */
    if( p_item->i_children == 0 )
        return;

    input_item_t *p_input = p_item->p_input;
    char *psz;
    mtime_t i_duration;

    /* leaves can be written directly */
    fputs( "\t\t<track>\n", p_file );

    /* -> the location */

    char *psz_uri = input_xml( p_input, input_item_GetURI );
    if( psz_uri && *psz_uri )
        fprintf( p_file, "\t\t\t<location>%s</location>\n", psz_uri );

    /* -> the name/title (only if different from uri)*/
    psz = input_xml( p_input, input_item_GetTitle );
    if( psz && strcmp( psz_uri, psz ) )
        fprintf( p_file, "\t\t\t<title>%s</title>\n", psz );
    free( psz );
    free( psz_uri );

    if( p_item->p_input->p_meta == NULL )
    {
        goto xspfexportitem_end;
    }

    /* -> the artist/creator */
    psz = input_xml( p_input, input_item_GetArtist );
    if( psz && *psz )
        fprintf( p_file, "\t\t\t<creator>%s</creator>\n", psz );
    free( psz );

    /* -> the album */
    psz = input_xml( p_input, input_item_GetAlbum );
    if( psz && *psz )
        fprintf( p_file, "\t\t\t<album>%s</album>\n", psz );
    free( psz );

    /* -> the track number */
    psz = input_xml( p_input, input_item_GetTrackNum );
    if( psz )
    {
        int i_tracknum = atoi( psz );

        free( psz );
        if( i_tracknum > 0 )
            fprintf( p_file, "\t\t\t<trackNum>%i</trackNum>\n", i_tracknum );
    }

    /* -> the description */
    psz = input_xml( p_input, input_item_GetDescription );
    if( psz && *psz )
        fprintf( p_file, "\t\t\t<annotation>%s</annotation>\n", psz );
    free( psz );

    psz = input_xml( p_input, input_item_GetURL );
    if( psz && *psz )
        fprintf( p_file, "\t\t\t<info>%s</info>\n", psz );
    free( psz );

    psz = input_xml( p_input, input_item_GetArtURL );
    if( psz && *psz )
        fprintf( p_file, "\t\t\t<image>%s</image>\n", psz );
    free( psz );

xspfexportitem_end:
    /* -> the duration */
    i_duration = input_item_GetDuration( p_item->p_input );
    if( i_duration > 0 )
        fprintf( p_file, "\t\t\t<duration>%"PRIu64"</duration>\n",
                 i_duration / 1000 );

    /* export the intenal id and the input's options (bookmarks, ...)
     * in <extension> */
    fputs( "\t\t\t<extension application=\""
           "http://www.videolan.org/vlc/playlist/0\">\n", p_file );

    /* print the id and increase the counter */
    fprintf( p_file, "\t\t\t\t<vlc:id>%i</vlc:id>\n", *p_i_count );
    ( *p_i_count )++;

    for( int i = 0; i < p_item->p_input->i_options; i++ )
    {
        char* psz_src = p_item->p_input->ppsz_options[i];
        char* psz_ret = NULL;

        if ( psz_src[0] == ':' )
            psz_src++;

        psz_ret = convert_xml_special_chars( psz_src );
        if ( psz_ret == NULL )
            continue;

        fprintf( p_file, "\t\t\t\t<vlc:option>%s</vlc:option>\n", psz_ret );
        free( psz_ret );
    }
    fputs( "\t\t\t</extension>\n", p_file );
    fputs( "\t\t</track>\n", p_file );
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
    fprintf( p_file, "\t\t\t<vlc:item tid=\"%i\"/>\n", *p_i_count );
    ( *p_i_count )++;

    return;
}

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
             "<playlist xmlns=\"http://xspf.org/ns/0/\" " \
              "xmlns:vlc=\"http://www.videolan.org/vlc/playlist/ns/0/\" " \
              "version=\"1\">\n" );

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
