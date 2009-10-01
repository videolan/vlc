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
#include <vlc_charset.h>

#include "playlist.h"

struct demux_sys_t
{
    char *psz_prefix;
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int Demux( demux_t *p_demux);
static int Control( demux_t *p_demux, int i_query, va_list args );
static char* ParseUriValue(char* psz_string);

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
    char       *psz_uri = NULL;
    char       *psz_parse;
    input_item_t *p_input;

    input_item_t *p_current_input = GetCurrentItem(p_demux);

    psz_line = stream_ReadLine( p_demux->s );
    while( psz_line )
    {
        psz_parse = psz_line;
        /* Skip leading tabs and spaces */
        while( *psz_parse == ' '  || *psz_parse == '\t' ||
               *psz_parse == '\n' || *psz_parse == '\r' )
            psz_parse++;

        /* if the line is the uri of the media item */
        if( !strncasecmp( psz_parse, "<media src=\"", strlen( "<media src=\"" ) ) )
        {
            psz_uri = ParseUriValue( psz_parse );
            if( !EMPTY_STR(psz_uri) )
            {
                psz_uri = ProcessMRL( psz_uri, p_demux->p_sys->psz_prefix );
                p_input = input_item_NewExt( p_demux, psz_uri, psz_uri,
                                        0, NULL, 0, -1 );
                input_item_AddSubItem( p_current_input, p_input );
            }
            free( psz_uri );
        }

        /* Fetch another line */
        free( psz_line );
        psz_line = stream_ReadLine( p_demux->s );

    }
    vlc_gc_decref(p_current_input);
    var_Destroy( p_demux, "wpl-extvlcopt" );
    return 0; /* Needed for correct operation of go back */
}

static int Control( demux_t *p_demux, int i_query, va_list args )
{
    VLC_UNUSED(p_demux); VLC_UNUSED(i_query); VLC_UNUSED(args);
    return VLC_EGENERIC;
}

static char* ParseUriValue( char* psz_string )
{
    int i_len = strlen( psz_string );
    if( i_len <= 3 )
        return NULL;
    char* psz_value = calloc( i_len, 1 );
    if( !psz_value )
        return NULL;

    sscanf( psz_string, "%*[^=]=\"%[^\r\t\n\"]", psz_value );

    return psz_value;
}


