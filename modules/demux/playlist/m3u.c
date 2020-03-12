/*****************************************************************************
 * m3u.c : M3U playlist format import
 *****************************************************************************
 * Copyright (C) 2004 VLC authors and VideoLAN
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Sigmund Augdal Helberg <dnumgis@videolan.org>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_access.h>
#include <vlc_charset.h>
#include <vlc_strings.h>

#include "playlist.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int ReadDir( stream_t *, input_item_node_t * );
static bool ContainsURL(const uint8_t *, size_t);

static char *GuessEncoding (const char *str)
{
    return IsUTF8 (str) ? strdup (str) : FromLatin1 (str);
}

static char *CheckUnicode (const char *str)
{
    return IsUTF8 (str) ? strdup (str): NULL;
}

static bool IsHLS(const unsigned char *buf, size_t length)
{
    static const char *const hlsexts[] =
    {
        "#EXT-X-MEDIA:",
        "#EXT-X-VERSION:",
        "#EXT-X-TARGETDURATION:",
        "#EXT-X-MEDIA-SEQUENCE:",
        "#EXT-X-STREAM-INF:",
    };

    for (size_t i = 0; i < ARRAY_SIZE(hlsexts); i++)
        if (strnstr((const char *)buf, hlsexts[i], length) != NULL)
            return true;

    return false;
}

/*****************************************************************************
 * Import_M3U: main import function
 *****************************************************************************/
int Import_M3U( vlc_object_t *p_this )
{
    stream_t *p_stream = (stream_t *)p_this;
    const uint8_t *p_peek;
    ssize_t i_peek;
    int offset = 0;

    CHECK_FILE(p_stream);
    i_peek = vlc_stream_Peek( p_stream->s, &p_peek, 1024 );
    if( i_peek < 8 )
        return VLC_EGENERIC;

    /* Encoding: UTF-8 or unspecified */
    char *(*pf_dup) (const char *) = GuessEncoding;

    if (stream_HasExtension(p_stream, ".m3u8")
     || strncasecmp((const char *)p_peek, "RTSPtext", 8) == 0) /* QuickTime */
        pf_dup = CheckUnicode;
    else
    if (memcmp( p_peek, "\xef\xbb\xbf", 3) == 0) /* UTF-8 Byte Order Mark */
    {
        if( i_peek < 12 )
            return VLC_EGENERIC;
        pf_dup = CheckUnicode;
        offset = 3;
        p_peek += offset;
        i_peek -= offset;
    }

    /* File type: playlist, or not (HLS manifest or whatever else) */
    char *type = stream_MimeType(p_stream->s);
    bool match;

    if (p_stream->obj.force)
        match = true;
    else
    if (type != NULL
     && !vlc_ascii_strcasecmp(type, "application/vnd.apple.mpegurl")) /* HLS */
        match = false;
    else
    if (memcmp(p_peek, "#EXTM3U", 7 ) == 0
     || (type != NULL
      && (vlc_ascii_strcasecmp(type, "application/mpegurl") == 0
       || vlc_ascii_strcasecmp(type, "application/x-mpegurl") == 0
       || vlc_ascii_strcasecmp(type, "audio/mpegurl") == 0
       || vlc_ascii_strcasecmp(type, "vnd.apple.mpegURL") == 0
       || vlc_ascii_strcasecmp(type, "audio/x-mpegurl") == 0))
     || stream_HasExtension(p_stream, ".m3u8")
     || stream_HasExtension(p_stream, ".m3u"))
        match = !IsHLS(p_peek, i_peek);
    else
    if (stream_HasExtension(p_stream, ".vlc")
     || strncasecmp((const char *)p_peek, "RTSPtext", 8) == 0
     || ContainsURL(p_peek, i_peek))
        match = true;
    else
        match = false;

    free(type);

    if (!match)
        return VLC_EGENERIC;

    if (offset != 0 && vlc_stream_Seek(p_stream->s, offset))
        return VLC_EGENERIC;

    msg_Dbg( p_stream, "found valid M3U playlist" );
    p_stream->p_sys = pf_dup;
    p_stream->pf_readdir = ReadDir;
    p_stream->pf_control = access_vaDirectoryControlHelper;

    return VLC_SUCCESS;
}

