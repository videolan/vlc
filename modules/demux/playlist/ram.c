/*****************************************************************************
 * ram.c : RAM playlist format import
 *****************************************************************************
 * Copyright (C) 2009 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Srikanth Raju <srikiraju@gmail.com>
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

/*
An example:
rtsp://helixserver.example.com/video1.rm?rpcontextheight=250
&rpcontextwidth=280&rpcontexturl="http://www.example.com/relatedinfo1.html"
rtsp://helixserver.example.com/video2.rm?rpurl="http://www.example.com/index.html"
rtsp://helixserver.example.com/sample1.smil?screensize=full
rtsp://helixserver.example.com/audio1.rm?start=55&end=1:25
rtsp://helixserver.example.com/introvid.rm?title="Introduction to Streaming Media
Production"&author="RealNetworks, Inc."&copyright="&#169;2001, RealNetworks, Inc."
rtsp://helixserver.example.com/song1.rm?clipinfo="title=Artist of the Year|artist name=Your Name
Here|album name=My Debut|genre=Rock|copyright=2001|year=2001|comments=This one really
knows how to rock!"

See also:
http://service.real.com/help/library/guides/realone/IntroGuide/HTML/htmfiles/ramsum.htm
http://service.real.com/help/library/guides/realone/IntroGuide/HTML/htmfiles/ramfile.htm
*/


/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <ctype.h>

#include <vlc_common.h>
#include <vlc_demux.h>
#include <vlc_url.h>
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
static void ParseClipInfo( const char * psz_clipinfo, char **ppsz_artist, char **ppsz_title,
                           char **ppsz_album, char **ppsz_genre, char **ppsz_year,
                           char **ppsz_cdnum, char **ppsz_comments );

/**
 * Import_RAM: main import function
 * @param p_this: this demux object
 * @return VLC_SUCCESS if everything is okay
 */
int Import_RAM( vlc_object_t *p_this )
{
    demux_t *p_demux = (demux_t *)p_this;
    const uint8_t *p_peek;

    if(! demux_IsPathExtension( p_demux, ".ram" ) ||
         demux_IsPathExtension( p_demux, ".rm" ) )
        return VLC_EGENERIC;

    /* Many Real Media Files are misdetected */
    if( stream_Peek( p_demux->s, &p_peek, 4 ) < 4 )
        return VLC_EGENERIC;
    if( !memcmp( p_peek, ".ra", 3 ) || !memcmp( p_peek, ".RMF", 4 ) )
    {
        return VLC_EGENERIC;
    }

    STANDARD_DEMUX_INIT_MSG( "found valid RAM playlist" );
    p_demux->p_sys->psz_prefix = FindPrefix( p_demux );

    return VLC_SUCCESS;
}

/**
 * Frees up memory on module close
 * @param p_this: this demux object
 */
void Close_RAM( vlc_object_t *p_this )
{
    demux_t *p_demux = (demux_t *)p_this;
    free( p_demux->p_sys->psz_prefix );
    free( p_demux->p_sys );
}

/**
 * Skips blanks in a given buffer
 * @param s: input string
 * @param i_strlen: length of the buffer
 */
static const char *SkipBlanks( const char *s, size_t i_strlen )
{
    while( i_strlen > 0 ) {
        switch( *s )
        {
            case ' ':
            case '\t':
            case '\r':
            case '\n':
                --i_strlen;
                ++s;
                break;
            default:
                i_strlen = 0;
        }
    }
    return s;
}

/**
 * Converts a time of format hour:minutes:sec.fraction to seconds
 * @param s: input string
 * @param i_strlen: length of the buffer
 * @return time in seconds
 */
static int ParseTime( const char *s, size_t i_strlen)
{
    // need to parse hour:minutes:sec.fraction string
    int result = 0;
    int val;
    const char *end = s + i_strlen;
    // skip leading spaces if any
    s = SkipBlanks(s, i_strlen);

    val = 0;
    while( (s < end) && isdigit((unsigned char)*s) )
    {
        int newval = val*10 + (*s - '0');
        if( newval < val )
        {
            // overflow
            val = 0;
            break;
        }
        val = newval;
        ++s;
    }
    result = val;
    s = SkipBlanks(s, end-s);
    if( *s == ':' )
    {
        ++s;
        s = SkipBlanks(s, end-s);
        result = result * 60;
        val = 0;
        while( (s < end) && isdigit((unsigned char)*s) )
        {
            int newval = val*10 + (*s - '0');
            if( newval < val )
            {
                // overflow
                val = 0;
                break;
            }
            val = newval;
            ++s;
        }
        result += val;
        s = SkipBlanks(s, end-s);
        if( *s == ':' )
        {
            ++s;
            s = SkipBlanks(s, end-s);
            result = result * 60;
            val = 0;
            while( (s < end) && isdigit((unsigned char)*s) )
            {
                int newval = val*10 + (*s - '0');
                if( newval < val )
                {
                    // overflow
                    val = 0;
                    break;
                }
                val = newval;
                ++s;
            }
            result += val;
            // TODO: one day, we may need to parse fraction for sub-second resolution
        }
    }
    return result;
}

