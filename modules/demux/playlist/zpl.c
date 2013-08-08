/*****************************************************************************
 * zpl.c : ZPL playlist format import
 *****************************************************************************
 * Copyright (C) 2009 the VideoLAN team
 *
 * Authors: Su Heaven <suheaven@gmail.com>
 *          Rémi Duraffort <ivoire@videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 51
 * Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
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
static char* ParseTabValue(char* psz_string);

/*****************************************************************************
 * Import_ZPL: main import function
 *****************************************************************************/
int Import_ZPL( vlc_object_t *p_this )
{
    demux_t *p_demux = (demux_t *)p_this;

    if(! ( demux_IsPathExtension( p_demux, ".zpl" ) || demux_IsForced( p_demux,  "zpl" )))
        return VLC_EGENERIC;

    STANDARD_DEMUX_INIT_MSG( "found valid ZPL playlist" );
    p_demux->p_sys->psz_prefix = FindPrefix( p_demux );

    return VLC_SUCCESS;
}


/*****************************************************************************
 * Deactivate: frees unused data
 *****************************************************************************/
void Close_ZPL( vlc_object_t *p_this )
{
    demux_t *p_demux = (demux_t *)p_this;
    free( p_demux->p_sys->psz_prefix );
    free( p_demux->p_sys );
}

static int Demux( demux_t *p_demux )
{
    char       *psz_line;

    mtime_t i_duration = -1;
    char *psz_title = NULL,       *psz_genre = NULL,      *psz_tracknum = NULL,
         *psz_language = NULL,    *psz_artist = NULL,     *psz_album = NULL,
         *psz_date = NULL,        *psz_publisher = NULL,  *psz_encodedby = NULL,
         *psz_description = NULL, *psz_url = NULL,        *psz_copyright = NULL,
         *psz_mrl = NULL;

    input_item_t *p_current_input = GetCurrentItem(p_demux);

    psz_line = stream_ReadLine( p_demux->s );
    char *psz_parse = psz_line;

    /* Skip leading tabs and spaces */
    while( *psz_parse == ' '  || *psz_parse == '\t' ||
           *psz_parse == '\n' || *psz_parse == '\r' )
        psz_parse++;

    /* if the 1st line is "AC", skip it */
    /* TODO: using this information ? */
    if( !strncasecmp( psz_parse, "AC", strlen( "AC" ) ) )
    {
        free( psz_line );
        psz_line = stream_ReadLine( p_demux->s );
    }

    input_item_node_t *p_subitems = input_item_node_Create( p_current_input );

    /* Loop on all lines */
    while( psz_line )
    {
        psz_parse = psz_line;

        /* Skip leading tabs and spaces */
        while( *psz_parse == ' '  || *psz_parse == '\t' ||
               *psz_parse == '\n' || *psz_parse == '\r' )
            psz_parse++;

        /* filename */
        if( !strncasecmp( psz_parse, "NM", strlen( "NM" ) ) )
        {
            char *psz_tabvalue = ParseTabValue( psz_parse );
            if( !EMPTY_STR(psz_tabvalue) )
            {
                free( psz_mrl );
                psz_mrl = ProcessMRL( psz_tabvalue, p_demux->p_sys->psz_prefix );
            }
            free( psz_tabvalue );
        }

        /* duration */
        else if( !strncasecmp( psz_parse, "DR", strlen( "DR" ) ) )
        {
            char *psz_tabvalue = ParseTabValue( psz_parse );
            if( !EMPTY_STR(psz_tabvalue) )
            {
                int i_parsed_duration = atoi( psz_tabvalue );
                if( i_parsed_duration >= 0 )
                    i_duration = i_parsed_duration * INT64_C(1000);
            }
            free( psz_tabvalue );
        }

#define PARSE(tag,variable)                                     \
    else if( !strncasecmp( psz_parse, tag, strlen( tag ) ) )    \
    {                                                           \
        free( variable );                                       \
        variable = ParseTabValue( psz_parse );                  \
    }

        PARSE( "TT", psz_title )
        PARSE( "TG", psz_genre )
        PARSE( "TR", psz_tracknum )
        PARSE( "TL", psz_language )
        PARSE( "TA", psz_artist )
        PARSE( "TB", psz_album )
        PARSE( "TY", psz_date )
        PARSE( "TH", psz_publisher )
        PARSE( "TE", psz_encodedby )
        PARSE( "TC", psz_description )
        PARSE( "TU", psz_url )
        PARSE( "TO", psz_copyright )

#undef PARSE

        /* force a duration ? */
        else if( !strncasecmp( psz_parse, "FD", strlen( "FD" ) ) )
        {}

        /* end of file entry */
        else if( !strncasecmp( psz_parse, "BR!", strlen( "BR!" ) ) )
        {
            /* create the input item */
            input_item_t *p_input = input_item_NewExt( psz_mrl,
                                        psz_title, 0, NULL, 0, i_duration );
            input_item_node_AppendItem( p_subitems, p_input );
            FREENULL( psz_mrl );
            FREENULL( psz_title );
            i_duration = -1;

#define SET(variable, type)                             \
    if( !EMPTY_STR(variable) )                          \
    {                                                   \
        input_item_Set##type( p_input, variable );      \
    }                                                   \
    FREENULL( variable );
            /* set the meta */
            SET( psz_genre, Genre );
            SET( psz_tracknum, TrackNum );
            SET( psz_language, Language );
            SET( psz_artist, Artist );
            SET( psz_album, Album );
            SET( psz_date, Date );
            SET( psz_encodedby, EncodedBy );
            SET( psz_description, Description );
            SET( psz_copyright, Copyright );
            SET( psz_url, URL );
            SET( psz_publisher, Publisher );
#undef SET

            vlc_gc_decref( p_input );
        }
        else
            msg_Warn( p_demux, "invalid line '%s'", psz_parse );

        /* Fetch another line */
        free( psz_line );
        psz_line = stream_ReadLine( p_demux->s );
    }

    input_item_node_PostAndDelete( p_subitems );

    vlc_gc_decref(p_current_input);

    // Free everything if the file is wrongly formated
    free( psz_title );
    free( psz_genre );
    free( psz_tracknum );
    free( psz_language );
    free( psz_artist );
    free( psz_album );
    free( psz_date );
    free( psz_publisher );
    free( psz_encodedby );
    free( psz_description );
    free( psz_url );
    free( psz_copyright );
    free( psz_mrl );

    var_Destroy( p_demux, "zpl-extvlcopt" );
    return 0; /* Needed for correct operation of go back */
}

static char* ParseTabValue(char* psz_string)
{
    int i_len = strlen( psz_string );
    if(i_len <= 3 )
        return NULL;
    char* psz_value = calloc( i_len, 1 );
    if( ! psz_value )
        return NULL;

    sscanf( psz_string,"%*[^=]=%[^\r\t\n]", psz_value );

    return psz_value;
}
