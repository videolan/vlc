/*****************************************************************************
 * core.c: Core libvlc new API functions : initialization, exceptions handling
 *****************************************************************************
 * Copyright (C) 2005 the VideoLAN team
 * $Id$
 *
 * Authors: Cl�ent Stenac <zorglub@videolan.org>
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
#include <stdarg.h>
#include <libvlc_internal.h>
#include <vlc/libvlc.h>

#include <vlc/intf.h>

/*************************************************************************
 * Exceptions handling
 *************************************************************************/
inline void libvlc_exception_init( libvlc_exception_t *p_exception )
{
    p_exception->b_raised = 0;
    p_exception->psz_message = NULL;
}

void libvlc_exception_clear( libvlc_exception_t *p_exception )
{
    if( p_exception->psz_message )
        free( p_exception->psz_message );
    p_exception->psz_message = NULL;
    p_exception->b_raised = 0;
}

inline int libvlc_exception_raised( libvlc_exception_t *p_exception )
{
    return p_exception->b_raised;
}

inline char* libvlc_exception_get_message( libvlc_exception_t *p_exception )
{
    if( p_exception->b_raised == 1 && p_exception->psz_message )
    {
        return p_exception->psz_message;
    }
    return NULL;
}

inline void libvlc_exception_raise( libvlc_exception_t *p_exception,
                                    char *psz_format, ... )
{
    va_list args;
    char *psz_message;
    va_start( args, p_exception->psz_message );
    vasprintf( &p_exception->psz_message, psz_format, args );
    va_end( args );

    if( p_exception == NULL ) return;
    p_exception->b_raised = 1;
}

libvlc_instance_t * libvlc_new( int argc, char **argv,
                                libvlc_exception_t *p_exception )
{
    int i_vlc_id;
    libvlc_instance_t *p_new;
    vlc_t *p_vlc;

    i_vlc_id = VLC_Create();
    p_vlc = (vlc_t* ) vlc_current_object( i_vlc_id );

    if( !p_vlc )
    {
        libvlc_exception_raise( p_exception, "VLC initialization failed" );
        return NULL;
    }
    p_new = (libvlc_instance_t *)malloc( sizeof( libvlc_instance_t ) );

    /** \todo Look for interface settings. If we don't have any, add -I dummy */
    /* Because we probably don't want a GUI by default */

    if( !p_new )
    {
        libvlc_exception_raise( p_exception, "Out of memory" );
        return NULL;
    }

    VLC_Init( i_vlc_id, argc, argv );

    p_new->p_vlc = p_vlc;
    p_new->p_playlist = (playlist_t *)vlc_object_find( p_new->p_vlc,
                                VLC_OBJECT_PLAYLIST, FIND_CHILD );
    p_new->p_vlm = NULL;

    if( !p_new->p_playlist )
    {
        libvlc_exception_raise( p_exception, "Playlist creation failed" );
        return NULL;
    }
    p_new->i_vlc_id = i_vlc_id;

    return p_new;
}

void libvlc_destroy( libvlc_instance_t *p_instance )
{
    if( p_instance->p_playlist )
        vlc_object_release( p_instance->p_playlist );
    vlc_object_release( p_instance->p_vlc );
    VLC_CleanUp( p_instance->i_vlc_id );
    VLC_Destroy( p_instance->i_vlc_id );
}
