/*****************************************************************************
 * loadsave.c : Playlist loading / saving functions
 *****************************************************************************
 * Copyright (C) 1999-2004 VideoLAN
 * $Id: loadsave.c,v 1.3 2004/01/06 08:50:20 zorglub Exp $
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
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
#include <stdlib.h>                                      /* free(), strtol() */
#include <stdio.h>                                              /* sprintf() */
#include <string.h>                                            /* strerror() */

#include <vlc/vlc.h>
#include <vlc/vout.h>
#include <vlc/sout.h>

#include "stream_control.h"
#include "input_ext-intf.h"

#include "vlc_playlist.h"

#define PLAYLIST_FILE_HEADER_0_5  "# vlc playlist file version 0.5"
#define PLAYLIST_FILE_HEADER_0_6  "# vlc playlist file version 0.6"


/*****************************************************************************
 * playlist_LoadFile: load a playlist file.
 ****************************************************************************/
int playlist_LoadFile( playlist_t * p_playlist, const char *psz_filename )
{
    FILE *file;
    char line[1024];
    int i_current_status;
    int i_format;
    int i;

    msg_Dbg( p_playlist, "opening playlist file %s", psz_filename );

    file = fopen( psz_filename, "rt" );
    if( !file )
    {
        msg_Err( p_playlist, "playlist file %s does not exist", psz_filename );
        return -1;
    }
    fseek( file, 0L, SEEK_SET );

    /* check the file is not empty */
    if ( ! fgets( line, 1024, file ) )
    {
        msg_Err( p_playlist, "playlist file %s is empty", psz_filename );
        fclose( file );
        return -1;
    }

    /* get rid of line feed */
    if( line[strlen(line)-1] == '\n' || line[strlen(line)-1] == '\r' )
    {
       line[strlen(line)-1] = (char)0;
       if( line[strlen(line)-1] == '\r' ) line[strlen(line)-1] = (char)0;
    }
    /* check the file format is valid */
    if ( !strcmp ( line , PLAYLIST_FILE_HEADER_0_5 ) )
    {
       i_format = 5;
    }
    else if( !strcmp ( line , PLAYLIST_FILE_HEADER_0_6 ) )
    {
       i_format = 6;
    }
    else
    {
        msg_Err( p_playlist, "playlist file %s format is unsupported"
                , psz_filename );
        fclose( file );
        return -1;
    }

    /* stop playing */
    i_current_status = p_playlist->i_status;
    if ( p_playlist->i_status != PLAYLIST_STOPPED )
    {
        playlist_Stop ( p_playlist );
    }

    /* delete current content of the playlist */
    for( i = p_playlist->i_size - 1; i >= 0; i-- )
    {
        playlist_Delete ( p_playlist , i );
    }

    /* simply add each line */
    while( fgets( line, 1024, file ) )
    {
       /* ignore comments or empty lines */
       if( (line[0] == '#') || (line[0] == '\r') || (line[0] == '\n')
               || (line[0] == (char)0) )
           continue;

       /* get rid of line feed */
       if( line[strlen(line)-1] == '\n' || line[strlen(line)-1] == '\r' )
       {
           line[strlen(line)-1] = (char)0;
           if( line[strlen(line)-1] == '\r' ) line[strlen(line)-1] = (char)0;
       }
       if( i_format == 5 )
       {
           playlist_Add ( p_playlist , (char *)&line , (char *)&line,
                         PLAYLIST_APPEND , PLAYLIST_END );
       }
       else
       {
           msg_Warn( p_playlist, "Not supported yet");
       }
    }

    /* start playing */
    if ( i_current_status != PLAYLIST_STOPPED )
    {
        playlist_Play ( p_playlist );
    }

    fclose( file );

    return 0;
}

/*****************************************************************************
 * playlist_SaveFile: Save a playlist in a file.
 *****************************************************************************/
int playlist_SaveFile( playlist_t * p_playlist, const char * psz_filename )
{
    FILE *file;
    int i;

    vlc_mutex_lock( &p_playlist->object_lock );

    msg_Dbg( p_playlist, "saving playlist file %s", psz_filename );

    file = fopen( psz_filename, "wt" );
    if( !file )
    {
        msg_Err( p_playlist , "could not create playlist file %s"
                , psz_filename );
        return -1;
    }
    /* Save is done in 0_5 mode at the moment*/

    fprintf( file , PLAYLIST_FILE_HEADER_0_5 "\n" );

    for ( i = 0 ; i < p_playlist->i_size ; i++ )
    {
        fprintf( file , p_playlist->pp_items[i]->psz_uri );
        fprintf( file , "\n" );
    }
#if 0
    fprintf( file, PLAYLIST_FILE_HEADER_0_6 "\n" );

    for ( i=0 ; i< p_playlist->i_size ; i++ )
    {
        fprintf( file, p_playlist->pp_items[i]->psz_uri );
        fprintf( file, "||" );
        fprintf( file, p_playlist->pp_items[i]->psz_name );
        fprintf( file, "||" );
        fprintf( file, "%i",p_playlist->pp_items[i]->b_enabled = VLC_TRUE ?
                       1:0 );
        fprintf( file, "||" );
        fprintf( file, "%i", p_playlist->pp_items[i]->i_group );
        fprintf( file, "||" );
        fprintf( file, p_playlist->pp_items[i]->psz_author );
        fprintf( file , "\n" );
    }
#endif
    fclose( file );

    vlc_mutex_unlock( &p_playlist->object_lock );

    return 0;
}
