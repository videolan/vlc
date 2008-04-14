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

#include <vlc_playlist.h>
char *E_(ProcessMRL)( char *, char * );
char *E_(FindPrefix)( demux_t * );

bool E_(FindItem)( demux_t *, playlist_t *, playlist_item_t **);

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

int E_(Import_xspf) ( vlc_object_t * );
void E_(Close_xspf) ( vlc_object_t * );

int E_(Import_Shoutcast) ( vlc_object_t * );
void E_(Close_Shoutcast) ( vlc_object_t * );

int E_(Import_ASX) ( vlc_object_t * );
void E_(Close_ASX) ( vlc_object_t * );

int E_(Import_SGIMB) ( vlc_object_t * );
void E_(Close_SGIMB) ( vlc_object_t * );

int E_(Import_QTL) ( vlc_object_t * );
void E_(Close_QTL) ( vlc_object_t * );

int E_(Import_GVP) ( vlc_object_t * );
void E_(Close_GVP) ( vlc_object_t * );

int E_(Import_IFO) ( vlc_object_t * );
void E_(Close_IFO) ( vlc_object_t * );

int E_(Import_VideoPortal) ( vlc_object_t * );
void E_(Close_VideoPortal) ( vlc_object_t * );

int E_(Import_iTML) ( vlc_object_t * );
void E_(Close_iTML) ( vlc_object_t * );

#define INIT_PLAYLIST_STUFF \
    playlist_t *p_playlist = pl_Yield( p_demux ); \
    input_thread_t *p_input_thread = (input_thread_t *)vlc_object_find( p_demux, VLC_OBJECT_INPUT, FIND_PARENT ); \
    input_item_t *p_current_input = input_GetItem( p_input_thread );

#define HANDLE_PLAY_AND_RELEASE \
    vlc_object_release( p_input_thread ); \
    vlc_object_release( p_playlist );

