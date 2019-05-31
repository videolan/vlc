/******************************************************************************
 * xspf.c : XSPF playlist export functions
 ******************************************************************************
 * Copyright (C) 2006-2009 the VideoLAN team
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
#include <vlc_input_item.h>
#include <vlc_playlist_export.h>
#include <vlc_strings.h>
#include <vlc_url.h>

#include <assert.h>

int xspf_export_playlist( vlc_object_t *p_this );

static char *input_xml( input_item_t *p_item, char *(*func)(input_item_t *) )
{
    char *tmp = func( p_item );
    if( tmp == NULL )
        return NULL;
    char *ret = vlc_xml_encode( tmp );
    free( tmp );
    return ret;
}

/**
 * \brief exports one item to file or traverse if item is a node
 * \param p_item playlist item to export
 * \param p_file file to write xml-converted item to
 * \param p_i_count counter for track identifiers
 */
static void xspf_export_item( input_item_t *p_input, FILE *p_file, uint64_t id)
{
    char *psz;
    vlc_tick_t i_duration;

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

    if( p_input->p_meta == NULL )
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
    i_duration = input_item_GetDuration( p_input );
    if( i_duration > 0 )
        fprintf( p_file, "\t\t\t<duration>%"PRIu64"</duration>\n",
                 MS_FROM_VLC_TICK(i_duration) );

    /* export the intenal id and the input's options (bookmarks, ...)
     * in <extension> */
    fputs( "\t\t\t<extension application=\""
           "http://www.videolan.org/vlc/playlist/0\">\n", p_file );

    /* print the id */
    fprintf( p_file, "\t\t\t\t<vlc:id>%"PRIu64"</vlc:id>\n", id );

    for( int i = 0; i < p_input->i_options; i++ )
    {
        char* psz_src = p_input->ppsz_options[i];
        char* psz_ret = NULL;

        if ( psz_src[0] == ':' )
            psz_src++;

        psz_ret = vlc_xml_encode( psz_src );
        if ( psz_ret == NULL )
            continue;

        fprintf( p_file, "\t\t\t\t<vlc:option>%s</vlc:option>\n", psz_ret );
        free( psz_ret );
    }
    fputs( "\t\t\t</extension>\n", p_file );
    fputs( "\t\t</track>\n", p_file );
}

/**
 * \brief Prints the XSPF header to file, writes each item by xspf_export_item()
 * and closes the open xml elements
 * \param p_this the VLC playlist object
 * \return VLC_SUCCESS if some memory is available, otherwise VLC_ENOMEM
 */
int xspf_export_playlist( vlc_object_t *p_this )
{
    struct vlc_playlist_export *p_export = (struct vlc_playlist_export *) p_this;

    /* write XSPF XML header */
    fprintf( p_export->file, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n" );
    fprintf( p_export->file,
             "<playlist xmlns=\"http://xspf.org/ns/0/\" " \
              "xmlns:vlc=\"http://www.videolan.org/vlc/playlist/ns/0/\" " \
              "version=\"1\">\n" );

    fprintf( p_export->file, "\t<trackList>\n" );
    size_t count = vlc_playlist_view_Count(p_export->playlist_view);
    for( size_t i = 0; i < count; ++i )
    {
        vlc_playlist_item_t *item =
            vlc_playlist_view_Get(p_export->playlist_view, i);
        input_item_t *media = vlc_playlist_item_GetMedia(item);

        xspf_export_item(media, p_export->file, i);
    }

    fprintf( p_export->file, "\t</trackList>\n" );

    /* close the header elements */
    fprintf( p_export->file, "</playlist>\n" );

    return VLC_SUCCESS;
}
