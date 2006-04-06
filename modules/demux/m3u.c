/*****************************************************************************
 * m3u.c: a meta demux to parse pls, m3u, asx et b4s playlists
 *****************************************************************************
 * Copyright (C) 2001-2006 the VideoLAN team
 * $Id$
 *
 * Authors: Sigmund Augdal Helberg <dnumgis@videolan.org>
 *          Gildas Bazin <gbazin@videolan.org>
 *          Clément Stenac <zorglub@via.ecp.fr>
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
#include <stdlib.h>                                      /* malloc(), free() */

#include <vlc/vlc.h>
#include <vlc/input.h>
#include <vlc_playlist.h>

/*****************************************************************************
 * Constants and structures
 *****************************************************************************/
#define MAX_LINE 8192

#define TYPE_UNKNOWN 0
#define TYPE_M3U 1
#define TYPE_ASX 2
#define TYPE_HTML 3
#define TYPE_PLS 4
#define TYPE_B4S 5
#define TYPE_WMP 6
#define TYPE_RTSP 7

struct demux_sys_t
{
    int i_type;                                   /* playlist type (m3u/asx) */
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Activate  ( vlc_object_t * );
static void Deactivate( vlc_object_t * );
static int  Demux     ( demux_t * );
static int  Control   ( demux_t *, int, va_list );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_category( CAT_INPUT );
    set_subcategory( SUBCAT_INPUT_DEMUX );
    set_description( _("Playlist metademux") );
    set_capability( "demux2", 5 );
    set_callbacks( Activate, Deactivate );
    add_shortcut( "m3u" );
    add_shortcut( "asx" );
    add_shortcut( "html" );
    add_shortcut( "pls" );
    add_shortcut( "b4s" );
vlc_module_end();

/*****************************************************************************
 * Activate: initializes m3u demux structures
 *****************************************************************************/
static int Activate( vlc_object_t * p_this )
{
    demux_t *p_demux = (demux_t *)p_this;
    char    *psz_ext;
    int     i_type  = TYPE_UNKNOWN;
    int     i_type2 = TYPE_UNKNOWN;

    p_demux->pf_control = Control;
    p_demux->pf_demux = Demux;

    /* Check for m3u/asx file extension or if the demux has been forced */
    psz_ext = strrchr ( p_demux->psz_path, '.' );

    if( ( psz_ext && !strcasecmp( psz_ext, ".m3u") ) ||
        /* a .ram file can contain a single rtsp link */
        ( psz_ext && !strcasecmp( psz_ext, ".ram") ) ||
        ( p_demux->psz_demux && !strcmp(p_demux->psz_demux, "m3u") ) )
    {
        i_type = TYPE_M3U;
    }
    else if( ( psz_ext && !strcasecmp( psz_ext, ".asx") ) ||
             ( p_demux->psz_demux && !strcmp(p_demux->psz_demux, "asx") ) )
    {
        i_type = TYPE_ASX;
    }
    else if( ( psz_ext && !strcasecmp( psz_ext, ".html") ) ||
             ( p_demux->psz_demux && !strcmp(p_demux->psz_demux, "html") ) )
    {
        i_type = TYPE_HTML;
    }
    else if( ( psz_ext && !strcasecmp( psz_ext, ".pls") ) ||
             ( p_demux->psz_demux && !strcmp(p_demux->psz_demux, "pls") ) )
    {
        i_type = TYPE_PLS;
    }
    else if( ( psz_ext && !strcasecmp( psz_ext, ".b4s") ) ||
             ( p_demux->psz_demux && !strcmp(p_demux->psz_demux, "b4s") ) )
    {
        i_type = TYPE_B4S;
    }

    /* we had no luck looking at the file extension, so we have a look
     * at the content. This is useful for .asp, .php and similar files
     * that are actually html. Also useful for some asx files that have
     * another extension */
    /* We double check for file != m3u as some asx are just m3u file */
    if( i_type != TYPE_M3U )
    {
        char *p_peek;
        int i_size = stream_Peek( p_demux->s, (uint8_t **)&p_peek, MAX_LINE );
        i_size -= sizeof("[Reference]") - 1;

        if( i_size > 0 )
        {
            while( i_size &&
                   strncasecmp(p_peek, "[playlist]", sizeof("[playlist]") - 1)
                   && strncasecmp( p_peek, "[Reference]", sizeof("[Reference]") - 1 )
                   && strncasecmp( p_peek, "<html>", sizeof("<html>") - 1 )
                   && strncasecmp( p_peek, "<asx", sizeof("<asx") - 1 )
                   && strncasecmp( p_peek, "rtsptext", sizeof("rtsptext") - 1 )
                   && strncasecmp( p_peek, "<?xml", sizeof("<?xml") -1 ) )
            {
                p_peek++;
                i_size--;
            }
            if( !i_size )
            {
                ;
            }
            else if( !strncasecmp( p_peek, "[playlist]", sizeof("[playlist]") -1 ) )
            {
                i_type2 = TYPE_PLS;
            }
            else if( !strncasecmp( p_peek, "[Reference]", sizeof("[Reference]") -1 ) )
            {
                i_type2 = TYPE_WMP;
            }
            else if( !strncasecmp( p_peek, "<html>", sizeof("<html>") -1 ) )
            {
                i_type2 = TYPE_HTML;
            }
            else if( !strncasecmp( p_peek, "<asx", sizeof("<asx") -1 ) )
            {
                i_type2 = TYPE_ASX;
            }
            else if( !strncasecmp( p_peek, "rtsptext", sizeof("rtsptext") -1 ) )
            {
                i_type2 = TYPE_RTSP;
            }
#if 0
            else if( !strncasecmp( p_peek, "<?xml", sizeof("<?xml") -1 ) )
            {
                i_type2 = TYPE_B4S;
            }
#endif
        }
    }
    if( i_type == TYPE_UNKNOWN && i_type2 == TYPE_UNKNOWN)
    {
        return VLC_EGENERIC;
    }
    if( i_type  != TYPE_UNKNOWN && i_type2 == TYPE_UNKNOWN )
    {
        i_type = TYPE_M3U;
    }
    else
    {
        i_type = i_type2;
    }

    /* Allocate p_m3u */
    p_demux->p_sys = malloc( sizeof( demux_sys_t ) );
    p_demux->p_sys->i_type = i_type;
    msg_Dbg( p_this, "playlist type: %d - %d", i_type, i_type2 );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Deactivate: frees unused data
 *****************************************************************************/
static void Deactivate( vlc_object_t *p_this )
{
    demux_t *p_demux = (demux_t *)p_this;
    free( p_demux->p_sys );
}

/*****************************************************************************
 * XMLSpecialChars: Handle the special chars in a XML file.
 * ***************************************************************************/
static void XMLSpecialChars ( char *str )
{
    char *src = str;
    char *dst = str;

    while( *src )
    {
        if( *src == '&' )
        {
            /* FIXME:
             * - should probably accept any sequence, rather than only those
             *   commonly found in French.
             * - output may have to be UTF-8 encoded (cannot assume Latin-1)
             */
            if( !strncasecmp( src, "&#xe0;", 6 ) ) *dst++ = '\xe0';
            else if( !strncasecmp( src, "&#xee;", 6 ) ) *dst++ = '\xee';
            else if( !strncasecmp( src, "&apos;", 6 ) ) *dst++ = '\'';
            else if( !strncasecmp( src, "&#xe8;", 6 ) ) *dst++ = '\xe8';
            else if( !strncasecmp( src, "&#xe9;", 6 ) ) *dst++ = '\xe9';
            else if( !strncasecmp( src, "&#xea;", 6 ) ) *dst++ = '\xea';
            else
            {
                *dst++ = '?';
            }
            src += 6;
        }
        else
        {
            *dst++ = *src++;
        }
    }

    *dst = '\0';
}

/*****************************************************************************
 * ParseLine: read a "line" from the file and add any entries found
 * to the playlist. Returns:
 * 0 if nothing was found
 * 1 if a URI was found (it is then copied in psz_data)
 * 2 if a name was found (  "  )
 *
 * XXX psz_data has the same length that psz_line so no problem if you don't
 * expand it
 *    psz_line is \0 terminated
 *****************************************************************************/
static int ParseLine( demux_t *p_demux, char *psz_line, char *psz_data,
                      vlc_bool_t *pb_done )
{
    demux_sys_t *p_m3u = p_demux->p_sys;
    char        *psz_bol, *psz_name;

    psz_bol = psz_line;
    *pb_done = VLC_FALSE;

    /* Remove unnecessary tabs or spaces at the beginning of line */
    while( *psz_bol == ' ' || *psz_bol == '\t' ||
           *psz_bol == '\n' || *psz_bol == '\r' )
    {
        psz_bol++;
    }

    if( p_m3u->i_type == TYPE_M3U )
    {
        /* Check for comment line */
        if( *psz_bol == '#' )
        {
            while( *psz_bol &&
                   strncasecmp( psz_bol, "EXTINF:",
                                sizeof("EXTINF:") - 1 ) &&
                   strncasecmp( psz_bol, "EXTVLCOPT:",
                                sizeof("EXTVLCOPT:") - 1 ) ) psz_bol++;

            if( !*psz_bol ) return 0;

            if( !strncasecmp( psz_bol, "EXTINF:", sizeof("EXTINF:") - 1 ) )
            {
                psz_bol = strchr( psz_bol, ',' );
                if ( !psz_bol ) return 0;
                psz_bol++;

                /* From now, we have a name line */
                strcpy( psz_data , psz_bol );
                return 2;
            }
            else
            {
                psz_bol = strchr( psz_bol, ':' );
                if ( !psz_bol ) return 0;
                psz_bol++;

                strcpy( psz_data , psz_bol );
                return 3;
            }
        }
        /* If we don't have a comment, the line is directly the URI */
    }
    else if( p_m3u->i_type == TYPE_PLS )
    {
        /* We are dealing with .pls files from shoutcast
         * We are looking for lines like "File1=http://..." */
        if( !strncasecmp( psz_bol, "File", sizeof("File") - 1 ) )
        {
            psz_bol += sizeof("File") - 1;
            psz_bol = strchr( psz_bol, '=' );
            if ( !psz_bol ) return 0;
            psz_bol++;
        }
        else
        {
            return 0;
        }
    }
    else if( p_m3u->i_type == TYPE_WMP )
    {
        /* We are dealing with some weird WMP stream playlist format
         * Hurray for idiotic M$. Lines look like: "Ref1=http://..." */
        if( !strncasecmp( psz_bol, "Ref", sizeof("Ref") - 1 ) )
        {
            psz_bol += sizeof("Ref") - 1;
            psz_bol = strchr( psz_bol, '=' );
            if ( !psz_bol ) return 0;
            psz_bol++;
            if( !strncasecmp( psz_bol, "http://", sizeof("http://") -1 ) )
            {
                psz_bol++;
                psz_bol[0] = 'm'; psz_bol[1] = 'm'; psz_bol[2] = 's';
            }
        }
        else
        {
            return 0;
        }
    }
    else if( p_m3u->i_type == TYPE_ASX )
    {
        /* We are dealing with ASX files.
         * We are looking for "<ref href=" xml markups that
         * begins with "mms://", "http://" or "file://" */
        char *psz_eol;

        while( *psz_bol &&
               strncasecmp( psz_bol, "ref", sizeof("ref") - 1 ) )
            psz_bol++;

        if( !*psz_bol ) return 0;

        while( *psz_bol &&
               strncasecmp( psz_bol, "href", sizeof("href") - 1 ) )
            psz_bol++;

        if( !*psz_bol ) return 0;

        while( *psz_bol &&
               strncasecmp( psz_bol, "mms://",
                            sizeof("mms://") - 1 ) &&
               strncasecmp( psz_bol, "mmsu://",
                            sizeof("mmsu://") - 1 ) &&
               strncasecmp( psz_bol, "mmst://",
                            sizeof("mmst://") - 1 ) &&
               strncasecmp( psz_bol, "http://",
                            sizeof("http://") - 1 ) &&
               strncasecmp( psz_bol, "file://",
                            sizeof("file://") - 1 ) )
            psz_bol++;

        if( !*psz_bol ) return 0;

        psz_eol = strchr( psz_bol, '"');
        if( !psz_eol )
          return 0;

        *psz_eol = '\0';
    }
    else if( p_m3u->i_type == TYPE_HTML )
    {
        /* We are dealing with a html file with embedded
         * video.  We are looking for "<param name="filename"
         * value=" html markups that begin with "http://" */
        char *psz_eol;

        while( *psz_bol &&
               strncasecmp( psz_bol, "param", sizeof("param") - 1 ) )
            psz_bol++;

        if( !*psz_bol ) return 0;

        while( *psz_bol &&
               strncasecmp( psz_bol, "filename", sizeof("filename") - 1 ) )
            psz_bol++;

        if( !*psz_bol ) return 0;

        while( *psz_bol &&
               strncasecmp( psz_bol, "http://",
                            sizeof("http://") - 1 ) )
            psz_bol++;

        if( !*psz_bol ) return 0;

        psz_eol = strchr( psz_bol, '"');
        if( !psz_eol )
          return 0;

        *psz_eol = '\0';

    }
    else if( p_m3u->i_type == TYPE_B4S )
    {

        char *psz_eol;

        msg_Dbg( p_demux, "b4s line=%s", psz_line );
        /* We are dealing with a B4S file from Winamp 3 */

        /* First, search for name *
         * <Name>Blabla</Name> */

        if( strstr ( psz_bol, "<Name>" ) )
        {
            /* We have a name */
            while ( *psz_bol &&
                    strncasecmp( psz_bol,"Name",sizeof("Name") -1 ) )
                psz_bol++;

            if( !*psz_bol ) return 0;

            psz_bol = psz_bol + 5 ;
            /* We are now at the beginning of the name */

            if( !psz_bol ) return 0;


            psz_eol = strchr(psz_bol, '<' );
            if( !psz_eol) return 0;

            *psz_eol='\0';

            XMLSpecialChars( psz_bol );

            strcpy( psz_data, psz_bol );
            return 2;
        }
        else if( strstr( psz_bol, "</entry>" ) || strstr( psz_bol, "</Entry>" ))
        {
            *pb_done = VLC_TRUE;
            return 0;
        }

        /* We are looking for <entry Playstring="blabla"> */

        while( *psz_bol &&
               strncasecmp( psz_bol,"Playstring",sizeof("Playstring") -1 ) )
            psz_bol++;

        if( !*psz_bol ) return 0;

        psz_bol = strchr( psz_bol, '=' );
        if ( !psz_bol ) return 0;

        psz_bol += 2;

        psz_eol= strchr(psz_bol, '"');
        if( !psz_eol ) return 0;

        *psz_eol= '\0';

        /* Handle the XML special characters */
        XMLSpecialChars( psz_bol );
    }
    else if( p_m3u->i_type == TYPE_RTSP )
    {
        /* We are dealing with rtsptext reference files
         * Ignore anthying that doesn't start with rtsp://..." */
        if( strncasecmp( psz_bol, "rtsp://", sizeof("rtsp://") - 1 ) )
        /* ignore */ return 0;
    }
    else
    {
        msg_Warn( p_demux, "unknown file type" );
        return 0;
    }

    /* empty line */
    if ( !*psz_bol ) return 0;

    /*
     * From now on, we know we've got a meaningful line
     */

    /* check for a protocol name */
    /* for URL, we should look for "://"
     * for MRL (Media Resource Locator) ([[<access>][/<demux>]:][<source>]),
     * we should look for ":"
     * so we end up looking simply for ":"*/
    /* PB: on some file systems, ':' are valid characters though*/
    psz_name = psz_bol;
    while( *psz_name && *psz_name!=':' )
    {
        psz_name++;
    }
#ifdef WIN32
    if ( *psz_name && ( psz_name == psz_bol + 1 ) )
    {
        /* if it is not an URL,
         * as it is unlikely to be an MRL (PB: if it is ?)
         * it should be an absolute file name with the drive letter */
        if ( *(psz_name+1) == '/' )/* "*:/" */
        {
            if ( *(psz_name+2) != '/' )/* not "*://" */
                while ( *psz_name ) *psz_name++;/* so now (*psz_name==0) */
        }
        else while ( *psz_name ) *psz_name++;/* "*:*"*/
    }
#endif

    /* if the line doesn't specify a protocol name,
     * check if the line has an absolute or relative path */
#ifndef WIN32
    if( !*psz_name && *psz_bol != '/' )
         /* If this line doesn't begin with a '/' */
#else
    if( !*psz_name
            && *psz_bol!='/'
            && *psz_bol!='\\'
            && *(psz_bol+1)!=':' )
         /* if this line doesn't begin with
          *  "/" or "\" or "*:" or "*:\" or "*:/" or "\\" */
#endif
    {
        /* assume the path is relative to the path of the m3u file. */
        char *psz_path = strdup( p_demux->psz_path );

#ifndef WIN32
        psz_name = strrchr( psz_path, '/' );
#else
        psz_name = strrchr( psz_path, '\\' );
        if ( ! psz_name ) psz_name = strrchr( psz_path, '/' );
#endif
        if( psz_name ) *psz_name = '\0';
        else *psz_path = '\0';
#ifndef WIN32
        psz_name = malloc( strlen(psz_path) + strlen(psz_bol) + 2 );
        sprintf( psz_name, "%s/%s", psz_path, psz_bol );
#else
        if ( *psz_path != '\0' )
        {
            psz_name = malloc( strlen(psz_path) + strlen(psz_bol) + 2 );
            sprintf( psz_name, "%s\\%s", psz_path, psz_bol );
        }
        else psz_name = strdup( psz_bol );
#endif
        free( psz_path );
    }
    else
    {
        psz_name = strdup( psz_bol );
    }

    strcpy(psz_data, psz_name ) ;

    free( psz_name );

    if( p_m3u->i_type != TYPE_B4S )
    {
       *pb_done = VLC_TRUE;
    }

    return 1;
}

static void ProcessLine ( demux_t *p_demux, playlist_t *p_playlist,
                          playlist_item_t *p_parent,
                          char *psz_line, char **ppsz_uri, char **ppsz_name,
                          int *pi_options, char ***pppsz_options,
                          vlc_bool_t b_flush )
{
    char psz_data[MAX_LINE];
    vlc_bool_t b_done;

    switch( ParseLine( p_demux, psz_line, psz_data, &b_done ) )
    {
        case 1:
            if( *ppsz_uri ) free( *ppsz_uri );
            *ppsz_uri = strdup( psz_data );
            break;
        case 2:
            if( *ppsz_name ) free( *ppsz_name );
            *ppsz_name = strdup( psz_data );
            break;
        case 3:
            (*pi_options)++;
            *pppsz_options = realloc( *pppsz_options,
                                      sizeof(char *) * *pi_options );
            (*pppsz_options)[*pi_options - 1] = strdup( psz_data );
            break;
        case 0:
        default:
            break;
    }

    if( (b_done || b_flush) && *ppsz_uri )
    {
        playlist_item_t *p_item =
            playlist_ItemNew( p_playlist, *ppsz_uri, *ppsz_name );
        int i;

        for( i = 0; i < *pi_options; i++ )
        {
            playlist_ItemAddOption( p_item, *pppsz_options[i] );
        }

        playlist_NodeAddItem( p_playlist, p_item,
                              p_parent->pp_parents[0]->i_view,
                              p_parent, PLAYLIST_APPEND, PLAYLIST_END );

        /* We need to declare the parents of the node as the
         * same of the parent's ones */
        playlist_CopyParents( p_parent, p_item );

        vlc_input_item_CopyOptions( &p_parent->input, &p_item->input );

        if( *ppsz_name ) free( *ppsz_name ); *ppsz_name = NULL;
        free( *ppsz_uri ); *ppsz_uri  = NULL;

        for( ; *pi_options; (*pi_options)-- )
        {
            free( (*pppsz_options)[*pi_options - 1] );
            if( *pi_options == 1 ) free( *pppsz_options );
        }
        *pppsz_options = NULL;
    }
}

static vlc_bool_t FindItem( demux_t *p_demux, playlist_t *p_playlist,
                            playlist_item_t **pp_item )
{
     vlc_bool_t b_play;

     if( &p_playlist->status.p_item->input ==
         ((input_thread_t *)p_demux->p_parent)->input.p_item )
     {
         msg_Dbg( p_playlist, "starting playlist playback" );
         *pp_item = p_playlist->status.p_item;
         b_play = VLC_TRUE;
     }
     else
     {
         input_item_t *p_current =
             ((input_thread_t*)p_demux->p_parent)->input.p_item;
         *pp_item = playlist_LockItemGetByInput( p_playlist, p_current );

         if( !*pp_item )
             msg_Dbg( p_playlist, "unable to find item in playlist");

         b_play = VLC_FALSE;
     }

     return b_play;
}

/*****************************************************************************
 * Demux: reads and demuxes data packets
 *****************************************************************************
 * Returns -1 in case of error, 0 in case of EOF, 1 otherwise
 *****************************************************************************/
static int Demux( demux_t *p_demux )
{
    demux_sys_t   *p_m3u = p_demux->p_sys;

    char          psz_line[MAX_LINE];
    char          p_buf[MAX_LINE], eol_tok;
    int           i_size, i_bufpos, i_linepos = 0;
    vlc_bool_t    b_discard = VLC_FALSE;

    char          *psz_name = NULL;
    char          *psz_uri  = NULL;
    int           i_options = 0;
    char          **ppsz_options = NULL;

    playlist_t      *p_playlist;
    playlist_item_t *p_parent;
    vlc_bool_t      b_play;

    p_playlist = (playlist_t *) vlc_object_find( p_demux, VLC_OBJECT_PLAYLIST,
                                                 FIND_ANYWHERE );
    if( !p_playlist )
    {
        msg_Err( p_demux, "can't find playlist" );
        return -1;
    }

    b_play = FindItem( p_demux, p_playlist, &p_parent );
    playlist_ItemToNode( p_playlist, p_parent );
    p_parent->input.i_type = ITEM_TYPE_PLAYLIST;

    /* Depending on wether we are dealing with an m3u/asf file, the end of
     * line token will be different */
    if( p_m3u->i_type == TYPE_ASX || p_m3u->i_type == TYPE_HTML )
        eol_tok = '>';
    else
        eol_tok = '\n';

    while( ( i_size = stream_Read( p_demux->s, p_buf, MAX_LINE ) ) )
    {
        i_bufpos = 0;

        while( i_size )
        {
            /* Build a line < MAX_LINE */
            while( p_buf[i_bufpos] != eol_tok && i_size )
            {
                if( i_linepos == MAX_LINE || b_discard == VLC_TRUE )
                {
                    /* line is bigger than MAX_LINE, discard it */
                    i_linepos = 0;
                    b_discard = VLC_TRUE;
                }
                else
                {
                    if ( eol_tok != '\n' || p_buf[i_bufpos] != '\r' )
                    {
                        psz_line[i_linepos] = p_buf[i_bufpos];
                        i_linepos++;
                    }
                }

                i_size--; i_bufpos++;
            }

            /* Check if we need more data */
            if( !i_size ) continue;

            i_size--; i_bufpos++;
            b_discard = VLC_FALSE;

            /* Check for empty line */
            if( !i_linepos ) continue;

            psz_line[i_linepos] = '\0';
            i_linepos = 0;

            ProcessLine( p_demux, p_playlist, p_parent,
                         psz_line, &psz_uri, &psz_name,
                         &i_options, &ppsz_options, VLC_FALSE );
        }
    }

    if( i_linepos && b_discard != VLC_TRUE && eol_tok == '\n' )
    {
        psz_line[i_linepos] = '\0';

        ProcessLine( p_demux, p_playlist, p_parent,
                     psz_line, &psz_uri, &psz_name,
                     &i_options, &ppsz_options, VLC_TRUE );
    }

    if( psz_uri ) free( psz_uri );
    if( psz_name ) free( psz_name );
    for( ; i_options; i_options-- )
    {
        free( ppsz_options[i_options - 1] );
        if( i_options == 1 ) free( ppsz_options );
    }

    /* Go back and play the playlist */
    if( b_play )
    {
        playlist_Control( p_playlist, PLAYLIST_VIEWPLAY,
                          p_playlist->status.i_view,
                          p_playlist->status.p_item, NULL );
    }

    vlc_object_release( p_playlist );

    return 0;
}

static int Control( demux_t *p_demux, int i_query, va_list args )
{
    return VLC_EGENERIC;
}
