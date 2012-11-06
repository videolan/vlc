/*****************************************************************************
 * playlist.h:  Playlist import module common functions
 *****************************************************************************
 * Copyright (C) 2004 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Sigmund Augdal Helberg <dnumgis@videolan.org>
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

#include <vlc_input.h>
#include <vlc_playlist.h>

int Control(demux_t *, int, va_list);
char *ProcessMRL( const char *, const char * );
char *FindPrefix( demux_t * );

int Import_Old ( vlc_object_t * );

int Import_Native ( vlc_object_t * );
void Close_Native ( vlc_object_t * );

int Import_M3U ( vlc_object_t * );
void Close_M3U ( vlc_object_t * );

int Import_RAM ( vlc_object_t * );
void Close_RAM ( vlc_object_t * );

int Import_PLS ( vlc_object_t * );
void Close_PLS ( vlc_object_t * );

int Import_B4S ( vlc_object_t * );

int Import_DVB ( vlc_object_t * );

int Import_podcast ( vlc_object_t * );

int Import_xspf ( vlc_object_t * );
void Close_xspf ( vlc_object_t * );

int Import_Shoutcast ( vlc_object_t * );

int Import_ASX ( vlc_object_t * );
void Close_ASX ( vlc_object_t * );

int Import_SGIMB ( vlc_object_t * );
void Close_SGIMB ( vlc_object_t * );

int Import_QTL ( vlc_object_t * );

int Import_GVP ( vlc_object_t * );
void Close_GVP ( vlc_object_t * );

int Import_IFO ( vlc_object_t * );
void Close_IFO ( vlc_object_t * );

int Import_VideoPortal ( vlc_object_t * );
void Close_VideoPortal ( vlc_object_t * );

int Import_iTML ( vlc_object_t * );
void Close_iTML ( vlc_object_t * );

int Import_WPL ( vlc_object_t * );
void Close_WPL ( vlc_object_t * );

int Import_ZPL ( vlc_object_t * );
void Close_ZPL ( vlc_object_t * );

extern input_item_t * GetCurrentItem(demux_t *p_demux);

#define STANDARD_DEMUX_INIT_MSG( msg ) do { \
    DEMUX_INIT_COMMON();                    \
    msg_Dbg( p_demux, "%s", msg ); } while(0)

#define DEMUX_BY_EXTENSION_MSG( ext, msg ) \
    demux_t *p_demux = (demux_t *)p_this; \
    if( !demux_IsPathExtension( p_demux, ext ) ) \
        return VLC_EGENERIC; \
    STANDARD_DEMUX_INIT_MSG( msg );

#define DEMUX_BY_EXTENSION_OR_FORCED_MSG( ext, module, msg ) \
    demux_t *p_demux = (demux_t *)p_this; \
    if( !demux_IsPathExtension( p_demux, ext ) && !demux_IsForced( p_demux, module ) ) \
        return VLC_EGENERIC; \
    STANDARD_DEMUX_INIT_MSG( msg );


#define CHECK_PEEK( zepeek, size ) do { \
    if( stream_Peek( p_demux->s , &zepeek, size ) < size ){ \
        msg_Dbg( p_demux, "not enough data" ); return VLC_EGENERIC; } } while(0)

#define POKE( peek, stuff, size ) (strncasecmp( (const char *)peek, stuff, size )==0)

