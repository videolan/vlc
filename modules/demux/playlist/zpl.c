/*****************************************************************************
 * zpl.c : ZPL playlist format import
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
static char* ParseTabValue(char* psz_string);

/*****************************************************************************
 * Import_ZPL: main import function
 *****************************************************************************/
int Import_ZPL( vlc_object_t *p_this )
{
    demux_t *p_demux = (demux_t *)p_this;
    const uint8_t *p_peek;
    CHECK_PEEK( p_peek, 8 );

    if(! ( demux_IsPathExtension( p_demux, ".zpl" ) || demux_IsForced( p_demux,  "zpl" )))
        return VLC_EGENERIC;

    STANDARD_DEMUX_INIT_MSG( "found valid ZPL playlist" );
    p_demux->p_sys->psz_prefix = FindPrefix( p_demux );

    return VLC_SUCCESS;
}

static bool ContainsURL( demux_t *p_demux )
{
    const uint8_t *p_peek, *p_peek_end;
    int i_peek;

    i_peek = stream_Peek( p_demux->s, &p_peek, 1024 );
    if( i_peek <= 0 ) return false;
    p_peek_end = p_peek + i_peek;

    while( p_peek + sizeof( "https://" ) < p_peek_end )
    {
        /* One line starting with an URL is enough */
        if( !strncasecmp( (const char *)p_peek, "http://", 7 ) ||
            !strncasecmp( (const char *)p_peek, "mms://", 6 ) ||
            !strncasecmp( (const char *)p_peek, "rtsp://", 7 ) ||
            !strncasecmp( (const char *)p_peek, "https://", 8 ) ||
            !strncasecmp( (const char *)p_peek, "ftp://", 6 ) )
        {
            return true;
        }
        /* Comments and blank lines are ignored */
        else if( *p_peek != '#' && *p_peek != '\n' && *p_peek != '\r')
        {
            return false;
        }

        while( p_peek < p_peek_end && *p_peek != '\n' )
            p_peek++;
        if ( *p_peek == '\n' )
            p_peek++;
    }
    return false;
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

static inline void MaybeFromLocaleRep (char **str)
{
    char *const orig_str = *str;

    if ((orig_str != NULL) && !IsUTF8 (orig_str))
    {
        *str = FromLocaleDup (orig_str);
        free (orig_str);
    }
}


static int Demux( demux_t *p_demux )
{
    char       *psz_line;
    char       *psz_name = NULL;
    char       *psz_artist = NULL;
    char       *psz_tabvalue = NULL;
    int        i_parsed_duration = 0;
    mtime_t    i_duration = -1;
    input_item_t *p_input;

    INIT_PLAYLIST_STUFF;

    psz_line = stream_ReadLine( p_demux->s );
    char *psz_parse = psz_line;

    /* Skip leading tabs and spaces */
    while( *psz_parse == ' ' || *psz_parse == '\t' ||
        *psz_parse == '\n' || *psz_parse == '\r' ) psz_parse++;
    if( !strncasecmp( psz_parse, "AC", sizeof( "AC" ) - 1 ) )/*if the 1st line is "AC"*/
    {
        free( psz_line );
        psz_line = stream_ReadLine( p_demux->s );
    }

    while( psz_line )
    {
        psz_parse = psz_line;

        /* Skip leading tabs and spaces */
        while( *psz_parse == ' ' || *psz_parse == '\t' ||
        *psz_parse == '\n' || *psz_parse == '\r' ) psz_parse++;

        if( !strncasecmp( psz_parse, "NM", sizeof( "NM" ) - 1 ) )/*if the  line is "NM"*/
        {

            psz_tabvalue = ParseTabValue( psz_parse );
            if( psz_tabvalue && *psz_tabvalue )
            {
                psz_tabvalue = ProcessMRL( psz_tabvalue, p_demux->p_sys->psz_prefix );
                MaybeFromLocaleRep( &psz_tabvalue );
                p_input = input_item_NewExt( p_demux, psz_tabvalue, psz_tabvalue,
                                        0, NULL, 0, i_duration );
            }

        }

        else if( !strncasecmp( psz_parse, "DR", sizeof( "DR" ) - 1 ) )/*if the  line is "DR"*/
        {
            psz_tabvalue = ParseTabValue( psz_parse );
            if( psz_tabvalue && *psz_tabvalue )
            {
                i_parsed_duration = atoi( psz_tabvalue );
                if( i_parsed_duration >= 0 )
                {
                    i_duration = i_parsed_duration * INT64_C(1000);
                    if( p_input )
                        input_item_SetDuration(p_input, i_duration);
                }
            }
        }

       else if( !strncasecmp( psz_parse, "TT", sizeof( "TT" ) - 1 ) )/*if the  line is "TT"*/
        {
            psz_tabvalue = ParseTabValue( psz_parse );
            if( psz_tabvalue && *psz_tabvalue )
            {
                if( p_input )
                    input_item_SetTitle( p_input, psz_tabvalue );
            }
        }

       else if( !strncasecmp( psz_parse, "TG", sizeof( "TG" ) - 1 ) )/*if the  line is "TG"*/
        {
            psz_tabvalue = ParseTabValue( psz_parse );
            if( psz_tabvalue && *psz_tabvalue )
            {
                if( p_input )
                    input_item_SetGenre( p_input, psz_tabvalue );
            }
        }

        else if( !strncasecmp( psz_parse, "TR", sizeof( "TR" ) - 1 ) )/*if the  line is "TR"*/
        {
            psz_tabvalue = ParseTabValue( psz_parse );
            if( psz_tabvalue && *psz_tabvalue )
            {
                int tracNum = atoi(psz_tabvalue);
                if( p_input )
                    input_item_SetTrackNum(p_input, tracNum);
            }
        }

        else if( !strncasecmp( psz_parse, "TL", sizeof( "TL" ) - 1 ) )/*if the  line is "TL"*/
        {
            psz_tabvalue = ParseTabValue( psz_parse );
            if( psz_tabvalue && *psz_tabvalue )
            {
                if( p_input )
                    input_item_SetLanguage( p_input, psz_tabvalue );
            }
        }

        else if( !strncasecmp( psz_parse, "TA", sizeof( "TA" ) - 1 ) )/*if the  line is "TA"*/
        {
            psz_tabvalue = ParseTabValue( psz_parse );
            if( psz_tabvalue && *psz_tabvalue )
            {
                if( p_input )
                    input_item_SetArtist( p_input, psz_tabvalue );
            }
        }

        else if( !strncasecmp( psz_parse, "TB", sizeof( "TB" ) - 1 ) )/*if the  line is "TB"*/
        {
            psz_tabvalue = ParseTabValue( psz_parse );
            if( psz_tabvalue && *psz_tabvalue )
            {
                if( p_input )
                    input_item_SetAlbum( p_input, psz_tabvalue );
            }
        }

        else if( !strncasecmp( psz_parse, "TY", sizeof( "TY" ) - 1 ) )/*if the  line is "TY"*/
        {
            psz_tabvalue = ParseTabValue( psz_parse );
            if( psz_tabvalue && *psz_tabvalue )
            {
                if( p_input )
                    input_item_SetDate( p_input, psz_tabvalue );
            }
        }

        else if( !strncasecmp( psz_parse, "TH", sizeof( "TH" ) - 1 ) )/*if the  line is "TH"*/
        {
            psz_tabvalue = ParseTabValue( psz_parse );
            if( psz_tabvalue && *psz_tabvalue )
            {
                if( p_input )
                    input_item_SetPublisher( p_input, psz_tabvalue );
            }
        }

        else if( !strncasecmp( psz_parse, "TE", sizeof( "TE" ) - 1 ) )/*if the  line is "TE"*/
        {
            psz_tabvalue = ParseTabValue( psz_parse );
            if( psz_tabvalue && *psz_tabvalue )
            {
                if( p_input )
                    input_item_SetEncodedBy( p_input, psz_tabvalue );
            }
        }

        else if( !strncasecmp( psz_parse, "TC", sizeof( "TC" ) - 1 ) )/*if the  line is "TC"*/
        {
            psz_tabvalue = ParseTabValue( psz_parse );
            if( psz_tabvalue && *psz_tabvalue )
            {
                if( p_input )
                    input_item_SetDescription( p_input, psz_tabvalue );
            }
        }

        else if( !strncasecmp( psz_parse, "TU", sizeof( "TU" ) - 1 ) )/*if the  line is "TU"*/
        {
            psz_tabvalue = ParseTabValue( psz_parse );
            if( psz_tabvalue && *psz_tabvalue )
            {
                if( p_input )
                    input_item_SetURL( p_input, psz_tabvalue );
            }
        }

        else if( !strncasecmp( psz_parse, "TO", sizeof( "TO" ) - 1 ) )/*if the  line is "TO"*/
        {
            psz_tabvalue = ParseTabValue( psz_parse );
            if( psz_tabvalue && *psz_tabvalue )
            {
                if( p_input )
                    input_item_SetCopyright( p_input, psz_tabvalue );
            }
        }

        else if( !strncasecmp( psz_parse, "FD", sizeof( "FD" ) - 1 ) )/*if the  line is "FD"*/
        {

        }

        else if( !strncasecmp( psz_parse, "BR!", sizeof( "BR!" ) - 1 ) )/*if the  line is "BR!"*/
        {
            input_item_AddSubItem( p_current_input, p_input );
            p_input = NULL;
        }

        if( psz_tabvalue )
            free( psz_tabvalue );
        psz_tabvalue = NULL;
        free( psz_line );
        psz_line = NULL;

        /* Fetch another line */
        psz_line = stream_ReadLine( p_demux->s );

        i_parsed_duration = 0;
        i_duration = -1;

    }
    HANDLE_PLAY_AND_RELEASE;
    var_Destroy( p_demux, "zpl-extvlcopt" );
    return 0; /* Needed for correct operation of go back */
}

static int Control( demux_t *p_demux, int i_query, va_list args )
{
    VLC_UNUSED(p_demux); VLC_UNUSED(i_query); VLC_UNUSED(args);
    return VLC_EGENERIC;
}

static char* ParseTabValue(char* psz_string)
{
    int i_len = strlen( psz_string );
    if(i_len <= 3 )
        return NULL;
    char* psz_value = (char *)malloc( i_len );
    if( ! psz_value )
        return NULL;
    memset( psz_value, 0, i_len );
    sscanf( psz_string,"%*[^=]=%[^\0\r\t\n]", psz_value );

    if( strlen( psz_value ) )
        return psz_value;
    free( psz_value );
    return NULL;
}


