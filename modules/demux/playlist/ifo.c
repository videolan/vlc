/*****************************************************************************
 * ifo.c: Dummy ifo demux to enable opening DVDs rips by double cliking on VIDEO_TS.IFO
 *****************************************************************************
 * Copyright (C) 2007 VLC authors and VideoLAN
 *
 * Authors: Antoine Cellerier <dionoea @t videolan d.t org>
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
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_access.h>
#include <vlc_url.h>
#include <assert.h>

#include "playlist.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int ReadDVD( stream_t *, input_item_node_t * );
static int ReadDVD_VR( stream_t *, input_item_node_t * );

static const char *StreamLocation( const stream_t *s )
{
    return s->psz_filepath ? s->psz_filepath : s->psz_url;
}

/*****************************************************************************
 * Import_IFO: main import function
 *****************************************************************************/
int Import_IFO( vlc_object_t *p_this )
{
    stream_t *p_stream = (stream_t *)p_this;

    CHECK_FILE(p_stream);

    if( !stream_HasExtension( p_stream, ".IFO" ) )
        return VLC_EGENERIC;

    const char *psz_location = StreamLocation( p_stream );
    if( psz_location == NULL )
        return VLC_EGENERIC;

    size_t len = strlen( psz_location );
    if( len < 12 )
        return VLC_EGENERIC;

    const char *psz_probe;
    const char *psz_file = &psz_location[len - 12];
    /* Valid filenames are :
     *  - VIDEO_TS.IFO
     *  - VTS_XX_X.IFO where X are digits
     */
    if( !strncasecmp( psz_file, "VIDEO_TS", 8 ) ||
        !strncasecmp( psz_file, "VTS_", 4 ) )
    {
        psz_probe = "DVDVIDEO";
        p_stream->pf_readdir = ReadDVD;
    }
    /* Valid filename for DVD-VR is VR_MANGR.IFO */
    else if( !strncasecmp( psz_file, "VR_MANGR", 8 ) )
    {
        psz_probe = "DVD_RTR_";
        p_stream->pf_readdir = ReadDVD_VR;
    }
    else
        return VLC_EGENERIC;

    const uint8_t *p_peek;
    ssize_t i_peek = vlc_stream_Peek( p_stream->s, &p_peek, 8 );
    if( i_peek < 8 || memcmp( p_peek, psz_probe, 8 ) )
        return VLC_EGENERIC;

    p_stream->pf_control = access_vaDirectoryControlHelper;

    return VLC_SUCCESS;
}

static int ReadDVD( stream_t *p_stream, input_item_node_t *node )
{
    const char *psz_location = StreamLocation(p_stream);

    char *psz_url = strndup( psz_location, strlen( psz_location ) - 12 );
    if( !psz_url )
        return VLC_ENOMEM;

    input_item_t *p_input = input_item_New( psz_url, psz_url );
    if( p_input )
    {
        input_item_AddOption( p_input, "demux=dvd", VLC_INPUT_OPTION_TRUSTED );
        input_item_node_AppendItem( node, p_input );
        input_item_Release( p_input );
    }

    free( psz_url );

    return VLC_SUCCESS;
}

static int ReadDVD_VR( stream_t *p_stream, input_item_node_t *node )
{
    const char *psz_location = StreamLocation(p_stream);

    size_t len = strlen( psz_location );
    char *psz_url = strdup( psz_location );

    if( unlikely( psz_url == NULL ) )
        return VLC_EGENERIC;

    strcpy( &psz_url[len - 12], "VR_MOVIE.VRO" );

    input_item_t *p_input = input_item_New( psz_url, psz_url );
    if( p_input )
    {
        input_item_node_AppendItem( node, p_input );
        input_item_Release( p_input );
    }

    free( psz_url );

    return VLC_SUCCESS;
}
