/*****************************************************************************
 * wpl.c : WPL playlist format import
 *****************************************************************************
 * Copyright (C) 2009 the VideoLAN team
 *
 * Authors: Su Heaven <suheaven@gmail.com>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_demux.h>

#include "playlist.h"

struct demux_sys_t
{
    char *psz_prefix;
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int Demux( demux_t *p_demux);

/*****************************************************************************
 * Import_WPL: main import function
 *****************************************************************************/
int Import_WPL( vlc_object_t *p_this )
{
    demux_t *p_demux = (demux_t *)p_this;

    if(! ( demux_IsPathExtension( p_demux, ".wpl" ) || demux_IsForced( p_demux,  "wpl" )))
        return VLC_EGENERIC;

    STANDARD_DEMUX_INIT_MSG( "found valid WPL playlist" );
    p_demux->p_sys->psz_prefix = FindPrefix( p_demux );

    return VLC_SUCCESS;
}



/*****************************************************************************
 * Deactivate: frees unused data
 *****************************************************************************/
void Close_WPL( vlc_object_t *p_this )
{
    demux_t *p_demux = (demux_t *)p_this;
    free( p_demux->p_sys->psz_prefix );
    free( p_demux->p_sys );
}

static int Demux( demux_t *p_demux )
{
    char       *psz_line;
    input_item_t *p_current_input = GetCurrentItem(p_demux);

    input_item_node_t *p_subitems = input_item_node_Create( p_current_input );

    while( (psz_line = stream_ReadLine( p_demux->s )) )
    {
        char *psz_parse = psz_line;
        /* Skip leading tabs and spaces */
        while( *psz_parse == ' '  || *psz_parse == '\t' ||
               *psz_parse == '\n' || *psz_parse == '\r' )
            psz_parse++;

        /* if the line is the uri of the media item */
        if( !strncasecmp( psz_parse, "<media src=\"", strlen( "<media src=\"" ) ) )
        {
            char *psz_uri = psz_parse + strlen( "<media src=\"" );

            psz_parse = strchr( psz_uri, '"' );
            if( psz_parse != NULL )
            {
                input_item_t *p_input;

                *psz_parse = '\0';
                psz_uri = ProcessMRL( psz_uri, p_demux->p_sys->psz_prefix );
                p_input = input_item_NewExt( psz_uri, psz_uri,
                                        0, NULL, 0, -1 );
                input_item_node_AppendItem( p_subitems, p_input );
                vlc_gc_decref( p_input );
            }
        }

        /* Fetch another line */
        free( psz_line );

    }

    input_item_node_PostAndDelete( p_subitems );

    vlc_gc_decref(p_current_input);
    var_Destroy( p_demux, "wpl-extvlcopt" );
    return 0; /* Needed for correct operation of go back */
}
