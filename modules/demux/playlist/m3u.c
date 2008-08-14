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
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int Demux( demux_t *p_demux);
static int Control( demux_t *p_demux, int i_query, va_list args );
static void parseEXTINF( char *psz_string, char **ppsz_artist, char **ppsz_name, int *pi_duration );
static bool ContainsURL( demux_t *p_demux );

/*****************************************************************************
 * Import_M3U: main import function
 *****************************************************************************/
int Import_M3U( vlc_object_t *p_this )
{
    demux_t *p_demux = (demux_t *)p_this;
    const uint8_t *p_peek;
    CHECK_PEEK( p_peek, 8 );

    if(! ( POKE( p_peek, "#EXTM3U", 7 ) || POKE( p_peek, "RTSPtext", 8 ) ||
           demux_IsPathExtension( p_demux, ".m3u" ) || demux_IsPathExtension( p_demux, ".vlc" ) ||
           /* A .ram file can contain a single rtsp link */
           demux_IsPathExtension( p_demux, ".ram" ) || demux_IsPathExtension( p_demux, ".rm" ) ||
           demux_IsForced( p_demux,  "m3u" ) || ContainsURL( p_demux ) ) )
        return VLC_EGENERIC;

    STANDARD_DEMUX_INIT_MSG( "found valid M3U playlist" );
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
void Close_M3U( vlc_object_t *p_this )
{
    demux_t *p_demux = (demux_t *)p_this;
    free( p_demux->p_sys->psz_prefix );
    free( p_demux->p_sys );
}


/* Gruik! */
static inline char *MaybeFromLocaleDup (const char *str)
{
    if (str == NULL)
        return NULL;

    return IsUTF8 (str) ? strdup (str) : FromLocaleDup (str);
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
    int        i_parsed_duration = 0;
    mtime_t    i_duration = -1;
    const char**ppsz_options = NULL;
    int        i_options = 0;
    bool b_cleanup = false;
    input_item_t *p_input;

    INIT_PLAYLIST_STUFF;

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
                parseEXTINF( psz_parse, &psz_artist, &psz_name, &i_parsed_duration );
                if( i_parsed_duration >= 0 )
                    i_duration = i_parsed_duration * INT64_C(1000000);
                if( psz_name )
                    psz_name = strdup( psz_name );
                if( psz_artist )
                    psz_artist = strdup( psz_artist );
            }
            else if( !strncasecmp( psz_parse, "EXTVLCOPT:",
                                   sizeof("EXTVLCOPT:") -1 ) )
            {
                /* VLC Option */
                char *psz_option;
                psz_parse += sizeof("EXTVLCOPT:") -1;
                if( !*psz_parse ) goto error;

                psz_option = MaybeFromLocaleDup( psz_parse );
                if( psz_option )
                    INSERT_ELEM( ppsz_options, i_options, i_options,
                                 psz_option );
            }
        }
        else if( !strncasecmp( psz_parse, "RTSPtext", sizeof("RTSPtext") -1 ) )
        {
            ;/* special case to handle QuickTime RTSPtext redirect files */
        }
        else if( *psz_parse )
        {
            char *psz_mrl;
            if( !psz_name || !*psz_name )
            {
                /* Use filename as name for relative entries */
                psz_name = MaybeFromLocaleDup( psz_parse );
            }

            psz_mrl = ProcessMRL( psz_parse, p_demux->p_sys->psz_prefix );
            MaybeFromLocaleRep( &psz_mrl );

            b_cleanup = true;
            if( !psz_mrl ) goto error;

            p_input = input_item_NewExt( p_demux, psz_mrl, psz_name,
                                        0, NULL, i_duration );

            if ( psz_artist && *psz_artist )
                input_item_SetArtist( p_input, psz_artist );

            input_item_AddSubItem( p_current_input, p_input );
            for( int i = 0; i < i_options; i++ )
                input_item_AddOpt( p_input, ppsz_options[i], 0 );
            vlc_gc_decref( p_input );
            free( psz_mrl );
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
            free( ppsz_options );
            ppsz_options = NULL; i_options = 0;
            free( psz_name );
            psz_name = NULL;
            free( psz_artist );
            psz_artist = NULL;
            i_parsed_duration = 0;
            i_duration = -1;

            b_cleanup = false;
        }
    }
    HANDLE_PLAY_AND_RELEASE;
    var_Destroy( p_demux, "m3u-extvlcopt" );
    return 0; /* Needed for correct operation of go back */
}

static int Control( demux_t *p_demux, int i_query, va_list args )
{
    VLC_UNUSED(p_demux); VLC_UNUSED(i_query); VLC_UNUSED(args);
    return VLC_EGENERIC;
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

