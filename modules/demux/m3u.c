/*****************************************************************************
 * m3u.c: a meta demux to parse pls, m3u, asx et b4s playlists
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: m3u.c,v 1.22 2003/06/29 19:15:04 fenrir Exp $
 *
 * Authors: Sigmund Augdal <sigmunau@idi.ntnu.no>
 *          Gildas Bazin <gbazin@netcourrier.com>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
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
#define MAX_LINE 1024

#define TYPE_UNKNOWN 0
#define TYPE_M3U 1
#define TYPE_ASX 2
#define TYPE_HTML 3
#define TYPE_PLS 4
#define TYPE_B4S 5

struct demux_sys_t
{
    int i_type;                                   /* playlist type (m3u/asx) */
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Activate  ( vlc_object_t * );
static void Deactivate( vlc_object_t * );
static int  Demux ( input_thread_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( _("playlist metademux") );
    set_capability( "demux", 180 );
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
    input_thread_t *p_input = (input_thread_t *)p_this;
    char           *psz_ext;
    int             i_type  = TYPE_UNKNOWN;
    int             i_type2 = TYPE_UNKNOWN;

    /* Initialize access plug-in structures. */
    if( p_input->i_mtu == 0 )
    {
        /* Improve speed. */
        p_input->i_bufsize = INPUT_DEFAULT_BUFSIZE;
    }

    p_input->pf_demux = Demux;
    p_input->pf_rewind = NULL;

    /* Check for m3u/asx file extension or if the demux has been forced */
    psz_ext = strrchr ( p_input->psz_name, '.' );

    if( ( psz_ext && !strcasecmp( psz_ext, ".m3u") ) ||
        ( p_input->psz_demux && !strcmp(p_input->psz_demux, "m3u") ) )
    {
        i_type = TYPE_M3U;
    }
    else if( ( psz_ext && !strcasecmp( psz_ext, ".asx") ) ||
             ( p_input->psz_demux && !strcmp(p_input->psz_demux, "asx") ) )
    {
        i_type = TYPE_ASX;
    }
    else if( ( psz_ext && !strcasecmp( psz_ext, ".html") ) ||
             ( p_input->psz_demux && !strcmp(p_input->psz_demux, "html") ) )
    {
        i_type = TYPE_HTML;
    }
    else if( ( psz_ext && !strcasecmp( psz_ext, ".pls") ) ||
             ( p_input->psz_demux && !strcmp(p_input->psz_demux, "pls") ) )
    {
        i_type = TYPE_PLS;
    }
    else if( ( psz_ext && !strcasecmp( psz_ext, ".b4s") ) ||
             ( p_input->psz_demux && !strcmp(p_input->psz_demux, "b4s") ) )
    {
        i_type = TYPE_B4S;
    }

    /* we had no luck looking at the file extention, so we have a look
     * at the content. This is useful for .asp, .php and similar files
     * that are actually html. Also useful for som asx files that have
     * another extention */
    /* XXX we double check for file != m3u as some asx ... are just m3u file */
    if( i_type != TYPE_M3U )
    {
        byte_t *p_peek;
        int i_size = input_Peek( p_input, &p_peek, MAX_LINE );
        i_size -= sizeof("[playlist]") - 1;
        if ( i_size > 0 ) {
            while ( i_size
                    && strncasecmp( p_peek, "[playlist]", sizeof("[playlist]") - 1 )
                    && strncasecmp( p_peek, "<html>", sizeof("<html>") - 1 )
                    && strncasecmp( p_peek, "<asx", sizeof("<asx") - 1 )
                    && strncasecmp( p_peek, "<?xml", sizeof("<?xml") -1 ) )
            {
                p_peek++;
                i_size--;
            }
            if ( !i_size )
            {
                ;
            }
            else if ( !strncasecmp( p_peek, "[playlist]", sizeof("[playlist]") -1 ) )
            {
                i_type2 = TYPE_PLS;
            }
            else if ( !strncasecmp( p_peek, "<html>", sizeof("<html>") -1 ) )
            {
                i_type2 = TYPE_HTML;
            }
            else if ( !strncasecmp( p_peek, "<asx", sizeof("<asx") -1 ) )
            {
                i_type2 = TYPE_ASX;
            }
            else if ( !strncasecmp( p_peek, "<?xml", sizeof("<?xml") -1 ) )
            {
                i_type2 = TYPE_B4S;
            }
        }
    }
    if ( i_type == TYPE_UNKNOWN && i_type2 == TYPE_UNKNOWN)
    {
        return VLC_EGENERIC;
    }
    if ( i_type  != TYPE_UNKNOWN && i_type2 == TYPE_UNKNOWN )
    {
        i_type = TYPE_M3U;
    }
    else
    {
        i_type = i_type2;
    }

    /* Allocate p_m3u */
    p_input->p_demux_data = malloc( sizeof( demux_sys_t ) );
    p_input->p_demux_data->i_type = i_type;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Deactivate: frees unused data
 *****************************************************************************/
static void Deactivate( vlc_object_t *p_this )
{
    input_thread_t *p_input = (input_thread_t *)p_this;

    free( p_input->p_demux_data );
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
            if( !strncasecmp( src, "&#xe0;", 6 ) ) *dst++ = 'à';
            else if( !strncasecmp( src, "&#xee;", 6 ) ) *dst++ = 'î';
            else if( !strncasecmp( src, "&apos;", 6 ) ) *dst++ = '\'';
            else if( !strncasecmp( src, "&#xe8;", 6 ) ) *dst++ = 'è';
            else if( !strncasecmp( src, "&#xe9;", 6 ) ) *dst++ = 'é';
            else if( !strncasecmp( src, "&#xea;", 6 ) ) *dst++ = 'ê';
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
 ******************************************************************************/
static int ParseLine ( input_thread_t *p_input, char *psz_line, char *psz_data, vlc_bool_t *pb_next )
{
    demux_sys_t   *p_m3u = p_input->p_demux_data;

    char          *psz_bol, *psz_name;

    psz_bol = psz_line;

    *pb_next = VLC_FALSE;

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
                   strncasecmp( psz_bol, "EXTINF:", sizeof("EXTINF:") - 1 ) )
               psz_bol++;
            if( !*psz_bol ) return 0;

            psz_bol = strchr( psz_bol, ',' );
            if ( !psz_bol ) return 0;
            psz_bol++;
            /* From now, we have a name line */

            strcpy( psz_data , psz_bol );
            return 2;
        }
        /* If we don't have a comment, the line is directly the URI */
    }
    else if ( p_m3u->i_type == TYPE_PLS )
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
    else if ( p_m3u->i_type == TYPE_ASX )
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
    else if ( p_m3u->i_type == TYPE_HTML )
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
    else if ( p_m3u->i_type == TYPE_B4S )
    {

        char *psz_eol;

        msg_Dbg( p_input, "b4s line=%s", psz_line );
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
            *pb_next = VLC_TRUE;
            return 0;
        }

         /* We are looking for <entry Playstring="blabla"> */


        while ( *psz_bol &&
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
    else
    {
        msg_Warn( p_input, "unknown file type" );
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
        char *psz_path = strdup( p_input->psz_name );

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
       *pb_next = VLC_TRUE;
    }

    return 1;
}

