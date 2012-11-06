/*****************************************************************************
 * ifo.c: Dummy ifo demux to enable opening DVDs rips by double cliking on VIDEO_TS.IFO
 *****************************************************************************
 * Copyright (C) 2007 VLC authors and VideoLAN
 * $Id$
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
#include <vlc_demux.h>
#include <assert.h>

#include "playlist.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int Demux( demux_t *p_demux);
static int DemuxDVD_VR( demux_t *p_demux);

/*****************************************************************************
 * Import_IFO: main import function
 *****************************************************************************/
int Import_IFO( vlc_object_t *p_this )
{
    demux_t *p_demux = (demux_t *)p_this;

    if( !p_demux->psz_file )
        return VLC_EGENERIC;

    size_t len = strlen( p_demux->psz_file );

    char *psz_file = p_demux->psz_file + len - strlen( "VIDEO_TS.IFO" );
    /* Valid filenames are :
     *  - VIDEO_TS.IFO
     *  - VTS_XX_X.IFO where X are digits
     */
    if( len > strlen( "VIDEO_TS.IFO" )
        && ( !strcasecmp( psz_file, "VIDEO_TS.IFO" )
        || (!strncasecmp( psz_file, "VTS_", 4 )
        && !strcasecmp( psz_file + strlen( "VTS_00_0" ) , ".IFO" ) ) ) )
    {
        int i_peek;
        const uint8_t *p_peek;
        i_peek = stream_Peek( p_demux->s, &p_peek, 8 );

        if( i_peek != 8 || memcmp( p_peek, "DVDVIDEO", 8 ) )
            return VLC_EGENERIC;

        p_demux->pf_demux = Demux;
    }
    /* Valid filename for DVD-VR is VR_MANGR.IFO */
    else if( len >= 12 && !strcmp( &p_demux->psz_file[len-12], "VR_MANGR.IFO" ) )
    {
        int i_peek;
        const uint8_t *p_peek;
        i_peek = stream_Peek( p_demux->s, &p_peek, 8 );

        if( i_peek != 8 || memcmp( p_peek, "DVD_RTR_", 8 ) )
            return VLC_EGENERIC;

        p_demux->pf_demux = DemuxDVD_VR;
    }
    else
        return VLC_EGENERIC;

//    STANDARD_DEMUX_INIT_MSG( "found valid VIDEO_TS.IFO" )
    p_demux->pf_control = Control;

    return VLC_SUCCESS;
}

static int Demux( demux_t *p_demux )
{
    char *psz_url, *psz_dir;

    psz_dir = strrchr( p_demux->psz_location, '/' );
    if( psz_dir != NULL )
       psz_dir[1] = '\0';

    if( asprintf( &psz_url, "dvd://%s", p_demux->psz_location ) == -1 )
        return 0;

    input_item_t *p_current_input = GetCurrentItem(p_demux);
    input_item_t *p_input = input_item_New( psz_url, psz_url );
    input_item_PostSubItem( p_current_input, p_input );
    vlc_gc_decref( p_input );

    vlc_gc_decref(p_current_input);
    free( psz_url );

    return 0; /* Needed for correct operation of go back */
}

static int DemuxDVD_VR( demux_t *p_demux )
{
    size_t len = strlen( p_demux->psz_location );
    char *psz_url = malloc( len + 1 );

    if( unlikely( psz_url == NULL ) )
        return 0;
    assert( len >= 12 );
    len -= 12;
    memcpy( psz_url, p_demux->psz_location, len );
    memcpy( psz_url + len, "VR_MOVIE.VRO", 13 );

    input_item_t *p_current_input = GetCurrentItem(p_demux);
    input_item_t *p_input = input_item_New( psz_url, psz_url );
    input_item_PostSubItem( p_current_input, p_input );

    vlc_gc_decref( p_input );

    vlc_gc_decref(p_current_input);
    free( psz_url );

    return 0; /* Needed for correct operation of go back */
}
