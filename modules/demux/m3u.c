/*****************************************************************************
 * m3u.c: a meta demux to parse m3u playlists
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: m3u.c,v 1.1 2002/11/12 22:18:54 sigmunau Exp $
 *
 * Authors: Sigmund Augdal <sigmunau@idi.ntnu.no>
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
 * Constants
 *****************************************************************************/

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Init  ( vlc_object_t * );
static int  Demux ( input_thread_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();                                      
    set_description( "m3u metademux" );                       
    set_capability( "demux", 10 );
    set_callbacks( Init, NULL );
vlc_module_end();

/*****************************************************************************
 * Init: initializes ES structures
 *****************************************************************************/
static int Init( vlc_object_t * p_this )
{
    input_thread_t *    p_input = (input_thread_t *)p_this;
    byte_t *            p_peek;
    int                 i_size, i_pos;
    playlist_t *        p_playlist;
    int                 b_first = 1;

    /* Initialize access plug-in structures. */
    if( p_input->i_mtu == 0 )
    {
        /* Improve speed. */
        p_input->i_bufsize = INPUT_DEFAULT_BUFSIZE;
    }

    p_input->pf_demux = Demux;
    p_input->pf_rewind = NULL;

    /* Have a peep at the show. */
    if( input_Peek( p_input, &p_peek, 7 ) < 7 )
    {
        /* Stream shorter than 7 bytes... */
        msg_Err( p_input, "cannot peek()" );
        return( -1 );
    }
    
#define MAX_LINE 1024
    if( !strncmp( p_peek, "#EXTM3U", 7)
        || ( p_input->psz_demux && !strncmp(p_input->psz_demux, "m3u", 3) ) )
    {
        p_playlist =
            (playlist_t *) vlc_object_find( p_input, VLC_OBJECT_PLAYLIST,
                                                 FIND_ANYWHERE );
        p_playlist->pp_items[p_playlist->i_index]->b_autodeletion = VLC_TRUE;
        i_size = input_Peek( p_input, &p_peek, MAX_LINE );
        while ( i_size > 0 ) {
            i_pos = 0;
            if ( *p_peek == '#' ) {/*line is comment or extended info,
                                     ignored for now */
                while ( *p_peek != '\n' && i_pos < i_size )
                {
                    
                    i_pos++;
                    p_peek++;
                }
                i_pos++;
            }
            else
            {
                byte_t *p_start = p_peek;
                char *psz_entry;
                while ( *p_peek != '\n' )
                {
                    i_pos++;
                    p_peek++;
                }
                if ( i_pos ) /*ignore empty lines to */
                {
                    psz_entry = malloc( i_pos + 1 );
                    memcpy( psz_entry, p_start, i_pos );
                    psz_entry[i_pos] = '\0';
                    playlist_Add( p_playlist, psz_entry,
                                  PLAYLIST_APPEND ,
                                  PLAYLIST_END );
                    b_first = 0;
                    free( psz_entry );
                }
                i_pos++;
            }
            p_input->p_current_data += i_pos;
            i_size = input_Peek( p_input, &p_peek, MAX_LINE );
        }
        p_input->b_eof = VLC_TRUE;
        return( 0 );
    }
    return( -1 );
}

static int Demux ( input_thread_t *p_intput )
{
    return 0;
}
