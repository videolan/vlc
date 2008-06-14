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

#include <vlc_input.h>

char *ProcessMRL( char *, char * );
char *FindPrefix( demux_t * );

int Import_Old ( vlc_object_t * );

int Import_Native ( vlc_object_t * );
void Close_Native ( vlc_object_t * );

int Import_M3U ( vlc_object_t * );
void Close_M3U ( vlc_object_t * );

int Import_PLS ( vlc_object_t * );
void Close_PLS ( vlc_object_t * );

int Import_B4S ( vlc_object_t * );
void Close_B4S ( vlc_object_t * );

int Import_DVB ( vlc_object_t * );
void Close_DVB ( vlc_object_t * );

int Import_podcast ( vlc_object_t * );
void Close_podcast ( vlc_object_t * );

int Import_xspf ( vlc_object_t * );
void Close_xspf ( vlc_object_t * );

int Import_Shoutcast ( vlc_object_t * );
void Close_Shoutcast ( vlc_object_t * );

int Import_ASX ( vlc_object_t * );
void Close_ASX ( vlc_object_t * );

int Import_SGIMB ( vlc_object_t * );
void Close_SGIMB ( vlc_object_t * );

int Import_QTL ( vlc_object_t * );
void Close_QTL ( vlc_object_t * );

int Import_GVP ( vlc_object_t * );
void Close_GVP ( vlc_object_t * );

int Import_IFO ( vlc_object_t * );
void Close_IFO ( vlc_object_t * );

int Import_VideoPortal ( vlc_object_t * );
void Close_VideoPortal ( vlc_object_t * );

int Import_iTML ( vlc_object_t * );
void Close_iTML ( vlc_object_t * );

#define INIT_PLAYLIST_STUFF \
    input_thread_t *p_input_thread = (input_thread_t *)vlc_object_find( p_demux, VLC_OBJECT_INPUT, FIND_PARENT ); \
    input_item_t *p_current_input = input_GetItem( p_input_thread );

#define HANDLE_PLAY_AND_RELEASE \
    vlc_object_release( p_input_thread );