static bool ContainsURL(const uint8_t *p_peek, size_t i_peek)
{
    const char *ps = (const char *)p_peek;
    const char *ps_end = (const char *) p_peek + i_peek;
    const size_t i_max = sizeof( "https://" );
    if( i_peek < i_max + 1 )
        return false;

    bool b_newline = true;
    while( ps + i_max + 1 < ps_end )
    {
        if( *ps <= 0 )
            return false;

        /* Goto next line */
        if( *ps == '\n' )
        {
            ps++;
            b_newline = true;
            continue;
        }

        /* One line starting with a URL is enough */
        if( b_newline )
        {
            const char *ps_match = strnstr( ps, "://", i_max );
            if(ps_match)
            {
                switch(ps_match - ps)
                {
                    case 3:
                        if( !strncasecmp( ps, "mms", 3 ) ||
                            !strncasecmp( ps, "ftp", 3 ) )
                            return true;
                        break;
                    case 4:
                        if( !strncasecmp( ps, "http", 4 ) ||
                            !strncasecmp( ps, "rtsp", 4 ) ||
                            !strncasecmp( ps, "ftps", 4 ) )
                            return true;
                        break;
                    case 5:
                        if( !strncasecmp( ps, "https", 5 ) ||
                            !strncasecmp( ps, "ftpes", 5 ) )
                            return true;
                    default:
                        break;
                }
            }

            /* Comments and blank lines are ignored */
            if( *ps != '#' && *ps != '\n' && *ps != '\r')
                return false;

            b_newline = false;
        }

        ps++;
    }
    return false;
}

struct entry_meta_s
{
    char *psz_name;
    char *psz_artist;
    char *psz_album_art;
    char *psz_mrl;
    char *psz_language;
    char *psz_tvgid;
    char *psz_grouptitle;
    vlc_tick_t i_duration;
    const char**ppsz_options;
    int   i_options;
};

static void entry_meta_Init( struct entry_meta_s *e )
{
    memset(e, 0, sizeof(*e));
    e->i_duration = INPUT_DURATION_INDEFINITE;
}

static void entry_meta_Clean( struct entry_meta_s *e )
{
    free( e->psz_name );
    free( e->psz_artist );
    free( e->psz_album_art );
    free( e->psz_mrl );
    free( e->psz_language );
    free( e->psz_tvgid );
    free( e->psz_grouptitle );
    while( e->i_options-- ) free( (char*)e->ppsz_options[e->i_options] );
}

static void parseEXTINF( char *, char *(*)(const char *), struct entry_meta_s * );

static int CreateEntry( input_item_node_t *p_node, const struct entry_meta_s *meta )
{
    if( !meta->psz_mrl )
        return VLC_EGENERIC;

    input_item_t *p_input =
        input_item_NewExt( meta->psz_mrl, meta->psz_name, meta->i_duration,
                           ITEM_TYPE_UNKNOWN, ITEM_NET_UNKNOWN );
    if( !p_input )
        return VLC_EGENERIC;

    input_item_AddOptions( p_input, meta->i_options, meta->ppsz_options, 0 );

    if( meta->psz_artist )
        input_item_SetArtist( p_input, meta->psz_artist );
    if( meta->psz_name )
        input_item_SetTitle( p_input, meta->psz_name );
    if( meta->psz_album_art )
        input_item_SetArtURL( p_input, meta->psz_album_art );
    if( meta->psz_language )
        input_item_SetLanguage( p_input, meta->psz_language );
    if( meta->psz_tvgid )
        input_item_AddInfo( p_input, "XMLTV", "tvg-id", "%s", meta->psz_tvgid );
    if( meta->psz_grouptitle )
        input_item_SetAlbum( p_input, meta->psz_grouptitle );

    input_item_node_AppendItem( p_node, p_input );
    input_item_Release( p_input );

    return VLC_SUCCESS;
}

