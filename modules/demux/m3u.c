/*****************************************************************************
 * m3u.c: a meta demux to parse m3u and asx playlists
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: m3u.c,v 1.9 2002/11/26 18:58:33 sigmunau Exp $
 *
 * Authors: Sigmund Augdal <sigmunau@idi.ntnu.no>
 *          Gildas Bazin <gbazin@netcourrier.com>
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
#include <string.h>                                              /* strdup() */
#include <errno.h>

#include <vlc/vlc.h>
#include <vlc/input.h>
#include <vlc_playlist.h>

#include <sys/types.h>

/*****************************************************************************
 * Constants and structures
 *****************************************************************************/
#define MAX_LINE 1024

#define TYPE_M3U 1
#define TYPE_ASX 2
#define TYPE_HTML 3

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
    set_description( "m3u/asx metademux" );
    set_capability( "demux", 10 );
    set_callbacks( Activate, Deactivate );
    add_shortcut( "m3u" );
    add_shortcut( "asx" );
    add_shortcut( "html" );
vlc_module_end();

/*****************************************************************************
 * Activate: initializes m3u demux structures
 *****************************************************************************/
static int Activate( vlc_object_t * p_this )
{
    input_thread_t *p_input = (input_thread_t *)p_this;
    char           *psz_ext;
    demux_sys_t    *p_m3u;
    int            i_type = 0;

    /* Initialize access plug-in structures. */
    if( p_input->i_mtu == 0 )
    {
        /* Improve speed. */
        p_input->i_bufsize = INPUT_DEFAULT_BUFSIZE;
    }

    p_input->pf_demux = Demux;
    p_input->pf_rewind = NULL;

    /* Check for m3u/asx file extension */
    psz_ext = strrchr ( p_input->psz_name, '.' );
    if( !strcasecmp( psz_ext, ".m3u") ||
        ( p_input->psz_demux && !strncmp(p_input->psz_demux, "m3u", 3) ) )
    {
        i_type = TYPE_M3U;
    }
    else if( !strcasecmp( psz_ext, ".asx") ||
             ( p_input->psz_demux && !strncmp(p_input->psz_demux, "asx", 3) ) )
    {
        i_type = TYPE_ASX;
    }
    else if( !strcasecmp( psz_ext, ".html") ||
             ( p_input->psz_demux && !strncmp(p_input->psz_demux, "html", 4) ) )
    {
        i_type = TYPE_HTML;
    }

    /* we had no luck looking at the file extention, so we have a look
     * at the content. This is useful for .asp, .php and similar files
     * that are actually html. Also useful for som asx files that have
     * another extention */
    if( !i_type )
    {
        byte_t *p_peek;
        int i_size = input_Peek( p_input, &p_peek, MAX_LINE );
        i_size -= sizeof("<html>") - 1;
        if ( i_size > 0 ) {
            while ( i_size
                    && strncasecmp( p_peek, "<html>", sizeof("<html>") - 1 )
                    && strncasecmp( p_peek, "<asx", sizeof("<asx") - 1 ) )
            {
                p_peek++;
                i_size--;
            }
            if ( !i_size )
            {
                return -1;
            }
            else if ( !strncasecmp( p_peek, "<html>", sizeof("<html>") -1 ) )
            {
                i_type = TYPE_HTML;
            }
            else if ( !strncasecmp( p_peek, "<asx", sizeof("<asx") -1 ) )
            {
                i_type = TYPE_ASX;
            }
        }
    }
    /* Allocate p_m3u */
    if( !( p_m3u = malloc( sizeof( demux_sys_t ) ) ) )
    {
        msg_Err( p_input, "out of memory" );
        return -1;
    }
    p_input->p_demux_data = p_m3u;

    p_m3u->i_type = i_type;

    return 0;
}

/*****************************************************************************
 * Deactivate: frees unused data
 *****************************************************************************/
static void Deactivate( vlc_object_t *p_this )
{
    input_thread_t *p_input = (input_thread_t *)p_this;
    demux_sys_t *p_m3u = (demux_sys_t *)p_input->p_demux_data  ; 

    free( p_m3u );
}

/*****************************************************************************
 * Demux: reads and demuxes data packets
 *****************************************************************************
 * Returns -1 in case of error, 0 in case of EOF, 1 otherwise
 *****************************************************************************/
