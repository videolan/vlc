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

#include <vlc/vlc.h>
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

/*****************************************************************************
 * Import_M3U: main import function
 *****************************************************************************/
int E_(Import_M3U)( vlc_object_t *p_this )
{
    demux_t *p_demux = (demux_t *)p_this;
    const uint8_t *p_peek;
    CHECK_PEEK( p_peek, 8 );

    if(! ( POKE( p_peek, "#EXTM3U", 7 ) || POKE( p_peek, "RTSPtext", 8 ) ||
           demux2_IsPathExtension( p_demux, ".m3u" ) || demux2_IsPathExtension( p_demux, ".vlc" ) ||
           /* A .ram file can contain a single rtsp link */
           demux2_IsPathExtension( p_demux, ".ram" ) || demux2_IsPathExtension( p_demux, ".rm" ) ||
           demux2_IsForced( p_demux,  "m3u" ) ) )
        return VLC_EGENERIC;

    STANDARD_DEMUX_INIT_MSG( "found valid M3U playlist" );
    p_demux->p_sys->psz_prefix = E_(FindPrefix)( p_demux );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Deactivate: frees unused data
 *****************************************************************************/
void E_(Close_M3U)( vlc_object_t *p_this )
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
    vlc_bool_t b_cleanup = VLC_FALSE;
    vlc_bool_t b_enable_extvlcopt = var_CreateGetInteger( p_demux,
                                                          "m3u-extvlcopt" );
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
                    i_duration = i_parsed_duration * I64C(1000000);
                if( psz_name )
                    psz_name = strdup( psz_name );
                if( psz_artist )
                    psz_artist = strdup( psz_artist );
            }
            else if( !strncasecmp( psz_parse, "EXTVLCOPT:",
                                   sizeof("EXTVLCOPT:") -1 ) )
            {
                if( b_enable_extvlcopt )
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
                else
                {
                    msg_Err( p_demux, "m3u EXTVLCOPT parsing is disabled for security reasons. If you need it and trust the m3u playlist you are trying to open, please append --m3u-extvlcopt to your command line." );
                }
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

            psz_mrl = E_(ProcessMRL)( psz_parse, p_demux->p_sys->psz_prefix );
            MaybeFromLocaleRep( &psz_mrl );

            b_cleanup = VLC_TRUE;
            if( !psz_mrl ) goto error;

            p_input = input_ItemNewExt( p_playlist, psz_mrl, psz_name,
                                        i_options, ppsz_options, i_duration );
            if ( psz_artist && *psz_artist )
                input_ItemAddInfo( p_input, _(VLC_META_INFO_CAT),
                                   _(VLC_META_ARTIST), "%s", psz_artist );
            input_ItemAddSubItem( p_current_input, p_input );
            vlc_gc_decref( p_input );
            free( psz_mrl );
        }

 error:

        /* Fetch another line */
        free( psz_line );
        psz_line = stream_ReadLine( p_demux->s );
        if( !psz_line ) b_cleanup = VLC_TRUE;

        if( b_cleanup )
        {
            /* Cleanup state */
            while( i_options-- ) free( (char*)ppsz_options[i_options] );
            if( ppsz_options ) free( ppsz_options );
            ppsz_options = NULL; i_options = 0;
            if( psz_name ) free( psz_name );
            psz_name = NULL;
            if ( psz_artist ) free( psz_artist );
            psz_artist = NULL;
            i_parsed_duration = 0;
            i_duration = -1;

            b_cleanup = VLC_FALSE;
        }
    }
    HANDLE_PLAY_AND_RELEASE;
    var_Destroy( p_demux, "m3u-extvlcopt" );
    return 0; /* Needed for correct operation of go back */
}

static int Control( demux_t *p_demux, int i_query, va_list args )
{
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