static int ReadDir( stream_t *p_demux, input_item_node_t *p_subitems )
{
    char       *psz_line;
    struct entry_meta_s meta;
    entry_meta_Init( &meta );
    char *    (*pf_dup) (const char *) = p_demux->p_sys;

    psz_line = vlc_stream_ReadLine( p_demux->s );
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

            if( !*psz_parse ) goto nextline;

            if( !strncasecmp( psz_parse, "EXTINF:", sizeof("EXTINF:") -1 ) )
            {
                /* Extended info */
                psz_parse += sizeof("EXTINF:") - 1;
                meta.i_duration = INPUT_DURATION_INDEFINITE;
                parseEXTINF( psz_parse, pf_dup, &meta );
            }
            else if( !strncasecmp( psz_parse, "EXTVLCOPT:",
                                   sizeof("EXTVLCOPT:") -1 ) )
            {
                /* VLC Option */
                char *psz_option;
                psz_parse += sizeof("EXTVLCOPT:") -1;
                if( !*psz_parse ) goto nextline;

                psz_option = pf_dup( psz_parse );
                if( psz_option )
                    TAB_APPEND( meta.i_options, meta.ppsz_options, psz_option );
            }
            /* Special case for jamendo which provide the albumart */
            else if( !strncasecmp( psz_parse, "EXTALBUMARTURL:",
                     sizeof( "EXTALBUMARTURL:" ) -1 ) )
            {
                psz_parse += sizeof( "EXTALBUMARTURL:" ) - 1;
                if( *psz_parse )
                {
                    free( meta.psz_album_art );
                    meta.psz_album_art = pf_dup( psz_parse );
                }
            }
        }
        else if( !strncasecmp( psz_parse, "RTSPtext", sizeof("RTSPtext") -1 ) )
        {
            ;/* special case to handle QuickTime RTSPtext redirect files */
        }
        else if( *psz_parse )
        {
            psz_parse = pf_dup( psz_parse );
            if( !meta.psz_name && psz_parse )
                /* Use filename as name for relative entries */
                meta.psz_name = strdup( psz_parse );

            meta.psz_mrl = ProcessMRL( psz_parse, p_demux->psz_url );
            free( psz_parse );

            CreateEntry( p_subitems, &meta );

            /* Cleanup state after entry */
            entry_meta_Clean( &meta );
            entry_meta_Init( &meta );
        }

 nextline:
        /* Fetch another line */
        free( psz_line );
        psz_line = vlc_stream_ReadLine( p_demux->s );
        if( !psz_line )
        {
            /* Cleanup state */
            entry_meta_Clean( &meta );
            entry_meta_Init( &meta );
        }
    }
    return VLC_SUCCESS; /* Needed for correct operation of go back */
}

static void parseEXTINFTitle( char *psz_string,
                              char *(*pf_dup) (const char *),
                              struct entry_meta_s *meta )
{
    char *psz_item = strstr( psz_string, " - " );

    /* here we have the 0.8.2+ format with artist */
    if ( psz_item )
    {
        /* *** "EXTINF:time,artist - name" */
        *psz_item = '\0';
        if( *psz_string )
            meta->psz_artist = pf_dup( psz_string );
        if( psz_item[3] )
            meta->psz_name = pf_dup( &psz_item[3] ); /* points directly after ' - ' */
        return;
    }

    /* reaching this point means: 0.8.1- with artist or something without artist */
    if ( *psz_string == ',' )
    {
        /* *** "EXTINF:time,,name" */
        psz_string++;
        if( *psz_string )
            meta->psz_name = pf_dup( psz_string );
        return;
    }

    psz_item = psz_string;
    psz_string = strchr( psz_string, ',' );
    if ( psz_string )
    {
        /* *** "EXTINF:time,artist,name" */
        *psz_string = '\0';
        if( *psz_item )
            meta->psz_artist = pf_dup( psz_item );
        if( psz_string[1] )
            meta->psz_name = pf_dup( &psz_string[1] );
    }
    else
    {
        /* *** "EXTINF:time,name" */
        if( *psz_item )
            meta->psz_name = pf_dup( psz_item );
    }
}

