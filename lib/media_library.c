/*****************************************************************************
 * media_library.c: libvlc tags tree functions
 * Create a tree of the 'tags' of a media_list's medias.
 *****************************************************************************
 * Copyright (C) 2007 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Pierre d'Herbemont <pdherbemont # videolan.org>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc/libvlc.h>
#include <vlc/libvlc_media.h>
#include <vlc/libvlc_media_list.h>
#include <vlc/libvlc_media_library.h>
#include <vlc/libvlc_events.h>

#include <vlc_common.h>

#include "libvlc_internal.h"

struct libvlc_media_library_t
{
    libvlc_event_manager_t * p_event_manager;
    libvlc_instance_t *      p_libvlc_instance;
    int                      i_refcount;
    libvlc_media_list_t *    p_mlist;
};


/*
 * Private functions
 */

/*
 * Public libvlc functions
 */

/**************************************************************************
 *       new (Public)
 **************************************************************************/
libvlc_media_library_t *
libvlc_media_library_new( libvlc_instance_t * p_inst )
{
    libvlc_media_library_t * p_mlib;

    p_mlib = malloc(sizeof(libvlc_media_library_t));

    if( !p_mlib )
    {
        libvlc_printerr( "Not enough memory" );
        return NULL;
    }

    p_mlib->p_libvlc_instance = p_inst;
    p_mlib->i_refcount = 1;
    p_mlib->p_mlist = NULL;

    p_mlib->p_event_manager = libvlc_event_manager_new( p_mlib, p_inst );
    if( unlikely(p_mlib->p_event_manager == NULL) )
    {
        free(p_mlib);
        return NULL;
    }

    return p_mlib;
}

/**************************************************************************
 *       release (Public)
 **************************************************************************/
void libvlc_media_library_release( libvlc_media_library_t * p_mlib )
{
    p_mlib->i_refcount--;

    if( p_mlib->i_refcount > 0 )
        return;

    libvlc_event_manager_release( p_mlib->p_event_manager );
    free( p_mlib );
}

/**************************************************************************
 *       retain (Public)
 **************************************************************************/
void libvlc_media_library_retain( libvlc_media_library_t * p_mlib )
{
    p_mlib->i_refcount++;
}

/**************************************************************************
 *       load (Public)
 *
 * It doesn't yet load the playlists
 **************************************************************************/
int libvlc_media_library_load( libvlc_media_library_t * p_mlib )
{
    char *psz_datadir = config_GetUserDir( VLC_DATA_DIR );
    char * psz_uri;

    if( psz_datadir == NULL
     || asprintf( &psz_uri, "file/xspf-open://%s" DIR_SEP "ml.xsp",
                  psz_datadir ) == -1 )
        psz_uri = NULL;
    free( psz_datadir );

    if( psz_uri == NULL )
    {
        libvlc_printerr( "Not enough memory" );
        return -1;
    }

    if( p_mlib->p_mlist )
        libvlc_media_list_release( p_mlib->p_mlist );

    p_mlib->p_mlist = libvlc_media_list_new( p_mlib->p_libvlc_instance );

    int ret = libvlc_media_list_add_file_content( p_mlib->p_mlist, psz_uri );
    free( psz_uri );
    return ret;
}

/**************************************************************************
 *        media_list (Public)
 **************************************************************************/
libvlc_media_list_t *
libvlc_media_library_media_list( libvlc_media_library_t * p_mlib )
{
    if( p_mlib->p_mlist )
        libvlc_media_list_retain( p_mlib->p_mlist );
    return p_mlib->p_mlist;
}
