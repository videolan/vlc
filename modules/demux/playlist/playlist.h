/*****************************************************************************
 * playlist.h:  Playlist import module common functions
 *****************************************************************************
 * Copyright (C) 2004 VLC authors and VideoLAN
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

char *ProcessMRL( const char *, const char * );

int Import_M3U ( vlc_object_t * );

int Import_RAM ( vlc_object_t * );

int Import_PLS ( vlc_object_t * );

int Import_B4S ( vlc_object_t * );

int Import_DVB ( vlc_object_t * );

int Import_podcast ( vlc_object_t * );

int Import_xspf ( vlc_object_t * );
void Close_xspf ( vlc_object_t * );

int Import_Shoutcast ( vlc_object_t * );

int Import_ASX ( vlc_object_t * );

int Import_SGIMB ( vlc_object_t * );
void Close_SGIMB ( vlc_object_t * );

int Import_QTL ( vlc_object_t * );

int Import_IFO ( vlc_object_t * );
void Close_IFO ( vlc_object_t * );

int Import_BDMV ( vlc_object_t * );
void Close_BDMV ( vlc_object_t * );

int Import_iTML ( vlc_object_t * );

int Import_WMS(vlc_object_t *);

int Import_WPL ( vlc_object_t * );
void Close_WPL ( vlc_object_t * );

#define GetCurrentItem(obj) ((obj)->p_input_item)
#define GetSource(obj) ((obj)->s)

#define CHECK_FILE(obj) \
do { \
    if( GetSource(obj)->pf_readdir != NULL ) \
        return VLC_EGENERIC; \
} while(0)
