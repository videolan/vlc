/*****************************************************************************
 * gvp.c: Google Video Playlist demuxer
 *****************************************************************************
 * Copyright (C) 2006 the VideoLAN team
 * $Id$
 *
 * Authors: Antoine Cellerier <dionoea @t videolan d.t org>
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

struct demux_sys_t
{
    input_item_t *p_current_input;
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int Demux( demux_t *p_demux);
static int Control( demux_t *p_demux, int i_query, va_list args );

/*****************************************************************************
 * Import_GVP: main import function
 *****************************************************************************/
int Import_GVP( vlc_object_t *p_this )
{
    demux_t *p_demux = (demux_t *)p_this;
    int i_peek, i, b_found = false;
    const uint8_t *p_peek;

    i_peek = stream_Peek( p_demux->s, &p_peek, MAX_LINE );

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

    STANDARD_DEMUX_INIT_MSG(  "using Google Video Playlist (gvp) import" );
    p_demux->pf_control = Control;
    p_demux->pf_demux = Demux;
    MALLOC_ERR( p_demux->p_sys, demux_sys_t );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Deactivate: frees unused data
 *****************************************************************************/
void Close_GVP( vlc_object_t *p_this )
{
    demux_t *p_demux = (demux_t *)p_this;
    demux_sys_t *p_sys = p_demux->p_sys;

    free( p_sys );
}

static int Demux( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    char *psz_line;
    char *psz_attrvalue;

    char *psz_version = NULL;
    char *psz_url = NULL;
    char *psz_docid = NULL;
    int i_duration = -1;
    char *psz_title = NULL;
    char *psz_description = NULL;
    input_item_t *p_input;

    INIT_PLAYLIST_STUFF;

    p_sys->p_current_input = p_current_input;

    while( ( psz_line = stream_ReadLine( p_demux->s ) ) )
    {
        if( *psz_line == '#' )
        {
            /* This is a comment */
            free( psz_line );
            continue;
        }
        psz_attrvalue = strchr( psz_line, ':' );
        if( !psz_attrvalue )
        {
            msg_Dbg( p_demux, "Unable to parse line (%s)", psz_line );
            free( psz_line );
            continue;
        }
        *psz_attrvalue = '\0';
        psz_attrvalue++;
        if( !strcmp( psz_line, "gvp_version" ) )
        {
            psz_version = strdup( psz_attrvalue );
        }
        else if( !strcmp( psz_line, "url" ) )
        {
            psz_url = strdup( psz_attrvalue );
        }
        else if( !strcmp( psz_line, "docid" ) )
        {
            psz_docid = strdup( psz_attrvalue );
        }
        else if( !strcmp( psz_line, "duration" ) )
        {
            i_duration = atoi( psz_attrvalue );
        }
        else if( !strcmp( psz_line, "title" ) )
        {
            psz_title = strdup( psz_attrvalue );
        }
        else if( !strcmp( psz_line, "description" ) )
        {
            char *buf;
            if( !psz_description )
            {
                psz_description = strdup( psz_attrvalue );
            }
            else
            {
                /* handle multi-line descriptions */
                buf = malloc( strlen( psz_description )
                            + strlen( psz_attrvalue ) + 2 );
                sprintf( buf, "%s\n%s", psz_description, psz_attrvalue );
                free( psz_description );
                psz_description = buf;
            }
            /* remove ^M char at the end of the line (if any) */
            buf = psz_description + strlen( psz_description );
            if( buf != psz_description )
            {
                buf--;
                if( *buf == '\r' ) *buf = '\0';
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
        p_input = input_item_NewExt( p_demux,
                                    psz_url, psz_title, 0, NULL, -1 );
#define SADD_INFO( type, field ) if( field ) { input_item_AddInfo( \
                    p_input, _("Google Video"), _(type), "%s", field ) ; }
        SADD_INFO( "gvp_version", psz_version );
        SADD_INFO( "docid", psz_docid );
        SADD_INFO( "description", psz_description );
        input_item_AddSubItem( p_current_input, p_input );
        vlc_gc_decref( p_input );
    }

    HANDLE_PLAY_AND_RELEASE;

    free( psz_version );
    free( psz_url );
    free( psz_docid );
    free( psz_title );
    free( psz_description );

    return 0; /* Needed for correct operation of go back */
}

static int Control( demux_t *p_demux, int i_query, va_list args )
{
    VLC_UNUSED(p_demux); VLC_UNUSED(i_query); VLC_UNUSED(args);
    return VLC_EGENERIC;
}