/**
 * Main demux callback function
 * @param p_demux: this demux object
 */
static int Demux( demux_t *p_demux )
{
    char       *psz_line;
    char       *psz_artist = NULL, *psz_album = NULL, *psz_genre = NULL, *psz_year = NULL;
    char       *psz_author = NULL, *psz_title = NULL, *psz_copyright = NULL, *psz_cdnum = NULL, *psz_comments = NULL;
    mtime_t    i_duration = -1;
    const char **ppsz_options = NULL;
    int        i_options = 0, i_start = 0, i_stop = 0;
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
            /* Ignore comments */
        }
        else if( *psz_parse )
        {
            char *psz_mrl, *psz_option_next, *psz_option;
            char *psz_param, *psz_value;

            /* Get the MRL from the file. Note that this might contain parameters of form ?param1=value1&param2=value2 in a RAM file */
            psz_mrl = ProcessMRL( psz_parse, p_demux->p_sys->psz_prefix );

            b_cleanup = true;
            if ( !psz_mrl ) goto error;

            /* We have the MRL, now we have to check for options and parse them from MRL */
            psz_option = strchr( psz_mrl, '?' ); /* Look for start of options */
            if( psz_option )
            {
                /* Remove options from MRL
                   because VLC can't get the file otherwise */
                *psz_option = '\0';
                psz_option++;
                psz_option_next = psz_option;
                while( 1 ) /* Process each option */
                {
                    /* Look for end of first option which maybe a & or \0 */
                    psz_option = psz_option_next;
                    psz_option_next = strchr( psz_option, '&' );
                    if( psz_option_next )
                    {
                        *psz_option_next = '\0';
                        psz_option_next++;
                    }
                    else
                        psz_option_next = strchr( psz_option, '\0' );
                    /* Quit if options are over */
                    if( psz_option_next == psz_option )
                        break;

                    /* Parse out param and value */
                    psz_param = psz_option;
                    psz_value = strchr( psz_option, '=' );
                    if( psz_value == NULL )
                        break;
                    *psz_value = '\0';
                    psz_value++;

                    /* Take action based on parameter value in the below if else structure */
                    /* TODO: Remove any quotes surrounding values if required */
                    if( !strcmp( psz_param, "clipinfo" ) )
                    {
                        ParseClipInfo( psz_value, &psz_artist, &psz_title,
                           &psz_album, &psz_genre, &psz_year,
                           &psz_cdnum, &psz_comments ); /* clipinfo has various sub parameters, which is parsed by this function */
                    }
                    else if( !strcmp( psz_param, "author" ) )
                    {
                        psz_author = decode_URI_duplicate(psz_value);
                        EnsureUTF8( psz_author );
                    }
                    else if( !strcmp( psz_param, "start" )
                            && strncmp( psz_mrl, "rtsp", 4 ) /* Our rtsp-real or our real demuxer is wrong */  )
                    {
                        i_start = ParseTime( psz_value, strlen( psz_value ) );
                        char *temp;
                        if( i_start )
                        {
                            if( asprintf( &temp, ":start-time=%d", i_start ) != -1 )
                                INSERT_ELEM( ppsz_options, i_options, i_options, temp );
                        }
                    }
                    else if( !strcmp( psz_param, "end" ) )
                    {
                        i_stop = ParseTime( psz_value, strlen( psz_value ) );
                        char *temp;
                        if( i_stop )
                        {
                            if( asprintf( &temp, ":stop-time=%d", i_stop ) != -1 )
                                INSERT_ELEM( ppsz_options, i_options, i_options, temp );
                        }
                    }
                    else if( !strcmp( psz_param, "title" ) )
                    {
                        psz_title = decode_URI_duplicate(psz_value);
                        EnsureUTF8( psz_title );
                    }
                    else if( !strcmp( psz_param, "copyright" ) )
                    {
                        psz_copyright = decode_URI_duplicate(psz_value);
                        EnsureUTF8( psz_copyright );
                    }
                    else
                    {   /* TODO: insert option anyway? Currently ignores*/
                        /* INSERT_ELEM( ppsz_options, i_options, i_options, psz_option ); */
                    }
                }
            }

            /* Create the input item and pump in all the options into playlist item */
            p_input = input_item_NewExt( psz_mrl, psz_title, i_options, ppsz_options, 0, i_duration );

            if( !EMPTY_STR( psz_artist ) ) input_item_SetArtist( p_input, psz_artist );
            if( !EMPTY_STR( psz_author ) ) input_item_SetPublisher( p_input, psz_author );
            if( !EMPTY_STR( psz_title ) ) input_item_SetTitle( p_input, psz_title );
            if( !EMPTY_STR( psz_copyright ) ) input_item_SetCopyright( p_input, psz_copyright );
            if( !EMPTY_STR( psz_album ) ) input_item_SetAlbum( p_input, psz_album );
            if( !EMPTY_STR( psz_genre ) ) input_item_SetGenre( p_input, psz_genre );
            if( !EMPTY_STR( psz_year ) ) input_item_SetDate( p_input, psz_year );
            if( !EMPTY_STR( psz_cdnum ) ) input_item_SetTrackNum( p_input, psz_cdnum );
            if( !EMPTY_STR( psz_comments ) ) input_item_SetDescription( p_input, psz_comments );

            input_item_node_AppendItem( p_subitems, p_input );
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
            FREENULL( ppsz_options );
            FREENULL( psz_artist );
            FREENULL( psz_title );
            FREENULL( psz_author );
            FREENULL( psz_copyright );
            FREENULL( psz_album );
            FREENULL( psz_genre );
            FREENULL( psz_year );
            FREENULL( psz_cdnum );
            FREENULL( psz_comments );
            i_options = 0;
            i_duration = -1;
            i_start = 0;
            i_stop = 0;
            b_cleanup = false;
        }
    }
    input_item_node_PostAndDelete( p_subitems );
    vlc_gc_decref(p_current_input);
    var_Destroy( p_demux, "m3u-extvlcopt" );
    return 0; /* Needed for correct operation of go back */
}

