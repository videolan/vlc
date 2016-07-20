/*****************************************************************************
 * gvp.c: Google Video Playlist demuxer
 *****************************************************************************
 * Copyright (C) 2006 VLC authors and VideoLAN
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

/**
 * Format seems to be:
 * gvp_version:<version> (1.1)
 * url:<the media's url>
 * docid:<integer>
 * duration:<integer ms ?>
 * title:<the title>
 * description:<desc line1>^M
 * description:<desc linei>^M
 * description:<desc final line (no ^M)>
 * lines starting with # are comments
 *
 * Example:

# download the free Google Video Player from http://video.google.com/
gvp_version:1.1
url:http://vp.video.google.com/videodownload?version=0&secureurl=uAAAAMVHt_Q99OwfGxlWVWH7jd6AA_3n4TboaxIELD_kCg3KcBPSxExZFvQv5DGAxrahVg57KZNZvd0EORPBM3xrxTJ3FdLEWBYiduklpviqjE1Q5zLAkiEZaUsUSFtmbBZDTUUBuN9moYY59eK8lpWXsgTbYB1tLVtaxNBpAMRMyVeHoiJ7BzYdENk-PqJeBbr50QbQ83WK87yJAbN2pSRnF-ucCuNMSLBV7wBL4IcxFpYb1WOK-YXkyxY0NtWlPBufTA&sigh=matNCEVSOR8c-3zN9Gtx0zGinwU&begin=0&len=59749&docid=-715862862672743260
docid:-715862862672743260
duration:59749
title:Apple Macintosh 1984 Superbowl Commercial
description:The now infamous Apple Macintosh commercial aired during the 1984 SuperBowl.

 */

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_demux.h>

#include "playlist.h"

#define MAX_LINE 1024

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int Demux( demux_t *p_demux);

/*****************************************************************************
 * Import_GVP: main import function
 *****************************************************************************/
int Import_GVP( vlc_object_t *p_this )
{
    demux_t *p_demux = (demux_t *)p_this;
    int i_peek, i, b_found = false;
    const uint8_t *p_peek;

    CHECK_FILE();
    i_peek = vlc_stream_Peek( p_demux->s, &p_peek, MAX_LINE );

    for( i = 0; i < i_peek - (int)sizeof("gvp_version:"); i++ )
    {
        if( p_peek[i] == 'g' && p_peek[i+1] == 'v' && p_peek[i+2] == 'p' &&
            !memcmp( p_peek+i, "gvp_version:", sizeof("gvp_version:") - 1 ) )
        {
            b_found = true;
            break;
        }
    }

    if( !b_found ) return VLC_EGENERIC;

    msg_Dbg( p_this, "using Google Video Playlist (gvp) import" );
    p_demux->pf_control = Control;
    p_demux->pf_demux = Demux;

    return VLC_SUCCESS;
}

static int Demux( demux_t *p_demux )
{
    char *psz_line;

    char *psz_version = NULL;
    char *psz_url = NULL;
    char *psz_docid = NULL;
    char *psz_title = NULL;
    char *psz_desc = NULL;
    size_t desclen = 0;
    input_item_t *p_input;

    input_item_t *p_current_input = GetCurrentItem(p_demux);

    input_item_node_t *p_subitems = input_item_node_Create( p_current_input );

    while( ( psz_line = vlc_stream_ReadLine( p_demux->s ) ) )
    {
        if( *psz_line == '#' )
        {
            /* This is a comment */
            free( psz_line );
            continue;
        }

        char *value = strchr( psz_line, ':' );
        if( value == NULL )
        {
            msg_Dbg( p_demux, "Unable to parse line (%s)", psz_line );
            free( psz_line );
            continue;
        }
        *(value++) = '\0';

        size_t len = strlen( value );
        if( len > 0 && value[len - 1] == '\r' )
            value[--len] = '\0'; /* strip trailing CR */

        if( psz_version == NULL && !strcmp( psz_line, "gvp_version" ) )
            psz_version = strdup( value );
        else if( psz_url == NULL && !strcmp( psz_line, "url" ) )
            psz_url = strdup( value );
        else if( psz_docid == NULL && !strcmp( psz_line, "docid" ) )
            psz_docid = strdup( value );
        else if( !strcmp( psz_line, "duration" ) )
            /*atoi( psz_attrvalue )*/;
        else if( psz_title == NULL && !strcmp( psz_line, "title" ) )
            psz_title = strdup( value );
        else if( !strcmp( psz_line, "description" )
              && desclen < 32768 && len < 32768 )
        {
            char *buf = realloc( psz_desc, desclen + 1 + len + 1 );
            if( buf != NULL )
            {
                if( desclen > 0 )
                    buf[desclen++] = '\n';
                memcpy( buf + desclen, value, len + 1 );
                desclen += len;
                psz_desc = buf;
            }
        }
        free( psz_line );
    }

    if( !psz_url )
    {
        msg_Err( p_demux, "URL not found" );
    }
    else
    {
        p_input = input_item_New( psz_url, psz_title );
#define SADD_INFO( type, field ) if( field ) { input_item_AddInfo( \
                    p_input, _("Google Video"), type, "%s", field ) ; }
        SADD_INFO( "gvp_version", psz_version );
        SADD_INFO( "docid", psz_docid );
        SADD_INFO( "description", psz_desc );
        input_item_node_AppendItem( p_subitems, p_input );
        vlc_gc_decref( p_input );
    }

    input_item_node_PostAndDelete( p_subitems );

    vlc_gc_decref(p_current_input);

    free( psz_version );
    free( psz_url );
    free( psz_docid );
    free( psz_title );
    free( psz_desc );

    return 0; /* Needed for correct operation of go back */
}