static void ProcessLine ( input_thread_t *p_input, playlist_t *p_playlist,
                          char *psz_line,
                          char **ppsz_uri, char **ppsz_name,
                          int *pi_position )
{
    char          psz_data[MAX_LINE];
    vlc_bool_t    b_next;

    switch( ParseLine( p_input, psz_line, psz_data, &b_next ) )
    {
        case 1:
            if( *ppsz_uri )
            {
                free( *ppsz_uri );
            }
            *ppsz_uri = strdup( psz_data );
            break;
        case 2:
            if( *ppsz_name )
            {
                free( *ppsz_name );
            }
            *ppsz_name = strdup( psz_data );
            break;
        case 0:
        default:
            break;
    }

    if( b_next && *ppsz_uri )
    {
        playlist_AddName( p_playlist,
                          *ppsz_name ? *ppsz_name : *ppsz_uri,
                          *ppsz_uri,
                          PLAYLIST_INSERT, *pi_position );
        (*pi_position)++;
        if( *ppsz_name )
        {
            free( *ppsz_name );
        }
        free( *ppsz_uri );
        *ppsz_name = NULL;
        *ppsz_uri  = NULL;
    }
}

/*****************************************************************************
 * Demux: reads and demuxes data packets
 *****************************************************************************
 * Returns -1 in case of error, 0 in case of EOF, 1 otherwise
 *****************************************************************************/
static int Demux ( input_thread_t *p_input )
{
    demux_sys_t   *p_m3u = p_input->p_demux_data;

    data_packet_t *p_data;
    char          psz_line[MAX_LINE];
    char          *p_buf, eol_tok;
    int           i_size, i_bufpos, i_linepos = 0;
    playlist_t    *p_playlist;
    vlc_bool_t    b_discard = VLC_FALSE;


    char          *psz_name = NULL;
    char          *psz_uri  = NULL;

    int           i_position;

    p_playlist = (playlist_t *) vlc_object_find( p_input, VLC_OBJECT_PLAYLIST,
                                                 FIND_ANYWHERE );
    if( !p_playlist )
    {
        msg_Err( p_input, "can't find playlist" );
        return -1;
    }

    p_playlist->pp_items[p_playlist->i_index]->b_autodeletion = VLC_TRUE;
    i_position = p_playlist->i_index + 1;

    /* Depending on wether we are dealing with an m3u/asf file, the end of
     * line token will be different */
    if( p_m3u->i_type == TYPE_ASX || p_m3u->i_type == TYPE_HTML )
        eol_tok = '>';
    else
        eol_tok = '\n';

    while( ( i_size = input_SplitBuffer( p_input, &p_data, MAX_LINE ) ) > 0 )
    {
        i_bufpos = 0; p_buf = p_data->p_payload_start;

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

            ProcessLine( p_input, p_playlist, psz_line, &psz_uri, &psz_name, &i_position );
        }

        input_DeletePacket( p_input->p_method_data, p_data );
    }

    if ( i_linepos && b_discard != VLC_TRUE && eol_tok == '\n' )
    {
        psz_line[i_linepos] = '\0';

        ProcessLine( p_input, p_playlist, psz_line, &psz_uri, &psz_name, &i_position );
        /* is there a pendding uri without b_next */
        if( psz_uri )
        {
            playlist_Add( p_playlist, psz_uri, PLAYLIST_INSERT, i_position );
        }
    }

    if( psz_uri )
    {
        free( psz_uri );
    }
    if( psz_name )
    {
        free( psz_name );
    }

    vlc_object_release( p_playlist );

    return 0;
}
