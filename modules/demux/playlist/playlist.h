/*****************************************************************************
 * playlist.h:  Playlist import module common functions
 *****************************************************************************
 * Copyright (C) 2004 the VideoLAN team
 * $Id$
 *
 * Authors: Sigmund Augdal Helberg <dnumgis@videolan.org>
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

char *E_(ProcessMRL)( char *, char * );
char *E_(FindPrefix)( demux_t * );

vlc_bool_t E_(FindItem)( demux_t *, playlist_t *, playlist_item_t **);

void E_(AddToPlaylist)( demux_t *, playlist_t*,input_item_t*,playlist_item_t*,int );

int E_(Import_Old) ( vlc_object_t * );

int E_(Import_Native) ( vlc_object_t * );
void E_(Close_Native) ( vlc_object_t * );

int E_(Import_M3U) ( vlc_object_t * );
void E_(Close_M3U) ( vlc_object_t * );

int E_(Import_PLS) ( vlc_object_t * );
void E_(Close_PLS) ( vlc_object_t * );

int E_(Import_B4S) ( vlc_object_t * );
void E_(Close_B4S) ( vlc_object_t * );

int E_(Import_DVB) ( vlc_object_t * );
void E_(Close_DVB) ( vlc_object_t * );

int E_(Import_podcast) ( vlc_object_t * );
void E_(Close_podcast) ( vlc_object_t * );

int E_(xspf_import_Activate) ( vlc_object_t * );

int E_(Import_Shoutcast) ( vlc_object_t * );
void E_(Close_Shoutcast) ( vlc_object_t * );

#define INIT_PLAYLIST_STUFF \
    int i_parent_id; \
    vlc_bool_t b_play; \
    playlist_item_t *p_current, *p_item_in_category = NULL; \
    input_item_t *p_input; \
    playlist_t *p_playlist = (playlist_t *) vlc_object_find( p_demux, \
                                        VLC_OBJECT_PLAYLIST, FIND_ANYWHERE ); \
    if( !p_playlist ) \
    { \
        msg_Err( p_demux, "can't find playlist" ); \
        return VLC_EGENERIC; \
    } \
    i_parent_id = var_CreateGetInteger( p_demux, "parent-item" ); \
    if( i_parent_id > 0 ) \
    { \
        b_play = VLC_FALSE;     \
        p_current = playlist_ItemGetById( p_playlist, i_parent_id );    \
    } \
    else \
    { \
        b_play = E_(FindItem)( p_demux, p_playlist, &p_current ); \
        p_item_in_category = playlist_ItemToNode( p_playlist, p_current ); \
        p_current->p_input->i_type = ITEM_TYPE_PLAYLIST;        \
    }

#define HANDLE_PLAY_AND_RELEASE \
    /* Go back and play the playlist */ \
    if( b_play && p_playlist->status.p_item && \
                  p_playlist->status.p_item->i_children > 0 ) \
    { \
        playlist_Control( p_playlist, PLAYLIST_VIEWPLAY, 1242, \
                          p_playlist->status.p_item, NULL ); \
    } \
    vlc_object_release( p_playlist );