static void parseEXTINFIptvDiots( char *psz_string,
                                  char *(*pf_dup)(const char *),
                                  struct entry_meta_s *meta )
{
    char **ppsz_meta = NULL;
    if( strncmp( psz_string, "tvg-", 4 ) &&
        strncmp( psz_string, "group-", 6 ) )
        return;
    char *psz_sep = strchr( psz_string + 4, '=' );
    if( unlikely(!psz_sep) )
        return;
    size_t i_keylen = psz_sep - psz_string;

    if( !strncmp( psz_string + 4, "logo", i_keylen - 4 ) )
        ppsz_meta = &meta->psz_album_art;
    else if( !strncmp( psz_string + 4, "name", i_keylen - 4 ) )
        ppsz_meta = &meta->psz_name;
    else if( !strncmp( psz_string + 4, "language", i_keylen - 4 ) )
        ppsz_meta = &meta->psz_language;
    else if( !strncmp( psz_string + 4, "id", i_keylen - 4 ) )
        ppsz_meta = &meta->psz_tvgid;
    else if( !strncmp( psz_string + 6, "title", i_keylen - 4 ) )
        ppsz_meta = &meta->psz_grouptitle;

    if( !ppsz_meta || *ppsz_meta /* no overwrite */ )
        return;

    char *psz_value = psz_sep + 1;
    size_t i_valuelen = strlen( psz_value );
    if( unlikely(i_valuelen == 0) )
        return;

    bool b_escaped = (*psz_value == '"');
    if( i_valuelen > 2 && b_escaped )
    {
        psz_value[ i_valuelen - 1 ] = 0;
        *ppsz_meta = pf_dup( psz_value + 1 );
    }
    else
        *ppsz_meta = pf_dup( psz_value );
}

static void parseEXTINFIptvDiotsInDuration( char *psz_string,
                                            char *(*pf_dup)(const char *),
                                            struct entry_meta_s *meta )
{
    for( ;; )
    {
        while( *psz_string == '\t' || *psz_string == ' ' )
            psz_string++;

        char *psz_start = psz_string;
        bool b_escaped = false;
        bool b_key = false;
        bool b_value = false;
        bool b_next = false;

        for( ;!b_next; ++psz_string )
        {
            switch( *psz_string )
            {
                case '"':
                    if(!b_escaped && b_value)
                        return;
                    if(!b_key)
                        return;
                    b_value = true;
                    b_escaped = !b_escaped;
                    break;
                case '\0':
                    if(!b_escaped && b_value)
                        parseEXTINFIptvDiots( psz_start, pf_dup, meta );
                    return;
                case ' ':
                    if(!b_escaped)
                    {
                        *psz_string = '\0';
                        if(b_value)
                            parseEXTINFIptvDiots( psz_start, pf_dup, meta );
                        b_next = true;
                    }
                    break;
                case '=':
                    b_key = true;
                    break;
                default:
                    if(b_key)
                        b_value = true;
                    break;
            }
        }
    }
}

static void parseEXTINF( char *psz_string,
                         char *(*pf_dup)(const char *),
                         struct entry_meta_s *meta )
{
    char *end = psz_string + strlen( psz_string );

    FREENULL( meta->psz_name );
    FREENULL( meta->psz_artist );

    /* strip leading whitespaces */
    while( psz_string < end && ( *psz_string == '\t' || *psz_string == ' ' ) )
        psz_string++;

    /* duration: read to next comma */
    char *psz_comma = strchr( psz_string, ',' );
    if( psz_comma )
    {
        *psz_comma = '\0'; /* Split strings */
        if( ++psz_comma < end )
            parseEXTINFTitle( psz_comma, pf_dup, meta );
    }

    /* Parse duration */
    char *psz_end = NULL;
    long i_parsed_duration = strtol( psz_string, &psz_end, 10 );
    if( i_parsed_duration > 0 )
        meta->i_duration = vlc_tick_from_sec( i_parsed_duration );

    if( psz_end && psz_end != psz_string && ( *psz_end == '\t' || *psz_end == ' ' ) )
    {
        parseEXTINFIptvDiotsInDuration( psz_end, pf_dup, meta );
    }
}

