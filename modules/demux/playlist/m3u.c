/*****************************************************************************
 * m3u.c : M3U playlist format import
 *****************************************************************************
 * Copyright (C) 2004 the VideoLAN team
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Sigmund Augdal Helberg <dnumgis@videolan.org>
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
    char *(*pf_dup) (const char *);
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int Demux( demux_t *p_demux);
static void parseEXTINF( char *psz_string, char **ppsz_artist, char **ppsz_name, int *pi_duration );
static bool ContainsURL( demux_t *p_demux );

static char *GuessEncoding (const char *str)
{
    return IsUTF8 (str) ? strdup (str) : FromLatin1 (str);
}

static char *CheckUnicode (const char *str)
{
    return IsUTF8 (str) ? strdup (str): NULL;
}

/*****************************************************************************
 * Import_M3U: main import function
 *****************************************************************************/
int Import_M3U( vlc_object_t *p_this )
{
    demux_t *p_demux = (demux_t *)p_this;
    const uint8_t *p_peek;
    CHECK_PEEK( p_peek, 8 );
    char *(*pf_dup) (const char *);

    if( POKE( p_peek, "RTSPtext", 8 ) /* QuickTime */
     || POKE( p_peek, "\xef\xbb\xbf" "#EXTM3U", 10) /* BOM at start */
     || demux_IsPathExtension( p_demux, ".m3u8" )
     || demux_IsForced( p_demux, "m3u8" ) )
        pf_dup = CheckUnicode; /* UTF-8 */
    else
    if( POKE( p_peek, "#EXTM3U", 7 )
     || demux_IsPathExtension( p_demux, ".m3u" )
     || demux_IsPathExtension( p_demux, ".vlc" )
     || demux_IsForced( p_demux, "m3u" )
     || ContainsURL( p_demux ) )
        pf_dup = GuessEncoding;
    else
        return VLC_EGENERIC;

    STANDARD_DEMUX_INIT_MSG( "found valid M3U playlist" );
    p_demux->p_sys->psz_prefix = FindPrefix( p_demux );
    p_demux->p_sys->pf_dup = pf_dup;

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
        /* One line starting with a URL is enough */
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
void Close_M3U( vlc_object_t *p_this )
{
    demux_t *p_demux = (demux_t *)p_this;
    free( p_demux->p_sys->psz_prefix );
    free( p_demux->p_sys );
}


static int Demux( demux_t *p_demux )
{
    char       *psz_line;
    char       *psz_name = NULL;
    char       *psz_artist = NULL;
    char       *psz_album_art = NULL;
    int        i_parsed_duration = 0;
    mtime_t    i_duration = -1;
    const char**ppsz_options = NULL;
    char *    (*pf_dup) (const char *) = p_demux->p_sys->pf_dup;
    int        i_options = 0;
    bool b_cleanup = false;
    input_item_t *p_input;

    input_item_t *p_current_input = GetCurrentItem(p_demux);
    input_item_node_t *p_subitems = input_item_node_Create( p_current_input );

    psz_line = stream_ReadLine( p_demux->s );
    while( psz_line )
    {
        char *psz_parse = psz_line;

        /* Skip leading tabs and spaces */
        while( *psz_parse == ' ' || *psz_parse == '\t' ||
               *psz_parse == '\n' || *psz_parse == '\r' ) psz_parse++;

        if( *psz_parse == '#' )
        {
            /* Parse extra info */

            /* Skip leading tabs and spaces */
            while( *psz_parse == ' ' || *psz_parse == '\t' ||
                   *psz_parse == '\n' || *psz_parse == '\r' ||
                   *psz_parse == '#' ) psz_parse++;

            if( !*psz_parse ) goto error;

            if( !strncasecmp( psz_parse, "EXTINF:", sizeof("EXTINF:") -1 ) )
            {
                /* Extended info */
                psz_parse += sizeof("EXTINF:") - 1;
                free(psz_name);
                free(psz_artist);
                parseEXTINF( psz_parse, &psz_artist, &psz_name, &i_parsed_duration );
                if( i_parsed_duration >= 0 )
                    i_duration = i_parsed_duration * INT64_C(1000000);
                if( psz_name )
                    psz_name = pf_dup( psz_name );
                if( psz_artist )
                    psz_artist = pf_dup( psz_artist );
            }
            else if( !strncasecmp( psz_parse, "EXTVLCOPT:",
                                   sizeof("EXTVLCOPT:") -1 ) )
            {
                /* VLC Option */
                char *psz_option;
                psz_parse += sizeof("EXTVLCOPT:") -1;
                if( !*psz_parse ) goto error;

                psz_option = pf_dup( psz_parse );
                if( psz_option )
                    INSERT_ELEM( ppsz_options, i_options, i_options,
                                 psz_option );
            }
            /* Special case for jamendo which provide the albumart */
            else if( !strncasecmp( psz_parse, "EXTALBUMARTURL:",
                     sizeof( "EXTALBUMARTURL:" ) -1 ) )
            {
                psz_parse += sizeof( "EXTALBUMARTURL:" ) - 1;
                free( psz_album_art );
                psz_album_art = pf_dup( psz_parse );
            }
        }
        else if( !strncasecmp( psz_parse, "RTSPtext", sizeof("RTSPtext") -1 ) )
        {
            ;/* special case to handle QuickTime RTSPtext redirect files */
        }
        else if( *psz_parse )
        {
            char *psz_mrl;

            psz_parse = pf_dup( psz_parse );
            if( !psz_name && psz_parse )
                /* Use filename as name for relative entries */
                psz_name = strdup( psz_parse );

            psz_mrl = ProcessMRL( psz_parse, p_demux->p_sys->psz_prefix );

            b_cleanup = true;
            if( !psz_mrl )
            {
                free( psz_parse );
                goto error;
            }

            p_input = input_item_NewExt( psz_mrl, psz_name,
                                        i_options, ppsz_options, 0, i_duration );

            free( psz_parse );
            free( psz_mrl );

            if ( !EMPTY_STR(psz_artist) )
                input_item_SetArtist( p_input, psz_artist );
            if( psz_name ) input_item_SetTitle( p_input, psz_name );
            if( !EMPTY_STR(psz_album_art) )
                input_item_SetArtURL( p_input, psz_album_art );

            input_item_node_AppendItem( p_subitems, p_input );
            vlc_gc_decref( p_input );
        }

 error:

        /* Fetch another line */
        free( psz_line );
        psz_line = stream_ReadLine( p_demux->s );
        if( !psz_line ) b_cleanup = true;

        if( b_cleanup )
        {
            /* Cleanup state */
            while( i_options-- ) free( (char*)ppsz_options[i_options] );
            FREENULL( ppsz_options );
            i_options = 0;
            FREENULL( psz_name );
            FREENULL( psz_artist );
            FREENULL( psz_album_art );
            i_parsed_duration = 0;
            i_duration = -1;

            b_cleanup = false;
        }
    }
    input_item_node_PostAndDelete( p_subitems );
    vlc_gc_decref(p_current_input);
    var_Destroy( p_demux, "m3u-extvlcopt" );
    return 0; /* Needed for correct operation of go back */
}

static void parseEXTINF(char *psz_string, char **ppsz_artist,
                        char **ppsz_name, int *pi_duration)
{
    char *end = NULL;
    char *psz_item = NULL;

    end = psz_string + strlen( psz_string );

    /* ignore whitespaces */
    for (; psz_string < end && ( *psz_string == '\t' || *psz_string == ' ' );
         psz_string++ );

    /* duration: read to next comma */
    psz_item = psz_string;
    psz_string = strchr( psz_string, ',' );
    if ( psz_string )
    {
        *psz_string = '\0';
        *pi_duration = atoi( psz_item );
    }
    else
    {
        return;
    }

    if ( psz_string < end )               /* continue parsing if possible */
        psz_string++;

    /* analyse the remaining string */
    psz_item = strstr( psz_string, " - " );

    /* here we have the 0.8.2+ format with artist */
    if ( psz_item )
    {
        /* *** "EXTINF:time,artist - name" */
        *psz_item = '\0';
        *ppsz_artist = psz_string;
        *ppsz_name = psz_item + 3;          /* points directly after ' - ' */
        return;
    }

    /* reaching this point means: 0.8.1- with artist or something without artist */
    if ( *psz_string == ',' )
    {
        /* *** "EXTINF:time,,name" */
        psz_string++;
        *ppsz_name = psz_string;
        return;
    }

    psz_item = psz_string;
    psz_string = strchr( psz_string, ',' );
    if ( psz_string )
    {
        /* *** "EXTINF:time,artist,name" */
        *psz_string = '\0';
        *ppsz_artist = psz_item;
        *ppsz_name = psz_string+1;
    }
    else
    {
        /* *** "EXTINF:time,name" */
        *ppsz_name = psz_item;
    }
    return;
}