/**
 * Parses clipinfo parameter
 * @param psz_clipinfo: string containing the clipinfo parameter along with quotes
 * @param ppsz_artist: Buffer to store artist name
 * @param ppsz_title: Buffer to store title
 * @param ppsz_album: Buffer to store album
 * @param ppsz_genre: Buffer to store genre
 * @param ppsz_year: Buffer to store year
 * @param ppsz_cdnum: Buffer to store cdnum
 * @param ppsz_comments: Buffer to store comments
 */
static void ParseClipInfo( const char *psz_clipinfo, char **ppsz_artist, char **ppsz_title,
                           char **ppsz_album, char **ppsz_genre, char **ppsz_year,
                           char **ppsz_cdnum, char **ppsz_comments )
{
    char *psz_option_next, *psz_option_start, *psz_param, *psz_value, *psz_suboption;
    char *psz_temp_clipinfo = strdup( psz_clipinfo );
    psz_option_start = strchr( psz_temp_clipinfo, '"' );
    if( !psz_option_start )
    {
        free( psz_temp_clipinfo );
        return;
    }

    psz_option_start++;
    psz_option_next = psz_option_start;
    while( 1 ) /* Process each sub option */
    {
        /* Get the sub option */
        psz_option_start = psz_option_next;
        psz_option_next = strchr( psz_option_start, '|' );
        if( psz_option_next )
            *psz_option_next = '\0';
        else
            psz_option_next = strchr( psz_option_start, '"' );
        if( psz_option_next )
            *psz_option_next = '\0';
        else
            psz_option_next = strchr( psz_option_start, '\0' );
        if( psz_option_next == psz_option_start )
            break;

        psz_suboption = strdup( psz_option_start );
        if( !psz_suboption )
            break;

        /* Parse out param and value */
        psz_param = psz_suboption;
        if( strchr( psz_suboption, '=' ) )
        {
            psz_value = strchr( psz_suboption, '=' ) + 1;
            *( strchr( psz_suboption, '=' ) ) = '\0';
        }
        else
            break;
        /* Put into args */
        if( !strcmp( psz_param, "artist name" ) )
            *ppsz_artist = decode_URI_duplicate( psz_value );
        else if( !strcmp( psz_param, "title" ) )
            *ppsz_title = decode_URI_duplicate( psz_value );
        else if( !strcmp( psz_param, "album name" ) )
            *ppsz_album = decode_URI_duplicate( psz_value );
        else if( !strcmp( psz_param, "genre" ) )
            *ppsz_genre = decode_URI_duplicate( psz_value );
        else if( !strcmp( psz_param, "year" ) )
            *ppsz_year = decode_URI_duplicate( psz_value );
        else if( !strcmp( psz_param, "cdnum" ) )
            *ppsz_cdnum = decode_URI_duplicate( psz_value );
        else if( !strcmp( psz_param, "comments" ) )
            *ppsz_comments = decode_URI_duplicate( psz_value );

        free( psz_suboption );
        psz_option_next++;
    }

    free( psz_temp_clipinfo );
}