static int Demux ( input_thread_t *p_input )
{
    data_packet_t *p_data;
    char          *p_buf, psz_line[MAX_LINE], *psz_bol, *psz_name, eol_tok;
    int           i_size, i_bufpos, i_linepos = 0;
    playlist_t    *p_playlist;
    vlc_bool_t    b_discard = VLC_FALSE;

    demux_sys_t   *p_m3u = (demux_sys_t *)p_input->p_demux_data;

    p_playlist = (playlist_t *) vlc_object_find( p_input, VLC_OBJECT_PLAYLIST,
                                                 FIND_ANYWHERE );
    if( !p_playlist )
    {
        msg_Err( p_input, "can't find playlist" );
        return -1;
    }

    p_playlist->pp_items[p_playlist->i_index]->b_autodeletion = VLC_TRUE;

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
            psz_bol = psz_line;
            i_linepos = 0;

            /* Remove unnecessary tabs or spaces at the beginning of line */
            while( *psz_bol == ' ' || *psz_bol == '\t' ||
                   *psz_bol == '\n' || *psz_bol == '\r' )
                psz_bol++;

            if( p_m3u->i_type == TYPE_M3U )
            {
                /* Check for comment line */
                if( *psz_bol == '#' )
                    /*line is comment or extended info, ignored for now */
                    continue;
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

                if( !*psz_bol ) continue;

                while( *psz_bol &&
                       strncasecmp( psz_bol, "href", sizeof("href") - 1 ) )
                    psz_bol++;

                if( !*psz_bol ) continue;

                while( *psz_bol &&
                       strncasecmp( psz_bol, "mms://",
                                    sizeof("mms://") - 1 ) &&
                       strncasecmp( psz_bol, "http://",
                                    sizeof("http://") - 1 ) &&
                       strncasecmp( psz_bol, "file://",
                                    sizeof("file://") - 1 ) )
                    psz_bol++;

                if( !*psz_bol ) continue;

                psz_eol = strchr( psz_bol, '"');
                if( !psz_eol )
                  continue;

                *psz_eol = '\0';
            }
            else
            {
                /* We are dealing with a html file with embedded
                 * video.  We are looking for "<param name="filename"
                 * value=" html markups that begin with "http://" */
                char *psz_eol;

                while( *psz_bol &&
                       strncasecmp( psz_bol, "param", sizeof("param") - 1 ) )
                    psz_bol++;

                if( !*psz_bol ) continue;

                while( *psz_bol &&
                       strncasecmp( psz_bol, "filename", sizeof("filename") - 1 ) )
                    psz_bol++;

                if( !*psz_bol ) continue;

                while( *psz_bol &&
                       strncasecmp( psz_bol, "http://",
                                    sizeof("http://") - 1 ) )
                    psz_bol++;

                if( !*psz_bol ) continue;

                psz_eol = strchr( psz_bol, '"');
                if( !psz_eol )
                  continue;

                *psz_eol = '\0';
                
            }

            /*
             * From now on, we know we've got a meaningful line
             */

	    /* Check if the line has an absolute or relative path */
	    psz_name = psz_bol;
            while( *psz_name && strncmp( psz_name, "://", sizeof("://") - 1 ) )
	    {
	        psz_name++;
	    }
	    if( !*psz_name && *psz_bol != '/' )
	    {
 	        /* the line doesn't specify a protocol name.
		 * If this line doesn't begin with a '/' then assume the path
		 * is relative to the path of the m3u file. */
                char *psz_path = strdup( p_input->psz_name );
                
                psz_name = strrchr( psz_path, '/' );
                if( psz_name ) *psz_name = '\0';
                else *psz_path = '\0';
                psz_name = malloc( strlen(psz_path) + strlen(psz_bol) + 2 );
                sprintf( psz_name, "%s/%s", psz_path, psz_bol );
                free( psz_path );
	    }
	    else
	    {
	        psz_name = strdup( psz_bol );
	    }

            playlist_Add( p_playlist, psz_name,
                          PLAYLIST_APPEND, PLAYLIST_END );

	    free( psz_name );

            continue;
        }

        input_DeletePacket( p_input->p_method_data, p_data );
    }

    vlc_object_release( p_playlist );

    return 0;
}
